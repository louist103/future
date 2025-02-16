#include "CustomSoundFontWindow.h"

#include <font_awesome.h>

#include "imgui.h"
#include "WindowMgr.h"
#include "imgui_internal.h"
#include "filebox.h"
#include "mio.hpp"
#include "zip_archive.h"
#include "mpq_archive.h"
#include "CustomStreamedAudio.h"
#include "dr_mp3.h"
#include "dr_wav.h"
#include "dr_flac.h"

// 'ID3' as a string
#define MP3_ID3_CHECK(d) ((d[0] == 'I') && (d[1] == 'D') && (d[2] == '3'))
// FF FB, FF F2, FF F3
#define MP3_NON_ID3_CHECK(d) ((d[0] == 0xFF) && ((d[1] == 0xFB) || (d[1] == 0xF2) || (d[1] == 0xF3)))
#define MP3_CHECK(d) (MP3_ID3_CHECK(d) || MP3_NON_ID3_CHECK(d))

#define WAV_CHECK(d) ((d[0] == 'R') && (d[1] == 'I') && (d[2] == 'F') && (d[3] == 'F'))

#define FLAC_CHECK(d) ((d[0] == 'f') && (d[1] == 'L') && (d[2] == 'a') && (d[3] == 'C'))

#define OGG_CHECK(d) ((d[0] == 'O') && (d[1] == 'g') && (d[2] == 'g') && (d[3] == 'S'))

static void DrawSample(const char* type, ZSample* sample, bool* modified);

#define TAG_SIZE(n) (sizeof(n) + sizeof(void*)*2)

static const char* GetMediumStr(uint8_t medium) {
    switch (medium) {
        case 0:
            return "Ram";
        case 1:
            return "Unk";
        case 2:
            return "Cart";
        case 3:
            return "Disk";
        case 5:
            return "RamUnloaded";
        default:
            return "ERROR";
    }
}

static const char* GetCachePolicyStr(uint8_t policy) {
    switch (policy) {
        case 0:
            return "Temporary";
        case 1:
            return "Persistent";
        case 2:
            return "Either";
        case 3:
            return "Permanent";
        default:
            return "ERROR";
    }
}
// strup but using `new` for compatibility with the file box API
static const char* StrDupNew(const char* orig) {
    if (orig == nullptr) {
        return nullptr;
    }
    size_t len = strlen(orig);
    const char* newStr = new char[len + 1];
    memset(const_cast<char*>(newStr), 0, len);
    strcpy(const_cast<char*>(newStr), orig);
    return newStr;
}

CustomSoundFontWindow::CustomSoundFontWindow() {
}

CustomSoundFontWindow::~CustomSoundFontWindow() {
    ClearPathBuff();
    ClearSoundFont();
}

void CustomSoundFontWindow::ClearPathBuff()
{
    if (mPathBuff != nullptr) {
        delete[] mPathBuff;
        mPathBuff = nullptr;
    }
}

void CustomSoundFontWindow::DrawWindow() {
    ImGui::Begin("Create Custom Sequenced Audio", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Main);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Create Archive With Sequenced Songs");
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 3.0f);


    if (ImGui::Button("Select Soundfont XML")) {
        sfParsed = false;
        ClearPathBuff();
        GetOpenFilePath(&mPathBuff, FileBoxType::Max);
        mDoc.LoadFile(mPathBuff);
        if (!mDoc.Error()) {
            ParseSoundFont();
            sfParsed = true;
        }
    }

    if (mDoc.Error()) {
        ImGui::Text("XML Error tinyxml returned: %s", mDoc.ErrorStr());
        goto end;
    }

    if (sfParsed) {
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            Save();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Save Patch Only");
        ImGui::SetItemTooltip("Only save instruments, drums, and SFX that are modified.\nRecommended for compatability with other packs.");
        ImGui::SameLine();
        ImGui::Checkbox("##patchonly", &mExportPatchOnly);

        char* sfStr = strrchr(mPathBuff, PATH_SEPARATOR);
        if (sfStr == nullptr) {
            sfStr = mPathBuff;
        }
        sfStr++;
        ImGui::Text("Soundfont: %s loaded", sfStr);
        const ImVec2 fiveCharsWidth = ImGui::CalcTextSize("000000");
        ImGui::PushItemWidth(fiveCharsWidth.x);
        const char* icon = mSfLocked ? ICON_FA_LOCK : ICON_FA_UNLOCK;
        if (ImGui::Button(icon)) {
            mSfLocked ^= 1;
        }
        DrawHeader(mSfLocked);
        DrawDrums(mSfLocked);
        DrawInstruments(mSfLocked);
        if (mSoundFont.numSfx != 0) {
            DrawSfxTbl(mSfLocked);
        }
    }
