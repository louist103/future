#ifndef CREATE_FROM_DIR_H
#define CREATE_FROM_DIR_H

#include "WindowBase.h"
#include <vector>
#include <memory>
#include <thread>

class CreateFromDirWindow : public WindowBase {
public:
    CreateFromDirWindow();
    ~CreateFromDirWindow();
    void DrawWindow() override;
    void FillFileQueue();

    std::vector<char*> mFileQueue;
    std::thread mAddFileThread;
    char* mPathBuff = nullptr;
    char* mSavePath = nullptr;
    double mProgress = 0.0;
    bool mThreadIsDone = false;
    bool mThreadStarted = false;
    int mRadioState = 0;
private:
    void DrawPendingFilesList();
    void ClearPathBuff();
    void ClearSaveBuff();
};

#endif
