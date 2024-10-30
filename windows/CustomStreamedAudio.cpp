#include "CustomStreamedAudio.h"
#include "archive.h"
#include "zip_archive.h"
#include "mpq_archive.h"

#include "xml_embed.h"

#include "imgui.h"
#include "imgui_toggle.h"
#include "WindowMgr.h"
#include "filebox.h"
#include "CRC64.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

#include <ogg/ogg.h>
#include <vorbis/vorbisfile.h>
#include <vorbis/vorbisenc.h>

#include <tinyxml2.h>
#include <array>
#include <atomic>
#include <cmath>

#include "mio.hpp"

CustomStreamedAudioWindow::CustomStreamedAudioWindow() {

}

CustomStreamedAudioWindow::~CustomStreamedAudioWindow() {
    for (size_t i = 0; i < mFileQueue.size(); i++) {
        delete[] mFileQueue[i];
    }
    mFileQueue.clear();
    ClearPathBuff();
    ClearSaveBuff();
}

void CustomStreamedAudioWindow::ClearPathBuff()
{
    if (mPathBuff != nullptr) {
        delete[] mPathBuff;
        mPathBuff = nullptr;
    }
}

void CustomStreamedAudioWindow::ClearSaveBuff()
{
    if (mSavePath != nullptr) {
        delete[] mSavePath;
        mSavePath = nullptr;
    }
}

enum AudioType {
    mp3,
    wav,
    ogg,
    flac,
};

constexpr static const char fontXmlBase[] = "custom/fonts/";
constexpr static const char sampleNameBase[] = "custom/samples/";
constexpr static const char sampleDataBase[] = "custom/sampleData/";
constexpr static const char sampleXmlBase[] = "custom/samples/";
constexpr static const char seqXmlBase[] = "custom/music/";

// 'ID3' as a string
#define MP3_ID3_CHECK(d) ((d[0] == 'I') && (d[1] == 'D') && (d[2] == '3'))
// FF FB, FF F2, FF F3
#define MP3_NON_ID3_CHECK(d) ((d[0] == 0xFF) && ((d[1] == 0xFB) || (d[1] == 0xF2) || (d[1] == 0xF3)))
#define MP3_CHECK(d) (MP3_ID3_CHECK(d) || MP3_NON_ID3_CHECK(d))

#define WAV_CHECK(d) ((d[0] == 'R') && (d[1] == 'I') && (d[2] == 'F') && (d[3] == 'F'))

#define FLAC_CHECK(d) ((d[0] == 'f') && (d[1] == 'L') && (d[2] == 'a') && (d[3] == 'C'))

#define OGG_CHECK(d) ((d[0] == 'O') && (d[1] == 'g') && (d[2] == 'g') && (d[3] == 'S'))

constexpr static std::array<const char*, 4> audioTypeToStr = {
    "mp3",
    "wav",
    "ogg",
    "flac",
};

static char* GetFileNameFromPath(char* input) {
    size_t len = strlen(input);
    for (size_t i = len; i > 0; i--) {
        if (input[i] == '/' || input[i] == '\\') {
            return &input[i + 1];
        }
    }
    return nullptr;
}

// Write `data` to either the archive, or if the archive is null, a file on disk
static void WriteFileData(char* path, void* data, size_t size, Archive* a) {
    if (a == nullptr) {
        FILE* file = fopen(path, "wb+");
        fwrite(data, size, 1, file);
        fclose(file);
    }
    else {
        const ArchiveDataInfo info = {
            .data = data, .size = size, .mode = DataCopy 
        };
        a->WriteFile(path, &info);
    }
}

// If the data comes from memory, `input` is used as the source buffer.
static void CopySampleData(char* input, char* fileName, bool fromDisk, size_t size, Archive* a) {
    size_t sampleDataPathLen = sizeof(sampleDataBase) + strlen(fileName) + 1;
    auto sampleDataPath = std::make_unique<char[]>(sampleDataPathLen);
    snprintf(sampleDataPath.get(), sampleDataPathLen, "%s%s", sampleDataBase, fileName);
    if (fromDisk) {
        mio::mmap_source seqFile(input);
        seqFile.release();

        const ArchiveDataInfo info = {
            .data = (void*)seqFile.data(), .size = seqFile.size(), .mode = MMappedFile
        };
        a->WriteFile(sampleDataPath.get(), &info);
    }
    else {
        const ArchiveDataInfo info = {
            .data = (void*)input, .size = size, .mode = DataCopy
        };
        a->WriteFile(sampleDataPath.get(), &info);
    }
}

