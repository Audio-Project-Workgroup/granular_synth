// functionality present in both the plugin and its host

#include "types.h"
#include "utils.h"
#include "math.h"
#include "arena.h"
#include "render.h"

// files

struct ReadFileResult
{
  usz contentsSize;
  u8 *contents;
};

#define PLATFORM_READ_ENTIRE_FILE(name) ReadFileResult (name)(char *filename, Arena *allocator)
typedef PLATFORM_READ_ENTIRE_FILE(PlatformReadEntireFile);

#define PLATFORM_FREE_FILE_MEMORY(name) void (name)(ReadFileResult file, Arena *allocator)
typedef PLATFORM_FREE_FILE_MEMORY(PlatformFreeFileMemory);

struct PlatformAPI
{
  PlatformReadEntireFile *readEntireFile;
  PlatformFreeFileMemory *freeFileMemory;
};

extern PlatformAPI globalPlatform;

// input

struct PluginMemory
{
  void *memory;
  PlatformAPI platformAPI;
};

struct ButtonState
{
  bool endedDown;
  u32 halfTransitionCount;
};

static inline bool
wasPressed(ButtonState button)
{
  bool result = ((button.halfTransitionCount > 1) ||
		 ((button.halfTransitionCount == 1) && button.endedDown));

  return(result);
}

static inline bool
isDown(ButtonState button)
{
  bool result = button.endedDown;

  return(result);
}

enum MouseButton
{
  MouseButton_left,
  MouseButton_right,

  MouseButton_count,
};

struct PluginInput
{
  r32 mousePositionX;
  r32 mousePositionY;
  
  ButtonState mouseButtons[MouseButton_count];
  
  int mouseScrollDelta;
};

struct PluginFileOperations
{
  char *filenameToOpen;
  void *destMemory;
  bool operationCompleted;
};

// video

#define RENDER_NEW_FRAME(name) void (name)(PluginMemory *memory, PluginInput *input, RenderCommands *renderCommands)
typedef RENDER_NEW_FRAME(RenderNewFrame);

// audio

enum AudioFormat
{
  AudioFormat_none,
  
  AudioFormat_s16,
  AudioFormat_r32,
};

struct MidiHeader
{
  u8 messageLength;
};

struct PluginAudioBuffer
{
  AudioFormat format;
  void *buffer;

  u32 sampleRate;
  u32 channels;

  u32 framesToWrite;

  u32 midiMessageCount;
  u8 *midiBuffer;
};

#define AUDIO_PROCESS(name) void (name)(PluginMemory *memory, PluginAudioBuffer *audioBuffer)
typedef AUDIO_PROCESS(AudioProcess);
