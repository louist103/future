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
    mPathBuff = new char[1];
    mPathBuff[0] = 0;
}

CreateFromDirWindow::~CreateFromDirWindow()
{
    if (mPathBuff != nullptr) {
        delete[] mPathBuff;
    }
}

void CreateFromDirWindow::DrawWindow()
{
    ImGui::Begin("Create From Directory", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Create);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Create Archive From Directory");
    ImGui::TextUnformatted("Open a directory and create an archive with the same structure and files");
    
    ImGui::InputText("Path", mPathBuff, 0, ImGuiInputTextFlags_::ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();

    if (ImGui::Button("Select Directory")) {
        GetOpenDirPath(&mPathBuff);
        FillFileQueue();
        ZipArchive zip("test.zip");
        zip.CreateArchiveFromList(fileQueue);
    }



    ImGui::End();
}

static void FillFileQueueImpl(std::queue<std::unique_ptr<char[]>>& files, char* startPath);

void CreateFromDirWindow::FillFileQueue()
{
    FillFileQueueImpl(fileQueue, mPathBuff);
}

static void FillFileQueueImpl(std::queue<std::unique_ptr<char[]>>& files, char* startPath) {
    WIN32_FIND_DATAA ffd;
    char buffer[MAX_PATH];
    strcpy(buffer, startPath);
    PathAppendA(buffer, "*");

    HANDLE h = FindFirstFileA(buffer, &ffd);

    do {
        // Skip '.' and '..'
        if (ffd.cFileName[0] == '.' || (ffd.cFileName[1] == '.' && ffd.cFileName[2] == 0)) {
            continue;
        } else if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char innerPath[MAX_PATH];
            strcpy(innerPath, startPath);
            PathAppendA(innerPath, ffd.cFileName);
            FillFileQueueImpl(files, innerPath);
        } else {
            size_t len = strlen(ffd.cFileName);
            // Start path is used because FindNextFile chokes on the path if it has a '*' in it which `buffer` does
            size_t len2 = strlen(startPath);

            std::unique_ptr<char[]> str = std::make_unique<char[]>(len + len2 + 1);
            PathAppendA(str.get(), startPath);
            PathAppendA(str.get(), ffd.cFileName);
            files.push(std::move(str));
        }
    } while (FindNextFileA(h, &ffd) != 0);
    FindClose(h);
}