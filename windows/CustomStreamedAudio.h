#ifndef CUSTOM_STREAMED_AUDIO_H
#define CUSTOM_STREAMED_AUDIO_H

#include "WindowBase.h"
#include "threadSafeQueue.h"

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
    SafeQueue<char*> mFileQueue;
    char* mPathBuff = nullptr;
    char* mSavePath = nullptr;
    unsigned int fileCount = 0;
    int mRadioState = 2;
    bool mThreadStarted = false;
    bool mThreadIsDone = false;
    bool mPackAsArchive = false;
};

#endif
