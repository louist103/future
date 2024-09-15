#ifndef FILEBOX_H
#define FILEBOX_H

#include <cstddef>

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

#ifdef _WIN32
#endif

#endif
