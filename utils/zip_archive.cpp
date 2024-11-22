#include "zip_archive.h"
#include "zip.h"
#include "filebox.h"
#include <cstring>
#include <memory>
ZipArchive::ZipArchive() {

}

ZipArchive::ZipArchive(const char* path) {
    OpenArchive(path);
}

ZipArchive::~ZipArchive() {
    CloseArchive();
    for (const auto d : mCopiedData) {
        free(d);
    }

    for (const auto m : mMemoryMaps) {
#if defined _WIN32
        UnmapFile(m.data, 0);
#else
        UnmapFile(m.data, m.size);
#endif
    }

    files.clear();
    
}

bool ZipArchive::OpenArchive(const char *path) {
    int error;
    mArchive = zip_open(path, ZIP_CHECKCONS | ZIP_CREATE, &error);
    if (mArchive == nullptr) {
        zip_error_t err;
            
        zip_error_init_with_code(&err, error);
        if (err.zip_err == ZIP_ER_INCONS) {
            if (ShowYesNoBox("ZIP Error", "ZIP Archive is inconsistent. Try opening it again without consistency checks?") == IDYES) {
                mArchive = zip_open(path, 0, &error);
                if (mArchive == nullptr) {
                    goto error;
                }
            }
        } else {
            error:
            const char* errStr = zip_error_strerror(&err);
            size_t errStrLen = strlen(errStr);
            std::unique_ptr<char[]> boxString;
            boxString = std::make_unique<char[]>((size_t)40 + errStrLen); // Enough space for the base string, numeric error, and error string.
            snprintf(boxString.get(), 0, "ZIP Error. libzip returned: %d %s", err.zip_err, boxString.get());
            ShowErrorBox("ZIP Error", boxString.get());
        }

        zip_error_fini(&err);
        return true;
    }


    return false;
}

bool ZipArchive::IsArchiveOpen() const
{
    return mArchive != nullptr;
}

bool ZipArchive::CloseArchive()
{
    if (!IsArchiveOpen()) {
        return false;
    }
    zip_close(mArchive);
    
    mArchive = nullptr;
    return true;
}

int64_t ZipArchive::GetNumFiles()
{
    if (!IsArchiveOpen()) {
        return 0;
    }

    return zip_get_num_entries(mArchive, 0);
}

void* ZipArchive::ReadFile(const char *filePath, size_t* readBytes) {
    if (!IsArchiveOpen()) {
        return nullptr;
    }
    size_t fileSize = GetFileSize(filePath);
    void* data = malloc(fileSize);

    if (data == nullptr) {
        return nullptr;
    }
    zip_file_t* zipFile = zip_fopen(mArchive, filePath, 0);
    *readBytes = zip_fread(zipFile, data, fileSize);
    zip_fclose(zipFile);
    
    return data;
}

size_t ZipArchive::GetFileSize(const char *path) const {
    if (!IsArchiveOpen()) {
        return 0;
    }

    zip_stat_t stat;
    zip_stat_init(&stat);
    zip_stat(mArchive, path, 0, &stat);
    return stat.size;
}

void ZipArchive::GenFileList() {
    size_t numFiles = GetNumFiles();
    if (numFiles != 0) {
        files.reserve(numFiles);
        for (zip_uint64_t i = 0; i < numFiles; i++) {
            files.push_back(zip_get_name(mArchive, i, ZIP_FL_ENC_GUESS));
        }
    }
}

void ZipArchive::CreateArchiveFromList(std::vector<char*>& list, char* pathBase) {
    size_t baseStrEnd = strlen(pathBase);
    while (!list.empty()) {
        zip_error_t err;
        // ZIP_LENGTH_TO_END does exists as of 1.10 but some linux distros don't support it yet
        size_t fileSize = GetDiskFileSize(list.back());
        zip_source_t* source = zip_source_file_create(list.back(), 0, fileSize, &err);

        char* newPath = &list.back()[baseStrEnd + 1];
        zip_int64_t rv = zip_file_add(mArchive, newPath, source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
        delete[] list.back();
        list.pop_back();
    }
}

void ZipArchive::RegisterProgressCallback(zip_progress_callback cb, void* callingClass) {
    zip_register_progress_callback_with_state(mArchive, 0.01, cb, nullptr, callingClass);
}

#if defined (_WIN32)
#define CREATE_MAPPED_INFO(data, size) {data}
#elif defined(__linux__) || defined(__APPLE__)
#define CREATE_MAPPED_INFO(data, size) {data, size}
#endif

void ZipArchive::WriteFile(char* path, const ArchiveDataInfo* data) {
    std::lock_guard<std::mutex> lock(m);
    WriteFileUnlocked(path, data);
    c.notify_one();
}

void ZipArchive::WriteFileUnlocked(char* path, const ArchiveDataInfo* data) {
    zip_error_t err;
    zip_source_t* source;

    switch (data->mode) {
    case DataCopy: {
        // libzip requires the data given used to create the source data
        // stay valid until the archive is closed. We will free this data in the destructor
        // after closing the archive. 
        // Must be allocated with `malloc`
        void* copy = malloc(data->size);

        memcpy(copy, data->data, data->size);
        source = zip_source_buffer_create(copy, data->size, 0, &err);
        mCopiedData.push_back(copy);
        break;
    }
    case MMappedFile: {
        // To avoid copying file data we can create a memory map of a file to add.
        // We still must maintain a pointer to this data but it avoids copies.
        // We will also unmap these in the destructor.
        source = zip_source_buffer_create(data->data, data->size, 0, &err);
        
        mMemoryMaps.push_back(CREATE_MAPPED_INFO(data->data, data->size));
        break;
    }
    }

    zip_int64_t rv = zip_file_add(mArchive, path, source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
    zip_set_file_compression(mArchive, rv, ZIP_CM_STORE, 0);
}