static MemberDefinition membersOf_PluginState[] = 
{
  {STR8_LIT("PluginFloatParameter"), STR8_LIT("parameters"), 0, PluginParameter_count},
  {STR8_LIT("u64"), STR8_LIT("osTimerFreq"), 0, 0},
  {STR8_LIT("Arena"), STR8_LIT("grainArena"), 0, 0},
  {STR8_LIT("Arena"), STR8_LIT("permanentArena"), 0, 0},
  {STR8_LIT("Arena"), STR8_LIT("frameArena"), 0, 0},
  {STR8_LIT("Arena"), STR8_LIT("framePermanentArena"), 0, 0},
  {STR8_LIT("Arena"), STR8_LIT("loadArena"), 0, 0},
  {STR8_LIT("PluginHost"), STR8_LIT("pluginHost"), 0, 0},
  {STR8_LIT("PluginMode"), STR8_LIT("pluginMode"), 0, 0},
  {STR8_LIT("String8"), STR8_LIT("outputDeviceNames"), 0, 32},
  {STR8_LIT("u32"), STR8_LIT("outputDeviceCount"), 0, 0},
  {STR8_LIT("u32"), STR8_LIT("selectedOutputDeviceIndex"), 0, 0},
  {STR8_LIT("String8"), STR8_LIT("inputDeviceNames"), 0, 32},
  {STR8_LIT("u32"), STR8_LIT("inputDeviceCount"), 0, 0},
  {STR8_LIT("u32"), STR8_LIT("selectedInputDeviceIndex"), 0, 0},
  {STR8_LIT("LoadedGrainPackfile"), STR8_LIT("loadedGrainPackfile"), 0, 0},
  {STR8_LIT("FileGrainState"), STR8_LIT("silo"), 0, 0},
  {STR8_LIT("r64"), STR8_LIT("phasor"), 0, 0},
  {STR8_LIT("r32"), STR8_LIT("freq"), 0, 0},
  {STR8_LIT("PluginBooleanParameter"), STR8_LIT("soundIsPlaying"), 0, 0},
  {STR8_LIT("PlayingSound"), STR8_LIT("loadedSound"), 0, 0},
  {STR8_LIT("LoadedBitmap"), STR8_LIT("testBitmap"), 0, 0},
  {STR8_LIT("LoadedFont"), STR8_LIT("testFont"), 0, 0},
  {STR8_LIT("UIContext"), STR8_LIT("uiContext"), 0, 0},
  {STR8_LIT("UIPanel"), STR8_LIT("rootPanel"), 1, 0},
  {STR8_LIT("UIPanel"), STR8_LIT("menuPanel"), 1, 0},
  {STR8_LIT("GrainManager"), STR8_LIT("grainManager"), 0, 0},
  {STR8_LIT("AudioRingBuffer"), STR8_LIT("grainBuffer"), 0, 0},
  {STR8_LIT("GrainStateView"), STR8_LIT("grainStateView"), 0, 0},
  {STR8_LIT("u32"), STR8_LIT("initializationMutex"), 0, 0},
  {STR8_LIT("bool"), STR8_LIT("initialized"), 0, 0},
};

static MetaStruct metaStructs[] = 
{
  {STR8_LIT("PluginState"), membersOf_PluginState},
};

