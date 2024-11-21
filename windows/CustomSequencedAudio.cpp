#include "CustomSequencedAudio.h"
#include "imgui.h"
#include "imgui_toggle.h"
#include "WindowMgr.h"
#include "filebox.h"
#include "archive.h"
#include "zip_archive.h"
#include "mpq_archive.h"
#include <algorithm>
#include <thread>
#include <sstream>
#include <vector>
#include "mio.hpp"
#include "tinyxml2.h"

CustomSequencedAudioWindow::CustomSequencedAudioWindow() {

}

CustomSequencedAudioWindow::~CustomSequencedAudioWindow() {
    for (const auto c : mFileQueue) {
        operator delete[] (c);
    }
    mFileQueue.clear();
}

void CustomSequencedAudioWindow::ClearPathBuff()
{
    if (mPathBuff != nullptr) {
        delete[] mPathBuff;
        mPathBuff = nullptr;
    }
}

void CustomSequencedAudioWindow::ClearSaveBuff()
{
    if (mSavePath != nullptr) {
        delete[] mSavePath;
        mSavePath = nullptr;
    }
}

static bool FillSeqFileCallback(char* path) {
    char* ext = strrchr(path, '.');
    if (ext != nullptr) {
        if ((strcmp(ext, ".seq") == 0 || strcmp(ext, ".aseq") == 0 || strcmp(ext, ".meta") == 0)) {
            return true;
        }
    }
    return false;
}

static bool FillMMRSFileCallback(char* path) {
    char* ext = strrchr(path, '.');
    if (ext != nullptr) {
        return strcmp(ext, ".mmrs") == 0;
    }
    return false;
}

static bool IsSeqExt(const char* ext) {
    return strcmp(ext, ".zseq") == 0 || strcmp(ext, ".aseq") == 0 || strcmp(ext, ".seq") == 0;
}

static bool IsFontExt(const char* ext) {
    return strcmp(ext, ".zbank") == 0;
}

static bool IsNoteExt(const char* ext) {
    return strcmp(ext, ".zsound") == 0;
}