static void CreateSampleXml(char* fileName, const char* audioType, uint64_t numFrames, uint64_t numChannels, Archive* a) {
    tinyxml2::XMLDocument sampleBaseRoot;
    tinyxml2::XMLError e = sampleBaseRoot.LoadFile("assets/sample-base.xml");
    if (e != 0) {
        printf("Failed to open sample-base.xml. Falling back to the embeded version...\n");
        e = sampleBaseRoot.Parse(gSampleBaseXml, SAMPLE_BASE_XML_SIZE);
        if (e != 0) {
            printf("Failed to parse embeded XML. Exiting...\n");
            exit(1);
        }
    }
    
    size_t sampleDataSize = sizeof(sampleDataBase) + strlen(fileName) + 1;
    auto sampleDataPath = std::make_unique<char[]>(sampleDataSize);
    snprintf(sampleDataPath.get(), sampleDataSize, "%s%s", sampleDataBase, fileName);

    // Fill in sample XML
    tinyxml2::XMLPrinter p;
    tinyxml2::XMLElement* root = sampleBaseRoot.RootElement();
    root->SetAttribute("LoopEnd", numFrames * numChannels);
    root->SetAttribute("CustomFormat", audioType);
    root->SetAttribute("SampleSize", numFrames * numChannels * 2);
    root->SetAttribute("SamplePath", sampleDataPath.get());
    size_t samplePathLen = sizeof(sampleXmlBase) + strlen(fileName) + sizeof("_SAMPLE.xml") + 1;
    std::unique_ptr<char[]> sampleXmlPath = std::make_unique<char[]>(samplePathLen);
    snprintf(sampleXmlPath.get(), samplePathLen, "%s%s_SAMPLE.xml", sampleXmlBase, fileName);
    root->Accept(&p);
    
    WriteFileData(sampleXmlPath.get(), (void*)p.CStr(), p.CStrSize() - 1, a);

    p.ClearBuffer();
}

static void CreateSequenceXml(char* fileName, char* fontPath, unsigned int length, bool isFanfare, bool stereo, Archive* a) {
    tinyxml2::XMLDocument seqBaseRoot;
    tinyxml2::XMLError e = seqBaseRoot.LoadFile("assets/seq-base.xml");
    if (e != 0) {
        printf("Failed to open seq-base.xml. Falling back to the embeded version...\n");
        e = seqBaseRoot.Parse(gSequenceBaseXml, SEQ_BASE_XML_SIZE);
        if (e != 0) {
            printf("Failed to parse embeded XML. Exiting...\n");
            exit(1);
        }
    }

    tinyxml2::XMLPrinter p;
    tinyxml2::XMLElement* root = seqBaseRoot.FirstChildElement();
    tinyxml2::XMLElement* fontIndiciesElement = root->FirstChildElement("FontIndicies");
    
    uint64_t crc = CRC64(fontPath);
    for (size_t i = 0; i < 8; i++) {
        tinyxml2::XMLElement* fontIdxElement = fontIndiciesElement->InsertNewChildElement("FontIndex");
        fontIdxElement->SetAttribute("FontIdx", ((uint8_t*)&crc)[i]);
        fontIndiciesElement->InsertEndChild(fontIdxElement);
    }
    root->SetAttribute("Length", length);
    std::unique_ptr<char[]> seqXmlPath;
    size_t seqPathLen = sizeof(seqXmlBase) + strlen(fileName) + sizeof("_SEQ.xml");
    if (!isFanfare) {
        root->SetAttribute("Looped", true);
        seqXmlPath = std::make_unique<char[]>(seqPathLen);
        snprintf(seqXmlPath.get(), seqPathLen, "%s%s_SEQ.xml", seqXmlBase, fileName);
    } else {
        // By default the game will loop streamed sequences. Tell the game to NOT loop songs used as fanfares.
        root->SetAttribute("Looped", false);
        seqPathLen += sizeof("_fanfare");
        seqXmlPath = std::make_unique<char[]>(seqPathLen);
        snprintf(seqXmlPath.get(), seqPathLen, "%s%s_SEQ.xml_fanfare", seqXmlBase, fileName);
    }
    root->SetAttribute("Stereo", stereo);
    seqBaseRoot.InsertEndChild(root);
    root->Accept(&p);

    WriteFileData(seqXmlPath.get(), (void*)p.CStr(), p.CStrSize() - 1, a);
    
    p.ClearBuffer();
}

