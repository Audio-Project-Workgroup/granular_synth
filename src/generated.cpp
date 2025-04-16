static MemberDefinition membersOf_PluginState[] = 
{
  {STR8_LIT("PluginFloatParameter"), STR8_LIT("parameters"), 0, 0, PluginParameter_count},
  {STR8_LIT("u64"), STR8_LIT("osTimerFreq"), 0, 0, 0},
  {STR8_LIT("Arena"), STR8_LIT("grainArena"), 0, 0, 0},
  {STR8_LIT("Arena"), STR8_LIT("permanentArena"), 0, 0, 0},
  {STR8_LIT("Arena"), STR8_LIT("frameArena"), 0, 0, 0},
  {STR8_LIT("Arena"), STR8_LIT("framePermanentArena"), 0, 0, 0},
  {STR8_LIT("Arena"), STR8_LIT("loadArena"), 0, 0, 0},
  {STR8_LIT("PluginHost"), STR8_LIT("pluginHost"), 0, 0, 0},
  {STR8_LIT("PluginMode"), STR8_LIT("pluginMode"), 0, 0, 0},
  {STR8_LIT("String8"), STR8_LIT("outputDeviceNames"), 0, 0, 32},
  {STR8_LIT("u32"), STR8_LIT("outputDeviceCount"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("selectedOutputDeviceIndex"), 0, 0, 0},
  {STR8_LIT("String8"), STR8_LIT("inputDeviceNames"), 0, 0, 32},
  {STR8_LIT("u32"), STR8_LIT("inputDeviceCount"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("selectedInputDeviceIndex"), 0, 0, 0},
  {STR8_LIT("LoadedGrainPackfile"), STR8_LIT("loadedGrainPackfile"), 0, 0, 0},
  {STR8_LIT("FileGrainState"), STR8_LIT("silo"), 0, 0, 0},
  {STR8_LIT("r64"), STR8_LIT("phasor"), 0, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("freq"), 0, 0, 0},
  {STR8_LIT("PluginBooleanParameter"), STR8_LIT("soundIsPlaying"), 0, 0, 0},
  {STR8_LIT("PlayingSound"), STR8_LIT("loadedSound"), 0, 0, 0},
  {STR8_LIT("LoadedBitmap"), STR8_LIT("testBitmap"), 0, 0, 0},
  {STR8_LIT("LoadedFont"), STR8_LIT("testFont"), 0, 0, 0},
  {STR8_LIT("UIContext"), STR8_LIT("uiContext"), 0, 0, 0},
  {STR8_LIT("UIPanel"), STR8_LIT("rootPanel"), 1, 0, 0},
  {STR8_LIT("UIPanel"), STR8_LIT("menuPanel"), 1, 0, 0},
  {STR8_LIT("GrainManager"), STR8_LIT("grainManager"), 0, 0, 0},
  {STR8_LIT("AudioRingBuffer"), STR8_LIT("grainBuffer"), 0, 0, 0},
  {STR8_LIT("GrainStateView"), STR8_LIT("grainStateView"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("initializationMutex"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("initialized"), 0, 0, 0},
};

static MemberDefinition membersOf_LoadedGrainPackfile[] = 
{
  {STR8_LIT("u8"), STR8_LIT("packfileMemory"), 1, 0, 0},
  {STR8_LIT("u64"), STR8_LIT("grainCount"), 0, 0, 0},
  {STR8_LIT("u64"), STR8_LIT("grainLength"), 0, 0, 0},
  {STR8_LIT("GrainPackfileTag"), STR8_LIT("tags"), 1, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("samples"), 1, 0, 0},
};

static MemberDefinition membersOf_FileGrainState[] = 
{
  {STR8_LIT("Arena"), STR8_LIT("allocator"), 1, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("queuedGrainCount"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("playingGrainCount"), 0, 0, 0},
  {STR8_LIT("u64"), STR8_LIT("samplesElapsedSinceLastQueue"), 0, 0, 0},
  {STR8_LIT("PlayingGrain"), STR8_LIT("grainQueue"), 1, 0, 0},
  {STR8_LIT("PlayingGrain"), STR8_LIT("playlistSentinel"), 1, 0, 0},
  {STR8_LIT("PlayingGrain"), STR8_LIT("freeList"), 1, 0, 0},
};

static MemberDefinition membersOf_UIContext[] = 
{
  {STR8_LIT("Arena"), STR8_LIT("frameArena"), 1, 0, 0},
  {STR8_LIT("Arena"), STR8_LIT("permanentArena"), 1, 0, 0},
  {STR8_LIT("LoadedFont"), STR8_LIT("font"), 1, 0, 0},
  {STR8_LIT("v3"), STR8_LIT("mouseP"), 0, 0, 0},
  {STR8_LIT("v3"), STR8_LIT("lastMouseP"), 0, 0, 0},
  {STR8_LIT("v2"), STR8_LIT("mouseLClickP"), 0, 0, 0},
  {STR8_LIT("v2"), STR8_LIT("mouseRClickP"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("leftButtonPressed"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("leftButtonReleased"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("leftButtonDown"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("rightButtonPressed"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("rightButtonReleased"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("rightButtonDown"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("escPressed"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("tabPressed"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("backspacePressed"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("enterPressed"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("minusPressed"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("plusPressed"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("windowResized"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("frameIndex"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("layoutCount"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("processedElementCount"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("selectedElementOrdinal"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("selectedElementLayoutIndex"), 0, 0, 0},
  {STR8_LIT("UIHashKey"), STR8_LIT("selectedElement"), 0, 0, 0},
};

static MemberDefinition membersOf_UIPanel[] = 
{
  {STR8_LIT("UIPanel"), STR8_LIT("first"), 1, 0, 0},
  {STR8_LIT("UIPanel"), STR8_LIT("last"), 1, 0, 0},
  {STR8_LIT("UIPanel"), STR8_LIT("next"), 1, 0, 0},
  {STR8_LIT("UIPanel"), STR8_LIT("prev"), 1, 0, 0},
  {STR8_LIT("UIPanel"), STR8_LIT("parent"), 1, 0, 0},
  {STR8_LIT("UIAxis"), STR8_LIT("splitAxis"), 0, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("sizePercentOfParent"), 0, 0, 0},
  {STR8_LIT("String8"), STR8_LIT("name"), 0, 0, 0},
  {STR8_LIT("v4"), STR8_LIT("color"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("fringeFlags"), 0, 0, 0},
  {STR8_LIT("Rect2"), STR8_LIT("region"), 0, 0, 0},
  {STR8_LIT("UILayout"), STR8_LIT("layout"), 0, 0, 0},
};

static MemberDefinition membersOf_AudioRingBuffer[] = 
{
  {STR8_LIT("r32"), STR8_LIT("samples"), 1, 0, 2},
  {STR8_LIT("u32"), STR8_LIT("capacity"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("writeIndex"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("readIndex"), 0, 0, 0},
};

static MemberDefinition membersOf_GrainManager[] = 
{
  {STR8_LIT("Arena"), STR8_LIT("grainAllocator"), 1, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("grainCount"), 0, 0, 0},
  {STR8_LIT("Grain"), STR8_LIT("grainPlayList"), 1, 0, 0},
  {STR8_LIT("Grain"), STR8_LIT("grainFreeList"), 1, 0, 0},
  {STR8_LIT("AudioRingBuffer"), STR8_LIT("grainBuffer"), 1, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("samplesProcessedSinceLastSeed"), 0, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("windowBuffer"), 1, 0, 4},
};

static MemberDefinition membersOf_GrainStateView[] = 
{
  {STR8_LIT("GrainBufferViewNode"), STR8_LIT("head"), 1, 0, 0},
  {STR8_LIT("GrainBufferViewNode"), STR8_LIT("tail"), 1, 0, 0},
  {STR8_LIT("Arena"), STR8_LIT("arena"), 1, 0, 0},
  {STR8_LIT("GrainBufferViewNode"), STR8_LIT("grainBufferViewFreelist"), 1, 0, 0},
  {STR8_LIT("GrainViewNode"), STR8_LIT("grainViewFreelist"), 1, 0, 0},
};

static MemberDefinition membersOf_Rect2[] = 
{
  {STR8_LIT("v2"), STR8_LIT("min"), 0, 0, 0},
  {STR8_LIT("v2"), STR8_LIT("max"), 0, 0, 0},
};

static MemberDefinition membersOf_LoadedBitmap[] = 
{
  {STR8_LIT("u32"), STR8_LIT("width"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("height"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("stride"), 0, 0, 0},
  {STR8_LIT("v2"), STR8_LIT("alignPercentage"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("pixels"), 1, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("glHandle"), 0, 0, 0},
};

static MemberDefinition membersOf_LoadedFont[] = 
{
  {STR8_LIT("RangeU32"), STR8_LIT("characterRange"), 0, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("verticalAdvance"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("glyphCount"), 0, 0, 0},
  {STR8_LIT("LoadedBitmap"), STR8_LIT("glyphs"), 1, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("horizontalAdvance"), 1, 0, 0},
};

static MemberDefinition membersOf_Arena[] = 
{
  {STR8_LIT("u8"), STR8_LIT("base"), 1, 0, 0},
  {STR8_LIT("usz"), STR8_LIT("capacity"), 0, 0, 0},
  {STR8_LIT("usz"), STR8_LIT("used"), 0, 0, 0},
};

static MemberDefinition membersOf_String8[] = 
{
  {STR8_LIT("u8"), STR8_LIT("str"), 1, 0, 0},
  {STR8_LIT("u64"), STR8_LIT("size"), 0, 0, 0},
};

static MemberDefinition membersOf_PluginBooleanParameter[] = 
{
  {STR8_LIT("bool"), STR8_LIT("value"), 0, 0, 0},
};

static MemberDefinition membersOf_PluginFloatParameter[] = 
{
  {STR8_LIT("RangeR32"), STR8_LIT("range"), 0, 0, 0},
  {STR8_LIT("ParameterTransform"), STR8_LIT("processingTransform"), 1, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("mutex"), 0, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("currentValue"), 0, 1, 0},
  {STR8_LIT("r32"), STR8_LIT("targetValue"), 0, 1, 0},
  {STR8_LIT("r32"), STR8_LIT("dValue"), 0, 1, 0},
  {STR8_LIT("u32"), STR8_LIT("currentValue_AsInt"), 0, 1, 0},
  {STR8_LIT("u32"), STR8_LIT("targetValue_AsInt"), 0, 1, 0},
  {STR8_LIT("u32"), STR8_LIT("dValue_AsInt"), 0, 1, 0},
};

static MemberDefinition membersOf_PlayingSound[] = 
{
  {STR8_LIT("LoadedSound"), STR8_LIT("sound"), 0, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("samplesPlayed"), 0, 0, 0},
};

static MemberDefinition membersOf_GrainPackfileTag[] = 
{
  {STR8_LIT("usz"), STR8_LIT("startSampleIndex"), 0, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("vector"), 0, 0, FILE_TAG_LENGTH},
};

static MemberDefinition membersOf_PlayingGrain[] = 
{
  {STR8_LIT("PlayingGrain"), STR8_LIT("nextPlaying"), 1, 0, 0},
  {STR8_LIT("PlayingGrain"), STR8_LIT("prevPlaying"), 1, 0, 0},
  {STR8_LIT("PlayingGrain"), STR8_LIT("nextQueued"), 1, 1, 0},
  {STR8_LIT("PlayingGrain"), STR8_LIT("nextFree"), 1, 1, 0},
  {STR8_LIT("bool"), STR8_LIT("onFreeList"), 0, 0, 0},
  {STR8_LIT("u64"), STR8_LIT("startSampleIndex"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("samplesRemaining"), 0, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("samples"), 1, 0, 2},
};

static MemberDefinition membersOf_UIHashKey[] = 
{
  {STR8_LIT("u64"), STR8_LIT("key"), 0, 0, 0},
  {STR8_LIT("String8"), STR8_LIT("name"), 0, 0, 0},
};

static MemberDefinition membersOf_UILayout[] = 
{
  {STR8_LIT("UIContext"), STR8_LIT("context"), 1, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("elementCount"), 0, 0, 0},
  {STR8_LIT("UIElement"), STR8_LIT("root"), 1, 0, 0},
  {STR8_LIT("UIElement"), STR8_LIT("currentParent"), 1, 0, 0},
  {STR8_LIT("UIElement"), STR8_LIT("elementCache"), 1, 0, 512},
  {STR8_LIT("UIElement"), STR8_LIT("elementFreeList"), 1, 0, 0},
  {STR8_LIT("Rect2"), STR8_LIT("regionRemaining"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("index"), 0, 0, 0},
  {STR8_LIT("UIHashKey"), STR8_LIT("selectedElement"), 0, 0, 0},
};

static MemberDefinition membersOf_Grain[] = 
{
  {STR8_LIT("Grain"), STR8_LIT("next"), 1, 0, 0},
  {STR8_LIT("Grain"), STR8_LIT("prev"), 1, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("start"), 1, 0, 2},
  {STR8_LIT("WindowType"), STR8_LIT("window"), 0, 0, 0},
  {STR8_LIT("s32"), STR8_LIT("samplesToPlay"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("length"), 0, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("lengthInv"), 0, 0, 0},
};

static MemberDefinition membersOf_GrainViewNode[] = 
{
  {STR8_LIT("GrainViewNode"), STR8_LIT("next"), 1, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("readIndex"), 0, 0, 0},
};

static MemberDefinition membersOf_GrainBufferViewNode[] = 
{
  {STR8_LIT("GrainBufferViewNode"), STR8_LIT("next"), 1, 0, 0},
  {STR8_LIT("GrainBufferViewNode"), STR8_LIT("nextFree"), 1, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("bufferReadIndex"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("bufferWriteIndex"), 0, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("bufferSamples"), 1, 0, 2},
  {STR8_LIT("u32"), STR8_LIT("grainCount"), 0, 0, 0},
  {STR8_LIT("GrainViewNode"), STR8_LIT("head"), 1, 0, 0},
};

static MemberDefinition membersOf_LoadedSound[] = 
{
  {STR8_LIT("u32"), STR8_LIT("sampleCount"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("channelCount"), 0, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("samples"), 1, 0, 2},
};

static MemberDefinition membersOf_UIElement[] = 
{
  {STR8_LIT("UIElement"), STR8_LIT("next"), 1, 0, 0},
  {STR8_LIT("UIElement"), STR8_LIT("prev"), 1, 0, 0},
  {STR8_LIT("UIElement"), STR8_LIT("first"), 1, 0, 0},
  {STR8_LIT("UIElement"), STR8_LIT("last"), 1, 0, 0},
  {STR8_LIT("UIElement"), STR8_LIT("parent"), 1, 0, 0},
  {STR8_LIT("UIHashKey"), STR8_LIT("hashKey"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("lastFrameTouched"), 0, 0, 0},
  {STR8_LIT("UIElement"), STR8_LIT("nextInHash"), 1, 0, 0},
  {STR8_LIT("UILayout"), STR8_LIT("layout"), 1, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("flags"), 0, 0, 0},
  {STR8_LIT("String8"), STR8_LIT("name"), 0, 0, 0},
  {STR8_LIT("v2"), STR8_LIT("textScale"), 0, 0, 0},
  {STR8_LIT("v4"), STR8_LIT("color"), 0, 0, 0},
  {STR8_LIT("UIParameter"), STR8_LIT("parameterType"), 0, 0, 0},
  {STR8_LIT("PluginBooleanParameter"), STR8_LIT("bParam"), 1, 1, 0},
  {STR8_LIT("PluginFloatParameter"), STR8_LIT("fParam"), 1, 1, 0},
  {STR8_LIT("r32"), STR8_LIT("aspectRatio"), 0, 0, 0},
  {STR8_LIT("UIAxis"), STR8_LIT("sizingDim"), 0, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("size"), 0, 1, 0},
  {STR8_LIT("r32"), STR8_LIT("heightPercentOfLayoutRegionRemaining"), 0, 1, 0},
  {STR8_LIT("r32"), STR8_LIT("widthPercentOfLayoutRegionRemaining"), 0, 1, 0},
  {STR8_LIT("v2"), STR8_LIT("offset"), 0, 0, 0},
  {STR8_LIT("Rect2"), STR8_LIT("region"), 0, 0, 0},
  {STR8_LIT("u32"), STR8_LIT("commFlags"), 0, 0, 0},
  {STR8_LIT("v2"), STR8_LIT("mouseClickedP"), 0, 0, 0},
  {STR8_LIT("r32"), STR8_LIT("fParamValueAtClick"), 0, 0, 0},
  {STR8_LIT("v2"), STR8_LIT("dragData"), 0, 0, 0},
  {STR8_LIT("bool"), STR8_LIT("showChildren"), 0, 0, 0},
};

static MetaStruct metaStructs[] = 
{
  {STR8_LIT("UIElement"), membersOf_UIElement},
  {STR8_LIT("LoadedSound"), membersOf_LoadedSound},
  {STR8_LIT("GrainBufferViewNode"), membersOf_GrainBufferViewNode},
  {STR8_LIT("GrainViewNode"), membersOf_GrainViewNode},
  {STR8_LIT("Grain"), membersOf_Grain},
  {STR8_LIT("UILayout"), membersOf_UILayout},
  {STR8_LIT("UIHashKey"), membersOf_UIHashKey},
  {STR8_LIT("PlayingGrain"), membersOf_PlayingGrain},
  {STR8_LIT("GrainPackfileTag"), membersOf_GrainPackfileTag},
  {STR8_LIT("PlayingSound"), membersOf_PlayingSound},
  {STR8_LIT("PluginFloatParameter"), membersOf_PluginFloatParameter},
  {STR8_LIT("PluginBooleanParameter"), membersOf_PluginBooleanParameter},
  {STR8_LIT("String8"), membersOf_String8},
  {STR8_LIT("Arena"), membersOf_Arena},
  {STR8_LIT("LoadedFont"), membersOf_LoadedFont},
  {STR8_LIT("LoadedBitmap"), membersOf_LoadedBitmap},
  {STR8_LIT("Rect2"), membersOf_Rect2},
  {STR8_LIT("GrainStateView"), membersOf_GrainStateView},
  {STR8_LIT("GrainManager"), membersOf_GrainManager},
  {STR8_LIT("AudioRingBuffer"), membersOf_AudioRingBuffer},
  {STR8_LIT("UIPanel"), membersOf_UIPanel},
  {STR8_LIT("UIContext"), membersOf_UIContext},
  {STR8_LIT("FileGrainState"), membersOf_FileGrainState},
  {STR8_LIT("LoadedGrainPackfile"), membersOf_LoadedGrainPackfile},
  {STR8_LIT("PluginState"), membersOf_PluginState},
};

