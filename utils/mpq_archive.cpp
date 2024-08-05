#include "mpq_archive.h"
#include <string.h>

MpqArchive::MpqArchive() {

}

MpqArchive::MpqArchive(const char* path) {
    OpenArchive(path);
}

MpqArchive::~MpqArchive()
{
    CloseArchive();
    for (auto s : files) {
        if (s != nullptr) {
            free((void*)s);
        }
    }
    files.clear();
}

bool MpqArchive::OpenArchive(const char *path) {
    SFileOpenArchive(path, 0, MPQ_OPEN_READ_ONLY, &mArchive);
    return false;
}

bool MpqArchive::IsArchiveOpen() const
{
    return mArchive != nullptr;
}

bool MpqArchive::CloseArchive()
{
    SFileCloseArchive(mArchive);
    mArchive = nullptr;
    return true;
}

int64_t MpqArchive::GetNumFiles()
{
    // TODO SIMD
    if (mFileCount == 0) {
        size_t fileSize;
        void* listFile = ReadFile("(listfile)", &fileSize);
        for (size_t i = 0; i < fileSize; i++) {
            if (static_cast<char*>(listFile)[i] == '\n'){
                mFileCount++;
            }
        }
    }
    return mFileCount;
}

void* MpqArchive::ReadFile(const char *filePath, size_t* bytesRead) {
    HANDLE mpqFile;
    DWORD bytesRead1;
    bool res1 = SFileOpenFileEx(mArchive, filePath, 0, &mpqFile);
    size_t fileSize = GetFileSize(mpqFile);
    void* data = malloc(fileSize);
    bool res = SFileReadFile(mpqFile, data, fileSize, (DWORD*)&bytesRead1, nullptr);
    SFileCloseFile(mpqFile);
    *bytesRead = bytesRead1;
    return data;
}

size_t MpqArchive::GetFileSize(const char *path) const {
    HANDLE file;
    // TODO error handling
    SFileOpenFileEx(mArchive, path, 0, &file);
    size_t size = GetFileSize(file);
    SFileCloseFile(file);
    return size;

}

size_t MpqArchive::GetFileSize(HANDLE fileHandle) const {
    DWORD size = SFileGetFileSize(fileHandle, nullptr);
    return static_cast<size_t>(size);
}

void MpqArchive::GenFileList() {
    size_t size = GetNumFiles();
    SFILE_FIND_DATA data;
    HANDLE file = SListFileFindFirstFile(mArchive, nullptr, "*", &data);
    files.reserve(size);

    files.push_back(strdup(data.cFileName));
    while (SListFileFindNextFile(file, &data)) {
        files.push_back(strdup(data.cFileName));
    }
    SListFileFindClose(file);
}
