#include "ExploreArchive.h"
#include "filebox.h"
#include "imgui.h"
#include "WindowMgr.h"
#include "zip.h"
#include "stdlib.h"

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

void ExploreWindow::DrawWindow()
{
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
	}

	if (mFileChangedSinceValidation) {
		mFileChangedSinceValidation = false;
		mFileValidated = ValidateInputFile();
	}





	ImGui::End();
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
