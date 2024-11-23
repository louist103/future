#include "CreateFromDir.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "WindowMgr.h"
#include "filebox.h"
#include "zip_archive.h"
#include "mpq_archive.h"
#include <string.h>
#include <filesystem>

CreateFromDirWindow::CreateFromDirWindow()
{
}

CreateFromDirWindow::~CreateFromDirWindow()
{
    ClearPathBuff();
    ClearSaveBuff();
    for (auto p : mFileQueue) {
        delete p;
    }
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
    ImGui::Begin("Create From Directory", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    
    ImGui::BeginDisabled(mThreadStarted);
    
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Create);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Create Archive From Directory");
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 3.0f);

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
        char* fileName = strrchr(s, PATH_SEPARATOR);
        fileName++;
        ImGui::Text("%s", fileName);
    }
    ImGui::EndChild();
}

static void FillFileQueueImpl(std::vector<char*>& files, char* startPath, int);

void CreateFromDirWindow::FillFileQueue()
{
    FillFileQueueImpl(mFileQueue, mPathBuff, 0);
}

static void FillFileQueueImpl(std::vector<char*>& files, char* startPath, int newPathLen) {
    std::filesystem::recursive_directory_iterator it(startPath);
    for (const auto& i : it) {
        if (!i.is_directory()) {
            files.push_back(strdup(i.path().string().c_str()));
        }
    }
#if 0 // vv This doesn't work... I give up for now...
#if defined (_WIN32)
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
#elif defined(__linux__) || defined(__APPLE__)
    //I give up...
    size_t basePathLen = strlen(startPath);
    char buffer[PATH_MAX];
    strncpy(buffer, startPath, PATH_MAX);
    strncat(buffer, "/", PATH_MAX - 1);
    DIR* d = opendir(startPath);

    struct dirent* dir;

    if (d != nullptr) {
        while((dir = readdir(d)) != nullptr) {
            if ((dir->d_name[0] == '.' && dir->d_name[1] == 0) || (dir->d_name[0] == '.' && dir->d_name[1] == '.')) {
                    continue;
            } 
            char innerPath[PATH_MAX];
            struct stat path;
            strncpy(innerPath, buffer, PATH_MAX);
            strncat(innerPath, dir->d_name, PATH_MAX);
            int ret = stat(innerPath, &path);
            if (S_ISDIR(path.st_mode)) {
                    //char innerPath[PATH_MAX];
                    size_t len = strlen(innerPath);
                    size_t len2 = strlen(dir->d_name);
                    //strncpy(innerPath, buffer, PATH_MAX);
                    //innerPath[len] = '/';
                    //strncat(innerPath, dir->d_name, PATH_MAX - (len + len2));
                    //innerPath[len+len2 + 1] = '/';
                    //innerPath[len+len2 + 2] = 0;
                    FillFileQueueImpl(files, innerPath, len2);
                
            } else {
                size_t len = strlen(innerPath);
                char* path = new char[len + 2];
                strncpy(path, innerPath, PATH_MAX);
                //strncat(path, dir->d_name, PATH_MAX - (len + len2));
                files.push_back(path);
                // Clear out the last file name but leave the folder path.
                // We could make another buffer and copy everything but the file
                // But we can't afford the stack space.
                innerPath[len + 1] = 0;
            }
        }
    }
    // Same as above but this time clear out everything before the base path
    //buffer[basePathLen - newPathLen] = 0;
    closedir(d);
#endif
#endif
}