end:
    ImGui::End();
}

void CustomSoundFontWindow::ParseSoundFont() {
    tinyxml2::XMLElement *root = mDoc.FirstChildElement("SoundFont");
    mSoundFont.medium = StrDupNew(root->Attribute("Medium"));
    mSoundFont.cachePolicy = StrDupNew(root->Attribute("CachePolicy"));
    mSoundFont.version = root->UnsignedAttribute("Version");
    mSoundFont.index = root->UnsignedAttribute("Num");
    mSoundFont.data1 = root->UnsignedAttribute("Data1");
    mSoundFont.data2 = root->UnsignedAttribute("Data2");
    mSoundFont.data3 = root->UnsignedAttribute("Data3");

    tinyxml2::XMLElement *child = root->FirstChildElement();
    while (child != nullptr) {
        const char *elemName = child->Name();
        if (strcmp(elemName, "Drums") == 0) {
            ParseDrums(child);
        } else if (strcmp(elemName, "Instruments") == 0) {
            ParseInstruments(child);
        } else if (strcmp(elemName, "SfxTable") == 0) {
            ParseSfxTbl(child);
        }
        child = child->NextSiblingElement();
    }
}

uint8_t CustomSoundFontWindow::ParseEnvelopes(tinyxml2::XMLElement* envsELem, ZEnvelope** envs) {
    unsigned int numEnvelopes = envsELem->UnsignedAttribute("Count");
    *envs = new ZEnvelope[numEnvelopes];
    tinyxml2::XMLElement* envelope = envsELem->FirstChildElement("Envelope");
    unsigned int i = 0;
    while (envelope != nullptr) {
        (*envs)[i].delay = envelope->UnsignedAttribute("Delay");
        (*envs)[i].arg = envelope->UnsignedAttribute("Arg");
        envelope = envelope->NextSiblingElement("Envelope");
        i++;
    }
    return i;
}

void CustomSoundFontWindow::ParseDrums(tinyxml2::XMLElement* drumsElem) {
    mSoundFont.numDrums = drumsElem->UnsignedAttribute("Count");
    mSoundFont.drums = new ZDrum[mSoundFont.numDrums];
    tinyxml2::XMLElement* drum = drumsElem->FirstChildElement("Drum");
    unsigned int i = 0;
    while (drum != nullptr) {
        mSoundFont.drums[i].releaseRate = drum->UnsignedAttribute("ReleaseRate");
        mSoundFont.drums[i].pan = drum->UnsignedAttribute("Pan");
        mSoundFont.drums[i].sample.tuning = drum->FloatAttribute("Tuning");
        mSoundFont.drums[i].sample.path = StrDupNew(drum->Attribute("SampleRef"));
        tinyxml2::XMLElement* envs = drum->FirstChildElement("Envelopes");
        mSoundFont.drums[i].numEnvelopes = ParseEnvelopes(envs, &mSoundFont.drums[i].envs);
        drum = drum->NextSiblingElement("Drum");
        i++;
    }
    // return true;
}

