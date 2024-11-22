#ifndef FILEBOX_H
#define FILEBOX_H

#include <cstddef>
#include <stdio.h>
#include <cstring>
#include <filesystem>

#if defined (_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
constexpr const char PATH_SEPARATOR = '\\';
#elif defined(__linux__) || defined(__APPLE__)
constexpr const char PATH_SEPARATOR = '/';
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

enum class FileBoxType {
	Archive,
	Max,
};

// Values come from windows.h
#ifndef IDYES
#define IDYES 6
#endif
#ifndef IDNO
#define IDNO 7
#endif

bool GetOpenDirPath(char** inputBuffer);
bool GetOpenFilePath(char** inputBuffer, FileBoxType type);
bool GetSaveFilePath(char** inputBuffer);
int ShowYesNoBox(const char* title, const char* box);
void ShowErrorBox(const char* title, const char* text);
size_t GetDiskFileSize(char* path);
int CopyFileData(char* src, char* dest);
int CreateDir(const char* dir);
void UnmapFile(void* data, size_t size);

typedef bool (*ExtCheckCallback)(char*);
// Fills a container of files in directory `mBasePath` filtered by extension.
// Extensions are filtered by the callback. The callback takes the full path and is
// responsible for getting the extension.
// dest can be any container that has the `push` function implemented.
// Caller is responsible for freeing the paths. Must be freed with `operator delete[]`
template <class T>
static void FillFileQueue(T& dest, char* mBasePath, ExtCheckCallback cb) {
#ifdef _WIN32
    char oldWorkingDir[MAX_PATH];
    char oldWorkingDir2[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, oldWorkingDir);
    bool ret = SetCurrentDirectoryA(mBasePath);
    GetCurrentDirectoryA(MAX_PATH, oldWorkingDir2);
    WIN32_FIND_DATAA ffd;
    HANDLE h = FindFirstFileA("*", &ffd);

    do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // Check for any standard N64 rom file extensions.
            if (cb(ffd.cFileName)) {
                size_t s1 = strlen(ffd.cFileName);
                size_t s2 = strlen(mBasePath);
                size_t sizeToAlloc = s1 + s2 + 2;

                char* fullPath = (char*)operator new[](sizeToAlloc, std::align_val_t(2));
                snprintf(fullPath, sizeToAlloc, "%s\\%s", mBasePath, ffd.cFileName);
                dest.push_back(fullPath);
            }
        }
    } while (FindNextFileA(h, &ffd) != 0);
    FindClose(h);
    SetCurrentDirectoryA(oldWorkingDir);
#elif defined(__linux__) || defined(__APPLE__)
    // Change the current working directory to the path selected.
    char oldWorkingDir[PATH_MAX];
    getcwd(oldWorkingDir, PATH_MAX);
    chdir(mBasePath);
    DIR* d = opendir(".");

    struct dirent* dir;

    if (d != nullptr) {
        // Go through each file in the directory
        while ((dir = readdir(d)) != nullptr) {
            struct stat path;

            // Check if current entry is not folder
            stat(dir->d_name, &path);
            if (S_ISREG(path.st_mode)) {
                if (cb(dir->d_name)) {
                    size_t s1 = strlen(dir->d_name);
                    size_t s2 = strlen(mBasePath);
                    size_t sizeToAlloc = s1 + s2 + 2;

                    char* fullPath = (char*)operator new[](sizeToAlloc, std::align_val_t(2)); //new char[sizeToAlloc];

                    snprintf(fullPath, sizeToAlloc, "%s%s", mBasePath, dir->d_name);
                    dest.push_back(fullPath);
                }
            }
        }
    }
    closedir(d);
    chdir(oldWorkingDir);
#else
    for (const auto& file : std::filesystem::directory_iterator("./")) {
        if (file.is_directory())
            continue;
        if ((file.path().extension() == ".wav") || (file.path().extension() == ".ogg") ||
            (file.path().extension() == ".mp3") || file.path().extension() == ".flac") {
                char* fullPath = (char*)operator new[](file.size(), std::align_val_t(2));
                strcpy(fullPath, file.path().string().c_str());
            dest.push_back(fullPath);
        }
    }
#endif
}

#endif
