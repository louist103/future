#include "CustomStreamedAudio.h"
#include "archive.h"
#include "zip_archive.h"
#include "mpq_archive.h"

#include "imgui.h"
#include "imgui_toggle.h"
#include "WindowMgr.h"
#include "filebox.h"
#include "CRC64.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

#include <ogg/ogg.h>
#include <vorbis/vorbisfile.h>

#include <tinyxml2.h>
#include <array>
#include <atomic>
#include <cmath>

#if defined (_WIN32)
#include <Windows.h>
#include <shlwapi.h>
#elif defined (__linux__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

CustomStreamedAudioWindow::CustomStreamedAudioWindow() {

}

CustomStreamedAudioWindow::~CustomStreamedAudioWindow() {
    for (size_t i = 0; i < mFileQueue.size(); i++) {
        delete[] mFileQueue[i];
    }
    mFileQueue.clear();
}

enum AudioType {
    mp3,
    wav,
    ogg,
    flac,
};

// 'ID3' as a string
#define MP3_ID3_CHECK(d) ((d[0] == 'I') && (d[1] == 'D') && (d[2] == '3'))
// FF FB, FF F2, FF F3
#define MP3_NON_ID3_CHECK(d) ((d[0] == 0xFF) && ((d[1] == 0xFB) || (d[1] == 0xF2) || (d[1] == 0xF3)))
#define MP3_CHECK(d) (MP3_ID3_CHECK(d) || MP3_NON_ID3_CHECK(d))

#define WAV_CHECK(d) ((d[0] == 'R') && (d[1] == 'I') && (d[2] == 'F') && (d[3] == 'F'))

#define FLAC_CHECK(d) ((d[0] == 'f') && (d[1] == 'L') && (d[2] == 'a') && (d[3] == 'C'))

#define OGG_CHECK(d) ((d[0] == 'O') && (d[1] == 'g') && (d[2] == 'g') && (d[3] == 'S'))

constexpr static std::array<const char*, 4> audioTypeToStr = {
    "mp3",
    "wav",
    "ogg",
    "flac",
};

static char* GetFileNameFromPath(char* input) {
    size_t len = strlen(input);
    for (size_t i = len; i > 0; i--) {
        if (input[i] == '/' || input[i] == '\\') {
            return &input[i + 1];
        }
    }
    return nullptr;
}

// Write `data` to either the archive, or if the archive is null, a file on disk
static void WriteFileData(char* path, void* data, size_t size, Archive* a) {
    if (a == nullptr) {
        FILE* file = fopen(path, "wb+");
        fwrite(data, size, 1, file);
        fclose(file);
    }
    else {
        const ArchiveDataInfo info = {
            .data = data, .size = size, .mode = DataCopy 
        };
        a->WriteFile(path, &info);
    }
}