static std::unique_ptr<char[]> CreateFontXml(char* fileName, uint64_t sampleRate, uint64_t channels, Archive* a) {
    tinyxml2::XMLDocument fontBaseRoot;
    tinyxml2::XMLError e = fontBaseRoot.LoadFile("assets/font-base.xml");
    if (e != 0) {
        printf("Failed to open font-base.xml. Falling back to the embeded version...\n");
        e = fontBaseRoot.Parse(gFontBaseXml, FONT_BASE_XML_SIZE);
        if (e != 0) {
            printf("Failed to parse embeded XML. Exiting...\n");
            exit(1);
        }
    }

    tinyxml2::XMLPrinter p;
    tinyxml2::XMLElement* root = fontBaseRoot.FirstChildElement();
    tinyxml2::XMLElement* instrumentsElement = root->FirstChildElement("Instruments");
    tinyxml2::XMLElement* instrumentElement = instrumentsElement->FirstChildElement("Instrument");
    tinyxml2::XMLElement* normalNoteElement = instrumentElement->FirstChildElement("NormalNotesSound");
    
    size_t sampleRefPathSize = sizeof(sampleNameBase) + strlen(fileName) + sizeof("_SAMPLE.xml") + 1;
    auto sampleRefPathXml = std::make_unique<char[]>(sampleRefPathSize);
    snprintf(sampleRefPathXml.get(), sampleRefPathSize, "%s%s_SAMPLE.xml", sampleNameBase, fileName);

    normalNoteElement->SetAttribute("SampleRef", sampleRefPathXml.get());
    normalNoteElement->SetAttribute("Tuning", ((float)sampleRate / 32000.0f) * channels);

    size_t fontPathLen = sizeof(fontXmlBase) + strlen(fileName) + sizeof("_FONT.xml");
    auto fontXmlPath = std::make_unique<char[]>(fontPathLen);

    snprintf(fontXmlPath.get(), fontPathLen, "%s%s_FONT.xml", fontXmlBase, fileName);
    root->Accept(&p);

    WriteFileData(fontXmlPath.get(), (void*)p.CStr(), p.CStrSize() - 1, a);

    p.ClearBuffer();

    return fontXmlPath;
}

