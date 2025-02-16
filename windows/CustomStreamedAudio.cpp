#include "CustomStreamedAudio.h"
#include "archive.h"
#include "zip_archive.h"
#include "mpq_archive.h"

#include "xml_embed.h"

#include "imgui.h"
#include "imgui_internal.h"
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
#include <thread>
#include <filesystem>
#include <opus/opus.h>
#include <opus/opusenc.h>

#include "mio.hpp"
#undef max

// Not part of the class because it needs to be accessed from a thread.
static void ClearFileQueue(std::vector<char*>* fileQueue) {
    for (auto f : *fileQueue) {
        // The lowest bit of the path is set to 1 if the file has been processed. We need to undo that to delete it.
        uintptr_t origPtr = (uintptr_t)f & (UINTPTR_MAX & (~(uintptr_t)1));
        operator delete[]((void*)origPtr, std::align_val_t(2));
    }
    fileQueue->clear();
}

CustomStreamedAudioWindow::~CustomStreamedAudioWindow() {
    ClearFileQueue(&mFileQueue);
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
// TODO simd
static void PcmS16ToF(float* dst, const int16_t* src, const uint64_t numFrames) {
    for (size_t i = 0; i < numFrames; i++) {
        dst[i] = static_cast<float>(src[i]) / 32768.0f;
    }
}

// Write `data` to either the archive, or if the archive is null, a file on disk
void WriteFileData(char* path, void* data, size_t size, Archive* a) {
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
std::unique_ptr<char[]> CopySampleData(char* input, char* fileName, bool fromDisk, size_t size, Archive* a) {
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
    return sampleDataPath;
}

std::unique_ptr<char[]> CreateSampleXml(char* fileName, const char* audioType, uint64_t numFrames, uint64_t numChannels, SeqMetaInfo* info, uint64_t sampleRate, bool loopTimeInSamples, Archive* a) {
    tinyxml2::XMLDocument sampleBaseRoot;
    tinyxml2::XMLError e = sampleBaseRoot.LoadFile("assets/sample-base.xml");
    if (e != 0) {
        printf("Failed to open sample-base.xml. Falling back to the embedded version...\n");
        e = sampleBaseRoot.Parse(gSampleBaseXml, SAMPLE_BASE_XML_SIZE);
        if (e != 0) {
            printf("Failed to parse embedded XML. Exiting...\n");
            exit(1);
        }
    }

    size_t sampleDataSize = sizeof(sampleDataBase) + strlen(fileName) + 1;
    auto sampleDataPath = std::make_unique<char[]>(sampleDataSize);
    snprintf(sampleDataPath.get(), sampleDataSize, "%s%s", sampleDataBase, fileName);

    // Fill in sample XML
    tinyxml2::XMLPrinter p;
    tinyxml2::XMLElement* root = sampleBaseRoot.RootElement();
    tinyxml2::XMLElement* loopElement = root->FirstChildElement("ADPCMLoop");
    if (!info->fanfare) {
        loopElement->SetAttribute("Count", "-1");
    }
    if (info->loopStart.i != 0) {
        uint64_t loopStartSample;
        if (loopTimeInSamples) {
            loopStartSample = info->loopStart.i;
            if (info->loopStart.i > numFrames * numChannels) {
                loopStartSample = 0;
            }
        } else {
            loopStartSample = info->loopStart.f * sampleRate * numChannels;
            if (loopStartSample > numFrames * numChannels) {
                loopStartSample = 0;
            }
        }
        loopElement->SetAttribute("Start", loopStartSample);
    }
    if (info->loopEnd.i != 0) {
        uint64_t loopEndSample;
        if (loopTimeInSamples) {
            if (info->loopEnd.i > numFrames * numChannels) {
                loopEndSample = numFrames * numChannels;
            }
            else {
                loopEndSample = info->loopEnd.i;
            }
        }
        else {
            loopEndSample = info->loopEnd.f * sampleRate * numChannels;
            if (loopEndSample > numFrames * numChannels) {
                loopEndSample = numFrames * numChannels;
            }
        }
        loopElement->SetAttribute("End", loopEndSample);
    }
    else {
        loopElement->SetAttribute("End", numFrames * numChannels);
    }
    root->InsertEndChild(loopElement);
    root->SetAttribute("CustomFormat", audioType);
    root->SetAttribute("Size", numFrames * numChannels * 2);
    root->SetAttribute("Path", sampleDataPath.get());
    size_t samplePathLen = sizeof(sampleXmlBase) + strlen(fileName) + sizeof("_SAMPLE.xml") + 1;
    std::unique_ptr<char[]> sampleXmlPath = std::make_unique<char[]>(samplePathLen);
    snprintf(sampleXmlPath.get(), samplePathLen, "%s%s_SAMPLE.xml", sampleXmlBase, fileName);
    root->Accept(&p);
    
    WriteFileData(sampleXmlPath.get(), (void*)p.CStr(), p.CStrSize() - 1, a);

    p.ClearBuffer();
    return sampleXmlPath;
}

static void CreateSequenceXml(char* fileName, char* fontPath, unsigned int length, bool isFanfare, bool stereo, Archive* a) {
    tinyxml2::XMLDocument seqBaseRoot;
    tinyxml2::XMLError e = seqBaseRoot.LoadFile("assets/seq-base.xml");
    if (e != 0) {
        printf("Failed to open seq-base.xml. Falling back to the embedded version...\n");
        e = seqBaseRoot.Parse(gSequenceBaseXml, SEQ_BASE_XML_SIZE);
        if (e != 0) {
            printf("Failed to parse embedded XML. Exiting...\n");
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
        printf("Failed to open font-base.xml. Falling back to the embedded version...\n");
        e = fontBaseRoot.Parse(gFontBaseXml, FONT_BASE_XML_SIZE);
        if (e != 0) {
            printf("Failed to parse embedded XML. Exiting...\n");
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
        printf("Failed to open font-base-multi.xml. Falling back to the embedded version...\n");
        e = fontBaseRoot.Parse(gFontBaseMultiXml, FONT_BASE_MULTI_XML_SIZE);
        if (e != 0) {
            printf("Failed to parse embedded XML. Exiting...\n");
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
typedef enum class OggType {
    None = -1,
    Vorbis,
    Opus,
} OggType;

static OggType GetOggType(OggFileData* data) {
    ogg_sync_state oy;
    ogg_stream_state os;
    ogg_page og;
    ogg_packet op;
    OggType type;

    ogg_sync_init(&oy);
    char* buffer = ogg_sync_buffer(&oy, 4096);
    memcpy(buffer, data->data, 4096);
    ogg_sync_wrote(&oy, 4096);

    ogg_sync_pageout(&oy, &og);
    ogg_stream_init(&os, ogg_page_serialno(&og));
    ogg_stream_pagein(&os, &og);
    ogg_stream_packetout(&os, &op);
    
    // Can't use strcmp because op.packet isn't a null terminated string.
    if (memcmp((char*)op.packet, "\x01vorbis", 7) == 0) {
        type = OggType::Vorbis;
    } else if (memcmp((char*)op.packet, "OpusHead", 8) == 0) {
        type = OggType::Opus;
    } else {
        type = OggType::None;
    }
    ogg_stream_clear(&os);
    ogg_sync_clear(&oy);
    return type;
}


static int OpeWriteCallback(void* vData, const unsigned char* ptr, opus_int32 len) {
    OggFileData* data = static_cast<OggFileData*>(vData);
    size_t toRead = len;

    if (toRead > data->size - data->pos) {
        toRead = data->size - data->pos;
    }

    memcpy((char*)data->data + data->pos, ptr, toRead);
    data->pos += toRead;
    
    return 0;
}

static int OpeCloseCallback(void* data) {
    return 0;
}

static const OpusEncCallbacks opusCbs = {
    OpeWriteCallback,
    OpeCloseCallback,
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

static constexpr ov_callbacks cbs = {
    VorbisReadCallback,
    VorbisSeekCallback,
    VorbisCloseCallback,
    VorbisTellCallback,
};

// We don't want this to show less files than exist in the folder to pack when the operation is finished.
// `atomic` variables will ensure there isn't any inconsistency due to multi-threading.
static std::atomic<unsigned int> filesProcessed = 0;
#if 0
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
#endif
typedef struct ChannelInfo {
    void* channelData[2];
    size_t channelSizes[2];
} ChannelInfo;

static void SplitOggVorbis(ChannelInfo* info, std::unique_ptr<float[]> channels[2], uint64_t* sampleRate, uint64_t numFrames, size_t fileSize, uint32_t numChannels) {
    for (size_t i = 0; i < numChannels; i++) {
        OggFileData data;
        data.data = malloc(fileSize);
        data.size = fileSize;
        data.pos = 0;
        OggOpusComments* comments = ope_comments_create();
        ope_comments_add(comments, "ENCODER", "future using libopus libopusenc");
        
        // TODO, hardcoded to 48KHz even if the original song isn't. Should we resample it to 48k?
        OggOpusEnc* enc = ope_encoder_create_callbacks(&opusCbs, &data, comments, 48000, 1, 0, nullptr);
        ope_encoder_write_float(enc, channels[i].get(), numFrames);
        ope_encoder_drain(enc);
        ope_encoder_destroy(enc);
        info->channelData[i] = data.data;
        info->channelSizes[i] = data.pos;
    }
    

    #if 0
    MemoryBuffer outBuffer[2];
    // fileSize will likely always be larger than the original file but it will never need to reallocate the buffer.
    init_memory_buffer(&outBuffer[0], fileSize);
    init_memory_buffer(&outBuffer[1], fileSize);
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

        while (true) {
            int result = ogg_stream_flush(&os[i], &og[i]);
            if (result == 0)break;
            write_to_buffer(&outBuffer[i], og[i].header, og[i].header_len);
            write_to_buffer(&outBuffer[i], og[i].body, og[i].body_len);
        }
    }

    size_t pos = 0;
    size_t read = 0;
    do {
        float** bufferL = vorbis_analysis_buffer(&vd[0], 1024);
        float** bufferR = vorbis_analysis_buffer(&vd[1], 1024);
        if (pos + 1024 > numFrames) {
            read = numFrames - pos;
        } else {
            read = 1024;
        }
        if (read > 0) {
            // Avoid memcpy if pcm data can be used directly
            memcpy(bufferL[0], &channels[0].get()[pos], read * 4);
            memcpy(bufferR[0], &channels[1].get()[pos], read * 4);
            
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
    #endif
}

static void DecodeOpusFile(uint32_t* numChannels, uint64_t* sampleRate, uint64_t* numSamples, OggFileData* fileData, std::unique_ptr<float[]> channels[2]) {
    ogg_sync_state oy;
    ogg_stream_state os;
    ogg_page og;
    ogg_packet op;

    ogg_sync_init(&oy);
    // Read the first page
    char* buffer = ogg_sync_buffer(&oy, 4096);
    VorbisReadCallback(buffer, 4096, 1, fileData);
    // It would be best to use the number of bytes read from the callback but if it's too small the file is
    // probably invalid anyway
    ogg_sync_wrote(&oy, 4096); // Tell the libogg data was put into its buffer for processing
    ogg_sync_pageout(&oy, &og);
    ogg_stream_init(&os, ogg_page_serialno(&og));
    ogg_stream_pagein(&os, &og);
    ogg_stream_packetout(&os, &op);
    *sampleRate = reinterpret_cast<uint32_t*>(op.packet)[3];
    *numChannels = op.packet[9];
    // We don't care about the second header and its data
    int curHdr = 0;
    // Consume the next header
    while (curHdr < 1) {
        while (curHdr < 1) {
            int result = ogg_sync_pageout(&oy, &og);
            if (result == 0)
                break; // Need more data
            if (result == 1) {
                ogg_stream_pagein(&os, &og);
                while (curHdr < 1) {
                    result = ogg_stream_packetout(&os, &op);
                    if (result == 0) // Again, needs more data. Get more from pageout, pagein
                        break;
                    if (result < 0) {
                        // This is bad. Handle it at some point...
                    }
                    curHdr++;
                }
            }
        }
        /* no harm in not checking before adding more */
        buffer = ogg_sync_buffer(&oy, 4096);
        VorbisReadCallback(buffer, 4096, 1, fileData);
        ogg_sync_wrote(&oy, 4096);
    }
    OpusDecoder* dec = opus_decoder_create(48000, 2, nullptr);
    int eos = 0;
    int read = 0;
    std::vector<float> samples;
    size_t pos = 0;
    while (!eos) {
        while (!eos) {
            int res = ogg_sync_pageout(&oy, &og);
            if (res == 0) // Need more data
                break;
            if (res < 0) {
                // This is bad...
            }
            ogg_stream_pagein(&os, &og);
            while (true) {
                res = ogg_stream_packetout(&os, &op);
                if (res == 0)
                    break;
                if (res < 0) {
                    // You should know by now this is bad...
                }
                float pcm[960 * 6 * 2]; // Largest possible size
                // Decode opus encoded samples
                const int frameSize = opus_decode_float(dec, op.packet, op.bytes, pcm, 960 * 6, 0);
                // There isn't a good way to know how long an opus file is, so we need to keep filling a buffer
                // like this.
                samples.resize(samples.size() + frameSize * 2);
                memcpy(samples.data() + pos, pcm, frameSize * sizeof(float) * 2);
                pos += frameSize * 2;
            }
            eos = ogg_page_eos(&og);
        }
        if (!eos) {
            buffer = ogg_sync_buffer(&oy, 4096);
            // Needs to read 4096 elements due to how the return works.
            read = VorbisReadCallback(buffer, 1, 4096, fileData);
            ogg_sync_wrote(&oy, read);
            if (read == 0) {
                eos = 1;
            }
        }
    }
    channels[0] = std::make_unique<float[]>(pos * sizeof(float));
    channels[1] = std::make_unique<float[]>(pos * sizeof(float));
    *numSamples = pos;
    pos = 0;
    for (size_t i = 0; i < *numSamples - 1; i += 2, pos++) {
        channels[0].get()[pos] = samples[i];
        channels[1].get()[pos] = samples[i + 1];
    }
    *numSamples /= 2;
    opus_decoder_destroy(dec);

    ogg_stream_clear(&os);
    ogg_sync_clear(&oy);
}

static void WriteWavData(int16_t* l, int16_t* r, ChannelInfo* info, uint64_t numFrames, uint64_t sampleRate) {
    drwav outWav;

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

static void ProcessAudioFile(std::vector<char*>* fileQueue, std::unordered_map<char*, SeqMetaInfo>* seqMetaMap, bool loopTimeInSamples, bool transcodeToOpus, Archive* a) {
    while (filesProcessed < fileQueue->size()) {
    char** inputp = &(*fileQueue)[filesProcessed.fetch_add(1, std::memory_order_relaxed)];
    char* input = *inputp;
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
    uint32_t numChannels;
    uint64_t sampleRate;
    int audioType;
    // Only used for multichannel files
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
        if (numChannels == 2) {
            audioType = AudioType::ogg;
            auto sampleData = std::make_unique<float[]>(numFrames * numChannels);
            drmp3_read_pcm_frames_f32(&mp3, numFrames, sampleData.get());
            std::unique_ptr<float[]> channels[2];
            channels[0] = std::make_unique<float[]>(numFrames);
            channels[1] = std::make_unique<float[]>(numFrames);
            size_t pos = 0;
            for (size_t i = 0; i < numFrames * numChannels - 1; i += 2, pos++) {
                channels[0].get()[pos] = sampleData.get()[i];
                channels[1].get()[pos] = sampleData.get()[i + 1];
            }
            SplitOggVorbis(&infos, channels, &sampleRate, numFrames, fileSize, 2);
        }
        else if (numChannels == 1) {
            if (transcodeToOpus) {
                audioType = AudioType::ogg;
                std::unique_ptr<float[]> channelDataF[2]; // `SplitOggVorbis` wants an array so we will give it one but with one element initialized.
                channelDataF[0] = std::make_unique<float[]>(numFrames);
                drmp3_read_pcm_frames_f32(&mp3, numFrames, channelDataF[0].get());
                SplitOggVorbis(&infos, channelDataF, &sampleRate, numFrames, fileSize, 1);

            }
        }

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
            int16_t* usedSampleData;
            if (wav.translatedFormatTag == DR_WAVE_FORMAT_PCM && wav.bitsPerSample == 16) {
                // If the data is known to be s16 we can just read the raw data directly from the file instead of copying it into our own buffers as long as we don't write to it.
                // This data will be copied else where at some point before freeing the file
                usedSampleData = (int16_t*)(wav.memoryStream.data + wav.memoryStream.currentReadPos);
            }
            else {
                // Otherwise let drwav convert it to s16
                drwav_read_pcm_frames_s16(&wav, numFrames, sampleData.get());
                usedSampleData = sampleData.get();
            }

            
            auto sampleDataL = std::make_unique<int16_t[]>(numFrames);
            auto sampleDataR = std::make_unique<int16_t[]>(numFrames);
            size_t pos = 0;
            for (size_t i = 0; i < numFrames * numChannels - 1; i += 2, pos++) {
                sampleDataL.get()[pos] = usedSampleData[i];
                sampleDataR.get()[pos] = usedSampleData[i + 1];
            }
            if (!transcodeToOpus) {
                WriteWavData(sampleDataL.get(), sampleDataR.get(), &infos, numFrames, sampleRate);
            } else {
                std::unique_ptr<float[]> channelDataF[2];
                channelDataF[0] = std::make_unique<float[]>(numFrames);
                channelDataF[1] = std::make_unique<float[]>(numFrames);
                PcmS16ToF(channelDataF[0].get(), sampleDataL.get(), numFrames);
                PcmS16ToF(channelDataF[1].get(), sampleDataR.get(), numFrames);
                SplitOggVorbis(&infos, channelDataF, &sampleRate, numFrames, fileSize, numChannels);
                audioType = AudioType::ogg;
            }
        }
        else if (numChannels == 1) {
            if (transcodeToOpus) {
                audioType = AudioType::ogg;
                std::unique_ptr<float[]> channelDataF[2]; // `SplitOggVorbis` wants an array so we will give it one but with one element initialized.
                channelDataF[0] = std::make_unique<float[]>(numFrames);
                if (wav.translatedFormatTag == DR_WAVE_FORMAT_PCM && wav.bitsPerSample == 16) {
                    // Small optimization to avoid multiple copies
                    PcmS16ToF(channelDataF[0].get(), (const int16_t*)(wav.memoryStream.data + wav.memoryStream.currentReadPos), numFrames);
                }
                else {
                    drwav_read_pcm_frames_f32(&wav, numFrames, channelDataF[0].get());
                }
                SplitOggVorbis(&infos, channelDataF, &sampleRate, numFrames, fileSize, 1);

            }
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
            if (!transcodeToOpus) {
                audioType = AudioType::wav;
                WriteWavData(sampleDataL.get(), sampleDataR.get(), &infos, numFrames, sampleRate);
            } else {
                std::unique_ptr<float[]> channelDataF[2];
                channelDataF[0] = std::make_unique<float[]>(numFrames);
                channelDataF[1] = std::make_unique<float[]>(numFrames);
                PcmS16ToF(channelDataF[0].get(), sampleDataL.get(), numFrames);
                PcmS16ToF(channelDataF[1].get(), sampleDataR.get(), numFrames);
                SplitOggVorbis(&infos, channelDataF, &sampleRate, numFrames, fileSize, 2);
                audioType = AudioType::ogg;
            }
        }
        else if (numChannels == 1) {
            if (transcodeToOpus) {
                audioType = AudioType::ogg;
                std::unique_ptr<float[]> channelDataF[2]; // `SplitOggVorbis` wants an array so we will give it one but with one element initialized.
                auto sampleData = std::make_unique<int16_t[]>(numFrames);
                channelDataF[0] = std::make_unique<float[]>(numFrames);
                drflac_read_pcm_frames_s16(flac, numFrames, sampleData.get());
                PcmS16ToF(channelDataF[0].get(), sampleData.get(), numFrames);
                SplitOggVorbis(&infos, channelDataF, &sampleRate, numFrames, fileSize, 1);
                
            }
        }
        drflac_close(flac);
    }
    else if (OGG_CHECK(dataU8)) {
        std::unique_ptr<float[]> channels[2];

        audioType = AudioType::ogg;
        OggVorbis_File vf;
        OggFileData fileData = {
            .data = data,
            .pos = 0,
            .size = (size_t)fileSize,
        };
        switch (GetOggType(&fileData)) {
            case OggType::Vorbis: {
                long read = 0;
                size_t pos = 0;
                if (const int ret = ov_open_callbacks(&fileData, &vf, nullptr, 0, cbs); ret < 0) {
                    printf("Vorbisfile failed %d\n", ret);
                    return;
                }
                vorbis_info* vi = ov_info(&vf, -1);

                numFrames = ov_pcm_total(&vf, -1);
                sampleRate = vi->rate;
                numChannels = vi->channels;
                if (numChannels == 2) {
                    channels[0] = std::make_unique<float[]>(numFrames);
                    channels[1] = std::make_unique<float[]>(numFrames);

                    do {
                        float** pcm;
                        int bitStream;
                        read = ov_read_float(&vf, &pcm, 4096, &bitStream);
                        memcpy(&channels[0].get()[pos], pcm[0], read * sizeof(float));
                        memcpy(&channels[1].get()[pos], pcm[1], read * sizeof(float));
                        pos += read;
                    } while (read > 0);
                    SplitOggVorbis(&infos, channels, &sampleRate, numFrames, fileSize, numChannels);
                }
                break;
            }
            case OggType::Opus: {
                DecodeOpusFile(&numChannels, &sampleRate, &numFrames, &fileData, channels);
                SplitOggVorbis(&infos, channels, &sampleRate, numFrames, fileSize, numChannels);
                break;
            }
            default:
                printf("OGG file not Vorbis or OPUS\n");
                return;;

        }
    }
    else {
        return;
    }

    std::unique_ptr<char[]> fontXmlPath;
    if (numChannels == 2) {
        CreateSampleXml(fileNames[0].get(), audioTypeToStr[audioType], numFrames, 1, &seqMetaMap->at(fileName), sampleRate, loopTimeInSamples, a);
        CopySampleData(reinterpret_cast<char*>(infos.channelData[0]), fileNames[0].get(), false, infos.channelSizes[0], a);

        CreateSampleXml(fileNames[1].get(), audioTypeToStr[audioType], numFrames, 1, &seqMetaMap->at(fileName), sampleRate, loopTimeInSamples, a);
        CopySampleData(reinterpret_cast<char*>(infos.channelData[1]), fileNames[1].get(), false, infos.channelSizes[1], a);
        fontXmlPath = CreateFontMultiXml(fileNames, fileName, sampleRate, a);
        free(infos.channelData[0]);
        free(infos.channelData[1]);
    } else {
        CreateSampleXml(fileName, audioTypeToStr[audioType], numFrames, numChannels, &seqMetaMap->at(fileName), sampleRate, loopTimeInSamples, a);
        CopySampleData(input, fileName, true, fileSize, a);
        fontXmlPath = CreateFontXml(fileName, sampleRate, numChannels, a);
    }
    // There is no good way to determine the length of the song when we go to load it so we need to store the length in seconds.
    float lengthF = (float)numFrames / (float)sampleRate;
    lengthF = ceilf(lengthF);
    unsigned int length = static_cast<unsigned int>(lengthF);
    CreateSequenceXml(fileName, fontXmlPath.get(), length, seqMetaMap->at(fileName).fanfare, numChannels == 2, a);
    // Since the function that allocates the paths allocates them 2 byte aligned, we can use the lowest bit as a signal that this file has been processed.
    uintptr_t inputU = reinterpret_cast<uintptr_t>(*inputp);
    inputU |= 1;
    *inputp= (char*)inputU;
    }
}

static void PackFilesMgrWorker(std::vector<char*>* fileQueue, std::unordered_map<char*, SeqMetaInfo>* fanfareMap, bool* threadStarted, bool* threadDone, CustomStreamedAudioWindow* thisx) {
    std::unique_ptr<Archive> a;
    switch (thisx->GetRadioState()) {
        case 1: {
            a = std::make_unique<MpqArchive>(thisx->GetSavePath());
            break;
        }
        case 2: {
            a = std::make_unique<ZipArchive>(thisx->GetSavePath());
            break;
        }
    }

    const unsigned int numThreads = std::thread::hardware_concurrency();
    auto packFileThreads = std::make_unique<std::thread[]>(numThreads);
    for (unsigned int i = 0; i < numThreads; i++) {
        packFileThreads[i] = std::thread(ProcessAudioFile, fileQueue, fanfareMap, thisx->GetLoopTimeType(), thisx->GetTranscode(), a.get());
    }

    for (unsigned int i = 0; i < numThreads; i++) {
        packFileThreads[i].join();
    }
    ClearFileQueue(fileQueue);
    a->CloseArchive();
    *threadStarted = false;
    *threadDone = true;
}

#if 0
static std::array<const char*, 2> sToggleLabels = {
    "Create Folder",
    "Create Archive",
};
#endif

int CustomStreamedAudioWindow::GetRadioState() const {
    return mRadioState;
}

char* CustomStreamedAudioWindow::GetSavePath() const {
    return mSavePath;
}

bool CustomStreamedAudioWindow::GetLoopTimeType() const {
    return mLoopIsISamples;
}

bool CustomStreamedAudioWindow::GetTranscode() const {
    return mTranscodeToOpus;
}

static bool FillFileCallback(char* path) {
    char* ext = strrchr(path, '.');
    if (ext != nullptr) {
        if ((strcmp(ext, ".wav") == 0 || strcmp(ext, ".ogg") == 0 || strcmp(ext, ".opus") == 0 || strcmp(ext, ".mp3") == 0) || strcmp(ext, ".flac") == 0) {
            return true;
        }
    }
    return false;
}

void CustomStreamedAudioWindow::DrawWindow() {
    ImGui::Begin("Create Custom Streamed Audio", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetWindowPos(ImGui::GetMainViewport()->Pos);

    ImGui::SetCursorPos({ 20.0f,20.0f });
    
    ImGui::BeginDisabled(mThreadStarted);
    
    if (ImGui::ArrowButton("Back", ImGuiDir::ImGuiDir_Left)) {
        gWindowMgr.SetCurWindow(WindowId::Create);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Create Archive With Streamed Songs");
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 3.0f);

    ImGui::TextUnformatted("Open a directory and create an archive with the files needed for streamed audio");

    if (ImGui::Button("Select Directory")) {
        ClearPathBuff();
        GetOpenDirPath(&mPathBuff);
        FillFileQueue(mFileQueue, mPathBuff, FillFileCallback);
        std::sort(mFileQueue.begin(), mFileQueue.end(), [](char* a, char* b) {
            return strcmp(a, b) < 0;
        });
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

    ImGui::SameLine();
    ImGui::Checkbox("Transcode to opus", &mTranscodeToOpus);
    ImGui::SetItemTooltip("Transcode uncompressed files to the opus codec.\nRecommended because of its speed and space savings.");

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
                std::thread packFilesMgrThread(PackFilesMgrWorker, &mFileQueue, &mSeqMetaMap, &mThreadStarted, &mThreadIsDone, this);
                packFilesMgrThread.detach();
            }
        }
    }

    ImGui::EndDisabled();

    DrawPendingFilesList();

    ImGui::End();
}

void CustomStreamedAudioWindow::FillFanfareMap() {
    mSeqMetaMap.clear();
    for (const auto f : mFileQueue) {
        char* fileName = strrchr(f, PATH_SEPARATOR);
        fileName++;
        mSeqMetaMap[fileName].fanfare = false;
        mSeqMetaMap[fileName].loopStart.i = 0;
        mSeqMetaMap[fileName].loopEnd.i = 0;
    }
}

void CustomStreamedAudioWindow::ClearFanfareMap() {
    for (auto& m : mSeqMetaMap) {
        m.second.loopEnd.i = 0;
        m.second.loopStart.i = 0;
        m.second.loopEnd.f = 0.0f;
        m.second.loopStart.f = 0.0f;
    }
}

static constexpr std::array<const char*, 2> loopToggleLabels = {
    "Loop Times in Seconds",
    "Loop Times in Samples",
};

void CustomStreamedAudioWindow::DrawPendingFilesList() {
    if (mFileQueue.empty()) {
        return;
    }

    const ImVec2 fiveCharsWidth = ImGui::CalcTextSize("00000000");
    const ImVec2 labelWidth = ImGui::CalcTextSize(loopToggleLabels[0]);
    ImGui::TextUnformatted("Files to Pack:");

    const float windowHeight = ImGui::GetWindowHeight();
    float maxLen = 0;
    for (auto f : mFileQueue) {
        if ((uintptr_t)f & 1 != 1) {
            maxLen = std::max(maxLen, ImGui::CalcTextSize(strrchr(f, PATH_SEPARATOR)).x);
        }
    }
    ImGui::SameLine();
    if (ImGui::Toggle(loopToggleLabels[mLoopIsISamples], &mLoopIsISamples)) {
        FillFanfareMap();
    }
    float totalPadding = maxLen + labelWidth.x + fiveCharsWidth.x;
    //if (labelWidth.x >   maxLen) {
    //    totalPadding += labelWidth.x;
    //}
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + totalPadding);
    ImGui::SameLine();
    const float startPos = ImGui::GetCursorPosX();
    ImGui::TextUnformatted("Loop Start");
    ImGui::SameLine();
    const float endPos = ImGui::GetCursorPosX();
    ImGui::TextUnformatted("LoopEnd");
    ImGui::SameLine();
    const float fanfarePos = ImGui::GetCursorPosX();
    ImGui::TextUnformatted("Fanfare");
    ImGui::SameLine();
    const float fanfareEndPos = ImGui::GetCursorPosX();
    // The checkbox will draw at the position specified. To draw the center of the box in the center of the text, start drawing the box at the 1/3 point of the text.
    const float fanfareDiff = (fanfareEndPos - fanfarePos);
    const float fanfareStartPos = (fanfareDiff * .33f) + fanfarePos;
    ImGui::NewLine();

    const ImGuiDataType type = mLoopIsISamples ? ImGuiDataType_S32 : ImGuiDataType_Float;
    ImGui::BeginChild("File List", {}, 0, 0);

    for (const auto s : mFileQueue) {
        if ((uintptr_t)s & 1 == 1) {
            continue;
        }
        const float scroll = ImGui::GetScrollY();
        const float cursorPosY = ImGui::GetCursorPosY();

        // Only draw elements that are on screen. This gives around 20x performance improvement.
        if ((cursorPosY < scroll) || (cursorPosY > (scroll + windowHeight))) {
            ImGui::NewLine();
            continue;
        }
        char* fileName = strrchr(s, PATH_SEPARATOR);
        fileName++;
        size_t len = strlen(fileName);
        auto checkboxTag = std::make_unique<char[]>(len + sizeof("Fanfare##") + 1);
        auto loopStartTag = std::make_unique<char[]>(len + sizeof("Loop Start##") + 1);
        auto loopEndTag = std::make_unique<char[]>(len + sizeof("Loop End##") + 1);
        sprintf(checkboxTag.get(), "##ff%s", fileName);
        sprintf(loopStartTag.get(), "##start%s", fileName);
        sprintf(loopEndTag.get(), "##end%s", fileName);
        ImGui::Text("%s", fileName);       
        ImGui::SameLine(totalPadding);
        ImGui::SetCursorPosX(startPos);
        ImGui::PushItemWidth(fiveCharsWidth.x);
        ImGui::InputScalarN(loopStartTag.get(), type, &mSeqMetaMap.at(fileName).loopStart, 1);
        ImGui::SameLine();
        ImGui::SetCursorPosX(endPos);
        ImGui::InputScalarN(loopEndTag.get(), type, &mSeqMetaMap.at(fileName).loopEnd, 1);
        ImGui::SameLine();
        ImGui::PopItemWidth();
        ImGui::SetCursorPosX(fanfareStartPos);
        ImGui::Checkbox(checkboxTag.get(), &mSeqMetaMap.at(fileName).fanfare);
        
    }
    ImGui::EndChild();
}