static void CopySampleData(char* input, char* fileName, Archive* a) {
    constexpr static const char sampleDataBase[] = "custom/sampleData/";
    size_t sampleDataPathLen = sizeof(sampleDataBase) + strlen(fileName) + 1;
    auto sampleDataPath = std::make_unique<char[]>(sampleDataPathLen);
    snprintf(sampleDataPath.get(), sampleDataPathLen, "%s%s", sampleDataBase, fileName);
    if (a == nullptr) {
        CopyFileData(input, sampleDataPath.get());
    }
    else {
        void* data;
        size_t fileSize;
#if defined (_WIN32)
        HANDLE hFile = CreateFileA(input, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        data = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
        LARGE_INTEGER size;
        GetFileSizeEx(hFile, &size);
        fileSize = size.QuadPart;
        CloseHandle(hMap);
        CloseHandle(hFile);
#elif defined(__linux__)
        int fd = open(input, O_RDONLY);
        struct stat st;
        fstat(fd, &st);
        fileSize = st.st_size;
        data = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
#endif
        const ArchiveDataInfo info = {
            .data = data, .size = fileSize, .mode = MappedFile
        };
        a->WriteFile(sampleDataPath.get(), &info);
    }
}

static void CreateSampleXml(char* fileName, const char* audioType, uint64_t numFrames, uint64_t numChannels, Archive* a) {
    constexpr static const char sampleXmlBase[] = "custom/samples/";
    constexpr static const char sampleDataBase[] = "custom/sampleData/";

    tinyxml2::XMLDocument sampleBaseRoot;
    tinyxml2::XMLError e = sampleBaseRoot.LoadFile("assets/sample-base.xml");
    assert(e == 0);
    
    size_t sampleDataSize = sizeof(sampleDataBase) + strlen(fileName) + 1;
    auto sampleDataPath = std::make_unique<char[]>(sampleDataSize);
    snprintf(sampleDataPath.get(), sampleDataSize, "%s%s", sampleDataBase, fileName);

    // Fill in sample XML
    tinyxml2::XMLPrinter p;
    tinyxml2::XMLElement* root = sampleBaseRoot.RootElement();
    root->SetAttribute("LoopEnd", numFrames * numChannels);
    root->SetAttribute("CustomFormat", audioType);
    root->SetAttribute("SampleSize", numFrames * numChannels * 2);
    root->SetAttribute("SamplePath", sampleDataPath.get());
    size_t samplePathLen = sizeof(sampleXmlBase) + strlen(fileName) + sizeof("_SAMPLE.xml") + 1;
    std::unique_ptr<char[]> sampleXmlPath = std::make_unique<char[]>(samplePathLen);
    snprintf(sampleXmlPath.get(), samplePathLen, "%s%s_SAMPLE.xml", sampleXmlBase, fileName);
    root->Accept(&p);

    WriteFileData(sampleXmlPath.get(), (void*)p.CStr(), p.CStrSize() - 1, a);

    p.ClearBuffer();
}

static void CreateSequenceXml(char* fileName, char* fontPath, unsigned int length, Archive* a) {
    constexpr static const char fontXmlBase[] = "custom/music/";
    tinyxml2::XMLDocument seqBaseRoot;
    tinyxml2::XMLError e = seqBaseRoot.LoadFile("assets/seq-base.xml");
    assert(e == 0);

    tinyxml2::XMLPrinter p;
    tinyxml2::XMLElement* root = seqBaseRoot.FirstChildElement();
    tinyxml2::XMLElement* fontIndiciesElement = root->FirstChildElement("FontIndicies");
    
    uint64_t crc = CRC64(fontPath);
    for (size_t i = 0; i < 8; i++) {
        tinyxml2::XMLElement* fontIdxElement = fontIndiciesElement->InsertNewChildElement("FontIndex");
        fontIdxElement->SetAttribute("FontIdx", ((uint8_t*)&crc)[i]);
        fontIndiciesElement->InsertEndChild(fontIdxElement);
    }
    root->SetAttribute("Length", length);
    size_t seqPathLen = sizeof(fontXmlBase) + strlen(fileName) + sizeof("_SEQ.xml");
    auto seqXmlPath = std::make_unique<char[]>(seqPathLen);
    snprintf(seqXmlPath.get(), seqPathLen, "%s%s_SEQ.xml", fontXmlBase, fileName);
    root->Accept(&p);

    WriteFileData(seqXmlPath.get(), (void*)p.CStr(), p.CStrSize() - 1, a);
    
    p.ClearBuffer();
}

static std::unique_ptr<char[]> CreateFontXml(char* fileName, uint64_t sampleRate, uint64_t channels, Archive* a) {
    constexpr static const char fontXmlBase[] = "custom/fonts/";
    constexpr static const char sampleNameBase[] = "custom/samples/";
    tinyxml2::XMLDocument fontBaseRoot;
    tinyxml2::XMLError e = fontBaseRoot.LoadFile("assets/font-base.xml");
    assert(e == 0);

    tinyxml2::XMLPrinter p;
    tinyxml2::XMLElement* root = fontBaseRoot.FirstChildElement();
    tinyxml2::XMLElement* instrumentsElement = root->FirstChildElement("Instruments");
    tinyxml2::XMLElement* instrumentElement = instrumentsElement->FirstChildElement("Instrument");
    tinyxml2::XMLElement* normalNoteElement = instrumentElement->FirstChildElement("NormalNotesSound");
    
    size_t sampleRefPathSize = sizeof(sampleNameBase) + strlen(fileName) + sizeof("_SAMPLE.xml") + 1;
    auto sampleRefPathXml = std::make_unique<char[]>(sampleRefPathSize);
    snprintf(sampleRefPathXml.get(), sampleRefPathSize, "%s%s_SAMPLE.xml", sampleNameBase, fileName);

    normalNoteElement->SetAttribute("SampleRef", sampleRefPathXml.get());
    normalNoteElement->SetAttribute("Tuning", ((float)sampleRate / 32000.0f) * channels);

    size_t fontPathLen = sizeof(fontXmlBase) + strlen(fileName) + sizeof("_FONT.xml");
    auto fontXmlPath = std::make_unique<char[]>(fontPathLen);

    snprintf(fontXmlPath.get(), fontPathLen, "%s%s_FONT.xml", fontXmlBase, fileName);
    root->Accept(&p);

    WriteFileData(fontXmlPath.get(), (void*)p.CStr(), p.CStrSize() - 1, a);

    p.ClearBuffer();

    return fontXmlPath;
}

struct OggFileData {
    void* data;
    size_t pos;
    size_t size;
};

static size_t VorbisReadCallback(void* out, size_t size, size_t elems, void* src) {
    OggFileData* data = static_cast<OggFileData*>(src);
    size_t toRead = size * elems;

    if (toRead > data->size - data->pos) {
        toRead = data->size - data->pos;
    }

    memcpy(out, static_cast<uint8_t*>(data->data) + data->pos, toRead);
    data->pos += toRead;
    
    return toRead / size;
}

static int VorbisSeekCallback(void* src, ogg_int64_t pos, int whence) {
    OggFileData* data = static_cast<OggFileData*>(src);
    size_t newPos;
    
    switch(whence) {
        case SEEK_SET:
            newPos = pos;
            break;
        case SEEK_CUR:
            newPos = data->pos + pos;
            break;
        case SEEK_END:
            newPos = data->size + pos;
            break;
        default:
            return -1;
    }
    if (newPos > data->size) {
        return -1;
    }
    data->pos = newPos;
    return 0;
}

static int VorbisCloseCallback([[maybe_unused]] void* src) {
    return 0;
}

static long VorbisTellCallback(void* src) {
    OggFileData* data = static_cast<OggFileData*>(src);
    return data->pos;
}

static const ov_callbacks cbs = {
    VorbisReadCallback,
    VorbisSeekCallback,
    VorbisCloseCallback,
    VorbisTellCallback,
};

// We don't want this to show less files than exist in the folder to pack when the operation is finished.
// `atomic` variables will ensure there isn't any inconsistency due to multi-threading.
static std::atomic<unsigned int> filesProcessed = 0;

static void ProcessAudioFile(SafeQueue<char*>* fileQueue, Archive* a) {
    while (!fileQueue->empty()) {
    filesProcessed++;
    char* input = fileQueue->pop();
    char* fileName = GetFileNameFromPath(input);
    void* data;
    uint8_t* dataU8;
    uint64_t numFrames;
    uint64_t numChannels;
    uint64_t sampleRate;
    int audioType;
    int fileSize;
    // Optimized file reading. Mapping files allows reading them as a buffer without
    // needing to allocate a buffer and copy the data in.
#if defined (_WIN32)
    HANDLE hFile = CreateFileA(input, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    data = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(hMap);
    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);
    CloseHandle(hFile);
    fileSize = size.QuadPart;
#elif defined(__linux__)
    int fd = open(input, O_RDONLY);
    struct stat st;
    fstat(fd, &st);
    fileSize = st.st_size;
    data = mmap(nullptr,st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
#endif
    dataU8 = (uint8_t*)data;

    // Read file header to determine which library needs to process it
    if (MP3_CHECK(dataU8)) {
        drmp3 mp3;
        audioType = AudioType::mp3;
        drmp3_init_memory(&mp3, data, fileSize, nullptr);
        numChannels = mp3.channels;
        sampleRate = mp3.sampleRate;
        numFrames = drmp3_get_pcm_frame_count(&mp3);
    } else if (WAV_CHECK(dataU8)) {
        drwav wav;
        audioType = AudioType::wav;
        drwav_init_memory(&wav, data, fileSize, nullptr);
        numChannels = wav.channels;
        sampleRate = wav.sampleRate;
        drwav_get_length_in_pcm_frames(&wav, (drwav_uint64*)&numFrames);
    } else if (FLAC_CHECK(dataU8)) {
        audioType = AudioType::flac;
        drflac* flac = drflac_open_memory(data, fileSize, nullptr);
        numChannels = flac->channels;
        sampleRate = flac->sampleRate;
        numFrames = flac->totalPCMFrameCount;
    } else if (OGG_CHECK(dataU8)) {
        audioType = AudioType::ogg;
        OggVorbis_File vf;
        OggFileData fileData =  {
            .data = data,
            .pos = 0,
            .size = (size_t)fileSize,
        };
        int ret = ov_open_callbacks(&fileData, &vf, nullptr, 0, cbs);
        
        vorbis_info* vi = ov_info(&vf, -1);

        numFrames = ov_pcm_total(&vf, -1);
        sampleRate = vi->rate;
        numChannels = vi->channels;
        ov_clear(&vf);
    }
    else {
        goto end;
    }
    // Must be scoped due to goto statement
    {
        CreateSampleXml(fileName, audioTypeToStr[audioType], numFrames, numChannels, a);

        CopySampleData(input, fileName, a);

        // Fill in sound font XML
        auto fontXmlPath = CreateFontXml(fileName, sampleRate, numChannels, a);
        // There is no good way to determine the length of the song when we go to load it so we need to store the length in seconds.
        float lengthF = (float)numFrames / (float)sampleRate;
        lengthF = ceilf(lengthF);
        unsigned int length = static_cast<unsigned int>(lengthF);
        CreateSequenceXml(fileName, fontXmlPath.get(), length, a);
    }
end:

    UnmapFile(data, fileSize);
    }
}

void CustomStreamedAudioWindow::ClearPathBuff()
{
    if (mPathBuff != nullptr) {
        delete[] mPathBuff;
        mPathBuff = nullptr;
    }
}

void CustomStreamedAudioWindow::ClearSaveBuff()
{
    if (mSavePath != nullptr) {
        delete[] mSavePath;
        mSavePath = nullptr;
    }
}


static void PackFilesMgrWorker(SafeQueue<char*>* fileQueue, bool* threadStarted, bool* threadDone, CustomStreamedAudioWindow* thisx) {
    Archive* a = nullptr;
    // 0 means don't create an archive and instead move the files into folders
    if (thisx->GetRadioState() == 0) {
        CreateDir("custom");
        CreateDir("custom/fonts");
        CreateDir("custom/music");
        CreateDir("custom/sampleData");
        CreateDir("custom/samples");
    }
    else {
        switch (thisx->GetRadioState()) {
            case 1: {
                a = new MpqArchive(thisx->GetSavePath());
                break;
            }
            case 2: {
                a = new ZipArchive(thisx->GetSavePath());
                break;
            }
        }
    }

    const unsigned int numThreads = std::thread::hardware_concurrency();
    auto packFileThreads = std::make_unique<std::thread[]>(numThreads);
    for (unsigned int i = 0; i < numThreads; i++) {
        packFileThreads[i] = std::thread(ProcessAudioFile, fileQueue, a);
    }

    for (unsigned int i = 0; i < numThreads; i++) {
        packFileThreads[i].join();
    }
    a->CloseArchive();
    *threadStarted = false;
    *threadDone = true;
}

static std::array<const char*, 2> sToggleLabels = {
    "Create Folder",
    "Create Archive",
};

int CustomStreamedAudioWindow::GetRadioState() {
    return mRadioState;
}

char* CustomStreamedAudioWindow::GetSavePath() {
    return mSavePath;
}

void CustomStreamedAudioWindow::DrawWindow() {
    ImGui::Begin("Create Custom Streamed Audio", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    
    ImGui::BeginDisabled(mThreadStarted);
    
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Create);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Create Archive With Streamed Songs");
    ImGui::TextUnformatted("Open a directory and create an archive with the files needed for streamed audio");

    if (ImGui::Button("Select Directory")) {
        ClearPathBuff();
        ClearSaveBuff();
        GetOpenDirPath(&mPathBuff);
        FillFileQueue();
        fileCount = mFileQueue.size();
        mThreadIsDone = false;
    }

    if (mPathBuff != nullptr) {
        if (mThreadIsDone) {
            ImGui::Text("Packing complete. Files saved in the \"custom\" folder in the program's directory");
        }
        else {
            ImGui::SameLine();
            ImGui::Text("Path of files to Pack: %s", mPathBuff);
        }
    }

    ImGui::Toggle(sToggleLabels[mPackAsArchive], &mPackAsArchive, ImGuiToggleFlags_Animated, 1.0f); //TODO it doesn't animate.
    
    if (mPackAsArchive) {
        ImGui::SameLine();
        ImGui::RadioButton("OTR", &mRadioState, 1);
        ImGui::SameLine();
        ImGui::RadioButton("O2R", &mRadioState, 2);
    }
    else {
        mRadioState = 0;
    }
    if (mPackAsArchive) {
        if (ImGui::Button("Set Save Path")) {
            GetSaveFilePath(&mSavePath);
        }
    }

    if (mThreadStarted && !mThreadIsDone) {
        ImGui::TextUnformatted("Packing files...");
        ImGui::Text("Files processed %d\\%d", filesProcessed.load(), fileCount);
    }



    if (mSavePath != nullptr) {
        ImGui::SameLine();
        ImGui::Text("Archive save path: %s", mSavePath);
    }

    if (!mFileQueue.empty() && mPathBuff != nullptr) {
        if (!mPackAsArchive || (mPackAsArchive && mSavePath != nullptr)) {
            if (ImGui::Button("Pack Archive")) {
                mThreadStarted = true;
                mThreadIsDone = false;
                filesProcessed = 0;
                std::thread packFilesMgrThread(PackFilesMgrWorker, &mFileQueue, &mThreadStarted, &mThreadIsDone, this);
                packFilesMgrThread.detach();
            }
        }
    }

    ImGui::EndDisabled();
    
    //if (mThreadStarted) {
    //    ImGui::Text("Packing archive. Progress: %.0f %%", mProgress * 100.0);
    //}

    DrawPendingFilesList();



    ImGui::End();
}

void CustomStreamedAudioWindow::DrawPendingFilesList() {
    if (mFileQueue.empty()) {
        return;
    }

    const ImVec2 cursorPos = ImGui::GetCursorPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImVec2 childWindowSize = { windowSize.x - cursorPos.x, windowSize.y - cursorPos.y };

    ImGui::BeginChild("File List", childWindowSize, 0, 0);
    ImGui::TextUnformatted("Files to Pack:");
    const float windowHeight = ImGui::GetWindowHeight();

    for (size_t i = 0; i < mFileQueue.size(); i++) {
        const float scroll = ImGui::GetScrollY();
        const float cursorPosY = ImGui::GetCursorPosY();

        // Only draw elements that are on screen. This gives around 20x performance improvement.
        if ((cursorPosY < scroll) || (cursorPosY > (scroll + windowHeight))) {
            ImGui::NewLine();
            continue;
        }
        size_t basePathLen = strlen(mPathBuff);
        ImGui::Text("%s", &mFileQueue[i][basePathLen + 1]);
    }
    ImGui::EndChild();
}

void CustomStreamedAudioWindow::FillFileQueue() {
#ifdef _WIN32
    char oldWorkingDir[MAX_PATH];
    char oldWorkingDir2[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, oldWorkingDir);
    bool ret = SetCurrentDirectoryA(mPathBuff);
    GetCurrentDirectoryA(MAX_PATH, oldWorkingDir2);
    WIN32_FIND_DATAA ffd;
    HANDLE h = FindFirstFileA("*", &ffd);
    
    do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char* ext = PathFindExtensionA(ffd.cFileName);

            // Check for any standard N64 rom file extensions.
            if (ext != NULL && (strcmp(ext, ".wav") == 0 || strcmp(ext, ".ogg") == 0 || strcmp(ext, ".mp3") == 0) || strcmp(ext, ".flac") == 0) {
                size_t s1 = strlen(ffd.cFileName);
                size_t s2 = strlen(mPathBuff);
                size_t sizeToAlloc = s1 + s2 + 2;

                char* fullPath = new char[sizeToAlloc];
                snprintf(fullPath, sizeToAlloc, "%s\\%s", mPathBuff, ffd.cFileName);
                mFileQueue.push(fullPath);
            }
        }
    } while (FindNextFileA(h, &ffd) != 0);
    FindClose(h);
    SetCurrentDirectoryA(oldWorkingDir);
#elif unix
    // Change the current working directory to the path selected.
    char oldWorkingDir[PATH_MAX];
    getcwd(oldWorkingDir, PATH_MAX);
    chdir(mPathBuff);
    DIR* d = opendir(".");

    struct dirent* dir;

    if (d != nullptr) {
        // Go through each file in the directory
        while ((dir = readdir(d)) != nullptr) {
            struct stat path;

            // Check if current entry is not folder
            stat(dir->d_name, &path);
            if (S_ISREG(path.st_mode)) {

                // Get the position of the extension character.
                char* ext = strrchr(dir->d_name, '.');
                if (ext == nullptr) continue;
                if ((strcmp(ext, ".wav") == 0 || strcmp(ext, ".ogg") == 0 || strcmp(ext, ".mp3") == 0) || strcmp(ext, ".flac") == 0) {
                    size_t s1 = strlen(dir->d_name);
                    size_t s2 = strlen(mPathBuff);
                    size_t sizeToAlloc = s1 + s2 + 2;

                    char* fullPath = new char[sizeToAlloc];
                    
                    snprintf(fullPath, sizeToAlloc, "%s%s", mPathBuff, dir->d_name);
                    mFileQueue.push(fullPath);
                }
            }
        }
    }
    closedir(d);
    chdir(oldWorkingDir);
#else
    for (const auto& file : std::filesystem::directory_iterator("./")) {
        if (file.is_directory())
            continue;
        if ((file.path().extension() == ".wav") || (file.path().extension() == ".ogg") ||
            (file.path().extension() == ".mp3") || file.path().extension() == ".flac") {
            mFileQueue.push(strdup((file.path().string().c_str()));
        }
    }
#endif
}