static std::unique_ptr<char[]> CreateFontMultiXml(std::unique_ptr<char[]> fileNames[2], char* baseFileName, uint64_t sampleRate, Archive* a) {
    tinyxml2::XMLDocument fontBaseRoot;
    tinyxml2::XMLError e = fontBaseRoot.LoadFile("assets/font-base-multi.xml");
    if (e != 0) {
        printf("Failed to open font-base-multi.xml. Falling back to the embeded version...\n");
        e = fontBaseRoot.Parse(gFontBaseMultiXml, FONT_BASE_MULTI_XML_SIZE);
        if (e != 0) {
            printf("Failed to parse embeded XML. Exiting...\n");
            exit(1);
        }
    }

    tinyxml2::XMLPrinter p;
    tinyxml2::XMLElement* root = fontBaseRoot.FirstChildElement();
    tinyxml2::XMLElement* instrumentsElement = root->FirstChildElement("Instruments");
    tinyxml2::XMLElement* instrumentElement = instrumentsElement->FirstChildElement("Instrument");
    tinyxml2::XMLElement* normalNoteElement = instrumentElement->FirstChildElement("NormalNotesSound");

    size_t sampleRefPathSize = sizeof(sampleNameBase) + strlen(fileNames[0].get()) + sizeof("_SAMPLE.xml") + 1;
    auto sampleRefPathXml = std::make_unique<char[]>(sampleRefPathSize);
    snprintf(sampleRefPathXml.get(), sampleRefPathSize, "%s%s_SAMPLE.xml", sampleNameBase, fileNames[0].get());

    normalNoteElement->SetAttribute("SampleRef", sampleRefPathXml.get());
    normalNoteElement->SetAttribute("Tuning", ((float)sampleRate / 32000.0f));

    instrumentElement = instrumentElement->NextSiblingElement("Instrument");
    normalNoteElement = instrumentElement->FirstChildElement("NormalNotesSound");

    sampleRefPathSize = sizeof(sampleNameBase) + strlen(fileNames[1].get()) + sizeof("_SAMPLE.xml") + 1;
    sampleRefPathXml = std::make_unique<char[]>(sampleRefPathSize);
    snprintf(sampleRefPathXml.get(), sampleRefPathSize, "%s%s_SAMPLE.xml", sampleNameBase, fileNames[1].get());

    normalNoteElement->SetAttribute("SampleRef", sampleRefPathXml.get());
    normalNoteElement->SetAttribute("Tuning", ((float)sampleRate / 32000.0f));
    instrumentsElement->InsertEndChild(instrumentElement);

    root->InsertEndChild(instrumentsElement);


    size_t fontPathLen = sizeof(fontXmlBase) + strlen(baseFileName) + sizeof("_FONT.xml");
    auto fontXmlPath = std::make_unique<char[]>(fontPathLen);

    snprintf(fontXmlPath.get(), fontPathLen, "%s%s_FONT.xml", fontXmlBase, baseFileName);
    root->Accept(&p);

    WriteFileData(fontXmlPath.get(), (void*)p.CStr(), p.CStrSize() - 1, a);

    p.ClearBuffer();

    return fontXmlPath;
}

struct OggFileData {
    void* data;
    size_t pos;
    size_t size;
};

static size_t VorbisReadCallback(void* out, size_t size, size_t elems, void* src) {
    OggFileData* data = static_cast<OggFileData*>(src);
    size_t toRead = size * elems;

    if (toRead > data->size - data->pos) {
        toRead = data->size - data->pos;
    }

    memcpy(out, static_cast<uint8_t*>(data->data) + data->pos, toRead);
    data->pos += toRead;
    
    return toRead / size;
}

static int VorbisSeekCallback(void* src, ogg_int64_t pos, int whence) {
    OggFileData* data = static_cast<OggFileData*>(src);
    size_t newPos;
    
    switch(whence) {
        case SEEK_SET:
            newPos = pos;
            break;
        case SEEK_CUR:
            newPos = data->pos + pos;
            break;
        case SEEK_END:
            newPos = data->size + pos;
            break;
        default:
            return -1;
    }
    if (newPos > data->size) {
        return -1;
    }
    data->pos = newPos;
    return 0;
}

static int VorbisCloseCallback([[maybe_unused]] void* src) {
    return 0;
}

static long VorbisTellCallback(void* src) {
    OggFileData* data = static_cast<OggFileData*>(src);
    return data->pos;
}

static const ov_callbacks cbs = {
    VorbisReadCallback,
    VorbisSeekCallback,
    VorbisCloseCallback,
    VorbisTellCallback,
};

// We don't want this to show less files than exist in the folder to pack when the operation is finished.
// `atomic` variables will ensure there isn't any inconsistency due to multi-threading.
static std::atomic<unsigned int> filesProcessed = 0;

typedef struct {
    unsigned char* data;
    size_t size;
    size_t capacity;
} MemoryBuffer;

void init_memory_buffer(MemoryBuffer* buffer, size_t initial_size) {
    buffer->data = (unsigned char*)malloc(initial_size);
    buffer->capacity = initial_size;
    buffer->size = 0;
}

void expand_memory_buffer(MemoryBuffer* buffer, size_t additional_size) {
    buffer->capacity += additional_size;
    buffer->data = (unsigned char*)realloc(buffer->data, buffer->capacity);
}

void write_to_buffer(MemoryBuffer* buffer, unsigned char* data, size_t data_size) {
    while (buffer->size + data_size > buffer->capacity) {
        expand_memory_buffer(buffer, data_size);
    }
    memcpy(buffer->data + buffer->size, data, data_size);
    buffer->size += data_size;
}

