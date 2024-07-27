#include "MainWindow.h"
#include "imgui.h"
#include <algorithm>
#include "WindowMgr.h"

void MainWindow::DrawWindow() {
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::Begin("Hello, world!", nullptr, ImGuiWindowFlags_NoDecoration);                          // Create a window called "Hello, world!" and append into it.
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    const ImVec2 windowSize = ImGui::GetWindowSize();
    float smallerDim = std::min(windowSize.x, windowSize.y);
    const ImVec2 windowSizeSq = { smallerDim, smallerDim };

    auto windowWidth = windowSize.x;
    auto textWidth = ImGui::CalcTextSize(("This is some useful text.")).x;
    ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);

    ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 50.0f);
    ImGui::Button("Create OTR/O2R", { windowSizeSq.x * .33f, windowSizeSq.y * .33f });
    ImGui::SameLine();
    if (ImGui::Button("Inspect OTR/O2R", { windowSizeSq.x * .33f, windowSizeSq.y * .33f })) {
        gWindowMgr.SetCurWindow(WindowId::Explore);
    }
    ImGui::SameLine();
    ImGui::Button("Debug", { windowSizeSq.x * .33f, windowSizeSq.y * .33f });


    ImGui::PopStyleVar();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::End();
}