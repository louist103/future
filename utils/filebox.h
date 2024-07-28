#ifndef FILEBOX_H
#define FILEBOX_H

enum class FileBoxType {
	Archive,
	Max,
};

bool GetOpenFilePath(char** inputBuffer, FileBoxType type);
bool GetSaveFilePath(char** inputBuffer);
#ifdef _WIN32
#endif

#endif
