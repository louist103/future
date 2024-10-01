#ifndef CUSTOM_SEQUENCED_AUDIO_H
#define CUSTOM_SEQUENCED_AUDIO_H

#include "WindowBase.h"
#include <vector>

typedef enum CheckState : uint8_t {
    Unchecked,
    Good,
    Bad,
} CheckState;
class CustomSequencedAudioWindow : public WindowBase {
public:
    CustomSequencedAudioWindow();
    ~CustomSequencedAudioWindow();
    void DrawWindow();
    int GetRadioState();
    char* GetSavePath();
private:
    void DrawPendingFilesList();
    void ClearPathBuff();
    void ClearSaveBuff();
    void CreateFilePairs();
    std::vector<char*> mFileQueue;
    // first is the meta file, second is the sequence
    std::vector<std::pair<char*, char*>> mFilePairs;
    char* mPathBuff = nullptr;
    char* mSavePath = nullptr;
    unsigned int fileCount = 0;
    int mRadioState = 2;
    bool mThreadStarted = false;
    bool mThreadIsDone = false;
    bool mPackAsArchive = false;
    CheckState pairCheckState = CheckState::Unchecked;
};

#endif