void CustomSoundFontWindow::ParseInstruments(tinyxml2::XMLElement* instrumentsElem) {
    mSoundFont.numInstruments = instrumentsElem->UnsignedAttribute("Count");
    mSoundFont.instruments = new ZInstrument[mSoundFont.numInstruments];
    tinyxml2::XMLElement* instrument = instrumentsElem->FirstChildElement("Instrument");
    unsigned int i = 0;
    while (instrument != nullptr) {
        mSoundFont.instruments[i].isValid = instrument->BoolAttribute("IsValid");
        mSoundFont.instruments[i].normalRangeLo = instrument->UnsignedAttribute("NormalRangeLo");
        mSoundFont.instruments[i].normalRangeHi = instrument->UnsignedAttribute("NormalRangeHi");
        mSoundFont.instruments[i].releaseRate = instrument->UnsignedAttribute("ReleaseRate");

        tinyxml2::XMLElement* instChild = instrument->FirstChildElement();
        while (instChild != nullptr) {
            if (strcmp(instChild->Name(), "Envelopes") == 0) {
                mSoundFont.instruments[i].numEnvelopes = ParseEnvelopes(instChild, &mSoundFont.instruments[i].envs);
            } else if (strcmp(instChild->Name(), "LowNotesSound") == 0) {
                mSoundFont.instruments[i].lowNoteSound.path = StrDupNew(instChild->Attribute("SampleRef"));
                mSoundFont.instruments[i].lowNoteSound.tuning = instChild->FloatAttribute("Tuning");
            } else if (strcmp(instChild->Name(), "NormalNotesSound") == 0) {
                mSoundFont.instruments[i].normalNoteSound.path = StrDupNew(instChild->Attribute("SampleRef"));
                mSoundFont.instruments[i].normalNoteSound.tuning = instChild->FloatAttribute("Tuning");
            } else if (strcmp(instChild->Name(), "HighNotesSound") == 0) {
                mSoundFont.instruments[i].highNoteSound.path = StrDupNew(instChild->Attribute("SampleRef"));
                mSoundFont.instruments[i].highNoteSound.tuning = instChild->FloatAttribute("Tuning");
            }
            instChild = instChild->NextSiblingElement();
        }
        instrument = instrument->NextSiblingElement("Instrument");
        i++;
    }
}

void CustomSoundFontWindow::ParseSfxTbl(tinyxml2::XMLElement *sfxTblElem) {
    mSoundFont.numSfx = sfxTblElem->UnsignedAttribute("Count");
    mSoundFont.sfx = new ZSfxTbl[mSoundFont.numSfx];
    tinyxml2::XMLElement* sfxElem = sfxTblElem->FirstChildElement("Sfx");
    unsigned int i = 0;
    while (sfxElem != nullptr) {
        mSoundFont.sfx[i].sample.path = StrDupNew(sfxElem->Attribute("SampleRef"));
        mSoundFont.sfx[i].sample.tuning = sfxElem->FloatAttribute("Tuning");
        sfxElem = sfxElem->NextSiblingElement("Sfx");
        i++;
    }
}

void CustomSoundFontWindow::ClearSoundFont() {
    for (unsigned int i = 0; i < mSoundFont.numDrums; i++) {
        delete[] mSoundFont.drums[i].envs;
        if (mSoundFont.drums[i].sample.path != nullptr) {
            delete[] mSoundFont.drums[i].sample.path;
        }
    }
    mSoundFont.numDrums = 0;
    if (mSoundFont.drums != nullptr) {
        delete[] mSoundFont.drums;
        mSoundFont.drums = nullptr;
    }

    for (unsigned int i = 0; i < mSoundFont.numInstruments; i++) {
        delete[] mSoundFont.instruments[i].envs;
    }
    mSoundFont.numInstruments = 0;
    if (mSoundFont.instruments != nullptr) {
        delete[] mSoundFont.instruments;
        mSoundFont.instruments = nullptr;
    }
    delete[] mSoundFont.sfx;
    delete[] mSoundFont.medium;
    delete[] mSoundFont.cachePolicy;
}

