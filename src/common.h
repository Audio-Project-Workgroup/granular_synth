// functionality present in both the plugin and its host
#define INTROSPECT

#define FILE_GRAIN_CHANNELS (2)
#define FILE_GRAIN_LENGTH (2400)
#define FILE_GRAIN_CHANNEL_LENGTH (FILE_GRAIN_LENGTH*FILE_GRAIN_CHANNELS)
#define FILE_TAG_LENGTH (200)

#include "context.h"
#include "types.h"
#include "utils.h"
#include "common.api.h"
#include "arena.h"
#include "strings.h"
#include "buffer.h"
#include "profile.h"
#include "math.h"
#include "render.h"
#include "parameters_common.h"

#include "meta.h"

#if BUILD_LOGGING
struct PluginLogger
{
  Arena *logArena;  
  String8List log;
  usz maxCapacity;

  volatile u32 mutex;
};

extern PluginLogger *globalLogger;
#endif

enum PluginHost
{
  PluginHost_executable,
  PluginHost_daw,
  PluginHost_web,
};

struct PluginMemory
{
  void *pluginHandle;

  u64 osTimerFreq;
  PluginHost host;

  String8 outputDeviceNames[32];
  u32 outputDeviceCount;
  u32 selectedOutputDeviceIndex;

  String8 inputDeviceNames[32];
  u32 inputDeviceCount;
  u32 selectedInputDeviceIndex;
  
  PlatformAPI platformAPI;

#if BUILD_LOGGING
  PluginLogger *logger;
#endif
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
  MouseButton_middle,
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
  KeyboardButton_esc,
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

enum AudioFormat
{
  AudioFormat_none,
  
  AudioFormat_s16,
  AudioFormat_r32,
};

struct MidiHeader
{
  u8 messageLength;
  u64 timestamp;
};

struct PluginAudioBuffer
{
  u64 millisecondsElapsedSinceLastCall;
  
  AudioFormat outputFormat;
  u32 outputBufferCapacity;
  const void *outputBuffer[2];
  u32 outputSampleRate;
  u32 outputChannels;
  u32 outputStride;

  AudioFormat inputFormat;
  u32 inputBufferCapacity;
  const void *inputBuffer[2];
  u32 inputSampleRate;
  u32 inputChannels;
  u32 inputStride;

  u32 framesToWrite;

  u32 midiMessageCount;
  u8 *midiBuffer;

  ParameterValueQueueEntry parameterValueQueueEntries[512];
  u32 parameterValueQueueReadIndex;
  u32 parameterValueQueueWriteIndex;
  volatile u32 queuedCount;
};

#include "plugin.api.h"
