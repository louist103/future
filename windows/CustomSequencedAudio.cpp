#include "CustomSequencedAudio.h"
#include "imgui.h"
#include "WindowMgr.h"


CustomSequencedAudioWindow::CustomSequencedAudioWindow() {

}

CustomSequencedAudioWindow::~CustomSequencedAudioWindow() {

}

void CustomSequencedAudioWindow::DrawWindow() {
    ImGui::Begin("Custom Audio", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Main);
    }

    ImGui::TextUnformatted("Not ready yet...");

    ImGui::End();
}