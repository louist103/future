#include "CreateFromDir.h"
#include "imgui.h"
#include "WindowMgr.h"
#include "filebox.h"

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
    }

    ImGui::End();
}
