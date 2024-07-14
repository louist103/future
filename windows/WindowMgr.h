#ifndef WINDOWMGR_H
#define WINDOWMGR_H

#include <array>
#include <functional>
#include "MainWindow.h"

enum class WindowId {
    Main,
    Explore,
    Create,
    Max,
};

typedef void (*drawFunc)();

class WindowMgr {
    WindowId mCurWindowId = WindowId::Main;
    std::array<drawFunc, (size_t)WindowId::Max> mDrawFuncs = {
        DrawMainWindow,
        nullptr,
        nullptr,
    };
public:
    WindowId GetCurWindow();
    void SetCurWindow(WindowId id);
    void DisplayCurWindow();
};



#endif //WINDOWMGR_H