typedef struct ChannelInfo {
    void* channelData[2];
    size_t channelSizes[2];
} ChannelInfo;

static void SplitOgg(OggVorbis_File* vf, ChannelInfo* info, uint64_t sampleRate, uint64_t numFrames) {
    MemoryBuffer outBuffer[2];
    init_memory_buffer(&outBuffer[0], (1024 * 1024) * 5);
    init_memory_buffer(&outBuffer[1], (1024 * 1024) * 5);
    ogg_stream_state os[2];
    ogg_page og[2];
    ogg_packet op[2];
    vorbis_info outInfo[2];
    vorbis_dsp_state vd[2];
    vorbis_block vb[2];
    vorbis_comment vc[2];
    ogg_packet header[2];
    ogg_packet header_comm[2];
    ogg_packet header_code[2];
    for (size_t i = 0; i < 2; i++) {
        int eos = 0;

        vorbis_info_init(&outInfo[i]);
        vorbis_encode_init_vbr(&outInfo[i], 1, sampleRate, 0.6f);
        vorbis_analysis_init(&vd[i], &outInfo[i]);
        vorbis_block_init(&vd[i], &vb[i]);
        vorbis_comment_init(&vc[i]);
        ogg_stream_init(&os[i], 0);

        vorbis_analysis_headerout(&vd[i], &vc[i], &header[i], &header_comm[i], &header_code[i]);
        ogg_stream_packetin(&os[i], &header[i]); /* automatically placed in its own
                                     page */
        ogg_stream_packetin(&os[i], &header_comm[i]);
        ogg_stream_packetin(&os[i], &header_code[i]);

        while (!eos) {
            int result = ogg_stream_flush(&os[i], &og[i]);
            if (result == 0)break;
            write_to_buffer(&outBuffer[i], og[i].header, og[i].header_len);
            write_to_buffer(&outBuffer[i], og[i].body, og[i].body_len);
        }
    }


    float** pcm;
    int bitStream;
    size_t pos = 0;
    long read = 0;
    
    do {
        float** bufferL = vorbis_analysis_buffer(&vd[0], 4096);
        float** bufferR = vorbis_analysis_buffer(&vd[1], 4096);
        read = ov_read_float(vf, &pcm, 4096, &bitStream);

        if (read > 0) {
            // Avoid memcpy if pcm data can be used directly
            memcpy(bufferL[0], pcm[0], read * 4);
            memcpy(bufferR[0], pcm[1], read * 4);
            
            vorbis_analysis_wrote(&vd[0], read);
            vorbis_analysis_wrote(&vd[1], read);
            pos += read;
        }
        else {
            vorbis_analysis_wrote(&vd[0], 0); // Signal end of input
            vorbis_analysis_wrote(&vd[1], 0); // Signal end of input
        }

        // Analysis and packet output
        for (size_t i = 0; i < 2; i++) {
            int eos = 0;
            while (vorbis_analysis_blockout(&vd[i], &vb[i]) == 1) {
                vorbis_analysis(&vb[i], NULL);
                vorbis_bitrate_addblock(&vb[i]);

                while (vorbis_bitrate_flushpacket(&vd[i], &op[i])) {
                    ogg_stream_packetin(&os[i], &op[i]);
                    while (!eos) {
                        if (!ogg_stream_pageout(&os[i], &og[i])) break;
                        write_to_buffer(&outBuffer[i], og[i].header, og[i].header_len);
                        write_to_buffer(&outBuffer[i], og[i].body, og[i].body_len);
                        if (ogg_page_eos(&og[i])) eos = 1;
                    }
                }
            }
        }
    } while (read > 0);


    // Clean up
    for (size_t i = 0; i < 2; i++) {
        ogg_stream_clear(&os[i]);
        vorbis_block_clear(&vb[i]);
        vorbis_dsp_clear(&vd[i]);
        vorbis_comment_clear(&vc[i]);
        vorbis_info_clear(&outInfo[i]);
    }

    info->channelData[0] = outBuffer[0].data;
    info->channelData[1] = outBuffer[1].data;
    info->channelSizes[0] = outBuffer[0].size;
    info->channelSizes[1] = outBuffer[1].size;
}



