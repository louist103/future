#ifndef EXPLORE_ARCHIVE_WINDOW_H
#define EXPLORE_ARCHIVE_WINDOW_H

#include "WindowBase.h"
#include <memory>
#include <vector>
#include "zip.h"
#include "StormLib.h"
#include "archive.h"

enum class ArchiveType : uint8_t {
    Unchecked,
    O2R,
    OTR,
};

class FileViewerWindow;

class ExploreWindow : public WindowBase {
public :
    ExploreWindow();
    ~ExploreWindow();
    void DrawWindow() override;
private:
    bool ValidateInputFile();
    bool OpenArchive();
    void SaveFile(char* path, const char* archiveFilePath);
    void DrawFileList();

    std::vector<const char*> mArchiveFiles;
    std::unique_ptr<char[]> mArchiveErrStr;
    char* mPathBuff;
    std::unique_ptr<Archive> mArchive;
    //union {
    //    zip_t* zipArchive = nullptr;
    //    HANDLE mpqArchive;
    //} mArchive;
    std::unique_ptr<FileViewerWindow> viewWindow;

    bool mFileValidated = false;
    bool mFileChangedSinceValidation = false;
    bool mFailedToOpenArchive = false;
    enum ArchiveType mArchiveType = ArchiveType::Unchecked;
};

class MemoryEditor;

class FileViewerWindow {
public:
    FileViewerWindow(Archive* archive, const char* path);
    ~FileViewerWindow();
    void DrawWindow();
    bool mIsOpen = true;
private:
    void* mData;
    size_t mSize;
    std::unique_ptr<MemoryEditor> mEditor;

};

#endif

