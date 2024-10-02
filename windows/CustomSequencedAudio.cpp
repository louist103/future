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

CustomSequencedAudioWindow::CustomSequencedAudioWindow() {

}

CustomSequencedAudioWindow::~CustomSequencedAudioWindow() {
    for (const auto c : mFileQueue) {
        delete[] c;
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

static bool FillFileCallback(char* path) {
    char* ext = strrchr(path, '.');
    if (ext != nullptr) {
        if ((strcmp(ext, ".seq") == 0 || strcmp(ext, ".aseq") == 0 || strcmp(ext, ".meta") == 0)) {
            return true;
        }
    }
    return false;
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
    Archive* a = nullptr;
    constexpr const char PATH_BASE[] = "custom/music";
    // 0 means don't create an archive and instead move the files into folders
    if (thisx->GetRadioState() == 0) {
        CreateDir(PATH_BASE);
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
        char* newName = new char[outFileNameLen + sizeof(PATH_BASE)];
        snprintf(newName, outFileNameLen + sizeof(PATH_BASE), "%s/%s", PATH_BASE, name);

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

        if (thisx->GetRadioState() == 0) {
            FILE* outFile = fopen(newName, "wb+");
            fwrite(rawData, stream.tellp(), 1, outFile);
            fclose(outFile);
        }
        else {
            ArchiveDataInfo info = { .data = (void*)rawData, .size = (size_t)stream.tellp(), .mode = DataHandleMode::DataCopy};
            a->WriteFile(newName, &info);
        }
        delete[] rawData;
    }
    //const unsigned int numThreads = std::thread::hardware_concurrency();
    //auto packFileThreads = std::make_unique<std::thread[]>(numThreads);
    //for (unsigned int i = 0; i < numThreads; i++) {
    //    packFileThreads[i] = std::thread(ProcessAudioFile, fileQueue, a);
    //}
    //
    //for (unsigned int i = 0; i < numThreads; i++) {
    //    packFileThreads[i].join();
    //}
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
        FillFileQueue(mFileQueue, mPathBuff, FillFileCallback);
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

    if (mThreadStarted && !mThreadIsDone) {
        ImGui::TextUnformatted("Packing files...");
        //ImGui::Text("Files processed %d\\%d", filesProcessed.load(), fileCount);
    }

    if (mSavePath != nullptr) {
        ImGui::SameLine();
        ImGui::Text("Archive save path: %s", mSavePath);
    }

    if (!mFilePairs.empty() && pairCheckState == CheckState::Good && mPathBuff != nullptr) {
        if (!mPackAsArchive || (mPackAsArchive && mSavePath != nullptr)) {
            if (ImGui::Button("Pack Archive")) {
                mThreadStarted = true;
                mThreadIsDone = false;
                std::thread packFilesMgrThread(PackFilesMgrWorker, &mFilePairs, &mThreadStarted, &mThreadIsDone, this);
                packFilesMgrThread.detach();
            }
        }
    }

    DrawPendingFilesList();

    ImGui::End();
}

void CustomSequencedAudioWindow::DrawPendingFilesList() {
    ImGui::BeginTable("Pending Sequences", 2);

    for (const auto& p : mFilePairs) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%s", p.first);
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s", p.second);
    }
    ImGui::EndTable();
}