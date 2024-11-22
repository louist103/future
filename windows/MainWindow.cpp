#include "MainWindow.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include "WindowMgr.h"
#include "font.h"
#include "style.h"

void MainWindow::DrawWindow() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    const ImVec2 windowSize = ImGui::GetWindowSize();

    if (BigIconButton("Create Archive", ICON_FA_ARCHIVE, (windowSize.x * .5f) - 45.0f)) {
        gWindowMgr.SetCurWindow(WindowId::Create);
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 3.0f);
    ImGui::SameLine();

    if (BigIconButton("Explore Archive", ICON_FA_SEARCH, (windowSize.x * .5f) - 45.0f)) {
        gWindowMgr.SetCurWindow(WindowId::Explore);
    }
    ImGui::End();
}