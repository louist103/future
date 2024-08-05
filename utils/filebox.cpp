#define _CRT_SECURE_NO_WARNINGS
#include "filebox.h"
#include <stdlib.h>
#if defined(_WIN32)
#include <Windows.h>
#include <shobjidl_core.h>
extern HWND gHwnd;
#elif defined(__linux__)
#include "portable-file-dialogs.h"
#include "SDL2/SDL.h"
#endif
#include <cstring>


bool GetOpenFilePath(char** inputBuffer, FileBoxType type) {
#if defined(_WIN32)
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));

    if (FAILED(hr)) {
        return false;
    }
    COMDLG_FILTERSPEC filters[3] = { 0 };
    switch (type) {
        case FileBoxType::Archive: {
            filters[0].pszName = L"All Supported Archives";
            filters[0].pszSpec = L"*.OTR;*.MPQ;*.O2R;*.ZIP";
            filters[1].pszName = L"OTR Archives";
            filters[1].pszSpec = L"*.OTR;*.MPQ";
            filters[2].pszName = L"O2R Archives";
            filters[2].pszSpec = L"*.O2R;*.ZIP";
            pfd->SetFileTypes(3, filters);
        }
    }
    DWORD options;
    pfd->GetOptions(&options);
    pfd->SetOptions(options | FOS_STRICTFILETYPES | FOS_NOCHANGEDIR);
    pfd->SetTitle(L"Open Archive");
    hr = pfd->Show(gHwnd);

    if (SUCCEEDED(hr)) {
        IShellItem* result;
        PWSTR path = nullptr;
        hr = pfd->GetResult(&result);
        result->GetDisplayName(SIGDN_FILESYSPATH, &path);
        size_t len = lstrlenW(path);
        if (*inputBuffer != nullptr) {
            delete[] *inputBuffer;
        }
        *inputBuffer = new char[len + 1];
        wcstombs(*inputBuffer, path, len);
        // wcstombs doesn't null terminate the string for some reason...
        (*inputBuffer)[len] = 0; 
        CoTaskMemFree(path);
        result->Release();
    }
    pfd->Release();
#elif defined(__linux__)
    //std::vector<std::string> filters;
    auto selection = pfd::open_file("Select a file", ".", { "All Supported Archives", "*.otr *.mpq *.o2r *.zip", "OTR Archives", "*.otr *.mpq", "O2R Archives", "*.o2r *.zip",}).result();

    if (selection.empty()) {
        return false;
    }
    if (*inputBuffer != nullptr) {
            delete[] *inputBuffer;
    }
    size_t len = selection[0].length();
    *inputBuffer = new char[len + 1];
    strcpy(*inputBuffer, selection[0].c_str());
#endif
    return true;

}

bool GetSaveFilePath(char** inputBuffer) {
#if defined(_WIN32)
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));

    if (FAILED(hr)) {
        return false;
    }

    pfd->SetTitle(L"Save File");
    hr = pfd->Show(gHwnd);

    if (SUCCEEDED(hr)) {
        IShellItem* result;
        PWSTR path = nullptr;
        hr = pfd->GetResult(&result);
        result->GetDisplayName(SIGDN_FILESYSPATH, &path);
        size_t len = lstrlenW(path);
        if (*inputBuffer != nullptr) {
            delete[] *inputBuffer;
        }
        *inputBuffer = new char[len + 1];
        wcstombs(*inputBuffer, path, len);
        // wcstombs doesn't null terminate the string for some reason...
        (*inputBuffer)[len] = 0;
        CoTaskMemFree(path);
        result->Release();
    }
    pfd->Release();
#elif (__linux__)
    auto selection = pfd::save_file("Save File").result();
    size_t len = selection.length();
    if (*inputBuffer != nullptr){
        delete[] *inputBuffer;
    }
    *inputBuffer = new char[len + 1];
    strcpy(*inputBuffer, selection.c_str());
#endif
    return true;
}

#ifndef IDYES
#define IDYES 6
#endif
#ifndef IDNO
#define IDNO 7
#endif

int ShowYesNoBox(const char* title, const char* box) {
    int ret;
#ifdef _WIN32
    ret = MessageBoxA(nullptr, box, title, MB_YESNO | MB_ICONQUESTION);
#else
    SDL_MessageBoxData boxData = { 0 };
    SDL_MessageBoxButtonData buttons[2] = { { 0 } };

    buttons[0].buttonid = IDYES;
    buttons[0].text = "Yes";
    buttons[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    buttons[1].buttonid = IDNO;
    buttons[1].text = "No";
    buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    boxData.numbuttons = 2;
    boxData.flags = SDL_MESSAGEBOX_INFORMATION;
    boxData.message = box;
    boxData.title = title;
    boxData.buttons = buttons;
    SDL_ShowMessageBox(&boxData, &ret);
#endif
    return ret;
}

void ShowErrorBox(const char* title, const char* text) {
#ifdef _WIN32
    MessageBoxA(nullptr, text, title, MB_OK | MB_ICONERROR);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, text, nullptr);
#endif
}

