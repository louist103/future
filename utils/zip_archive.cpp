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

ZipArchive::~ZipArchive()
{
    files.clear();
    CloseArchive();
}

bool ZipArchive::OpenArchive(const char *path) {
    int error;
    mArchive = zip_open(path, ZIP_CHECKCONS, &error);
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
        for (zip_int64_t i = 0; i < numFiles; i++) {
            files.push_back(zip_get_name(mArchive, i, ZIP_FL_ENC_GUESS));
        }
    }
}
