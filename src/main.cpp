// a simple host application that opens a window with an OpenGL context, allocates some memory,
// registers a callback with an audio playback device, and calls into a dynamic library to render
// audio and video.

/* TODO:
   - audio output device selection (tentatively done)
     - it would be a good idea to set the sample format and channel count of the
       plugin audio buffer structure according to a format that we know the
       devices we are connecting to support, and not just guessing that they
       will support signed-16-bit stereo (and also update the format when we
       connect to a different device!)
   -keyboard input (tentatively done)
   - midi device input
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOST_LAYER
#define PLUGIN_DYNAMIC
#include "common.h"
static String8 executablePath;
static String8 basePath;
#include "platform.h"

// TODO: it would be cool to not use the crt implementations
#include <math.h>

#include <glad/glad.h>
// #if OS_MAC || OS_LINUX
// #include <GL/glew.h>
// #endif
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

#if BUILD_DEBUG
#  define GL_PRINT_ERROR(msg, ...) do {         \
    GLenum err = glGetError();                  \
    if(err) {                                   \
      fprintf(stderr, msg, err, ##__VA_ARGS__); \
    }                                           \
  } while(0)
// #  define GL_CATCH_ERROR() do { GLenum err = glGetError(); if(err != GL_NO_ERROR) ASSERT(0); } while(0)
#else
#  define GL_PRINT_ERROR(msg, ...)
// #  define GL_CATCH_ERROR()
#endif

#include "miniaudio.h"

#include <glad/src/glad.c>

#include "render.cpp"
//#include "onnx.cpp"

static ma_device_info *maPlaybackInfos;
static u32 maPlaybackCount;
static u32 maPlaybackIndex;
static ma_device_info *maCaptureInfos;
static u32 maCaptureCount;
static u32 maCaptureIndex;

static PluginInput *newInput;
static GLFWcursor *standardCursor;
static GLFWcursor *hResizeCursor;
static GLFWcursor *vResizeCursor;
static GLFWcursor *handCursor;
static GLFWcursor *textCursor;

static inline void
glfwSetCursorState(GLFWwindow *window, RenderCursorState cursorState)
{
  switch(cursorState)
    {
    case CursorState_default:
      {
        glfwSetCursor(window, standardCursor);
      } break;
    case CursorState_hArrow:
      {
        glfwSetCursor(window, hResizeCursor);
      } break;
    case CursorState_vArrow:
      {
        glfwSetCursor(window, vResizeCursor);
      } break;
    case CursorState_hand:
      {
        glfwSetCursor(window, handCursor);
      } break;
    case CursorState_text:
      {
        glfwSetCursor(window, textCursor);
      } break;
    }
}

static inline void
glfwProcessButtonPress(ButtonState *newState, bool pressed)
{
  newState->endedDown = pressed;
  ++newState->halfTransitionCount;
}

// TODO: handle holding down keys!

static void
glfwKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  UNUSED(window);
  UNUSED(scancode);
  UNUSED(mods);
  // if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
  //   {
  //     glfwSetWindowShouldClose(window, GLFW_TRUE);
  //   }
  if(key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
    {
      glfwProcessButtonPress(&newInput->keyboardState.modifiers[KeyboardModifier_shift],
                             action == GLFW_PRESS || action == GLFW_REPEAT);
    }
  else if(key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL)
    {
      glfwProcessButtonPress(&newInput->keyboardState.modifiers[KeyboardModifier_control],
                             action == GLFW_PRESS || action == GLFW_REPEAT);
    }
  else if(key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT)
    {
      glfwProcessButtonPress(&newInput->keyboardState.modifiers[KeyboardModifier_alt],
                             action == GLFW_PRESS || action == GLFW_REPEAT);
    }
  else if(key == GLFW_KEY_ESCAPE)
    {
      glfwProcessButtonPress(&newInput->keyboardState.keys[KeyboardButton_esc],
                             action == GLFW_PRESS || action == GLFW_REPEAT);
    }
  else if(key == GLFW_KEY_TAB)
    {
      glfwProcessButtonPress(&newInput->keyboardState.keys[KeyboardButton_tab],
                             action == GLFW_PRESS || action == GLFW_REPEAT);
    }
  else if(key == GLFW_KEY_BACKSPACE)
    {
      glfwProcessButtonPress(&newInput->keyboardState.keys[KeyboardButton_backspace],
                             action == GLFW_PRESS || action == GLFW_REPEAT);
    }
  else if(key == GLFW_KEY_MINUS)
    {
      glfwProcessButtonPress(&newInput->keyboardState.keys[KeyboardButton_minus],
                             action == GLFW_PRESS || action == GLFW_REPEAT);
    }
  else if(key == GLFW_KEY_EQUAL)
    {
      glfwProcessButtonPress(&newInput->keyboardState.keys[KeyboardButton_equal],
                             action == GLFW_PRESS || action == GLFW_REPEAT);
    }
  else if(key == GLFW_KEY_ENTER)
    {
      glfwProcessButtonPress(&newInput->keyboardState.keys[KeyboardButton_enter],
                             action == GLFW_PRESS || action == GLFW_REPEAT);
    }
}

static void
glfwMouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
  UNUSED(window);
  UNUSED(mods);

  if(button == GLFW_MOUSE_BUTTON_LEFT)
    {
      glfwProcessButtonPress(&newInput->mouseState.buttons[MouseButton_left],
                             action == GLFW_PRESS);
    }
  else if(button == GLFW_MOUSE_BUTTON_RIGHT)
    {
      glfwProcessButtonPress(&newInput->mouseState.buttons[MouseButton_right],
                             action == GLFW_PRESS);
    }
}

#define SR 48000
#define CHANNELS 2

struct MACallbackData
{
  PluginMemory *memory;
  PluginAudioBuffer *audioBuffer;
};

void
maDataCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
  MACallbackData *data = (MACallbackData*)pDevice->pUserData;
  if(data)
  {
    PluginMemory *memory = data->memory;
    PluginAudioBuffer *audioBuffer = data->audioBuffer;

    audioBuffer->framesToWrite = frameCount;
    audioBuffer->outputBuffer[0] = pOutput;
    audioBuffer->outputBuffer[1] = (u8*)pOutput + audioBuffer->outputStride/2;
    audioBuffer->inputBuffer[0] = pInput;
    audioBuffer->inputBuffer[1] = pInput;

    gsAudioProcess(memory, audioBuffer);
  }
}

#pragma pack(push, 1)
struct BitmapHeader
{
  u16 signature;
  u32 fileSize;
  u32 reserved;
  u32 dataOffset;

  u32 headerSize;
  u32 width;
  u32 height;
  u16 planes;
  u16 bitsPerPixel;
  u32 compression;
  u32 imageSize;
  u32 xPixelsPerM;
  u32 yPixelsPerM;
  u32 colorsUsed;
  u32 colorsImportant;

  u32 redMask;
  u32 greenMask;
  u32 blueMask;
};
#pragma pack(pop)

// #define FOURCC(str) (((u32)((str)[0]) << 0) | ((u32)((str)[1]) << 8) | ((u32)((str)[2]) << 16) | ((u32)((str)[3]) << 24))

static LoadedBitmap
loadBitmap(Arena *destAllocator, String8 filename, b32 flip = 1)
{
  LoadedBitmap result = {};
  LoadedBitmap temp = {};

  TemporaryMemory scratch = arenaGetScratch(&destAllocator, 1);

  Buffer readResult = platformReadEntireFile((char *)filename.str, scratch.arena);
  if(readResult.contents)
    {
      BitmapHeader *header = (BitmapHeader *)readResult.contents;
      ASSERT(header->signature == (u16)FOURCC("BM  "));
      ASSERT(header->fileSize == readResult.size - 1);

      result.width = temp.width = header->width;
      result.height = temp.height = header->height;
      temp.pixels = (u32 *)(readResult.contents + header->dataOffset);
      result.pixels = arenaPushArray(destAllocator, result.width*result.height, u32);

      u16 bytesPerPixel = header->bitsPerPixel / 8;
      ASSERT(bytesPerPixel == 4);
      u32 compression = header->compression;
      (void)compression;
      ASSERT(header->imageSize == bytesPerPixel*result.width*result.height);

      u32 redMask = header->redMask;
      u32 greenMask = header->greenMask;
      u32 blueMask = header->blueMask;
      u32 alphaMask = ~(redMask | greenMask | blueMask);

      u32 alphaShiftDown = alphaMask ? lowestOrderBit(alphaMask) : 24;
      u32 redShiftDown = redMask ? lowestOrderBit(redMask) : 16;
      u32 greenShiftDown = greenMask ? lowestOrderBit(greenMask) : 8;
      u32 blueShiftDown = blueMask ? lowestOrderBit(blueMask) : 0;

      temp.stride = result.stride = result.width*bytesPerPixel;

      u8 *srcRow = flip ? (u8 *)temp.pixels + temp.stride*(temp.height - 1) : (u8 *)temp.pixels;
      u8 *destRow = (u8 *)result.pixels;
      s32 srcRowAdvance = flip ? -(s32)result.stride : (s32)result.stride;
      s32 destRowAdvance = (s32)result.stride;
      for(u32 y = 0; y < result.height; ++y)
        {
          u32 *srcPixel = (u32 *)srcRow;
          u32 *destPixel = (u32 *)destRow;

          for(u32 x = 0; x < result.width; ++x)
            {
              u32 c = *srcPixel++;

              u32 red = (c & redMask) >> redShiftDown;
              u32 green = (c & greenMask) >> greenShiftDown;
              u32 blue = (c & blueMask) >> blueShiftDown;
              u32 alpha = (c & alphaMask) >> alphaShiftDown;

              // NOTE: format is rgba
              *destPixel++ = ((alpha << 24) |
                              (blue << 16) |
                              (green << 8) |
                              (red << 0));
            }

          srcRow += srcRowAdvance;//-result.stride;
          destRow += destRowAdvance;//result.stride;
        }
    }

  arenaReleaseScratch(scratch);

  return(result);
}

static inline LoadedBitmap
makeBitmap(Arena *allocator, s32 width, s32 height, u32 color)
{
  LoadedBitmap result = {};
  result.width = width;
  result.height = height;
  result.stride = width*sizeof(u32);
  result.pixels = arenaPushArray(allocator, width*height, u32);

  u32 *dest = result.pixels;
  for(s32 y = 0; y < height; ++y)
    {
      for(s32 x = 0; x < width; ++x)
        {
          *dest++ = color;
        }
    }

  return(result);
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
static LoadedBitmap
loadPNG(String8 path, b32 flip = 1)
{
  stbi_set_flip_vertically_on_load(flip);

  int width, height, nChannels;
  u8 *data = stbi_load((char*)path.str, &width, &height, &nChannels, 4);
  ASSERT(nChannels == 4);

  LoadedBitmap result = {};
  result.width = width;
  result.height = height;
  result.stride = width * nChannels;
  result.pixels = (u32*)data;
  return(result);
}

void
gsCopyMemory(void *dest, void *src, usz size)
{
  memcpy(dest, src, size);
}

void
gsSetMemory(void *dest, int value, usz size)
{
  memset(dest, value, size);
}

struct HostMemoryState
{
  usz arenaHeaderPoolSize;
  usz arenaHeaderPoolUsed;
  void *arenaHeaderPool;

  usz arenaDataPoolSize;
  usz arenaDataPoolUsed;
  void *arenaDataPool;

  Arena *firstFree;
  Arena *lastFree;
};

//static HostMemoryState *hostMemoryState = 0;

#define ARENA_MIN_ALLOCATION_SIZE KILOBYTES(64)
//#define ARENA_MIN_ALLOCATION_SIZE MEGABYTES(1)

Arena*
gsArenaAcquire(usz size)
{
  usz allocSize = MAX(size, ARENA_MIN_ALLOCATION_SIZE);

  void *base = platformAllocateMemory(allocSize);
  Arena *result = (Arena*)base;
  result->current = result;
  result->prev = 0;
  result->base = 0;
  result->capacity = allocSize;
  result->pos = ARENA_HEADER_SIZE;

  return(result);
}

void
gsArenaDiscard(Arena *arena)
{
  platformFreeMemory(arena, arena->capacity);
}

static r32
gsRand(RangeR32 range)
{
  int randVal = rand();
  r32 rand01 = (r32)randVal / (r32)RAND_MAX;
  r32 result = mapToRange(rand01, range);
  return(result);
}

static r32
gsAbs(r32 num)
{
  return(fabsf(num));
}

static r32
gsSqrt(r32 num)
{
  return(sqrtf(num));
}

static r32
gsSin(r32 num)
{
  return(sinf(num));
}

static r32
gsCos(r32 num)
{
  return(cosf(num));
}

static r32
gsPow(r32 base, r32 exp)
{
  return(powf(base, exp));
}

int
main(int argc, char **argv)
{
  UNUSED(argc);
  UNUSED(argv);

#if BUILD_DEBUG
  printf("plugin path: %s\n", PLUGIN_PATH);
#endif
  int result = 0;

  TemporaryMemory scratch = arenaGetScratch(0, 0);

  executablePath = platformGetPathToModule(0, (void *)main, scratch.arena);
  basePath = stringGetParentPath(executablePath);
#if BUILD_DEBUG
  fprintf(stderr, "executable path: %.*s\n", (int)executablePath.size, executablePath.str);
  fprintf(stderr, "base path: %.*s\n", (int)basePath.size, basePath.str);
#endif

#if BUILD_DEBUG
  String8 glfwPath = platformGetPathToModule(0, (void *)glfwCreateWindow, scratch.arena);
  String8 openGLPath = platformGetPathToModule(0, (void *)glTexImage2D, scratch.arena);
  printf("glfw module source: %.*s\n", (int)glfwPath.size, glfwPath.str);
  printf("opengl module source: %.*s\n", (int)openGLPath.size, openGLPath.str);
#endif

  if(glfwInit())
  {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    GLFWwindow *window = glfwCreateWindow(640, 480, "granade", NULL, NULL);
    if(window)
    {
      LoadedBitmap iconBitmap =
        //loadBitmap(scratch.arena, STR8_LIT("../data/BMP/DENSITYPOMEGRANATE_BUTTON.bmp"));
        loadPNG(STR8_LIT("../data/PNG/DENSITYPOMEGRANATE_BUTTON.png"), 0);

      GLFWimage appIcon[1];
      appIcon[0].width = iconBitmap.width;
      appIcon[0].height = iconBitmap.height;
      appIcon[0].pixels = (u8 *)iconBitmap.pixels;
      glfwSetWindowIcon(window, 1, appIcon);

      standardCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
      hResizeCursor = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
      vResizeCursor = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
      handCursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
      textCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);

      // input setup

      PluginInput pluginInput[2] = {};
      newInput = &pluginInput[0];
      PluginInput *oldInput = &pluginInput[1];

      glfwSetKeyCallback(window, glfwKeyCallback);
      glfwSetMouseButtonCallback(window, glfwMouseButtonCallback);
      glfwMakeContextCurrent(window);

      gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

      glfwSwapInterval(1);

      // memory/grahics setup

      PluginMemory pluginMemory = {};
      pluginMemory.osTimerFreq = getOSTimerFreq();
      pluginMemory.host = PluginHost_executable;

      pluginMemory.platformAPI.gsReadEntireFile  = platformReadEntireFile;
      pluginMemory.platformAPI.gsFreeFileMemory  = platformFreeFileMemory;
      pluginMemory.platformAPI.gsWriteEntireFile = platformWriteEntireFile;
      pluginMemory.platformAPI.gsGetPathToModule = platformGetPathToModule;

      //pluginMemory.platformAPI.runModel = platformRunModel;

      pluginMemory.platformAPI.gsGetCurrentTimestamp = platformGetCurrentTimestamp;

      pluginMemory.platformAPI.gsRand = gsRand;
      pluginMemory.platformAPI.gsAbs  = gsAbs;
      pluginMemory.platformAPI.gsSqrt = gsSqrt;
      pluginMemory.platformAPI.gsSin  = gsSin;
      pluginMemory.platformAPI.gsCos  = gsCos;
      pluginMemory.platformAPI.gsPow  = gsPow;

      pluginMemory.platformAPI.gsAllocateMemory = platformAllocateMemory;
      pluginMemory.platformAPI.gsFreeMemory     = platformFreeMemory;
      pluginMemory.platformAPI.gsCopyMemory     = gsCopyMemory;
      pluginMemory.platformAPI.gsSetMemory      = gsSetMemory;
      pluginMemory.platformAPI.gsArenaAcquire   = gsArenaAcquire;
      pluginMemory.platformAPI.gsArenaDiscard   = gsArenaDiscard;

      pluginMemory.platformAPI.gsAtomicLoad                   = atomicLoad;
      pluginMemory.platformAPI.gsAtomicStore                  = atomicStore;
      pluginMemory.platformAPI.gsAtomicAdd                    = atomicAdd;
      pluginMemory.platformAPI.gsAtomicCompareAndSwap         = atomicCompareAndSwap;
      pluginMemory.platformAPI.gsAtomicCompareAndSwapPointers = atomicCompareAndSwapPointers;

#if BUILD_LOGGING
      Arena *loggerArena = gsArenaAcquire(0);

      PluginLogger logger = {};
      logger.logArena = loggerArena;
      logger.maxCapacity = loggerArena->capacity/2;

      pluginMemory.logger = &logger;
#endif

      // TODO: don't load this at runtime. Try baking it into some object file
      //LoadedBitmap atlas = loadBitmap(scratch.arena, STR8_LIT("../data/test_atlas.bmp"), 0);
      LoadedBitmap atlas = loadPNG(STR8_LIT("../data/test_atlas.png"), 1);
      RenderCommands *commands = allocRenderCommands();
      commands->atlas = &atlas;

      // plugin setup

#if OS_MAC || OS_LINUX
      String8 pluginPath = concatenateStrings(scratch.arena, basePath, STR8_LIT("/" PLUGIN_PATH));
#else
      String8 pluginPath = STR8_LIT(PLUGIN_PATH);
#endif

#if defined(PLUGIN_DYNAMIC)
      PluginCode plugin = loadPluginCode((char *)pluginPath.str);
      pluginMemory.pluginHandle = plugin.pluginCode;
#define X(name, ret, args) gs##name = plugin.pluginAPI.gs##name;
      PLUGIN_API_XLIST
#undef X
#endif

      // model setup

      // const ORTCHAR_T *modelPath = ORT_TSTR("../data/test_model.onnx");
      // onnxState = onnxInitializeState(modelPath);

      // if(onnxState.session && onnxState.env)
      {
        // audio setup

        PluginAudioBuffer audioBuffer = {};
        audioBuffer.outputFormat = AudioFormat_s16;
        audioBuffer.outputSampleRate = SR;
        audioBuffer.outputChannels = CHANNELS;
        audioBuffer.outputStride = 2*sizeof(s16);
        audioBuffer.inputFormat = AudioFormat_s16;
        audioBuffer.inputSampleRate = SR;
        audioBuffer.inputChannels = CHANNELS;
        audioBuffer.inputStride = 2*sizeof(s16);
        audioBuffer.midiMessageCount = 0; // TODO: send midi messages to the plugin
        audioBuffer.midiBuffer = (u8 *)calloc(KILOBYTES(1), 1);

        // TODO: it would be better to default to using jack over pulseaudio on
        // linux. However, miniaudio's jack implementation does not provide the
        // same level of experience as pulseaudio, particularly when it comes to
        // device enumeration (and if the system uses pipewire, it will not work
        // at all). Miniaudio could be modified to fix these problems.
        ma_backend maBackends[] = {
          ma_backend_wasapi,
          ma_backend_dsound,
          ma_backend_winmm,
          ma_backend_coreaudio,
          ma_backend_pulseaudio,
          ma_backend_alsa,
          ma_backend_jack,
        };
        u32 maBackendCount = sizeof(maBackends)/sizeof(maBackends[0]);

        ma_context maContext;
        if(ma_context_init(maBackends, maBackendCount, NULL, &maContext) == MA_SUCCESS)
        {
          ma_device_info maDefaultPlaybackDeviceInfo;
          ma_device_info maDefaultCaptureDeviceInfo;

          if(ma_context_get_devices(&maContext, &maPlaybackInfos, &maPlaybackCount,
                                    &maCaptureInfos, &maCaptureCount) == MA_SUCCESS)
          {
            // NOTE: enumerate devices, get default device indices
            if(ma_context_get_device_info(&maContext, ma_device_type_playback, NULL,
                                          &maDefaultPlaybackDeviceInfo) == MA_SUCCESS)
            {
              for(u32 iPlayback = 0; iPlayback < maPlaybackCount; ++iPlayback)
              {
                String8 deviceStr = STR8_CSTR(maPlaybackInfos[iPlayback].name);
                ASSERT(pluginMemory.outputDeviceCount < ARRAY_COUNT(pluginMemory.outputDeviceNames));
                pluginMemory.outputDeviceNames[pluginMemory.outputDeviceCount] = deviceStr;
                if(stringsAreEqual(deviceStr, STR8_CSTR(maDefaultPlaybackDeviceInfo.name)))
                {
                  maPlaybackIndex = iPlayback;
                  pluginMemory.selectedOutputDeviceIndex = pluginMemory.outputDeviceCount;
                }

                ++pluginMemory.outputDeviceCount;
              }
            }

            if(ma_context_get_device_info(&maContext, ma_device_type_capture, NULL,
                                          &maDefaultCaptureDeviceInfo) == MA_SUCCESS)
            {
              for(u32 iCapture = 0; iCapture < maCaptureCount; ++iCapture)
              {
                String8 deviceStr = STR8_CSTR(maCaptureInfos[iCapture].name);
                ASSERT(pluginMemory.inputDeviceCount < ARRAY_COUNT(pluginMemory.inputDeviceNames));
                pluginMemory.inputDeviceNames[pluginMemory.inputDeviceCount] = deviceStr;
                if(stringsAreEqual(deviceStr, STR8_CSTR(maDefaultCaptureDeviceInfo.name)))
                {
                  maCaptureIndex = iCapture;
                  pluginMemory.selectedInputDeviceIndex = pluginMemory.inputDeviceCount;
                }

                ++pluginMemory.inputDeviceCount;
              }
            }
            else
            {
              audioBuffer.inputFormat = AudioFormat_none;
              // audioBuffer.inputSampleRate = 0;
              // audioBuffer.inputChannels = 0;
              // audioBuffer.inputStride = 0;
            }

            MACallbackData maCallbackData = {};
            maCallbackData.memory = &pluginMemory;
            maCallbackData.audioBuffer = &audioBuffer;
#if 1
            ma_device_config maConfig;
            if(maCaptureCount)
            {
              maConfig = ma_device_config_init(ma_device_type_duplex);
              maConfig.playback.format = ma_format_s16;
              maConfig.playback.channels = CHANNELS;
              maConfig.capture.format = ma_format_s16;
              maConfig.capture.channels = CHANNELS;
              maConfig.sampleRate = SR;
              maConfig.dataCallback = maDataCallback;
              maConfig.pUserData = &maCallbackData;
            }
            else
            {
              maConfig = ma_device_config_init(ma_device_type_playback);
              maConfig.playback.format = ma_format_s16;
              maConfig.playback.channels = CHANNELS;
              maConfig.sampleRate = SR;
              maConfig.dataCallback = maDataCallback;
              maConfig.pUserData = &maCallbackData;
            }
#else
            // TODO: it would be nice if we could get the default configuration
            // from audio devices, instead of relying on miniaudio to convert
            // between the kind of data we want to deal with and the default
            // device formats. But that requires us to do some level of
            // conversion ourselves, and as long as we're using miniaudio, we
            // may as well leave it that way.
            ma_device_config maConfig =
              ma_device_config_init(maCaptureCount ? ma_device_type_duplex : ma_device_type_playback);
            maConfig.dataCallback = maDataCallback;
            maConfig.pUserData = &maCallbackData;
#endif
            ma_device maDevice;
            if(ma_device_init(&maContext, &maConfig, &maDevice) == MA_SUCCESS)
            {
              ma_device_start(&maDevice);

              // main loop

              u64 frameElapsedTime = 0;
              //u64 lastAudioProcessCallTime = 0;
              while(!glfwWindowShouldClose(window))
              {
                u64 frameStartTime = readOSTimer();

                // reload plugin
#if BUILD_DEBUG
                u64 newWriteTime = getLastWriteTimeU64((char *)pluginPath.str);
                if(newWriteTime != plugin.lastWriteTime)
                {
                  unloadPluginCode(&plugin);
                  for(u32 tryIndex = 0; !plugin.isValid && (tryIndex < 50); ++tryIndex)
                  {
                    plugin = loadPluginCode((char *)pluginPath.str);
                    msecWait(10);
                  }
                }
#endif

                // handle input

                int framebufferWidth, framebufferHeight;
                glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

                for(u32 buttonIndex = 0; buttonIndex < MouseButton_COUNT; ++buttonIndex)
                {
                  newInput->mouseState.buttons[buttonIndex].halfTransitionCount = 0;
                  newInput->mouseState.buttons[buttonIndex].endedDown =
                    oldInput->mouseState.buttons[buttonIndex].endedDown;
                }
                for(u32 keyIndex = 0; keyIndex < KeyboardButton_COUNT; ++keyIndex)
                {
                  newInput->keyboardState.keys[keyIndex].halfTransitionCount = 0;
                  newInput->keyboardState.keys[keyIndex].endedDown =
                    oldInput->keyboardState.keys[keyIndex].endedDown;
                }
                for(u32 modifierIndex = 0; modifierIndex < KeyboardModifier_COUNT; ++modifierIndex)
                {
                  newInput->keyboardState.modifiers[modifierIndex].halfTransitionCount = 0;
                  newInput->keyboardState.modifiers[modifierIndex].endedDown =
                    oldInput->keyboardState.modifiers[modifierIndex].endedDown;
                }

                newInput->frameMillisecondsElapsed = frameElapsedTime;

                // render new frame

                GL_CATCH_ERROR();

                glViewport(0, 0, framebufferWidth, framebufferHeight);
                glScissor(0, 0, framebufferWidth, framebufferHeight);

                GL_CATCH_ERROR();

                glClearColor(0.2f, 0.2f, 0.2f, 0.f);
                glClearDepth(1);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                GL_CATCH_ERROR();

                r32 targetAspectRatio = 16.f/9.f;
                r32 windowAspectRatio = (r32)framebufferWidth/(r32)framebufferHeight;
                v2 viewportMin, viewportDim;
                if(windowAspectRatio < targetAspectRatio)
                {
                  // NOTE: width constrained
                  viewportDim.x = (r32)framebufferWidth;
                  viewportDim.y = viewportDim.x/targetAspectRatio;
                  viewportMin = V2(0, ((r32)framebufferHeight - viewportDim.y)*0.5f);
                }
                else
                {
                  // NOTE: height constrained
                  viewportDim.y = (r32)framebufferHeight;
                  viewportDim.x = viewportDim.y*targetAspectRatio;
                  viewportMin = V2(((r32)framebufferWidth - viewportDim.x)*0.5f, 0);
                }

                renderBeginCommands(commands, (u32)viewportDim.x, (u32)viewportDim.y);

                glViewport((GLint)viewportMin.x, (GLint)viewportMin.y,
                           (GLsizei)viewportDim.x, (GLsizei)viewportDim.y);
                glScissor((GLint)viewportMin.x, (GLint)viewportMin.y,
                          (GLsizei)viewportDim.x, (GLsizei)viewportDim.y);

                GL_CATCH_ERROR();
                GL_PRINT_ERROR("GL ERROR: %u at frame start\n");

                double mouseX, mouseY;
                glfwGetCursorPos(window, &mouseX, &mouseY);
                newInput->mouseState.position =
                  V2(mouseX, (r64)framebufferHeight - mouseY) - viewportMin;

                if(gsRenderNewFrame)
                {
                  gsRenderNewFrame(&pluginMemory, oldInput, commands);
                  glfwSetCursorState(window, commands->cursorState);
                  renderCommands(commands);
                  GL_CATCH_ERROR();
                  renderEndCommands(commands);
#if BUILD_LOGGING
                  while(atomicCompareAndSwap(&pluginMemory.logger->mutex, 0, 1) != 0) {}
                  for(String8Node *node = pluginMemory.logger->log.first;
                      node;
                      node = node->next)
                  {
                    String8 string = node->string;
                    if(string.str)
                    {
                      fwrite(string.str, sizeof(*string.str), string.size,
                             stdout);
                      fprintf(stdout, "\n");
                    }
                  }

                  ZERO_STRUCT(&pluginMemory.logger->log);
                  arenaEnd(pluginMemory.logger->logArena);
                  while(atomicCompareAndSwap(&pluginMemory.logger->mutex, 1, 0) != 1) {}
#endif

                  if(commands->outputAudioDeviceChanged ||
                     commands->inputAudioDeviceChanged)
                  {
                    ma_device_stop(&maDevice);
                    ma_device_uninit(&maDevice);

                    if(commands->outputAudioDeviceChanged)
                    {
                      maPlaybackIndex = commands->selectedOutputAudioDeviceIndex;
                      maConfig.playback.pDeviceID = &maPlaybackInfos[maPlaybackIndex].id;
                    }

                    if(commands->inputAudioDeviceChanged)
                    {
                      maCaptureIndex = commands->selectedInputAudioDeviceIndex;
                      maConfig.capture.pDeviceID = &maCaptureInfos[maCaptureIndex].id;
                    }

                    ASSERT(ma_device_init(&maContext, &maConfig, &maDevice) ==
                           MA_SUCCESS);
                    ma_device_start(&maDevice);
                  }
                }

                glfwSwapBuffers(window);
                glfwPollEvents();

                PluginInput *temp = newInput;
                newInput = oldInput;
                oldInput = temp;

                u64 frameEndTime = readOSTimer();
                frameElapsedTime = frameEndTime - frameStartTime;
              }

              ma_device_stop(&maDevice);
              ma_device_uninit(&maDevice);
            }
          }

          ma_context_uninit(&maContext);
        }

        // onnxState.api->ReleaseSession(onnxState.session);
        // onnxState.api->ReleaseEnv(onnxState.env);
      }

      glfwDestroyCursor(standardCursor);
      glfwDestroyCursor(hResizeCursor);
      glfwDestroyCursor(vResizeCursor);
      glfwDestroyCursor(handCursor);
      glfwDestroyCursor(textCursor);

#if BUILD_DEBUG
      fprintf(stderr, "freeing loggerMemory\n");
#endif

      glfwDestroyWindow(window);
    }

    glfwTerminate();
  }

  return(result);
}
