#ifndef WINDOWMGR_H
#define WINDOWMGR_H

#include <array>
#include <memory>
#include "WindowBase.h"

enum class WindowId {
    Main,
    Explore,
    Create,
    TexReplace,
    CustomAudio,
    FromDir,
    Max,
};

typedef void (*drawFunc)();

class WindowMgr {
    std::unique_ptr<WindowBase> mCurWindow;
    WindowId mCurWindowId = WindowId::Main;
    bool windowShouldChange = false;
public:
    WindowId GetCurWindow();
    void SetCurWindow(WindowId id);
    void DisplayCurWindow();
    void ProcessWindowChange();
};

extern WindowMgr gWindowMgr;

#endif //WINDOWMGR_H
