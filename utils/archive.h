#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <cstdint>
#include <vector>
#include <cstdlib>

class Archive {
    public:
    Archive();
    virtual ~Archive();

    virtual bool OpenArchive(const char* path) = 0;
    virtual bool IsArchiveOpen() const = 0;
    virtual bool CloseArchive() = 0;
    //virtual bool ValidateArchive();
    virtual int64_t GetNumFiles() = 0;

    virtual void* ReadFile(const char* filePath, size_t* bytesRead) = 0;
    //virtual void ReadFile(const char* filePath, void** outBuffer);
    virtual size_t GetFileSize(const char* path) const = 0;
    virtual void GenFileList() = 0;
    // ZIP will keep the file names valid until the archive is closed so we don't need to free them.
    // MPQ will not so we need to allocate and free the strings.
    std::vector<const char*> files;
};


#endif
