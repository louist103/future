#define _CRT_SECURE_NO_WARNINGS
#include "filebox.h"
#include <stdlib.h>
#if defined(_WIN32)
#include <Windows.h>
#include <shobjidl_core.h>
extern HWND gHwnd;
#endif


bool GetOpenFilePath(char** inputBuffer, FileBoxType type) {
#if defined(_WIN32)
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));

    if (FAILED(hr)) {
        return false;
    }
    COMDLG_FILTERSPEC filters[3];
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
#endif
    return true;

}