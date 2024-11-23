#include "style.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "font.h"

void SetupStyles() {
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
    auto style = &ImGui::GetStyle();
    style->Colors[ImGuiCol_WindowBg] = COLOR_SLATE_950;
    style->Colors[ImGuiCol_Button] = COLOR_SLATE_800;
    style->Colors[ImGuiCol_ButtonHovered] = COLOR_SLATE_700;
    style->Colors[ImGuiCol_ButtonActive] = COLOR_SLATE_800;
    style->Colors[ImGuiCol_FrameBg] = COLOR_SLATE_700;
    style->Colors[ImGuiCol_Separator] = COLOR_SLATE_900;
    style->FrameRounding = 4.0f;
}

bool BigIconButton(const char* label, const char* icon, float size) {
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    ImGui::PushFont(FontXL);
    const ImVec2 icon_size = ImGui::CalcTextSize(icon, NULL, true);
    ImGui::PopFont();

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImVec2 pos = window->DC.CursorPos;
    const ImGuiID id = window->GetID(label);
    const ImRect bb(pos, ImVec2(pos.x + size, pos.y + ImGui::GetContentRegionAvail().y));

    ImGui::ItemSize(bb, style.FramePadding.y);

    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);

    if (hovered) {
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR_SLATE_400);
    }

    ImGui::RenderFrame(bb.Min, bb.Max, ImGui::GetColorU32(COLOR_SLATE_950), true, style.FrameRounding);

    ImGui::PushFont(FontXL);
    ImGui::RenderTextClipped(ImVec2(bb.Min.x + style.FramePadding.x, bb.Min.y + style.FramePadding.y), ImVec2(bb.Max.x - style.FramePadding.x, bb.Max.y - style.FramePadding.y), 
        icon, NULL, &icon_size, ImVec2(0.5f, 0.4f), &bb);
    ImGui::PopFont();

    ImGui::RenderTextClipped(ImVec2(bb.Min.x + style.FramePadding.x, bb.Min.y + style.FramePadding.y), ImVec2(bb.Max.x - style.FramePadding.x, bb.Max.y - style.FramePadding.y), 
        label, NULL, &label_size, ImVec2(0.5f, 0.6f), &bb);

    if (hovered) {
        ImGui::PopStyleColor();
    }

    return pressed;
}