void CustomSoundFontWindow::DrawHeader(bool locked) const{
    ImGui::BeginDisabled(locked);
    ImGui::TextUnformatted("Version");
    ImGui::SameLine();
    ImGui::InputScalarN("##v", ImGuiDataType_S8, (void*)&mSoundFont.version, 1);
    ImGui::SameLine();
    ImGui::TextUnformatted("Index");
    ImGui::SameLine();
    ImGui::InputScalarN("##i", ImGuiDataType_S8, (void*)&mSoundFont.index, 1);
    ImGui::SameLine();
    ImGui::TextUnformatted("Data 1");
    ImGui::SameLine();
    ImGui::InputScalarN("##d1", ImGuiDataType_S16, (void*)&mSoundFont.data1, 1);
    ImGui::SameLine();
    ImGui::TextUnformatted("Data 2");
    ImGui::SameLine();
    ImGui::InputScalarN("##d2", ImGuiDataType_S16, (void*)&mSoundFont.data2, 1);
    ImGui::SameLine();
    ImGui::TextUnformatted("Data 3");
    ImGui::SameLine();
    ImGui::InputScalarN("##d3", ImGuiDataType_S16, (void*)&mSoundFont.data3, 1);
    ImGui::EndDisabled();
}

void CustomSoundFontWindow::DrawDrums(bool locked) const {
    ImGui::BeginDisabled(locked);
    char drumTreeTag[sizeof("Drums()") + 9];
    sprintf(drumTreeTag, "Drums(%u)", mSoundFont.numDrums);
    if (ImGui::TreeNode(drumTreeTag)) {

        for (unsigned int i = 0; i < mSoundFont.numDrums; i++) {
            bool modified = false;
            char envTreeTag[sizeof("Envelopes()") + 3];

            DrawSample("Drum", &mSoundFont.drums[i].sample, &modified);
            sprintf(envTreeTag, "Envelopes(%u)", mSoundFont.drums[i].numEnvelopes);
            if (ImGui::TreeNodeEx(mSoundFont.drums[i].envs, ImGuiTreeNodeFlags_None, "%s", envTreeTag)) {
                for (unsigned int j = 0; j < mSoundFont.drums[i].numEnvelopes; j++) {
                    char envTag[TAG_SIZE("##")];
                    sprintf(envTag, "Delay##%p", (&mSoundFont.drums[i].envs[j]));
                    if (ImGui::InputScalarN(envTag, ImGuiDataType_S16, &mSoundFont.drums[i].envs[j].delay, 1)) {
                        modified = true;
                    }
                    ImGui::SameLine();
                    sprintf(envTag, "Arg##%p", (&mSoundFont.drums[i].envs[j]) + 1);
                    if (ImGui::InputScalarN(envTag, ImGuiDataType_S16, &mSoundFont.drums[i].envs[j].arg, 1)) {
                        modified = true;
                    }
                }
                ImGui::TreePop();
            }
            if (modified) {
                mSoundFont.drums[i].modified = true;
            }

        }
        ImGui::TreePop();
    }
    ImGui::EndDisabled();
}

