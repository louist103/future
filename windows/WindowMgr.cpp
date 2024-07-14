#include "WindowMgr.h"

WindowId WindowMgr::GetCurWindow(){
    return mCurWindowId;
}

void WindowMgr::SetCurWindow(WindowId id) {
    mCurWindowId = id;
}

void WindowMgr::DisplayCurWindow() {
    mDrawFuncs[(size_t)mCurWindowId]();
}