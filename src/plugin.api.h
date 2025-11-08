// NOTE: functions the host calls and the plugin impelements

#define PLUGIN_API_XLIST\
  X(RenderNewFrame, void, (PluginMemory *memory, PluginInput *input, RenderCommands *renderCommands))\
  X(AudioProcess, void, (PluginMemory *memory, PluginAudioBuffer *audioBuffer))\
  X(InitializePluginState, PluginState*, (PluginMemory *memoryBlock))\

struct PluginState;
#define X(name, ret, args) typedef ret GS_##name args;
PLUGIN_API_XLIST
#undef X

struct PluginAPI
{
#define X(name, ret, args) GS_##name *gs##name;
  PLUGIN_API_XLIST
#undef X
};

#if defined(PLUGIN_DYNAMIC) && defined(HOST_LAYER)
#define X(name, ret, args) static GS_##name *gs##name = 0;
PLUGIN_API_XLIST
#undef X
#endif
