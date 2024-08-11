#ifndef CREATE_FROM_DIR_H
#define CREATE_FROM_DIR_H

#include "WindowBase.h"

class CreateFromDirWindow : public WindowBase {
public:
    CreateFromDirWindow();
    ~CreateFromDirWindow();
    void DrawWindow() override;
private:
    char* mPathBuff;
};

#endif
