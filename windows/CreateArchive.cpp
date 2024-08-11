#include "CreateArchive.h"
#include "imgui.h"
#include "WindowMgr.h"

CreateArchiveWindow::CreateArchiveWindow() {

}

CreateArchiveWindow::~CreateArchiveWindow() {

}

void CreateArchiveWindow::DrawWindow() {
    ImGui::Begin("Create Archive", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Main);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Create Archive");

    const ImVec2 windowSize = ImGui::GetWindowSize();
    float smallerDim = std::min(windowSize.x, windowSize.y);
    const ImVec2 windowSizeSq = { smallerDim, smallerDim };

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 50.0f);
    if (ImGui::Button("Replace Textures", { windowSizeSq.x * .33f, windowSizeSq.y * .33f })) {
        gWindowMgr.SetCurWindow(WindowId::TexReplace);
    }
    ImGui::SameLine();
    if (ImGui::Button("Custom Audio", { windowSizeSq.x * .33f, windowSizeSq.y * .33f })) {
        gWindowMgr.SetCurWindow(WindowId::CustomAudio);
    }
    ImGui::SameLine();
    if (ImGui::Button("Create From Directory", { windowSizeSq.x * .33f, windowSizeSq.y * .33f })) {
        gWindowMgr.SetCurWindow(WindowId::FromDir);
    }
    ImGui::PopStyleVar();


    ImGui::End();
}