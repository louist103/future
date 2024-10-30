#ifndef CUSTOM_STREAMED_AUDIO_H
#define CUSTOM_STREAMED_AUDIO_H

#include "WindowBase.h"
#include "threadSafeQueue.h"
#include <unordered_map>

class CustomStreamedAudioWindow : public WindowBase {
public:
    CustomStreamedAudioWindow();
    ~CustomStreamedAudioWindow();
    void DrawWindow();
    int GetRadioState();
    char* GetSavePath();
private:
    void DrawPendingFilesList();
    void ClearPathBuff();
    void ClearSaveBuff();
    void FillFanfareMap();
    SafeQueue<char*> mFileQueue;
    std::unordered_map<char*, bool> mFanfareMap;
    char* mPathBuff = nullptr;
    char* mSavePath = nullptr;
    unsigned int fileCount = 0;
    int mRadioState = 2;
    bool mThreadStarted = false;
    bool mThreadIsDone = false;
    bool mPackAsArchive = true;
};

#endif