static void WriteWavData(int16_t* l, int16_t* r, ChannelInfo* info, uint64_t numFrames, uint64_t sampleRate) {
    drwav outWav;
    size_t outDataSize = 0;

    const drwav_data_format format = {
        .container = drwav_container_riff,
        .format = DR_WAVE_FORMAT_PCM,
        .channels = 1,
        .sampleRate = (drwav_uint32)sampleRate,
        .bitsPerSample = 16,
    };

    drwav_init_memory_write(&outWav, &info->channelData[0], &info->channelSizes[0], &format, nullptr);
    drwav_write_pcm_frames(&outWav, numFrames, l);
    // drwav_uninit only frees data related to the writer, not the wav file data.
    drwav_uninit(&outWav);

    drwav_init_memory_write(&outWav, &info->channelData[1], &info->channelSizes[1], &format, nullptr);
    drwav_write_pcm_frames(&outWav, numFrames, r);
    drwav_uninit(&outWav);
}

static void ProcessAudioFile(SafeQueue<char*>* fileQueue, std::unordered_map<char*, bool>* fanfareMap, Archive* a) {
    while (!fileQueue->empty()) {
    filesProcessed++;
    char* input = fileQueue->pop();
    char* fileName = strrchr(input, PATH_SEPARATOR);
    fileName++;
    size_t fileNameLen = strlen(fileName);

    const size_t outFileLen = fileNameLen + sizeof("_L") + 1;
    
    std::unique_ptr<char[]> fileNames[2];

    fileNames[0] = std::make_unique<char[]>(outFileLen);
    snprintf(fileNames[0].get(), outFileLen, "%s_L", fileName);

    fileNames[1] = std::make_unique<char[]>(outFileLen);
    snprintf(fileNames[1].get(), outFileLen, "%s_R", fileName);
    
    uint8_t* dataU8;
    uint64_t numFrames;
    uint64_t numChannels;
    uint64_t sampleRate;
    int audioType;
    // Only used for multi-channel files
    ChannelInfo infos{};


    mio::mmap_source file(input);

    void* data = (void*)file.data();
    size_t fileSize = file.size();

    dataU8 = (uint8_t*)data;

    // Read file header to determine which library needs to process it
    if (MP3_CHECK(dataU8)) {
        drmp3 mp3;
        audioType = AudioType::mp3;
        drmp3_init_memory(&mp3, data, fileSize, nullptr);
        numChannels = mp3.channels;
        sampleRate = mp3.sampleRate;
        numFrames = drmp3_get_pcm_frame_count(&mp3);
    } else if (WAV_CHECK(dataU8)) {
        drwav wav;
        audioType = AudioType::wav;
        drwav_init_memory(&wav, data, fileSize, nullptr);
        numChannels = wav.channels;
        sampleRate = wav.sampleRate;
        numFrames = wav.totalPCMFrameCount;

        if (numChannels == 2) {
            // Split the two channels

            auto sampleData = std::make_unique<int16_t[]>(numFrames * numChannels);
            drwav_read_pcm_frames_s16(&wav, numFrames, sampleData.get());
            auto sampleDataL = std::make_unique<int16_t[]>(numFrames);
            auto sampleDataR = std::make_unique<int16_t[]>(numFrames);
            size_t pos = 0;
            for (size_t i = 0; i < numFrames * numChannels - 1; i += 2, pos++) {
                sampleDataL.get()[pos] = sampleData.get()[i];
                sampleDataR.get()[pos] = sampleData.get()[i + 1];
            }
            WriteWavData(sampleDataL.get(), sampleDataR.get(), &infos, numFrames, sampleRate);
        }
    } else if (FLAC_CHECK(dataU8)) {
        audioType = AudioType::flac;
        drflac* flac = drflac_open_memory(data, fileSize, nullptr);
        numChannels = flac->channels;
        sampleRate = flac->sampleRate;
        numFrames = flac->totalPCMFrameCount;
        if (numChannels == 2) {
            size_t pos = 0;
            auto sampleData = std::make_unique<int16_t[]>(numFrames * numChannels);
            auto sampleDataL = std::make_unique<int16_t[]>(numFrames);
            auto sampleDataR = std::make_unique<int16_t[]>(numFrames);

            drflac_read_pcm_frames_s16(flac, numFrames, sampleData.get());
            for (size_t i = 0; i < numFrames * numChannels - 1; i += 2, pos++) {
                sampleDataL.get()[pos] = sampleData.get()[i];
                sampleDataR.get()[pos] = sampleData.get()[i + 1];
            }
            WriteWavData(sampleDataL.get(), sampleDataR.get(), &infos, numFrames, sampleRate);
            audioType = AudioType::wav;
        }
        drflac_close(flac);
    }
    else if (OGG_CHECK(dataU8)) {
        char dataBuff[4096];
        long read = 0;
        size_t pos = 0;

        audioType = AudioType::ogg;
        OggVorbis_File vf;
        OggFileData fileData = {
            .data = data,
            .pos = 0,
            .size = (size_t)fileSize,
        };
        int ret = ov_open_callbacks(&fileData, &vf, nullptr, 0, cbs);

        vorbis_info* vi = ov_info(&vf, -1);

        numFrames = ov_pcm_total(&vf, -1);
        sampleRate = vi->rate;
        numChannels = vi->channels;

        if (numChannels == 2) {
            SplitOgg(&vf, &infos, sampleRate, numFrames);
        }
    }
    else {
        return;
    }

    std::unique_ptr<char[]> fontXmlPath;
    if (numChannels == 2) {
        CreateSampleXml(fileNames[0].get(), audioTypeToStr[audioType], numFrames, 1, a);
        CopySampleData(reinterpret_cast<char*>(infos.channelData[0]), fileNames[0].get(), false, infos.channelSizes[0], a);

        CreateSampleXml(fileNames[1].get(), audioTypeToStr[audioType], numFrames, 1, a);
        CopySampleData(reinterpret_cast<char*>(infos.channelData[1]), fileNames[1].get(), false, infos.channelSizes[1], a);
        fontXmlPath = CreateFontMultiXml(fileNames, fileName, sampleRate, a);
        free(infos.channelData[0]);
        free(infos.channelData[1]);
    } else {
        CreateSampleXml(fileName, audioTypeToStr[audioType], numFrames, numChannels, a);
        CopySampleData(input, fileName, true, fileSize, a);
        fontXmlPath = CreateFontXml(fileName, sampleRate, numChannels, a);
    }
    // There is no good way to determine the length of the song when we go to load it so we need to store the length in seconds.
    float lengthF = (float)numFrames / (float)sampleRate;
    lengthF = ceilf(lengthF);
    unsigned int length = static_cast<unsigned int>(lengthF);
    CreateSequenceXml(fileName, fontXmlPath.get(), length, (*fanfareMap).at(fileName), numChannels == 2, a);
    // the file name is allocated on the heap. We must free it here.
    delete[] input;
    }
}

