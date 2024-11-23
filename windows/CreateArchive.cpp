#include "CreateArchive.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "WindowMgr.h"
#include "style.h"
#include "font.h"

CreateArchiveWindow::CreateArchiveWindow() {

}

CreateArchiveWindow::~CreateArchiveWindow() {

}

void CreateArchiveWindow::DrawWindow() {
    ImGui::Begin("Create Archive", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Main);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Create Archive");
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 3.0f);

    const ImVec2 windowSize = ImGui::GetWindowSize();

    if (BigIconButton("Replace Textures", ICON_FA_PICTURE_O, (windowSize.x * .33f) - 45.0f)) {
        gWindowMgr.SetCurWindow(WindowId::TexReplace);
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 3.0f);
    ImGui::SameLine();

    if (BigIconButton("Custom Audio", ICON_FA_MUSIC, (windowSize.x * .33f) - 45.0f)) {
        gWindowMgr.SetCurWindow(WindowId::CustomAudio);
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 3.0f);
    ImGui::SameLine();

    if (BigIconButton("Create From Directory", ICON_FA_FOLDER_OPEN, (windowSize.x * .33f) - 45.0f)) {
        gWindowMgr.SetCurWindow(WindowId::FromDir);
    }

    ImGui::End();
}