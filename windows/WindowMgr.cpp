#include "WindowMgr.h"
#include "MainWindow.h"
#include "ExploreArchive.h"
#include "CreateArchive.h"
#include "CreateFromDir.h"
#include "CustomAudio.h"
#include "CustomStreamedAudio.h"
#include "CustomSequencedAudio.h"
#include"CustomSoundFontWindow.h"

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
            case WindowId::CustomAudio:
                mCurWindow = std::make_unique<CustomAudioWindow>();
                break;
            case WindowId::CustomStreamedAudio:
                mCurWindow = std::make_unique<CustomStreamedAudioWindow>();
                break;
            case WindowId::CustomSequencedAudio:
                mCurWindow = std::make_unique<CustomSequencedAudioWindow>();
                break;
            case WindowId::CustomSoundFont:
                mCurWindow = std::make_unique<CustomSoundFontWindow>();
            break;
        }
    }
    windowShouldChange = false;
}
