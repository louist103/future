#include "ExploreArchive.h"
#include "filebox.h"
#include "imgui.h"
#include "WindowMgr.h"
#include "zip.h"
#include "stdlib.h"
#include "imgui_memory_editor.h"

#if defined (_WIN32)
#include <Windows.h>
#elif defined (__linux__)
#include <unistd.h>
#endif

ExploreWindow::ExploreWindow() {
    // ImGui::InputText can't handle a null buffer being passed in.
    // We will allocate one char to draw the box with no text. It will be resized
    // after being returned from the file input box.
    mPathBuff = new char[1];
    mPathBuff[0] = 0;
}

ExploreWindow::~ExploreWindow() {
    delete[] mPathBuff;
    mPathBuff = nullptr;
}

void ExploreWindow::DrawWindow() {
    ImGui::Begin("Explore Archive", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Main);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Explore Archive");
    ImGui::InputText("Path", mPathBuff, 0, ImGuiInputTextFlags_::ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Open Archive")) {
        GetOpenFilePath(&mPathBuff, FileBoxType::Archive);
        mFileChangedSinceValidation = true;

        if (mFileChangedSinceValidation && mPathBuff[0] != 0) {
            mFileChangedSinceValidation = false;
            mFailedToOpenArchive = false;
            mFileValidated = ValidateInputFile();
        }

        if (zipArchive == nullptr && !mFailedToOpenArchive) {
            mFailedToOpenArchive = OpenArchive();
        }

        if (!mFailedToOpenArchive && mArchiveFiles.empty()) {
            zip_int64_t numFiles = zip_get_num_entries(zipArchive,0);
            if (numFiles != 0) {
                for (zip_int64_t i = 0; i < numFiles; i++) {
                    mArchiveFiles.push_back(zip_get_name(zipArchive, i, ZIP_FL_ENC_GUESS));
                }
            }
        }
    }

    DrawFileList();
    if (viewWindow != nullptr) {
        viewWindow->DrawWindow();
        if (!viewWindow->mIsOpen)
            viewWindow = nullptr;
    }
    //for (const auto s : mArchiveFiles) {
    //    ImGui::Text("%s", s);
    //}

    ImGui::End();
}

void ExploreWindow::DrawFileList() {
    ImGui::BeginChild("File List", ImGui::GetWindowSize(), 0, 0);
    unsigned int i = 0;
    for (const auto s : mArchiveFiles) {
        char btnId[12];
        snprintf(btnId, 12, "I%u", i);
        ImGui::Text("%s", s);
        ImGui::SameLine();
        if (ImGui::ArrowButton(btnId, ImGuiDir::ImGuiDir_Left)) {
            viewWindow = std::make_unique<FileViewerWindow>(zipArchive, s);
        }
        ImGui::SameLine();
        btnId[0] = 'E';
        if (ImGui::ArrowButton(btnId,ImGuiDir::ImGuiDir_Right)) {
            char* outPath = nullptr;
            GetSaveFilePath(&outPath);
            SaveFile(outPath, s);
        }
        i++;
    }
    ImGui::EndChild();

}

bool ExploreWindow::OpenArchive() {
    switch (mArchiveType) {
        case ArchiveType::O2R: {
            int error;
            zipArchive = zip_open(mPathBuff, ZIP_CHECKCONS | ZIP_RDONLY, &error);
            if (zipArchive == nullptr) {
                zip_error_t err;
                zip_error_init_with_code(&err, error);
                const char* errStr = zip_error_strerror(&err);
                size_t errStrLen = strlen(errStr);
                mArchiveErrStr = std::make_unique<char[]>(errStrLen + 1);
                strncpy(mArchiveErrStr.get(), errStr, errStrLen);
                zip_error_fini(&err);
                return true;
            }
            return false;
        }
    }
}

bool ExploreWindow::ValidateInputFile() {
    bool rv;
    uint32_t header = 0;
#if defined(_WIN32)
    HANDLE inFile = CreateFileA(mPathBuff, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (!ReadFile(inFile, &header, 4, nullptr, nullptr)) {
        CloseHandle(inFile);
        return false;
    }

    CloseHandle(inFile);
#elif defined(__linux__)
    int fd;
    fd = open(mPathBuff);
    if (read(fd, &header, 4) != 4) {
        return false;
    }
    close(fd);
    
#endif
    if (((char*)&header)[0] == 'P' && ((char*)&header)[1] == 'K' && ((char*)&header)[2] == 3 && ((char*)&header)[3] == 4) {
        rv = true;
        mArchiveType = ArchiveType::O2R;
    }
    else {
        rv = false;
    }
    return rv;
}

void ExploreWindow::SaveFile(char* outPath, const char* archiveFilePath) {
    zip_file_t* zipFile = zip_fopen(zipArchive, archiveFilePath, 0);

    zip_stat_t stat;
    zip_stat_init(&stat);
    zip_stat(zipArchive, archiveFilePath, 0, &stat);

    // Using malloc because new and delete doesn't like void*
    void* mData = malloc(stat.size);
    if (mData == nullptr) {
        return;
    }

    zip_fread(zipFile, mData, stat.size);
    FILE* outFile = fopen(outPath, "wb+");
    fwrite(mData, stat.size, 1, outFile);
    fclose(outFile);
    free(mData);
    zip_fclose(zipFile);
}

FileViewerWindow::FileViewerWindow(zip_t* archive, const char* path) {
    zip_file_t* zipFile = zip_fopen(archive, path, 0);
    
    zip_stat_t stat;
    zip_stat_init(&stat);
    zip_stat(archive, path, 0, &stat);
    
    mSize = stat.size;
    // Using malloc because new and delete doesn't like void*
    mData = malloc(mSize);
    
    zip_fread(zipFile, mData, mSize);
    zip_fclose(zipFile);
    
    mEditor = std::make_unique<MemoryEditor>();
}

FileViewerWindow::~FileViewerWindow() {
    free(mData);
}

void FileViewerWindow::DrawWindow() {
    ImGui::Begin("Memory Editor",&mIsOpen);
    
    ImGui::SetWindowFocus();
    mEditor->DrawContents(mData, mSize);
    ImGui::End();
}