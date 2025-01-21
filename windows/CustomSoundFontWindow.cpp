#include "CustomSoundFontWindow.h"

#include <font_awesome.h>

#include "imgui.h"
#include "WindowMgr.h"
#include "imgui_internal.h"
#include "filebox.h"

static void DrawSample(const char* type, ZSample* sample);

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
    ImGui::PopStyleColor();
    ImGui::End();
}

void CustomSoundFontWindow::ParseSoundFont() {
    tinyxml2::XMLElement *root = mDoc.FirstChildElement("SoundFont");
    mSoundFont.medium = root->Attribute("Medium");
    mSoundFont.cachePolicy = root->Attribute("CachePolicy");
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
    // return true;

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
        mSoundFont.drums[i].sample.path = drum->Attribute("SampleRef");
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
                mSoundFont.instruments[i].lowNoteSound.path = instChild->Attribute("SampleRef");
                mSoundFont.instruments[i].lowNoteSound.tuning = instChild->FloatAttribute("Tuning");
            } else if (strcmp(instChild->Name(), "NormalNotesSound") == 0) {
                mSoundFont.instruments[i].normalNoteSound.path = instChild->Attribute("SampleRef");
                mSoundFont.instruments[i].normalNoteSound.tuning = instChild->FloatAttribute("Tuning");
            } else if (strcmp(instChild->Name(), "HighNotesSound") == 0) {
                mSoundFont.instruments[i].highNoteSound.path = instChild->Attribute("SampleRef");
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
        mSoundFont.sfx[i].sample.path = sfxElem->Attribute("SampleRef");
        mSoundFont.sfx[i].sample.tuning = sfxElem->FloatAttribute("Tuning");
        sfxElem = sfxElem->NextSiblingElement("Sfx");
        i++;
    }
}

