#ifndef FILEBOX_H
#define FILEBOX_H

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

#ifdef _WIN32
#endif

#endif