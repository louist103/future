
// Used as a fallback for when the XML file can't be opened. Make sure to update both this and the file when doing modifications.
static const char gFontBaseXml[] =
"<SoundFont Version=\"0\" Num=\"0\" Medium=\"Cart\" CachePolicy=\"Either\" Data1=\"511\" Data2=\"3840\" Data3=\"0\">\
<Drums/>\
<Instruments>\
<Instrument IsValid=\"true\" Loaded=\"false\"NormalRangeLo=\"0\" NormalRangeHi=\"127\" ReleaseRate=\"239\">\
<Envelopes>\
<Envelope Delay=\"1\" Arg=\"32000\"/>\
<Envelope Delay=\"1000\" Arg=\"32000\"/>\
<Envelope Delay=\"-1\" Arg=\"0\"/>\
<Envelope Delay=\"0\" Arg=\"0\"/>\
</Envelopes>\
<NormalNotesSound/>\
</Instrument>\
</Instruments>\
</SoundFont>\
";

static const char gFontBaseMultiXml[] =
"<SoundFont Version=\"0\" Num=\"0\" Medium=\"Cart\" CachePolicy=\"Either\" Data1=\"511\" Data2=\"3840\" Data3=\"0\">\
<Drums/>\
<Instruments>\
<Instrument IsValid=\"true\" Loaded=\"false\" NormalRangeLo=\"0\" NormalRangeHi=\"127\" ReleaseRate=\"239\">\
<Envelopes>\
<Envelope Delay=\"1\" Arg=\"32000\"/>\
<Envelope Delay=\"1000\" Arg=\"32000\"/>\
<Envelope Delay=\"-1\" Arg=\"0\"/>\
<Envelope Delay=\"0\" Arg=\"0\"/>\
</Envelopes>\
<NormalNotesSound/>\
</Instrument>\
<Instrument IsValid=\"true\" Loaded=\"false\" NormalRangeLo=\"0\" NormalRangeHi=\"127\" ReleaseRate=\"239\">\
<Envelopes>\
<Envelope Delay=\"1\" Arg=\"32000\"/>\
<Envelope Delay=\"1000\" Arg=\"32000\"/>\
<Envelope Delay=\"-1\" Arg=\"0\"/>\
<Envelope Delay=\"0\" Arg=\"0\"/>\
</Envelopes>\
<NormalNotesSound/>\
</Instrument>\
</Instruments>\
</SoundFont>\
";

static const char gSampleBaseXml[] = "<Sample Version=\"0\" Codec=\"S16\" Medium=\"Cart\" bit26=\"0\" Relocated=\"0\" LoopStart=\"0\" LoopCount=\"0\"/>";

static const char gSequenceBaseXml[] = "<Sequence Index=\"0\" Medium=\"Cart\" CachePolicy=\"Either\" Size=\"0\" Streamed=\"true\">\
<FontIndicies>\
</FontIndicies>\
</Sequence>\
";

static constexpr size_t FONT_BASE_XML_SIZE = sizeof(gFontBaseXml);
static constexpr size_t FONT_BASE_MULTI_XML_SIZE = sizeof(gFontBaseMultiXml);
static constexpr size_t SAMPLE_BASE_XML_SIZE = sizeof(gSampleBaseXml);
static constexpr size_t SEQ_BASE_XML_SIZE = sizeof(gSequenceBaseXml);
