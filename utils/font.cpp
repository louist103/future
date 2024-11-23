#include "font.h"
#include <string>
#include <array>

ImFont* FontS;
ImFont* FontM;
ImFont* FontL;
ImFont* FontXL;

std::array<ImFont**, 4> fonts = { &FontS, &FontM, &FontL, &FontXL };

void LoadFonts() {
  ImGuiIO& io = ImGui::GetIO();
  std::string fontPath = "assets";

#if defined(__linux__) || defined(__APPLE__)
    // Get the path of the mounted app image and build the font path from there. Is this better than just embedding the font into the binary?
    char* appDir = getenv("APPDIR");

    if (appDir != nullptr) {
        fontPath = std::string(appDir) + "/assets";
    }
#endif
    float baseFontSize = 16.0f;

    static const ImWchar sIconsRanges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    ImFontConfig iconsConfig;
    iconsConfig.MergeMode = true;
    iconsConfig.PixelSnapH = true;


    for (int i = 0; i < 4; i++) {
        float fontSize = baseFontSize * (i + 1);
        // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly
        float iconFontSize = fontSize * 2.0f / 3.0f;
        iconsConfig.GlyphMinAdvanceX = iconFontSize;

        *fonts[i] = io.Fonts->AddFontFromFileTTF((fontPath + "/Roboto-Medium.ttf").c_str(), fontSize);
        io.Fonts->AddFontFromFileTTF((fontPath + "/fontawesome.ttf").c_str(), iconFontSize, &iconsConfig, sIconsRanges);
    }

    io.FontDefault = FontM;
}
