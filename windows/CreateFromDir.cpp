#include "CreateFromDir.h"
#include "imgui.h"
#include "WindowMgr.h"
#include "filebox.h"
#include <Windows.h>
#include <Shlwapi.h>
#include "zip_archive.h"
#include "mpq_archive.h"

#pragma comment (lib, "shlwapi.lib")

CreateFromDirWindow::CreateFromDirWindow()
{
}

CreateFromDirWindow::~CreateFromDirWindow()
{
    ClearPathBuff();
    ClearSaveBuff();
}

void CreateFromDirWindow::ClearPathBuff()
{
    if (mPathBuff != nullptr) {
        delete[] mPathBuff;
        mPathBuff = nullptr;
    }
}

void CreateFromDirWindow::ClearSaveBuff()
{
    if (mSavePath != nullptr) {
        delete[] mSavePath;
        mSavePath = nullptr;
    }
}

static void CallBack(zip_t* archive, double progress, void* data) {
    CreateFromDirWindow* thisx = (CreateFromDirWindow*)data;
    thisx->mProgress = progress;
}

static void AddFilesWorker(CreateFromDirWindow* thisx) {
    thisx->mThreadIsDone = false;
    thisx->mThreadStarted = true;

    Archive* a;
    switch (thisx->mRadioState) {
        case 0: {
            a = new MpqArchive(thisx->mSavePath);
            break;
        }
        case 1: {
            a = new ZipArchive(thisx->mSavePath);
            ZipArchive* za = (ZipArchive*)a;
            za->RegisterProgressCallback(CallBack, thisx);
            break;
        }
    }

    a->CreateArchiveFromList(thisx->mFileQueue, thisx->mPathBuff);
    a->CloseArchive();
    delete a;
    
    thisx->mThreadIsDone = true;
    thisx->mThreadStarted = false;
}

void CreateFromDirWindow::DrawWindow()
{
    ImGui::Begin("Create From Directory", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    
    ImGui::BeginDisabled(mThreadStarted);
    
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Create);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Create Archive From Directory");
    ImGui::TextUnformatted("Open a directory and create an archive with the same structure and files");
    
    ImGui::SameLine();

    ImGui::RadioButton("OTR", &mRadioState, 0);
    ImGui::SameLine();
    ImGui::RadioButton("O2R", &mRadioState, 1);
    if (ImGui::Button("Select Directory")) {
        mThreadIsDone = false;
        ClearPathBuff();
        ClearSaveBuff();
        GetOpenDirPath(&mPathBuff);
        FillFileQueue();
    }
    if (mPathBuff != nullptr) {
        ImGui::SameLine();
        ImGui::Text("Path to Pack: %s", mPathBuff);
    }
    if (ImGui::Button("Set Save Path")) {
        GetSaveFilePath(&mSavePath);
    }

    if (mSavePath != nullptr) {
        ImGui::SameLine();
        ImGui::Text("Path to New Archive: %s", mSavePath);
    }


    if (!mFileQueue.empty() && mPathBuff != nullptr && mSavePath != nullptr) {
        if (ImGui::Button("Pack Archive")) {
            mAddFileThread = std::thread(AddFilesWorker, this);
            mAddFileThread.detach();
        }
    }

    ImGui::EndDisabled();
    
    if (mThreadStarted) {
        ImGui::Text("Packing archive. Progress: %.0f %%", mProgress * 100.0);
    }

    DrawPendingFilesList();



    ImGui::End();
}

void CreateFromDirWindow::DrawPendingFilesList() {
    if (mFileQueue.empty()) {
        return;
    }

    const ImVec2 cursorPos = ImGui::GetCursorPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImVec2 childWindowSize = { windowSize.x - cursorPos.x, windowSize.y - cursorPos.y };

    ImGui::BeginChild("File List", childWindowSize, 0, 0);
    ImGui::TextUnformatted("Files to Pack:");
    const float windowHeight = ImGui::GetWindowHeight();
    const float textHeight = ImGui::GetTextLineHeight();

    for (const auto s:  mFileQueue) {
        const float scroll = ImGui::GetScrollY();
        const float cursorPosY = ImGui::GetCursorPosY();

        // Only draw elements that are on screen. This gives around 20x performance improvement.
        if ((cursorPosY < scroll) || (cursorPosY > (scroll + windowHeight))) {
            ImGui::NewLine();
            continue;
        }
        size_t basePathLen = strlen(mPathBuff);
        ImGui::Text("%s", &s[basePathLen + 1]);
    }
    ImGui::EndChild();
}

static void FillFileQueueImpl(std::vector<char*>& files, char* startPath);

void CreateFromDirWindow::FillFileQueue()
{
    FillFileQueueImpl(mFileQueue, mPathBuff);
}

static void FillFileQueueImpl(std::vector<char*>& files, char* startPath) {
    WIN32_FIND_DATAA ffd;
    char buffer[MAX_PATH];
    strncpy_s(buffer, startPath, MAX_PATH);
    PathAppendA(buffer, "*");

    HANDLE h = FindFirstFileA(buffer, &ffd);

    do {
        // Skip '.' and '..'
        if (ffd.cFileName[0] == '.' && ffd.cFileName[1] == 0 || (ffd.cFileName[1] == '.' && ffd.cFileName[2] == 0)) {
            continue;
        } else if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char innerPath[MAX_PATH];
            strncpy_s(innerPath, startPath, MAX_PATH);
            PathAppendA(innerPath, ffd.cFileName);
            FillFileQueueImpl(files, innerPath);
        } else {
            size_t len = strlen(ffd.cFileName);
            // Start path is used because FindNextFile chokes on the path if it has a '*' in it which `buffer` does
            size_t len2 = strlen(startPath);

            char* str = new char[len + len2 + 2]; // Don't change. Prevents overflows
            memset(str, 0, len + len2 + 1);
            PathAppendA(str, startPath);
            PathAppendA(str, ffd.cFileName);
            files.push_back((str));
        }
    } while (FindNextFileA(h, &ffd) != 0);
    FindClose(h);
}