void CustomSoundFontWindow::ClearSoundFont() {
    for (unsigned int i = 0; i < mSoundFont.numDrums; i++) {
        delete[] mSoundFont.drums[i].envs;
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
    auto drumTreeTag = std::make_unique<char[]>(sizeof("Drums()") + 9);
    auto envTreeTag = std::make_unique<char[]>(sizeof("Envelopes()") + 3);
    sprintf(drumTreeTag.get(), "Drums(%u)", mSoundFont.numInstruments);
    if (ImGui::TreeNode(drumTreeTag.get())) {
        auto tuningTag = std::make_unique<char[]>(TAG_SIZE("##"));
        auto envTag = std::make_unique<char[]>(TAG_SIZE("##"));
        for (unsigned int i = 0; i < mSoundFont.numDrums; i++) {
            DrawSample("Drum", &mSoundFont.drums[i].sample);
            ImGui::Text("Num Envelopes: %d", mSoundFont.drums[i].numEnvelopes);
            sprintf(envTreeTag.get(), "Envelopes(%u)", mSoundFont.drums[i].numEnvelopes);
            if (ImGui::TreeNodeEx(mSoundFont.drums[i].envs, ImGuiTreeNodeFlags_None, envTreeTag.get())) {
                for (unsigned int j = 0; j < mSoundFont.drums[i].numEnvelopes; j++) {
                    sprintf(envTag.get(), "Delay##%p", (&mSoundFont.drums[i].envs[j]));
                    ImGui::InputScalarN(envTag.get(), ImGuiDataType_S16, &mSoundFont.drums[i].envs[j].delay, 1);
                    ImGui::SameLine();
                    sprintf(envTag.get(), "Arg##%p", (&mSoundFont.drums[i].envs[j]) + 1);
                    ImGui::InputScalarN(envTag.get(), ImGuiDataType_S16, &mSoundFont.drums[i].envs[j].arg, 1);
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
    ImGui::EndDisabled();
}

void CustomSoundFontWindow::DrawInstruments(bool locked) const {
    auto instTreeTag = std::make_unique<char[]>(sizeof("Instruments()") + 9);
    auto envTreeTag = std::make_unique<char[]>(sizeof("Envelopes()") + 3);
    sprintf(instTreeTag.get(), "Instruments(%u)", mSoundFont.numInstruments);
    if (ImGui::TreeNode(instTreeTag.get())) {
        auto envTag = std::make_unique<char[]>(TAG_SIZE("##"));
        auto checkboxTag = std::make_unique<char[]>(TAG_SIZE("##"));
        auto rangeLoTag = std::make_unique<char[]>(TAG_SIZE("##"));
        auto rangeHiTag = std::make_unique<char[]>(TAG_SIZE("##"));
        auto releaseRateTag = std::make_unique<char[]>(TAG_SIZE("##"));
        //auto tuningTag = std::make_unique<char[]>(TAG_SIZE("##"));
        //auto sampleButtonTag = std::make_unique<char[]>(TAG_SIZE(ICON_FA_PENCIL"##"));
        for (unsigned int i = 0; i < mSoundFont.numInstruments; i++) {
            sprintf(checkboxTag.get(), "##%p", &mSoundFont.instruments[i].isValid);
            sprintf(rangeLoTag.get(), "##%p", &mSoundFont.instruments[i].normalRangeLo);
            sprintf(rangeHiTag.get(), "##%p", &mSoundFont.instruments[i].normalRangeHi);
            sprintf(releaseRateTag.get(), "##%p", &mSoundFont.instruments[i].releaseRate);
            ImGui::TextUnformatted("Valid ");
            ImGui::SameLine();
            ImGui::Checkbox(checkboxTag.get(), &mSoundFont.instruments[i].isValid);
            ImGui::SameLine();
            ImGui::TextUnformatted("  Range Low ");
            ImGui::SameLine();
            ImGui::InputScalarN(rangeLoTag.get(), ImGuiDataType_U8, &mSoundFont.instruments[i].normalRangeLo, 1);
            ImGui::SameLine();
            ImGui::TextUnformatted("  Range Hi ");
            ImGui::SameLine();
            ImGui::InputScalarN(rangeHiTag.get(), ImGuiDataType_U8, &mSoundFont.instruments[i].normalRangeHi, 1);
            ImGui::SameLine();
            ImGui::TextUnformatted("  Release Rate ");
            ImGui::SameLine();
            ImGui::InputScalarN(releaseRateTag.get(), ImGuiDataType_U8, &mSoundFont.instruments[i].releaseRate, 1);

            DrawSample("Low Note Sound", &mSoundFont.instruments[i].lowNoteSound);
            DrawSample("Normal Note Sound", &mSoundFont.instruments[i].normalNoteSound);
            DrawSample("High Note Sound", &mSoundFont.instruments[i].highNoteSound);
#if 0
            if (mSoundFont.instruments[i].lowNoteSound.path != nullptr) {
                ImGui::Text("Low Note Sound %s  ", mSoundFont.instruments[i].lowNoteSound.path);
                sprintf(tuningTag.get(), "##%p", &mSoundFont.instruments[i].lowNoteSound);
                ImGui::SameLine();
                ImGui::TextUnformatted("Tuning");
                ImGui::SameLine();
                ImGui::InputScalarN(tuningTag.get(), ImGuiDataType_Float, &mSoundFont.instruments[i].lowNoteSound.tuning, 1);
            } else {
                ImGui::TextUnformatted("Low Note Sound Empty");
            }

            if (mSoundFont.instruments[i].normalNoteSound.path != nullptr) {
                ImGui::Text("Medium Note Sound %s  ", mSoundFont.instruments[i].normalNoteSound.path);
                sprintf(tuningTag.get(), "##%p", &mSoundFont.instruments[i].normalNoteSound);
                sprintf(sampleButtonTag.get(), "%s##%p", ICON_FA_PENCIL, &mSoundFont.instruments[i].normalNoteSound);
                ImGui::SameLine();
                ImGui::Button(sampleButtonTag.get());
                ImGui::SameLine();
                ImGui::TextUnformatted("Tuning");
                ImGui::SameLine();
                ImGui::InputScalarN(tuningTag.get(), ImGuiDataType_Float, &mSoundFont.instruments[i].normalNoteSound.tuning, 1);
            } else {
                ImGui::TextUnformatted("Medium Note Sound Empty");
            }

            if (mSoundFont.instruments[i].highNoteSound.path != nullptr) {
                ImGui::Text("Hi Note Sound %s  ", mSoundFont.instruments[i].highNoteSound.path);
                sprintf(tuningTag.get(), "##%p", &mSoundFont.instruments[i].highNoteSound);
                ImGui::SameLine();
                ImGui::TextUnformatted("Tuning");
                ImGui::SameLine();
                ImGui::InputScalarN(tuningTag.get(), ImGuiDataType_Float, &mSoundFont.instruments[i].highNoteSound.tuning, 1);
            } else {
                ImGui::TextUnformatted("Hi Note Sound Empty");
            }
            sprintf(envTreeTag.get(), "Envelopes(%u)", mSoundFont.drums[i].numEnvelopes);
            if (ImGui::TreeNodeEx(mSoundFont.drums[i].envs, ImGuiTreeNodeFlags_None, envTreeTag.get())) {
                for (unsigned int j = 0; j < mSoundFont.drums[i].numEnvelopes; j++) {
                    sprintf(envTag.get(), "##%p", (&mSoundFont.drums[i].envs[j]));
                    ImGui::TextUnformatted("Delay ");
                    ImGui::SameLine();
                    ImGui::InputScalarN(envTag.get(), ImGuiDataType_S16, &mSoundFont.drums[i].envs[j].delay, 1);
                    ImGui::SameLine();
                    sprintf(envTag.get(), "##%p", (&mSoundFont.drums[i].envs[j]) + 1);
                    ImGui::TextUnformatted("Arg ");
                    ImGui::SameLine();
                    ImGui::InputScalarN(envTag.get(), ImGuiDataType_S16, &mSoundFont.drums[i].envs[j].arg, 1);
                }
                ImGui::TreePop();
            }
#endif
        }

        ImGui::TreePop();
    }
}

void CustomSoundFontWindow::DrawSfxTbl(bool locked) const {
    auto sfxTreeTag = std::make_unique<char[]>(sizeof("Sfx()") + 8);
    sprintf(sfxTreeTag.get(), "Sfx(%u)", mSoundFont.numSfx);
    if (ImGui::TreeNode(sfxTreeTag.get())) {
        auto tuningTag = std::make_unique<char[]>(TAG_SIZE("##"));

        for (unsigned int i = 0; i < mSoundFont.numSfx; i++) {
            DrawSample("Sfx", &mSoundFont.sfx[i].sample);
#if 0
            ImGui::Text("%u  ", i);
            ImGui::SameLine();
            if (mSoundFont.sfx[i].sample.path != nullptr) {
                ImGui::Text("Sample %s  ", mSoundFont.sfx[i].sample.path);
                ImGui::SameLine();
                ImGui::TextUnformatted("Tuning ");
                ImGui::SameLine();
                sprintf(tuningTag.get(), "##%p", &mSoundFont.sfx[i].sample.tuning);
                ImGui::InputScalarN(tuningTag.get(), ImGuiDataType_Float, &mSoundFont.sfx[i].sample.tuning, 1);
            } else {
                ImGui::TextUnformatted("Empty");
            }
#endif
        }
    }
}

static void DrawSample(const char* type, ZSample* sample) {
    auto sampleButtonTag = std::make_unique<char[]>(TAG_SIZE(ICON_FA_PENCIL"##"));
    sprintf(sampleButtonTag.get(), "%s##%p", ICON_FA_PENCIL, &sample->path);
    if (sample->path != nullptr) {
        auto tuningTag = std::make_unique<char[]>(TAG_SIZE("##"));

        //ImGui::Text("%s %s  ",type, sample->path);
        const char* sampleStart = strrchr(sample->path, PATH_SEPARATOR);
        const char* sampleEnd = strrchr(sample->path, '_');
        ImGui::Text("%s", type);
        ImGui::SameLine();
        ImGui::TextEx(sampleStart, sampleEnd);
        sprintf(tuningTag.get(), "##%p", &sample->path);

        ImGui::SameLine();
        if (ImGui::Button(sampleButtonTag.get())) {
            goto openNewSample;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Tuning");
        ImGui::SameLine();
        ImGui::InputScalarN(tuningTag.get(), ImGuiDataType_Float, &sample->tuning, 1);
    } else {
        ImGui::Text("%s Empty", type);
        if (ImGui::Button(sampleButtonTag.get())) {
            openNewSample:
            if (GetOpenFilePath(const_cast<char**>(&sample->path), FileBoxType::Max)) {
                sample->modified = true;
                // At this point `path` contains a path to a location on disk, it will be converted and added to the archive when saved.
            }
        }
    }
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

static void WriteInstrument(ZSample* zSample, tinyxml2::XMLElement* instrument, const char* name) {
    tinyxml2::XMLElement* inst = instrument->InsertNewChildElement(name);
    if (zSample->path != nullptr) {
        inst->SetAttribute("SampleRef", zSample->path);
        inst->SetAttribute("Tuning", zSample->tuning);
    }
    instrument->InsertEndChild(inst);
}

void CustomSoundFontWindow::Save() {
    char* outBuffer = nullptr;
    GetSaveFilePath(&outBuffer);
    tinyxml2::XMLDocument outDoc;
    tinyxml2::XMLElement* root = outDoc.NewElement("SoundFont");
    root->SetAttribute("Version", 0);
    root->SetAttribute("Num", mSoundFont.index);
    root->SetAttribute("Medium", mSoundFont.medium);
    root->SetAttribute("CachePolicy", mSoundFont.cachePolicy);
    root->SetAttribute("Data1", mSoundFont.data1);
    root->SetAttribute("Data2", mSoundFont.data2);
    root->SetAttribute("Data3", mSoundFont.data3);
    outDoc.InsertFirstChild(root);
    tinyxml2::XMLElement* drums = root->InsertNewChildElement("Drums");
    drums->SetAttribute("Count", mSoundFont.numDrums);
    for (unsigned int i = 0; i < mSoundFont.numDrums; i++) {
        tinyxml2::XMLElement* drum = drums->InsertNewChildElement("Drum");
        drum->SetAttribute("ReleaseRate", mSoundFont.drums[i].releaseRate);
        drum->SetAttribute("Pan", mSoundFont.drums[i].pan);
        drum->SetAttribute("Loaded", 0);
        drum->SetAttribute("SampleRef", mSoundFont.drums[i].sample.path);
        drum->SetAttribute("Tuning", mSoundFont.drums[i].sample.tuning);
        WriteEnvData(mSoundFont.drums[i].envs, mSoundFont.drums[i].numEnvelopes, drum);
        drums->InsertEndChild(drum);
    }
    root->InsertEndChild(drums);

    tinyxml2::XMLElement* instruments = root->InsertNewChildElement("Instruments");
    instruments->SetAttribute("Count", mSoundFont.numInstruments);
    for (unsigned int i = 0; i < mSoundFont.numInstruments; i++) {
        tinyxml2::XMLElement* instrument = instruments->InsertNewChildElement("Instrument");

        instrument->SetAttribute("IsValid", mSoundFont.instruments[i].isValid);
        instrument->SetAttribute("Loaded", 0);
        instrument->SetAttribute("NormalRangeLo", mSoundFont.instruments[i].normalRangeLo);
        instrument->SetAttribute("NormalRangeHi", mSoundFont.instruments[i].normalRangeHi);
        instrument->SetAttribute("ReleaseRate", mSoundFont.instruments[i].releaseRate);
        WriteEnvData(mSoundFont.instruments[i].envs, mSoundFont.instruments[i].numEnvelopes, instrument);
        WriteInstrument(&mSoundFont.instruments[i].lowNoteSound, instrument, "LowNotesSound");
        WriteInstrument(&mSoundFont.instruments[i].normalNoteSound, instrument, "NormalNotesSound");
        WriteInstrument(&mSoundFont.instruments[i].highNoteSound, instrument, "HighNotesSound");
        instruments->InsertEndChild(instrument);
    }
    root->InsertEndChild(instruments);

    tinyxml2::XMLElement* sfxTbl = root->InsertNewChildElement("SfxTable");
    sfxTbl->SetAttribute("Count", mSoundFont.numSfx);
    for (unsigned int i = 0; i < mSoundFont.numSfx; i++) {
        WriteInstrument(&mSoundFont.sfx[i].sample, sfxTbl, "Sfx");
    }
    root->InsertEndChild(sfxTbl);
    outDoc.InsertEndChild(root);
    FILE* outFile = fopen(outBuffer, "w");
    tinyxml2::XMLPrinter printer;
    outDoc.Accept(&printer);
    fwrite(printer.CStr(), printer.CStrSize() - 1, 1, outFile);
    fclose(outFile);
}