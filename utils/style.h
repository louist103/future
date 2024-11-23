#pragma once
#include "imgui.h"

static const ImVec4 COLOR_SLATE_950 = ImVec4(0.0078f, 0.0235f, 0.0901f, 1.0f);
static const ImVec4 COLOR_SLATE_900 = ImVec4(0.0588f, 0.0901f, 0.1647f, 1.0f);
static const ImVec4 COLOR_SLATE_800 = ImVec4(0.1176f, 0.1607f, 0.2313f, 1.0f);
static const ImVec4 COLOR_SLATE_700 = ImVec4(0.2039f, 0.2549f, 0.3333f, 1.0f);
static const ImVec4 COLOR_SLATE_600 = ImVec4(0.2784f, 0.3333f, 0.4117f, 1.0f);
static const ImVec4 COLOR_SLATE_500 = ImVec4(0.3921f, 0.4549f, 0.5450f, 1.0f);
static const ImVec4 COLOR_SLATE_400 = ImVec4(0.5803f, 0.6392f, 0.7215f, 1.0f);
static const ImVec4 COLOR_SLATE_300 = ImVec4(0.7960f, 0.8352f, 0.8823f, 1.0f);
static const ImVec4 COLOR_SLATE_200 = ImVec4(0.8862f, 0.9097f, 0.9411f, 1.0f);
static const ImVec4 COLOR_SLATE_100 = ImVec4(0.9450f, 0.9607f, 0.9764f, 1.0f);
static const ImVec4 COLOR_SLATE_50 = ImVec4(0.9725f, 0.9803f, 0.9882f, 1.0f);

void SetupStyles();
bool BigIconButton(const char* label, const char* icon, float size);
