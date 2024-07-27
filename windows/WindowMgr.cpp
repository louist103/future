#include "WindowMgr.h"

WindowId WindowMgr::GetCurWindow(){
    return mCurWindowId;
}

void WindowMgr::SetCurWindow(WindowId id) {
    mCurWindowId = id;
    windowShouldChange = true;
}

void WindowMgr::DisplayCurWindow() {
    mCurWindow->DrawWindow();
}

void WindowMgr::ProcessWindowChange()
{
    if (windowShouldChange) {
        switch (mCurWindowId) {
            case WindowId::Main:
                mCurWindow = std::make_unique<MainWindow>();
                break;
            case WindowId::Explore:
                mCurWindow = std::make_unique<ExploreWindow>();
                break;
        }
    }
    windowShouldChange = false;
}
