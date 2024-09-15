#ifndef MPQ_ARCHIVE_H
#define MPQ_ARCHIVE_H

#include "archive.h"
#include "StormLib.h"

class MpqArchive : public Archive {
public:
    MpqArchive();
    MpqArchive(const char* path);
    ~MpqArchive();

    bool OpenArchive(const char* path) override;
    bool IsArchiveOpen() const override;
    bool CloseArchive() override;
    //bool ValidateArchive() override;
    int64_t GetNumFiles() override;

    // Will return a blob of data, allocated with `malloc`
    void* ReadFile(const char* filePath, size_t* byteRead) override;

    // Will return a blob of data. outBuffer already be allocated and be large enough to hold the data.
    //void ReadFile(const char* filePath, void** outBuffer) override;

    size_t GetFileSize(const char* path) const override;
    void GenFileList() override;
    void CreateArchiveFromList(std::vector<char*>& list, char* basePath) override;
    
    void WriteFile(char* path, const ArchiveDataInfo* data) override;
    void WriteFileUnlocked(char* path, const ArchiveDataInfo* data) override;
private:
    size_t GetFileSize(HANDLE fileHandle) const;
    HANDLE mArchive = nullptr;
    size_t mFileCount = 0;
};

#endif
