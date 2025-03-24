// functionality present in both the plugin and its host

#include "context.h"
#include "types.h"
#include "utils.h"
#include "arena.h"
#include "strings.h"
#include "simd_intrinsics.h"
#include "profile.h"
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

// atomics

#define ATOMIC_LOAD(name) u32 (name)(volatile u32 *src)
typedef ATOMIC_LOAD(AtomicLoad);

#define ATOMIC_STORE(name) u32 (name)(volatile u32 *dest, u32 value)
typedef ATOMIC_STORE(AtomicStore);

#define ATOMIC_ADD(name) u32 (name)(volatile u32 *addend, u32 value)
typedef ATOMIC_ADD(AtomicAdd);

#define ATOMIC_COMPARE_AND_SWAP(name) u32 (name)(volatile u32 *value, u32 oldval, u32 newval)
typedef ATOMIC_COMPARE_AND_SWAP(AtomicCompareAndSwap);

#define ATOMIC_COMPARE_AND_SWAP_POINTERS(name) void *(name)(volatile void **value, void *oldval, void *newval)
typedef ATOMIC_COMPARE_AND_SWAP_POINTERS(AtomicCompareAndSwapPointers);

struct PlatformAPI
{
  PlatformReadEntireFile *readEntireFile;  
  PlatformFreeFileMemory *freeFileMemory;
  PlatformWriteEntireFile *writeEntireFile;

  PlatformRunModel *runModel;

  PlatformGetCurrentTimestamp *getCurrentTimestamp;

  AtomicLoad *atomicLoad;
  AtomicStore *atomicStore;
  AtomicAdd *atomicAdd;
  AtomicCompareAndSwap *atomicCompareAndSwap;
  AtomicCompareAndSwapPointers *atomicCompareAndSwapPointers;
};

struct PluginLogger
{
  Arena *logArena;
  String8List log;
  volatile u32 mutex;
};

extern PlatformAPI globalPlatform;
extern PluginLogger *globalLogger;

struct PluginMemory
{
  void *memory;
  
  u64 osTimerFreq;
  
  PlatformAPI platformAPI;

  PluginLogger *logger;
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

  AudioFormat outputFormat;
  void *outputBuffer[2];
  u32 outputSampleRate;
  u32 outputChannels;
  u32 outputStride;

  AudioFormat inputFormat;
  const void *inputBuffer[2];
  u32 inputSampleRate;
  u32 inputChannels;
  u32 inputStride;

  u32 framesToWrite;

  u32 midiMessageCount;
  u8 *midiBuffer;
};

#define AUDIO_PROCESS(name) void (name)(PluginMemory *memory, PluginAudioBuffer *audioBuffer)
typedef AUDIO_PROCESS(AudioProcess);

struct PluginState;
#define INITIALIZE_PLUGIN_STATE(name) PluginState *(name)(PluginMemory *memoryBlock)
typedef INITIALIZE_PLUGIN_STATE(InitializePluginState);
