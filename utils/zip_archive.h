#ifndef ZIP_ARCHIVE_H
#define ZIP_ARCHIVE_H

#include "archive.h"
#include "stdlib.h"
#include "zip.h"
#include "mio.hpp"

typedef struct MappedFileInfo {
    void* data;
    // linux needs the size of the mapped region to unmap it. Windows does not.
#if not defined(_WIN32)
    size_t size;
#endif
} MappedFileInfo;

class ZipArchive : public Archive {
public:
    ZipArchive();
    ZipArchive(const char* path);
    ~ZipArchive();

    bool OpenArchive(const char* path) override;
    bool IsArchiveOpen() const override;
    bool CloseArchive() override;
    //bool ValidateArchive() override;
    int64_t GetNumFiles() override;

    // Will return a blob of data, allocated with `malloc`
    void* ReadFile(const char* filePath, size_t* bytesRead) override;

    // Will return a blob of data. outBuffer already be allocated and be large enough to hold the data.
    //void ReadFile(const char* filePath, void** outBuffer) override;

    size_t GetFileSize(const char* path) const override;
    void GenFileList() override;
    void CreateArchiveFromList(std::vector<char*>& list, char* basePath) override;
    void RegisterProgressCallback(zip_progress_callback cb, void* callingClass);

    void WriteFile(char* path, const ArchiveDataInfo* data) override;
    void WriteFileUnlocked(char* path, const ArchiveDataInfo* data) override;
private:
    std::vector<void*> mCopiedData;
    std::vector<MappedFileInfo> mMemoryMaps;
    zip_t* mArchive = nullptr;

};

#endif
