#include "ExploreArchive.h"
#include "imgui.h"
#include "WindowMgr.h"
#include "Windows.h"
#include "zip.h"
#include "stdlib.h"

extern bool GetOpenFilePath(char** inputBuffer);

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
		GetOpenFilePath(&mPathBuff);
		mFileChangedSinceValidation = true;
	}

	if (mFileChangedSinceValidation) {
		mFileChangedSinceValidation = false;
		mFileValidated = ValidateInputFile();
	}





	ImGui::End();
}


bool ExploreWindow::ValidateInputFile()
{
	HANDLE inFile = CreateFileA(mPathBuff, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	uint32_t header = 0;
	bool rv;

	if (!ReadFile(inFile, &header, 4, nullptr, nullptr)) {
		CloseHandle(inFile);
		return false;
	}

	if (((char*)&header)[0] == 'P' && ((char*)&header)[1] == 'K' && ((char*)&header)[2] == 3 && ((char*)&header)[3] == 4) {
		rv = true;
		mArchiveType = ArchiveType::O2R;
	}
	else {
		rv = false;
	}
	CloseHandle(inFile);
	return rv;
}
