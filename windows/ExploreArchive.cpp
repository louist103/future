#include "ExploreArchive.h"
#include "filebox.h"
#include "imgui.h"
#include "WindowMgr.h"
#include "zip.h"
#include "stdlib.h"
#include "imgui_memory_editor.h"
#include "images.h"
#include "zip_archive.h"
#include "mpq_archive.h"
#if defined (_WIN32)
#include <Windows.h>
#elif defined (__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif
//#include <immintrin.h>

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
            if (mArchive != nullptr && mArchive->IsArchiveOpen()) {
                mArchive->CloseArchive();
                mArchive = nullptr;
            }
        }

        if (mArchive == nullptr && !mFailedToOpenArchive) {
            mFailedToOpenArchive = OpenArchive();
        }

        if (!mFailedToOpenArchive && mArchive->files.empty()) {
            mArchive->GenFileList();
        }
    }
    if (mArchive != nullptr) {
        long long start, stop;
        //start = __rdtsc();
        DrawFileList();
        //stop = __rdtsc();
        ImVec2 curPos = ImGui::GetCursorPos();
        ImGui::SetCursorPos({400.0f,20.0f});
        //ImGui::Text("MS to show list: %llu", (stop-start)/1996800);
        ImGui::SetCursorPos(curPos);
        if (viewWindow != nullptr) {
            viewWindow->DrawWindow();
            if (!viewWindow->mIsOpen)
                viewWindow = nullptr;
        }
    }


    ImGui::End();
}

void ExploreWindow::DrawFileList() {
    const ImVec2 cursorPos = ImGui::GetCursorPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImVec2 childWindowSize = {windowSize.x - cursorPos.x, windowSize.y -cursorPos.y};
    
    ImGui::BeginChild("File List", childWindowSize, 0, 0);
    const float windowHeight = ImGui::GetWindowHeight();
    unsigned int i = 0;
    const float textHeight = ImGui::GetTextLineHeight();
    for (const auto s : mArchive->files) {
        const float scroll = ImGui::GetScrollY();
        const float cursorPosY = ImGui::GetCursorPosY();
        char btnId[12];
        int width;
        int height;

        // Only draw elements that are on screen. This gives around 20x performance improvement.
        if ((cursorPosY < scroll) || (cursorPosY >(scroll + windowHeight))) {
            ImGui::NewLine();
            i++;
            continue;
        }

        ImGui::Text("%s", s);
        snprintf(btnId, 12, "I%u", i);
        
        ImGui::SameLine();
        ImGui::PushID(btnId);
        
        if (ImGui::ImageButton(LoadTextureByName("R:\\MagBlack.png", &width, &height), ImVec2(32, 32))) {
            viewWindow = std::make_unique<FileViewerWindow>(mArchive.get(), s);
        }
        ImGui::PopID();
        ImGui::SameLine();
        btnId[0] = 'E';
        ImGui::PushID(btnId);
        if (ImGui::ImageButton(LoadTextureByName("R:\\Export.png", &width, &height), ImVec2(32, 32))) {
            char* outPath = nullptr;
            GetSaveFilePath(&outPath);
            SaveFile(outPath, s);
        }
        ImGui::PopID();
        i++;
    }
    ImGui::EndChild();

}

bool ExploreWindow::OpenArchive() {
    switch (mArchiveType) {
        case ArchiveType::O2R: {
            mArchive = std::make_unique<ZipArchive>(mPathBuff);
            break;
        }
        case ArchiveType::OTR: {
            mArchive = std::make_unique<MpqArchive>(mPathBuff);
            break;
        }
    }
    return !mArchive->IsArchiveOpen();
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
    fd = open(mPathBuff, O_RDONLY);
    if (read(fd, &header, 4) != 4) {
        return false;
    }
    close(fd);
    
#endif
    if (((char*)&header)[0] == 'P' && ((char*)&header)[1] == 'K' && ((char*)&header)[2] == 3 && ((char*)&header)[3] == 4) {
        rv = true;
        mArchiveType = ArchiveType::O2R;
    } else if (((char*)&header)[0] == 'M' && ((char*)&header)[1] == 'P' && ((char*)&header)[2] == 'Q' && ((char*)&header)[3] == 0x1A) {
        rv = true;
        mArchiveType = ArchiveType::OTR;
    }
    else {
        rv = false;
    }
    return rv;
}

void ExploreWindow::SaveFile(char* outPath, const char* archiveFilePath) {
    size_t uncompressedSize;
    void* data = mArchive->ReadFile(outPath, &uncompressedSize);

    FILE* outFile = fopen(outPath, "wb+");

    fwrite(data, uncompressedSize, 1, outFile);
    fclose(outFile);
    free(data);

}

FileViewerWindow::FileViewerWindow(Archive* archive, const char* path) {
    mData = archive->ReadFile(path, &mSize);
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
