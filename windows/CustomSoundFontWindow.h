#ifndef CUSTOMSOUNDFONTWINDOW_H
#define CUSTOMSOUNDFONTWINDOW_H

#include <memory>

#include "WindowBase.h"
#include "tinyxml2.h"

// Not the actual order

struct ZSample {
    const char* path;
    float tuning;
    bool modified;
};

struct ZEnvelope {
    int16_t delay;
    int16_t arg;
};

struct ZDrum {
    ZSample sample;
    ZEnvelope* envs;
    uint8_t numEnvelopes;
    uint8_t releaseRate;
    uint8_t pan;
};

struct ZInstrument {
    ZSample lowNoteSound;
    ZSample normalNoteSound;
    ZSample highNoteSound;
    ZEnvelope* envs;
    uint8_t numEnvelopes;
    bool isValid;
    uint8_t normalRangeLo;
    uint8_t normalRangeHi;
    uint8_t releaseRate;
};

struct ZSfxTbl {
    ZSample sample;
};

struct ZSoundFont {
    const char* medium;
    const char* cachePolicy;
    ZDrum* drums;
    ZInstrument* instruments;
    ZSfxTbl* sfx;
    uint32_t numDrums;
    uint32_t numInstruments;
    uint32_t numSfx;

    uint16_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t version;
    uint8_t index;
};


class CustomSoundFontWindow : public WindowBase {
public:
    CustomSoundFontWindow();
    ~CustomSoundFontWindow();
    void DrawWindow() override;
private:
    void ClearPathBuff();
    void ParseSoundFont();
    void ClearSoundFont();
    uint8_t ParseEnvelopes(tinyxml2::XMLElement* envsELem, ZEnvelope** envs);
    void ParseDrums(tinyxml2::XMLElement* drumsElem);
    void ParseInstruments(tinyxml2::XMLElement* instrumentsElem);
    void ParseSfxTbl(tinyxml2::XMLElement* sfxTblElem);
    void DrawHeader(bool locked) const;
    void DrawDrums(bool locked) const;
    void DrawInstruments(bool locked) const;
    void DrawSfxTbl(bool locked) const;
    void Save();
    //void DrawSample(char* type);
    char* mPathBuff = nullptr;
    char* mSavePath = nullptr;
    tinyxml2::XMLDocument mDoc;
    ZSoundFont mSoundFont {};
    bool sfParsed = false;
    bool mSfLocked = false;
};



#endif