void CustomSequencedAudioWindow::CreateFilePairs() {
    // There must be a pair of files for each song. If there is an odd number of files there must be an issue.
    if (mFileQueue.size() % 2 != 0) {
        pairCheckState = CheckState::Bad;
        return;
    }
    std::sort(mFileQueue.begin(), mFileQueue.end(), [](char* a, char* b) {return strcmp(a, b) < 0; });
    for (size_t i = 0; i < mFileQueue.size(); i += 2) {
        // First make sure the extensions are not the same. If they are, then there are either two meta or two sequences, meaning the other is missing.
        // IE the folder contains the files song1.meta and song2.meta but song1 doesn't have a .seq file to go with it.
        char* aExt = strrchr(mFileQueue[i], '.');
        char* bExt = strrchr(mFileQueue[i + 1], '.');
        if (strcmp(aExt, bExt) != 0) {
            // Next, make sure the file names before the extension are the same. If so they are 'pairs'
            if (std::equal(mFileQueue[i], aExt, mFileQueue[i + 1], bExt)) {
                // first is the meta file, second is the sequence. There are two possible extensions for sequences so check against meta
                if (strcmp(aExt, ".meta") == 0) {
                    mFilePairs.push_back({ mFileQueue[i], mFileQueue[i + 1] });
                }
                else {
                    mFilePairs.push_back({ mFileQueue[i + 1], mFileQueue[i] });
                }
            }
            else {
                pairCheckState = CheckState::Bad;
                mFilePairs.clear();
                return;
            }
        }
        else {
            pairCheckState = CheckState::Bad;
            mFilePairs.clear();
            return;
        }
    }
    pairCheckState = CheckState::Good;
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

typedef struct OTRHeader {
    uint8_t endianness;
    uint8_t padding_1[3];
    uint32_t resType;
    uint32_t resVersion;
    uint32_t id1; // This is technically against the OTR spec, but there is no good w
    uint32_t id2;
    uint64_t romCrc;
    uint32_t romEnum;
    uint8_t padding_40[0x40 - 0x28];
}OTRHeader;

static void PackFilesMgrWorker(std::vector<std::pair<char*, char*>>* fileQueue, bool* threadStarted, bool* threadDone, CustomSequencedAudioWindow* thisx) {
    std::unique_ptr<Archive> a;
    constexpr const char PATH_BASE[] = "custom/music";
    // 0 means don't create an archive and instead move the files into folders
    if (thisx->GetRadioState() == 0) {
        CreateDir(PATH_BASE);
    }
    else {
        switch (thisx->GetRadioState()) {
        case 1: {
            a = std::make_unique<MpqArchive>(thisx->GetSavePath());
            break;
        }
        case 2: {
            a = std::make_unique<ZipArchive>(thisx->GetSavePath());
            break;
        }
        }
    }
    for (auto& p : *fileQueue) {
        constexpr int ZERO = 0;
        constexpr int ONE = 1;
        constexpr int TWO = 2;
        bool isFanfare = false;
        // TODO get rid of this once both games support XML sequences
        char buffer[260];
        char name[260];
        FILE* metaFile = fopen(p.first, "r");

        mio::mmap_source seqFile(p.second);
        size_t SeqSize = seqFile.size();
        
        // The .meta file always follows a structure of:
        // line 1: Friendly name
        // line 2: SoundFont Index
        // line 3: bgm/fanfare (defaults to bgm if not specified)
        fgets(name, sizeof(name), metaFile);
        fgets(buffer, sizeof(buffer), metaFile);
        int font = strtol(buffer, nullptr, 16);
        if (fgets(buffer, sizeof(buffer), metaFile) != nullptr) {
            if (strcmp(buffer, "fanfare") == 0) {
                isFanfare = true;
            }
        }
        const OTRHeader header = {
            .endianness = 0,
            .resType = 0x4F534551, // OSEQ
            .resVersion = 2,
            .id1 = 0x07151129,
            .id2 = 0x07151129,
            .romCrc = 0,
            .romEnum = 0,
        };

        size_t outFileNameLen = strlen(name);
        name[outFileNameLen - 1] = 0;
        auto newName = std::make_unique<char[]>(outFileNameLen + sizeof(PATH_BASE));
        snprintf(newName.get(), outFileNameLen + sizeof(PATH_BASE), "%s/%s", PATH_BASE, name);

        std::stringstream stream;
        stream.write((const char*)&header, sizeof(header));
        stream.write((const char*)&SeqSize, 4);
        stream.write((const char*)seqFile.data(), seqFile.size());
        stream.write((const char*)&ZERO, 1);
        stream.write((const char*)&TWO, 1);
        stream.write((const char*)&TWO, 1);
        stream.write((const char*)&ONE, 4);
        stream.write((const char*)&font, 1);
        
        char* rawData = new char[stream.tellp()];
        stream.read(rawData, stream.tellp());

        WriteFileData(newName.get(), rawData, stream.tellg(), a.get());
    }

    if (thisx->GetRadioState() != 0) {
        a->CloseArchive();
    }
    *threadStarted = false;
    *threadDone = true;
}

static void PackMMRSFilesMgrWorker(std::vector<char*>* fileQueue, bool* threadStarted, bool* threadDone, CustomSequencedAudioWindow* thisx) {
    std::unique_ptr<Archive> a;
    constexpr const char PATH_BASE[] = "custom/music/";
    constexpr static const char seqDataBase[] = "custom/sequenceData/";

    // 0 means don't create an archive and instead move the files into folders
    if (thisx->GetRadioState() == 0) {
        CreateDir(PATH_BASE);
        CreateDir(seqDataBase);
    }
    else {
        switch (thisx->GetRadioState()) {
        case 1: {
            a = std::make_unique<MpqArchive>(thisx->GetSavePath());
            break;
        }
        case 2: {
            a = std::make_unique<ZipArchive>(thisx->GetSavePath());
            break;
        }
        }
    }

    for (auto f : *fileQueue) {
        zip_t* mmrsFile = zip_open(f, ZIP_RDONLY, nullptr);
        int numFiles = zip_get_num_entries(mmrsFile, ZIP_FL_UNCHANGED);
        char* seqPath = nullptr;
        char* fontPath = nullptr;
        char* notePath = nullptr;

        for (int i = 0; i < numFiles; i++) {
            const char* file = zip_get_name(mmrsFile, i, ZIP_FL_ENC_RAW);
            const char* ext = strrchr(file, '.');
            if (IsSeqExt(ext)) {
                seqPath = const_cast<char*>(file);
            }
            else if (IsFontExt(ext)) {
                fontPath = const_cast<char*>(file);
            }
            else if (IsNoteExt(ext)) {
                notePath = const_cast<char*>(file);
            }
        }
        if (seqPath == nullptr) {
            printf("No sequence found in archive %s. Skipping...\n", f);
            zip_close(mmrsFile);
            continue;
        }
        char* seqPathExt = strrchr(seqPath, '.');
        int fontIdx = strtol(seqPath, &seqPathExt, 16);
        if (fontPath != nullptr || notePath != nullptr) {
            printf("Only sequences are supported right now. : %s\n", f);
            zip_close(mmrsFile);
            continue;
        }
        // We don't need the extension anymore so remove it.
        char* mmrsExt = strrchr(f, '.');
        *mmrsExt = 0;
        // Remote the beginning of the path which may include the disk and \ from the FS.
        // We don't want to replace `f` because the pointer needs to stay the same so we can delete it later.
        char* name = strrchr(f, PATH_SEPARATOR);
        name++;
        zip_stat_t stat;
        zip_stat(mmrsFile, seqPath, 0, &stat);
        void* seqData = malloc(stat.size);
        zip_file_t* seqZipFile = zip_fopen(mmrsFile, seqPath, 0);
        zip_fread(seqZipFile, seqData, stat.size);
        zip_fclose(seqZipFile);

        tinyxml2::XMLPrinter p;
        tinyxml2::XMLDocument seqDoc;
        seqDoc.LoadFile("assets/seq-base.xml");

        tinyxml2::XMLElement* root = seqDoc.FirstChildElement();
        root->SetAttribute("Size", (uint64_t)stat.size);
        root->SetAttribute("Streamed", false);
        // Write the font index
        tinyxml2::XMLElement* fontIndiciesElement = root->FirstChildElement("FontIndicies");
        tinyxml2::XMLElement* fontIdxElement = fontIndiciesElement->InsertNewChildElement("FontIndex");
        fontIdxElement->SetAttribute("FontIdx", fontIdx);
        fontIndiciesElement->InsertEndChild(fontIdxElement);

        // Write the raw sequence data.
        size_t seqDataZipPathLen = sizeof(seqDataBase) + strlen(name) + sizeof("_RAW") + 1;
        auto seqDataZipPath = std::make_unique<char[]>(seqDataZipPathLen);
        snprintf(seqDataZipPath.get(), seqDataZipPathLen, "%s%s_RAW", seqDataBase, name);
        WriteFileData(seqDataZipPath.get(), seqData, stat.size, a.get());

        // Write the path to the raw sequence data to the META xml file.
        root->SetAttribute("Path", seqDataZipPath.get());
        seqDoc.InsertEndChild(root);
        seqDoc.Accept(&p);

        // Write the META xml file.
        size_t seqXMLZipPathLen = sizeof(PATH_BASE) + strlen(name) + sizeof("_META") + 1;
        auto seqXMLZipPath = std::make_unique<char[]>(seqXMLZipPathLen);
        snprintf(seqXMLZipPath.get(), seqXMLZipPathLen, "%s%s_META", PATH_BASE, name);
        WriteFileData(seqXMLZipPath.get(), (void*)p.CStr(), p.CStrSize(), a.get());
        zip_close(mmrsFile);
    }

    if (thisx->GetRadioState() != 0) {
        a->CloseArchive();
    }
    *threadStarted = false;
    *threadDone = true;
}

int CustomSequencedAudioWindow::GetRadioState() {
    return mRadioState;
}

char* CustomSequencedAudioWindow::GetSavePath() {
    return mSavePath;
}

static std::array<const char*, 2> sToggleLabels = {
    "Create Folder",
    "Create Archive",
};

void CustomSequencedAudioWindow::DrawWindow() {
    ImGui::Begin("Create Custom Sequenced Audio", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Main);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Create Archive With Sequenced Songs");
    ImGui::TextUnformatted("Open a directory and create an archive with the files needed for sequenced audio");

    if (ImGui::Button("Select Directory")) {
        ClearPathBuff();
        ClearSaveBuff();
        GetOpenDirPath(&mPathBuff);
        FillFileQueue(mFileQueue, mPathBuff, FillSeqFileCallback);
        FillFileQueue(mMMRSFiles, mPathBuff, FillMMRSFileCallback);
        CreateFilePairs();

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

    if ((mThreadStarted && !mThreadIsDone) || (mMMRSThreadStarted && !mMMRSThreadIsDone)) {
        ImGui::TextUnformatted("Packing files...");
        //ImGui::Text("Files processed %d\\%d", filesProcessed.load(), fileCount);
    }

    if (mSavePath != nullptr) {
        ImGui::SameLine();
        ImGui::Text("Archive save path: %s", mSavePath);
    }

    if ((!mFilePairs.empty() && pairCheckState == CheckState::Good) || (!mMMRSFiles.empty()) && mPathBuff != nullptr) {
        if (!mPackAsArchive || (mPackAsArchive && mSavePath != nullptr)) {
            if (ImGui::Button("Pack Archive")) {
                mThreadStarted = true;
                mThreadIsDone = false;
                std::thread packFilesMgrThread(PackFilesMgrWorker, &mFilePairs, &mThreadStarted, &mThreadIsDone, this);
                std::thread packMMRSFilesMgrThread(PackMMRSFilesMgrWorker, &mMMRSFiles, &mMMRSThreadStarted, &mMMRSThreadIsDone, this);
                packFilesMgrThread.detach();
                packMMRSFilesMgrThread.detach();
            }
        }
    }

    DrawPendingFilesList();

    ImGui::End();
}

void CustomSequencedAudioWindow::DrawPendingFilesList() {
    if (!mFilePairs.empty()) {
        ImGui::BeginTable("Pending Sequences", 2);
        ImGui::TextUnformatted("OOTR Sequences:");
        for (const auto& p : mFilePairs) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", p.first);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", p.second);
        }
        ImGui::EndTable();
    }
    if (!mMMRSFiles.empty()) {
        ImGui::TextUnformatted("MMR Sequences");
        for (const auto& f : mMMRSFiles) {
            ImGui::Text("%s", f);
        }
    }
}