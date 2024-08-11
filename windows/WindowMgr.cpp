#include "WindowMgr.h"
#include "MainWindow.h"
#include "ExploreArchive.h"
#include "CreateArchive.h"
#include "CreateFromDir.h"

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
            case WindowId::Create:
                mCurWindow = std::make_unique<CreateArchiveWindow>();
                break;
            case WindowId::FromDir:
                mCurWindow = std::make_unique<CreateFromDirWindow>();
                break;
        }
    }
    windowShouldChange = false;
}
