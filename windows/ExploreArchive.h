#ifndef EXPLORE_ARCHIVE_WINDOW_H
#define EXPLORE_ARCHIVE_WINDOW_H

#include "WindowBase.h"
#include <memory>
#include "zip.h"

enum class ArchiveType : uint8_t {
	Unchecked,
	O2R,
	OTR,
};

class ExploreWindow : public WindowBase {
public :
	ExploreWindow();
	~ExploreWindow();
	void DrawWindow() override;
private:
	bool ValidateInputFile();

	char* mPathBuff;
	zip_t* zipArchive = nullptr;
	bool mFileValidated = false;
	bool mFileChangedSinceValidation = false;
	enum ArchiveType mArchiveType = ArchiveType::Unchecked;
};

#endif

