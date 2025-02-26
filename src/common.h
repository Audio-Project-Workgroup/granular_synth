// functionality present in both the plugin and its host

#include "context.h"
#include "types.h"
#include "utils.h"
#include "arena.h"
#include "math.h"
#include "render.h"

//
// functionality the host provides to the plugin
// 

// model

#define PLATFORM_RUN_MODEL(name) void *(name)(r32 *inputData, s64 inputLength)
typedef PLATFORM_RUN_MODEL(PlatformRunModel);

// timing

#define PLATFORM_GET_CURRENT_TIMESTAMP(name) u64 (name)(void)
typedef PLATFORM_GET_CURRENT_TIMESTAMP(PlatformGetCurrentTimestamp);

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

#define PLATFORM_WRITE_ENTIRE_FILE(name) void (name)(char *filename, void *fileMemory, usz fileSize)
typedef PLATFORM_WRITE_ENTIRE_FILE(PlatformWriteEntireFile);

struct PlatformAPI
{
  PlatformReadEntireFile *readEntireFile;  
  PlatformFreeFileMemory *freeFileMemory;
  PlatformWriteEntireFile *writeEntireFile;

  PlatformRunModel *runModel;

  PlatformGetCurrentTimestamp *getCurrentTimestamp;
};

extern PlatformAPI globalPlatform;

struct PluginMemory
{
  void *memory;
  u64 osTimerFreq;
  PlatformAPI platformAPI;
};

// input

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

inline bool
wasReleased(ButtonState button)
{
  bool result = ((button.halfTransitionCount > 1) ||
		 ((button.halfTransitionCount == 1) && !button.endedDown));

  return(result);
}

enum MouseButton
{
  MouseButton_left,
  MouseButton_right,

  MouseButton_COUNT,
};

struct MouseState
{
  v2 position;
  
  ButtonState buttons[MouseButton_COUNT];
  
  int scrollDelta;
};

enum KeyboardButton
{
  KeyboardButton_a,
  KeyboardButton_b,
  KeyboardButton_c,
  KeyboardButton_d,
  KeyboardButton_e,
  KeyboardButton_f,
  KeyboardButton_g,
  KeyboardButton_h,
  KeyboardButton_i,
  KeyboardButton_j,
  KeyboardButton_k,
  KeyboardButton_l,
  KeyboardButton_m,
  KeyboardButton_n,
  KeyboardButton_o,
  KeyboardButton_p,
  KeyboardButton_q,
  KeyboardButton_r,
  KeyboardButton_s,
  KeyboardButton_t,
  KeyboardButton_u,
  KeyboardButton_v,
  KeyboardButton_w,
  KeyboardButton_x,
  KeyboardButton_y,
  KeyboardButton_z,
  KeyboardButton_tab,  
  KeyboardButton_backspace,
  KeyboardButton_minus,
  KeyboardButton_equal,
  KeyboardButton_enter,
  KeyboardButton_COUNT,
};

enum KeyboardModifier
{
  KeyboardModifier_shift,
  KeyboardModifier_control,
  KeyboardModifier_alt,
  KeyboardModifier_COUNT,
};

struct KeyboardState
{
  ButtonState keys[KeyboardButton_COUNT];
  ButtonState modifiers[KeyboardModifier_COUNT];
};

struct PluginInput
{
  u64 frameMillisecondsElapsed;

  MouseState mouseState;
  KeyboardState keyboardState;
};

struct PluginFileOperations
{
  char *filenameToOpen;
  void *destMemory;
  bool operationCompleted;
};

//
// functionality the plugin provides to the host
//

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
  u64 millisecondsElapsedSinceLastCall;

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
