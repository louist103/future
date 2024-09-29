#ifndef MAPPED_FILE_H
#define MAPPED_FILE_H

#if defined (_WIN32)
#include <Windows.h>
#endif

enum MappedFlag {
    ReadOnly = 0x1,
    ReadWrite = 0x2,
    OpenExisting = 0x10,
    Create = 0x11,
};

class MappedFile {
public:
    MappedFile();
    MappedFile(char* path, int flag);
    void Open(char* path, int flag);
    void Close();
    ~MappedFile();
    void* GetData() const;
    size_t GetSize() const;
    void Release();
private:
#if defined(_WIN32)
    HANDLE mHFile = INVALID_HANDLE_VALUE;
    HANDLE mHMap = INVALID_HANDLE_VALUE;
#elif defined(__linux__)
    int fd = -1;
#endif
    void* mData = nullptr;
    size_t mSize = 0;
    // Release this map so its not unmapped when the object is destroyed.
    // Only safe to use when the pointer, and size (linux only) is stored by something else that will unmap it later.
    bool released = false;
};

#endif