static void PackFilesMgrWorker(SafeQueue<char*>* fileQueue, std::unordered_map<char*, bool>* fanfareMap, bool* threadStarted, bool* threadDone, CustomStreamedAudioWindow* thisx) {
    Archive* a = nullptr;
    switch (thisx->GetRadioState()) {
        case 1: {
            a = new MpqArchive(thisx->GetSavePath());
            break;
        }
        case 2: {
            a = new ZipArchive(thisx->GetSavePath());
            break;
        }
    }

    const unsigned int numThreads = std::thread::hardware_concurrency();
    auto packFileThreads = std::make_unique<std::thread[]>(numThreads);
    for (unsigned int i = 0; i < numThreads; i++) {
        packFileThreads[i] = std::thread(ProcessAudioFile, fileQueue, fanfareMap, a);
    }

    for (unsigned int i = 0; i < numThreads; i++) {
        packFileThreads[i].join();
    }

    a->CloseArchive();
    delete a;

    *threadStarted = false;
    *threadDone = true;
}

static std::array<const char*, 2> sToggleLabels = {
    "Create Folder",
    "Create Archive",
};

int CustomStreamedAudioWindow::GetRadioState() {
    return mRadioState;
}

char* CustomStreamedAudioWindow::GetSavePath() {
    return mSavePath;
}