void CustomSoundFontWindow::DrawInstruments(bool locked) const {
    char instTreeTag[sizeof("Instruments()") + 9];

    sprintf(instTreeTag, "Instruments(%u)", mSoundFont.numInstruments);
    if (ImGui::TreeNode(instTreeTag)) {
        for (unsigned int i = 0; i < mSoundFont.numInstruments; i++) {
            bool modified = false;
            char checkboxTag[TAG_SIZE("##")];
            char rangeLoTag[TAG_SIZE("##")];
            char rangeHiTag[TAG_SIZE("##")];
            char releaseRateTag[TAG_SIZE("##")];

            sprintf(checkboxTag, "##%p", &mSoundFont.instruments[i].isValid);
            sprintf(rangeLoTag, "##%p", &mSoundFont.instruments[i].normalRangeLo);
            sprintf(rangeHiTag, "##%p", &mSoundFont.instruments[i].normalRangeHi);
            sprintf(releaseRateTag, "##%p", &mSoundFont.instruments[i].releaseRate);
            ImGui::TextUnformatted("Valid ");
            ImGui::SameLine();
            ImGui::Checkbox(checkboxTag, &mSoundFont.instruments[i].isValid);
            ImGui::SameLine();
            ImGui::TextUnformatted("  Range Low ");
            ImGui::SameLine();
            if (ImGui::InputScalarN(rangeLoTag, ImGuiDataType_U8, &mSoundFont.instruments[i].normalRangeLo, 1)) {
                modified = true;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted("  Range Hi ");
            ImGui::SameLine();
            if (ImGui::InputScalarN(rangeHiTag, ImGuiDataType_U8, &mSoundFont.instruments[i].normalRangeHi, 1)) {
                modified = true;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted("  Release Rate ");
            ImGui::SameLine();
            if (ImGui::InputScalarN(releaseRateTag, ImGuiDataType_U8, &mSoundFont.instruments[i].releaseRate, 1)) {
                modified = true;
            }
            DrawSample("Low Note Sound", &mSoundFont.instruments[i].lowNoteSound, &modified);
            DrawSample("Normal Note Sound", &mSoundFont.instruments[i].normalNoteSound, &modified);
            DrawSample("High Note Sound", &mSoundFont.instruments[i].highNoteSound, &modified);
            if (modified) {
                mSoundFont.instruments[i].modified = true;
            }
        }

        ImGui::TreePop();
    }
}

void CustomSoundFontWindow::DrawSfxTbl(bool locked) const {
    char sfxTreeTag[sizeof("Sfx()") + 3];
    sprintf(sfxTreeTag, "Sfx(%u)", mSoundFont.numSfx);
    if (ImGui::TreeNode(sfxTreeTag)) {
        for (unsigned int i = 0; i < mSoundFont.numSfx; i++) {
            bool modified;
            DrawSample("Sfx", &mSoundFont.sfx[i].sample, &modified);
            if (modified) {
                mSoundFont.sfx[i].modified = true;
            }
        }
        ImGui::TreePop();
    }
}

static void DrawSample(const char* type, ZSample* sample, bool* modified) {
    char sampleButtonTag[TAG_SIZE(ICON_FA_PENCIL"##")];
    sprintf(sampleButtonTag, "%s##%p", ICON_FA_PENCIL, &sample->path);
    ImGui::Indent();
    if (sample->path != nullptr && sample->path[0] != 0) {
        char tuningTag[TAG_SIZE("##")];

        //ImGui::Text("%s %s  ",type, sample->path);
        const char* sampleStart = strrchr(sample->path, PATH_SEPARATOR) + 1;
        const char* sampleEnd = strrchr(sample->path, '_');
        ImGui::Text("%s", type);
        ImGui::SameLine();
        ImGui::TextEx(sampleStart, sampleEnd);
        sprintf(tuningTag, "##%p", &sample->path);

        ImGui::SameLine();
        if (ImGui::Button(sampleButtonTag)) {
            goto openNewSample;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Tuning");
        ImGui::SameLine();
        if (ImGui::InputScalarN(tuningTag, ImGuiDataType_Float, &sample->tuning, 1)) {
            *modified = true;
        }
    } else {
        ImGui::Text("%s Empty", type);
        ImGui::SameLine();
        if (ImGui::Button(sampleButtonTag)) {
            openNewSample:
            if (GetOpenFilePath(const_cast<char**>(&sample->path), FileBoxType::Max)) {
                *modified = true;
                // At this point `path` contains a path to a location on disk, it will be converted and added to the archive when saved.
            }
        }
    }
    ImGui::Unindent();
}

static void WriteEnvData(ZEnvelope* zEnvs, unsigned int numEnvelopes, tinyxml2::XMLElement* envDoc) {
    tinyxml2::XMLElement* envs = envDoc->InsertNewChildElement("Envelopes");
    envs->SetAttribute("Count", numEnvelopes);

    for (unsigned int i = 0; i < numEnvelopes; i++) {
        tinyxml2::XMLElement* env = envs->InsertNewChildElement("Envelope");
        env->SetAttribute("Delay", zEnvs[i].delay);
        env->SetAttribute("Arg", zEnvs[i].arg);
    }
    envDoc->InsertEndChild(envs);
}

extern std::unique_ptr<char[]> CopySampleData(char* input, char* fileName, bool fromDisk, size_t size, Archive* a);
extern std::unique_ptr<char[]> CreateSampleXml(char* fileName, const char* audioType, uint64_t numFrames, uint64_t numChannels, SeqMetaInfo* info, uint64_t sampleRate, bool loopTimeInSamples, Archive* a);

enum AudioType {
    mp3,
    wav,
    ogg,
    flac,
};

constexpr static char fontXmlBase[] = "custom/fonts/";
constexpr static char sampleNameBase[] = "custom/samples/";
constexpr static char sampleDataBase[] = "custom/sampleData/";
constexpr static char sampleXmlBase[] = "custom/samples/";

static void SaveSample(ZSample* sample, tinyxml2::XMLElement* elem, Archive* a) {
    if (sample->path != nullptr) {
        if (sample->path[0] == '/') {
            std::unique_ptr<char[]> samplePath;
            std::unique_ptr<char[]> sampleXmlPath;
            const char* fileName = strrchr(sample->path, '/');
            fileName++;
            uint32_t numChannels;
            uint64_t sampleRate;
            uint64_t numFrames;

            mio::mmap_source file(sample->path);
            if (WAV_CHECK(file.data())) {
                drwav wav;
                drwav_init_memory(&wav, file.data(), file.size(), nullptr);
                numChannels = wav.channels;
                sampleRate = wav.sampleRate;
                numFrames = wav.totalPCMFrameCount;
                SeqMetaInfo info;
                info.fanfare = true;
                info.loopEnd.i = numFrames;
                info.loopStart.i = 0;

                if (!(wav.translatedFormatTag == DR_WAVE_FORMAT_PCM && wav.bitsPerSample == 16)) {
                    // Uh oh
                    return;
                }
                sampleXmlPath = CreateSampleXml(const_cast<char*>(fileName), "wav", numFrames, numChannels, &info, sampleRate, true, a);
                CopySampleData(const_cast<char*>(sample->path), const_cast<char*>(fileName), true, file.size(), a);

            } else if (MP3_CHECK(file.data())) {

            } else if (FLAC_CHECK(file.data())) {

            } else if (OGG_CHECK(file.data())) {

            }
            elem->SetAttribute("SampleRef", sampleXmlPath.get());
            elem->SetAttribute("Tuning", ((float)sampleRate * (float)numChannels) / 32000.0f);
            return;
        }
    }
    elem->SetAttribute("SampleRef", sample->path);
    elem->SetAttribute("Tuning", sample->tuning);
}

static void WriteInstrument(ZSample* zSample, tinyxml2::XMLElement* instrument, const char* name, Archive* a) {
    tinyxml2::XMLElement* inst = instrument->InsertNewChildElement(name);
    if (zSample->path != nullptr) {
        SaveSample(zSample, inst, a);
    }
    instrument->InsertEndChild(inst);
}

void CustomSoundFontWindow::Save() {
    char* outBuffer = nullptr;
    ZipArchive a;

    GetSaveFilePath(&outBuffer);
    a.OpenArchive(outBuffer);
    char* sfStr = strrchr(mPathBuff, PATH_SEPARATOR);
    sfStr++;

    tinyxml2::XMLDocument outDoc;
    tinyxml2::XMLElement* root = outDoc.NewElement("SoundFont");

    root->SetAttribute("Version", 0);
    root->SetAttribute("Num", mSoundFont.index);
    root->SetAttribute("Medium", mSoundFont.medium);
    root->SetAttribute("CachePolicy", mSoundFont.cachePolicy);
    root->SetAttribute("Data1", mSoundFont.data1);
    root->SetAttribute("Data2", mSoundFont.data2);
    root->SetAttribute("Data3", mSoundFont.data3);
    root->SetAttribute("Patches", sfStr);
    outDoc.InsertFirstChild(root);
    tinyxml2::XMLElement* drums = root->InsertNewChildElement("Drums");
    drums->SetAttribute("Count", mSoundFont.numDrums);
    for (unsigned int i = 0; i < mSoundFont.numDrums; i++) {
        if (!mExportPatchOnly || mSoundFont.drums[i].modified) {
            tinyxml2::XMLElement* drum = drums->InsertNewChildElement("Drum");
            drum->SetAttribute("ReleaseRate", mSoundFont.drums[i].releaseRate);
            drum->SetAttribute("Pan", mSoundFont.drums[i].pan);
            drum->SetAttribute("Loaded", 0);
            SaveSample(&mSoundFont.drums[i].sample, drum, &a);
            WriteEnvData(mSoundFont.drums[i].envs, mSoundFont.drums[i].numEnvelopes, drum);
            if (mExportPatchOnly && mSoundFont.drums[i].modified) { // If we aren't exporting only the changes, it doesn't matter which entry is being modified
                drum->SetAttribute("Patches", i);
            }
            drums->InsertEndChild(drum);
        }
    }
    root->InsertEndChild(drums);

    tinyxml2::XMLElement* instruments = root->InsertNewChildElement("Instruments");
    instruments->SetAttribute("Count", mSoundFont.numInstruments);
    for (unsigned int i = 0; i < mSoundFont.numInstruments; i++) {
        if (!mExportPatchOnly || mSoundFont.instruments[i].modified) {
            tinyxml2::XMLElement* instrument = instruments->InsertNewChildElement("Instrument");
            instrument->SetAttribute("IsValid", mSoundFont.instruments[i].isValid);
            instrument->SetAttribute("Loaded", 0);
            instrument->SetAttribute("NormalRangeLo", mSoundFont.instruments[i].normalRangeLo);
            instrument->SetAttribute("NormalRangeHi", mSoundFont.instruments[i].normalRangeHi);
            instrument->SetAttribute("ReleaseRate", mSoundFont.instruments[i].releaseRate);
            if (mExportPatchOnly && mSoundFont.instruments[i].modified) {
                instrument->SetAttribute("Patches", i);
            }
            WriteEnvData(mSoundFont.instruments[i].envs, mSoundFont.instruments[i].numEnvelopes, instrument);
            WriteInstrument(&mSoundFont.instruments[i].lowNoteSound, instrument, "LowNotesSound", &a);
            WriteInstrument(&mSoundFont.instruments[i].normalNoteSound, instrument, "NormalNotesSound", &a);
            WriteInstrument(&mSoundFont.instruments[i].highNoteSound, instrument, "HighNotesSound", &a);
            instruments->InsertEndChild(instrument);
        }
    }
    root->InsertEndChild(instruments);

    tinyxml2::XMLElement* sfxTbl = root->InsertNewChildElement("SfxTable");
    sfxTbl->SetAttribute("Count", mSoundFont.numSfx);
    for (unsigned int i = 0; i < mSoundFont.numSfx; i++) {
        if (!mExportPatchOnly || mSoundFont.sfx[i].modified) {
            tinyxml2::XMLElement* inst = sfxTbl->InsertNewChildElement("Sfx");
            if (mSoundFont.sfx[i].sample.path != nullptr) {
                SaveSample(&mSoundFont.sfx[i].sample, inst, &a);
                //inst->SetAttribute("SampleRef", mSoundFont.sfx[i].sample.path);
                //inst->SetAttribute("Tuning", mSoundFont.sfx[i].sample.tuning);
                if (mExportPatchOnly && mSoundFont.sfx[i].modified) {
                    inst->SetAttribute("Patches", i);
                }
            }
            sfxTbl->InsertEndChild(inst);
        }
    }
    root->InsertEndChild(sfxTbl);
    outDoc.InsertEndChild(root);
    tinyxml2::XMLPrinter printer;
    outDoc.Accept(&printer);
    ArchiveDataInfo info = {
        .data = (void*)printer.CStr(), .size = printer.CStrSize() - 1, .mode = DataCopy
    };
    auto outSfPath = std::make_unique<char[]>(sizeof(fontXmlBase) + strlen(sfStr) + sizeof("_PATCH") + 1);
    sprintf(outSfPath.get(), "%s%s_PATCH", fontXmlBase, sfStr);
    a.WriteFile(outSfPath.get(), &info);
    a.CloseArchive();
    delete[] outBuffer;
}