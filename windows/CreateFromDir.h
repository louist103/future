#ifndef CREATE_FROM_DIR_H
#define CREATE_FROM_DIR_H

#include "WindowBase.h"
#include <queue>
#include <memory>

class CreateFromDirWindow : public WindowBase {
public:
    CreateFromDirWindow();
    ~CreateFromDirWindow();
    void DrawWindow() override;
    void FillFileQueue();
private:
    std::queue<std::unique_ptr<char[]>> fileQueue;
    char* mPathBuff;
};

#endif
