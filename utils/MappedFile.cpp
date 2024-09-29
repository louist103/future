
#include "MappedFile.h"

#include <cassert>

MappedFile::MappedFile(char* path, int flag) {
    Open(path, flag);
}
#include <stdio.h>
void MappedFile::Open(char* path, int flag) {
#if defined (_WIN32)
    DWORD access;
    DWORD shareMode;
    DWORD protectMode;
    DWORD desiredAccess;
    DWORD createMode;
    assert(flag != 0);

    if ((flag & ReadOnly) == ReadOnly) {
        access = GENERIC_READ;
        shareMode = FILE_SHARE_READ;
        protectMode = PAGE_READONLY;
        desiredAccess = FILE_MAP_READ;
    }
    else if ((flag & ReadWrite) == ReadWrite) {
        access = GENERIC_READ | GENERIC_WRITE;
        shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
        protectMode = PAGE_READWRITE;
        desiredAccess = FILE_MAP_READ | FILE_MAP_WRITE;
    }
    else {
        return;
    }
    if ((flag & OpenExisting) == OpenExisting) {
        createMode = OPEN_EXISTING;
    }
    else if ((flag & Create) == Create) {
        createMode = CREATE_ALWAYS;
    }
    else {
        return;
    }
    mHFile = CreateFileA(path, access, shareMode, nullptr, createMode, FILE_ATTRIBUTE_NORMAL, nullptr);
    mHMap = CreateFileMappingA(mHFile, nullptr, protectMode, 0, 0, nullptr);
    mData = MapViewOfFile(mHMap, desiredAccess, 0, 0, 0);
    LARGE_INTEGER size;
    GetFileSizeEx(mHFile, &size);
    mSize = size.QuadPart;
#elif defined (__linux__)
#endif
}

void* MappedFile::GetData() const {
    return mData;
}

size_t MappedFile::GetSize() const
{
    return mSize;
}

void MappedFile::Close() {
#if defined (_WIN32)
    UnmapViewOfFile(mData);
    CloseHandle(mHMap);
    CloseHandle(mHFile);
#elif defined (__linux__)
    close(fd);
    munmap(mData, size);
#endif

}

// Release this map so its not unmapped when the object is destroyed.
// Only safe to use when the pointer, and size (linux only) is stored by something else that will unmap it later.
void MappedFile::Release() {
    released = true;
}

MappedFile::~MappedFile()
{
    if (!released) {
        Close();
    }
}
