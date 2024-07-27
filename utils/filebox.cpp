#include "filebox.h"
#include <Windows.h>
#include <shobjidl_core.h>
#include <stdlib.h>

extern HWND gHwnd;

bool GetOpenFilePath(char** inputBuffer) {
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));

    if (FAILED(hr)) {
        return false;
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

    return true;

}