static bool FillFileCallback(char* path) {
    char* ext = strrchr(path, '.');
    if (ext != nullptr) {
        if (ext != NULL && (strcmp(ext, ".wav") == 0 || strcmp(ext, ".ogg") == 0 || strcmp(ext, ".mp3") == 0) || strcmp(ext, ".flac") == 0) {
            return true;
        }
    }
    return false;
}

void CustomStreamedAudioWindow::DrawWindow() {
    ImGui::Begin("Create Custom Streamed Audio", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    
    ImGui::BeginDisabled(mThreadStarted);
    
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Create);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Create Archive With Streamed Songs");
    ImGui::TextUnformatted("Open a directory and create an archive with the files needed for streamed audio");

    if (ImGui::Button("Select Directory")) {
        ClearPathBuff();
        ClearSaveBuff();
        GetOpenDirPath(&mPathBuff);
        FillFileQueue(mFileQueue, mPathBuff, FillFileCallback);
        FillFanfareMap();
        fileCount = mFileQueue.size();
        mThreadIsDone = false;
    }

    if (mPathBuff != nullptr) {
        if (mThreadIsDone) {
            ImGui::Text("Packing complete. Files saved in the \"custom\" folder in the program's directory");
        }
        else {
            ImGui::SameLine();
            ImGui::Text("Path of files to Pack: %s", mPathBuff);
        }
    }

    ImGui::SameLine();
    ImGui::RadioButton("OTR", &mRadioState, 1);
    ImGui::SameLine();
    ImGui::RadioButton("O2R", &mRadioState, 2);

    if (ImGui::Button("Set Save Path")) {
        GetSaveFilePath(&mSavePath);
    }

    if (mThreadStarted && !mThreadIsDone) {
        ImGui::TextUnformatted("Packing files...");
        ImGui::Text("Files processed %d\\%d", filesProcessed.load(), fileCount);
    }

    if (mSavePath != nullptr) {
        ImGui::SameLine();
        ImGui::Text("Archive save path: %s", mSavePath);
    }

    if (!mFileQueue.empty() && mPathBuff != nullptr) {
        if (mSavePath != nullptr) {
            if (ImGui::Button("Pack Archive")) {
                mThreadStarted = true;
                mThreadIsDone = false;
                filesProcessed = 0;
                std::thread packFilesMgrThread(PackFilesMgrWorker, &mFileQueue, &mFanfareMap, &mThreadStarted, &mThreadIsDone, this);
                packFilesMgrThread.detach();
            }
        }
    }

    ImGui::EndDisabled();

    DrawPendingFilesList();

    ImGui::End();
}

void CustomStreamedAudioWindow::FillFanfareMap() {
    mFanfareMap.clear();
    for (size_t i = 0; i < mFileQueue.size(); i++) {
        char* fileName = strrchr(mFileQueue[i], PATH_SEPARATOR);
        fileName++;
        mFanfareMap[fileName] = false;
    }
}

void CustomStreamedAudioWindow::DrawPendingFilesList() {
    if (mFileQueue.empty()) {
        return;
    }

    const ImVec2 cursorPos = ImGui::GetCursorPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImVec2 childWindowSize = { windowSize.x - cursorPos.x, windowSize.y - cursorPos.y };

    ImGui::BeginChild("File List", childWindowSize, 0, 0);
    ImGui::TextUnformatted("Files to Pack:");
    const float windowHeight = ImGui::GetWindowHeight();

    for (size_t i = 0; i < mFileQueue.size(); i++) {
        const float scroll = ImGui::GetScrollY();
        const float cursorPosY = ImGui::GetCursorPosY();

        // Only draw elements that are on screen. This gives around 20x performance improvement.
        if ((cursorPosY < scroll) || (cursorPosY > (scroll + windowHeight))) {
            ImGui::NewLine();
            continue;
        }
        char* fileName = strrchr(mFileQueue[i], PATH_SEPARATOR);
        fileName++;
        size_t len = strlen(fileName);
        auto checkboxTag = std::make_unique<char[]>(len + sizeof("Fanfare##") + 1);
        sprintf(checkboxTag.get(), "Fanfare##%s", fileName);
        ImGui::Text("%s", fileName);
        ImGui::SameLine();
        ImGui::Checkbox(checkboxTag.get(), &mFanfareMap.at(fileName));
        
    }
    ImGui::EndChild();
}
