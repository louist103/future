#include "CustomAudio.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "WindowMgr.h"
#include "style.h"
#include "font.h"


CustomAudioWindow::CustomAudioWindow() {

}

CustomAudioWindow::~CustomAudioWindow() {

}

void CustomAudioWindow::DrawWindow() {
    ImGui::Begin("Custom Audio", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Main);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Custom Audio");
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 3.0f);

    const ImVec2 windowSize = ImGui::GetWindowSize();

    if (BigIconButton("Streamed", ICON_FA_FILE_AUDIO_O, (windowSize.x * .5f) - 45.0f)) {
        gWindowMgr.SetCurWindow(WindowId::CustomStreamedAudio);
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 3.0f);
    ImGui::SameLine();

    if (BigIconButton("Sequenced", ICON_FA_FILE_CODE_O, (windowSize.x * .5f) - 45.0f)) {
        gWindowMgr.SetCurWindow(WindowId::CustomSequencedAudio);
    }

    ImGui::End();
}