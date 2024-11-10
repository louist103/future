#ifndef CUSTOM_STREAMED_AUDIO_H
#define CUSTOM_STREAMED_AUDIO_H

#include "WindowBase.h"
#include "threadSafeQueue.h"
#include <unordered_map>

typedef union IntFloat {
    float f;
    uint32_t i;
} IntFloat;

typedef struct SeqMetaInfo {
    IntFloat loopStart;
    IntFloat loopEnd;
    bool fanfare;
} SeqMetaInfo;

class CustomStreamedAudioWindow : public WindowBase {
public:
    CustomStreamedAudioWindow();
    ~CustomStreamedAudioWindow();
    void DrawWindow();
    int GetRadioState();
    char* GetSavePath();
    bool GetLoopTimeType();
private:
    void DrawPendingFilesList();
    void ClearPathBuff();
    void ClearSaveBuff();
    void ClearFanfareMap();
    void FillFanfareMap();
    SafeQueue<char*> mFileQueue;
    std::unordered_map<char*, SeqMetaInfo> mSeqMetaMap;
    char* mPathBuff = nullptr;
    char* mSavePath = nullptr;
    unsigned int fileCount = 0;
    int mRadioState = 2;
    bool mThreadStarted = false;
    bool mThreadIsDone = false;
    bool mPackAsArchive = true;
    bool mLoopIsISamples = false;
};

#endif
