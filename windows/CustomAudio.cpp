#include "CustomAudio.h"
#include "imgui.h"
#include "WindowMgr.h"


CustomAudioWindow::CustomAudioWindow() {

}

CustomAudioWindow::~CustomAudioWindow() {

}

void CustomAudioWindow::DrawWindow() {
    ImGui::Begin("Custom Audio", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Main);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Custom Audio");

    const ImVec2 windowSize = ImGui::GetWindowSize();
    float smallerDim = std::min(windowSize.x, windowSize.y);
    const ImVec2 windowSizeSq = { smallerDim, smallerDim };

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 50.0f);
    if (ImGui::Button("Streamed", { windowSizeSq.x * .33f, windowSizeSq.y * .33f })) {
        gWindowMgr.SetCurWindow(WindowId::CustomStreamedAudio);
    }
    ImGui::SameLine();
    if (ImGui::Button("Sequenced", { windowSizeSq.x * .33f, windowSizeSq.y * .33f })) {
        gWindowMgr.SetCurWindow(WindowId::CustomSequencedAudio);
    }

    ImGui::PopStyleVar();


    ImGui::End();
}