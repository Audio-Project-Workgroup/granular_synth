// a simple host application that opens a window with an OpenGL context, allocates some memory,
// registers a callback with an audio playback device, and calls into a dynamic library to render
// audio and video.

/* TODO:
   - multithreading (tentatively done)
   - audio output device selection (tentatively done)
   - keyboard input (tentatively done)
   - midi device input
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
static String8 executablePath;
static String8 basePath;
#include "platform.h"

#if OS_MAC || OS_LINUX
#include <GL/glew.h>
#endif
#include <GLFW/glfw3.h>

#define GL_PRINT_ERROR(msg, ...) do {		\
    GLenum err = glGetError();			\
    if(err) {					\
      fprintf(stderr, msg, err, ##__VA_ARGS__);	\
    }						\
  } while(0)

#include "miniaudio.h"

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

struct MACallbackUserData
{
  ma_pcm_rb *outputBuffer;
  ma_pcm_rb *inputBuffer;
};

void
maDataCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{  
  //ma_pcm_rb *buffer = (ma_pcm_rb *)pDevice->pUserData;
  MACallbackUserData *buffers = (MACallbackUserData *)pDevice->pUserData;
  if(buffers)
    {
      ma_pcm_rb *outputBuffer = buffers->outputBuffer;
      if(outputBuffer)
	{
	  ma_uint32 framesRemaining = frameCount;
	  while(framesRemaining)
	    {
	      void *srcBuffer = 0;
	      ma_uint32 framesToRead = framesRemaining;
	      ma_result result = ma_pcm_rb_acquire_read(outputBuffer, &framesToRead, &srcBuffer);
	      if(result == MA_SUCCESS)
		{
		  usz bytesToRead = CHANNELS*framesToRead*sizeof(s16);
		  memcpy(pOutput, srcBuffer, bytesToRead);
#if 0
		  s16 *test = (s16 *)srcBuffer;
		  for(u32 i = 0; i < framesToRead; ++i)
		    {
		      for(u32 j = 0; j < CHANNELS; ++j)
			{
			  fprintf(stderr, "srcBuffer val (%u, %u): %d\n", i, j, *test++);
			}
		    }
#endif
	      
		  result = ma_pcm_rb_commit_read(outputBuffer, framesToRead);
		  if(result == MA_SUCCESS)
		    {
		      framesRemaining -= framesToRead;
		      pOutput = (u8 *)pOutput + bytesToRead;		 		 
		    }
		  else
		    {
#if BUILD_DEBUG
		      fprintf(stderr, "ERROR: maDataCallback: ma_pcm_rb_commit_read failed: %s\n",
			      ma_result_description(result));
#endif
		      break;
		    }
		}
	      else
		{
#if BUILD_DEBUG
		  fprintf(stderr, "ERROR: maDataCallback ma_pcm_rb_acquire_read failed: %s\n",
			  ma_result_description(result));
#endif
		  break;
		}
	    }     
	}
#if 1
      ma_pcm_rb *inputBuffer = buffers->inputBuffer;
      if(inputBuffer)
	{
	  ma_uint32 framesRemaining = frameCount;
	  while(framesRemaining)
	    {
	      void *destBuffer = 0;
	      ma_uint32 framesToWrite = framesRemaining;
	      ma_result result = ma_pcm_rb_acquire_write(inputBuffer, &framesToWrite, &destBuffer);
	      if(result == MA_SUCCESS)
		{
		  usz bytesToWrite = CHANNELS*framesToWrite*sizeof(s16);
		  memcpy(destBuffer, pInput, bytesToWrite);

		  result = ma_pcm_rb_commit_write(inputBuffer, framesToWrite);
		  if(result == MA_SUCCESS)
		    {
		      framesRemaining -= framesToWrite;
		      pInput = (u8 *)pInput + bytesToWrite;
		    }
		  else
		    {
#if BUILD_DEBUG
		      fprintf(stderr, "ERROR: maDataCallback: ma_pcm_rb_commit_write failed: %s\n",
			      ma_result_description(result));
#endif
		      break;
		    }
		}
	      else
		{
#if BUILD_DEBUG
		  fprintf(stderr, "ERROR: maDataCallback: ma_pcm_rb_acquire_write failed: %s\n",
			  ma_result_description(result));
#endif
		  break;
		}
	    }
	}
#endif
    }
}

struct AudioThreadData
{
  volatile u32 *lock;
  u32 *cancel;
  u32 *finished;

  ma_pcm_rb *outputBuffer;
  ma_pcm_rb *inputBuffer;

  u32 targetLatencySamples;
  PluginCode *plugin;
  PluginMemory *pluginMemory;
  PluginAudioBuffer *audioBuffer;  
};

static BASE_THREAD_PROC(audioThreadProc)
{
  AudioThreadData *audioData = (AudioThreadData *)data;
  PluginCode *plugin = audioData->plugin;
  PluginMemory *pluginMemory = audioData->pluginMemory;
  PluginAudioBuffer *audioBuffer = audioData->audioBuffer;
  ma_pcm_rb *outputRingBuffer = audioData->outputBuffer;
  ma_pcm_rb *inputRingBuffer = audioData->inputBuffer;

  u32 baseWaitTimeMS = 1;
  u32 waitTimeMS = baseWaitTimeMS;

  for(;;)
    {
      //fprintf(stderr, "for(;;)\n");      

      s32 outputRBPtrDistance = ma_pcm_rb_pointer_distance(outputRingBuffer);
      if(inputRingBuffer)
	{
	  s32 inputRBPtrDistance = ma_pcm_rb_pointer_distance(inputRingBuffer);
	  UNUSED(inputRBPtrDistance);
	  //fprintf(stdout, "\n output rb ptr distance: %d\n", outputRBPtrDistance);
	  //fprintf(stdout, "input rb ptr distance: %d\n\n", inputRBPtrDistance);
	}
      // TODO: this timing code was hastily hacked together and should be thought out
      //       more and made better
      u32 framesRemaining = (outputRBPtrDistance < 2*(s32)audioData->targetLatencySamples) ?
	audioData->targetLatencySamples : 0;
      if(framesRemaining)
	{
	  waitTimeMS = baseWaitTimeMS;
	  
	  ma_result maResult;
	  while(framesRemaining)
	    {
	      //fprintf(stderr, "while(framesRemaining): %u\n", framesRemaining);	      
	      
	      void *writePtr = 0;
	      void *readPtr = 0;
	      u32 framesToWrite = framesRemaining;
	      // TODO: think out better how to read from input buffer and write to output buffer simultaneously
	      //       right now, we're frequently in a situation where framesToWrite gets set to zero after two
	      //       calls to ma ring buffer functions.
	      maResult = ma_pcm_rb_acquire_write(outputRingBuffer, &framesToWrite, &writePtr);
	      if(inputRingBuffer)
		{	
		  maResult = ma_pcm_rb_acquire_read(inputRingBuffer, &framesToWrite, &readPtr);
		}
	      
	      if(maResult == MA_SUCCESS && framesToWrite)
		{
		  //NOTE: check if we are canceled, set finished if we are
		  while(atomicCompareAndSwap(audioData->lock, 0, 1) != 0)
		    {
		    }
		  if(*audioData->cancel == 1)
		    {
		      *audioData->finished = 1;
		      atomicStore(audioData->lock, 0);
		      return;
		    }
		  else
		    {
		      atomicStore(audioData->lock, 0);
		    }		  
		  if(plugin->audioProcess)
		    {
		      // u64 audioProcessCallTime = readOSTimer();
		      // audioBuffer.millisecondsElapsedSinceLastCall =
		      //   audioProcessCallTime - lastAudioProcessCallTime;
		      // lastAudioProcessCallTime = audioProcessCallTime;

		      audioBuffer->outputBuffer[0] = writePtr;
		      audioBuffer->outputBuffer[1] = (u8 *)writePtr + sizeof(s16);
		      audioBuffer->inputBuffer[0] = readPtr;
		      audioBuffer->inputBuffer[1] = readPtr;
		      audioBuffer->framesToWrite = framesToWrite;
		      //fprintf(stderr, "calling audioProcess(). frameToWrite: %u\n", framesToWrite);
		      plugin->audioProcess(pluginMemory, audioBuffer);
		    }

		  maResult = ma_pcm_rb_commit_write(outputRingBuffer, framesToWrite);		  
		  if(inputRingBuffer)
		    {  
		      maResult = ma_pcm_rb_commit_read(inputRingBuffer, framesToWrite);
		    }
		  
		  if(maResult == MA_SUCCESS)
		    {
		      //fprintf(stderr, "wrote %u frames\n", framesToWrite);
		      framesRemaining -= framesToWrite;
		    }
		  else
		    {
#if BUILD_DEBUG
		      fprintf(stderr,
			      "ERROR: audioProcess: ma_pcm_rb_commit_write failed: %s\n",
			      ma_result_description(maResult));
#endif
		    }
		}
	      else
		{
#if BUILD_DEBUG
		  fprintf(stderr,
			  "ERROR: audioProcess: ma_pcm_rb_acquire_write failed: %s\n",
			  ma_result_description(maResult));
#endif
		}
	    }
	}
      else
	{
	  msecWait(waitTimeMS);
	  waitTimeMS *= 2;
	}
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

#define FOURCC(str) (((u32)((str)[0]) << 0) | ((u32)((str)[1]) << 8) | ((u32)((str)[2]) << 16) | ((u32)((str)[3]) << 24))

static LoadedBitmap
loadBitmap(Arena *destAllocator, Arena *loadAllocator, String8 filename)
{
  LoadedBitmap result = {};
  LoadedBitmap temp = {};

  ReadFileResult readResult = platformReadEntireFile((char *)filename.str, loadAllocator);
  if(readResult.contents)
    {
      BitmapHeader *header = (BitmapHeader *)readResult.contents;      
      ASSERT(header->signature == (u16)FOURCC("BM  "));
      ASSERT(header->fileSize == readResult.contentsSize - 1);
            
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

      u8 *srcRow = (u8 *)temp.pixels + temp.stride*(temp.height - 1);
      u8 *destRow = (u8 *)result.pixels;
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

	  srcRow -= result.stride;
	  destRow += result.stride;
	}      
    }

  return(result);
}

int
main(int argc, char **argv)
{
#if BUILD_DEBUG  
  printf("plugin path: %s\n", PLUGIN_PATH);
#endif
  int result = 0;

  usz stringMemorySize = KILOBYTES(1);
  void *stringMemory = calloc(stringMemorySize, 1);
  Arena stringArena = arenaBegin(stringMemory, stringMemorySize);

  executablePath = platformGetPathToModule(0, (void *)main, &stringArena);
  basePath = stringGetParentPath(executablePath);
#if BUILD_DEBUG
  fprintf(stderr, "executable path: %.*s\n", (int)executablePath.size, executablePath.str);
  fprintf(stderr, "base path: %.*s\n", (int)basePath.size, basePath.str);
#endif

#if BUILD_DEBUG
  String8 glfwPath = platformGetPathToModule(0, (void *)glfwCreateWindow, &stringArena);
  String8 openGLPath = platformGetPathToModule(0, (void *)glBegin, &stringArena);
  printf("glfw module source: %.*s\n", (int)glfwPath.size, glfwPath.str);
  printf("opengl module source: %.*s\n", (int)openGLPath.size, openGLPath.str);
#endif
  
  if(glfwInit())
    {
      //glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
      //glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
      //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
      //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

      GLFWwindow *window = glfwCreateWindow(640, 480, "granade", NULL, NULL);
      if(window)	
	{
	  // TODO: stop using crt allocation functions - use os allocation functions (VirtualAlloc(), mmap())
	  // NOTE: set applcation icon
	  usz loadMemorySize = MEGABYTES(64);
	  void *loadMemory = calloc(loadMemorySize, 1);
	  Arena loadArena = arenaBegin(loadMemory, loadMemorySize);
	  Arena destArena = arenaSubArena(&loadArena, loadMemorySize/2);
	  LoadedBitmap iconBitmap = loadBitmap(&destArena, &loadArena,
					       STR8_LIT("../data/BMP/DENSITYPOMEGRANATE_BUTTON.bmp"));
	  
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

	  glfwSwapInterval(1);
	  
	  glEnable(GL_TEXTURE_2D);
	  GL_PRINT_ERROR("GL ERROR %u: enable texture 2D failed at startup\n");
	  
	  glEnable(GL_BLEND);
	  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	  GL_PRINT_ERROR("GL ERROR %u: enable alpha blending failed at startup\n");

	  glEnable(GL_SCISSOR_TEST);
	  
	  // memory/grahics setup

	  PluginMemory pluginMemory = {};
	  pluginMemory.memory = calloc(MEGABYTES(512), 1);
	  pluginMemory.osTimerFreq = getOSTimerFreq();
	  pluginMemory.host = PluginHost_executable;
	  
	  pluginMemory.platformAPI.readEntireFile  = platformReadEntireFile;
	  pluginMemory.platformAPI.freeFileMemory  = platformFreeFileMemory;
	  pluginMemory.platformAPI.writeEntireFile = platformWriteEntireFile;
	  pluginMemory.platformAPI.getPathToModule = platformGetPathToModule;
	  
	  //pluginMemory.platformAPI.runModel = platformRunModel;
	  
	  pluginMemory.platformAPI.getCurrentTimestamp = platformGetCurrentTimestamp;

	  pluginMemory.platformAPI.atomicLoad			= atomicLoad;
	  pluginMemory.platformAPI.atomicStore			= atomicStore;
	  pluginMemory.platformAPI.atomicAdd			= atomicAdd;
	  pluginMemory.platformAPI.atomicCompareAndSwap		= atomicCompareAndSwap;
	  pluginMemory.platformAPI.atomicCompareAndSwapPointers = atomicCompareAndSwapPointers;

#if BUILD_DEBUG
	  usz loggerMemorySize = KILOBYTES(512);
	  void *loggerMemory = calloc(loggerMemorySize, 1);
	  Arena loggerArena = arenaBegin(loggerMemory, loggerMemorySize);

	  PluginLogger logger = {};
	  logger.logArena = &loggerArena;
	  logger.maxCapacity = loggerMemorySize/2;

	  pluginMemory.logger = &logger;
#endif

	  RenderCommands commands = {};	  

	  // plugin setup
	  
#if OS_MAC || OS_LINUX	  	  
	  String8 pluginPath = concatenateStrings(&stringArena, basePath, STR8_LIT("/" PLUGIN_PATH));	  	  
#else
	  String8 pluginPath = STR8_LIT(PLUGIN_PATH);
#endif
	  PluginCode plugin = loadPluginCode((char *)pluginPath.str);
	  pluginMemory.pluginHandle = plugin.pluginCode;

	  // model setup

	  // const ORTCHAR_T *modelPath = ORT_TSTR("../data/test_model.onnx");
	  // onnxState = onnxInitializeState(modelPath);
	  
	  // if(onnxState.session && onnxState.env)
	    {	      	      
	      // audio setup

	      u32 audioBufferFrameCount = 48000/2;
	      void *audioBufferDataOutput = calloc(audioBufferFrameCount, CHANNELS*sizeof(s16));
	      void *audioBufferDataInput = calloc(audioBufferFrameCount, CHANNELS*sizeof(s16));

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

	      ma_context maContext;
	      if(ma_context_init(NULL, 0, NULL, &maContext) == MA_SUCCESS)
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
		      
		      // TODO: this is about the lowest latency I could get on my machine while
		      //       avoiding buffer underflows. Putting the audioProcess() call on a
		      //       separate thread that doesn't have to wait for the monitor vblank
		      //       will almost certainly enable us to get lower latency, but I don't
		      //       want to introduce that extra complexity just yet. -Ry
		      r32 targetLatencyMs = 30.f;
		      u32 targetLatencySamples = (u32)(targetLatencyMs*(r32)SR/1000.f);

		      ma_pcm_rb maOutputRingBuffer, maInputRingBuffer;
		      if((ma_pcm_rb_init(ma_format_s16, CHANNELS, audioBufferFrameCount,
					 audioBufferDataOutput, NULL, &maOutputRingBuffer) == MA_SUCCESS) &&
			 (ma_pcm_rb_init(ma_format_s16, CHANNELS, audioBufferFrameCount,
					 audioBufferDataInput, NULL, &maInputRingBuffer) == MA_SUCCESS))
			{
			  s32 maOutputRBPtrDistance = ma_pcm_rb_pointer_distance(&maOutputRingBuffer);
			  s32 maInputRBPtrDistance = ma_pcm_rb_pointer_distance(&maInputRingBuffer);
			  
			  ASSERT(ma_pcm_rb_seek_write(&maOutputRingBuffer,
						      targetLatencySamples) == MA_SUCCESS);
			  maOutputRBPtrDistance = ma_pcm_rb_pointer_distance(&maOutputRingBuffer);
			  UNUSED(maOutputRBPtrDistance);
			  
			  ASSERT(ma_pcm_rb_seek_write(&maInputRingBuffer,
						      audioBufferFrameCount - targetLatencySamples) == MA_SUCCESS);
			  maInputRBPtrDistance = ma_pcm_rb_pointer_distance(&maInputRingBuffer);
			  UNUSED(maInputRBPtrDistance);

			  MACallbackUserData maCallbackUserData; 

			  ma_device_config maConfig;
			  if(maCaptureCount)
			    {
			      maCallbackUserData = {&maOutputRingBuffer, &maInputRingBuffer};
			      maConfig = ma_device_config_init(ma_device_type_duplex);
			      maConfig.playback.format = maOutputRingBuffer.format;
			      maConfig.playback.channels = maOutputRingBuffer.channels;
			      maConfig.capture.format = maInputRingBuffer.format;
			      maConfig.capture.channels = maInputRingBuffer.channels;
			      maConfig.sampleRate = SR;
			      maConfig.dataCallback = maDataCallback;
			      maConfig.pUserData = &maCallbackUserData;
			    }
			  else
			    {
			      maCallbackUserData = {&maOutputRingBuffer, 0};
			      maConfig = ma_device_config_init(ma_device_type_playback);
			      maConfig.playback.format = maOutputRingBuffer.format;
			      maConfig.playback.channels = maOutputRingBuffer.channels;
			      maConfig.sampleRate = SR;
			      maConfig.dataCallback = maDataCallback;
			      maConfig.pUserData = &maCallbackUserData;
			    }
			  
			  ma_device maDevice;
			  if(ma_device_init(NULL, &maConfig, &maDevice) == MA_SUCCESS)
			    {
			      ma_device_start(&maDevice);

			      OSThread audioThread;
			      AudioThreadData audioThreadData = {};
			      audioThreadData.lock = &audioThread.lock;
			      audioThreadData.cancel = &audioThread.cancel;
			      audioThreadData.finished = &audioThread.finished;
			      audioThreadData.outputBuffer = &maOutputRingBuffer;
			      audioThreadData.inputBuffer = maCaptureCount ? &maInputRingBuffer : 0;
			      audioThreadData.targetLatencySamples = targetLatencySamples;
			      audioThreadData.plugin = &plugin;
			      audioThreadData.pluginMemory = &pluginMemory;
			      audioThreadData.audioBuffer = &audioBuffer;  

			      threadCreate(&audioThread, audioThreadProc, &audioThreadData);
			      //threadStart(audioThread);

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

				  glViewport(0, 0, framebufferWidth, framebufferHeight);
				  glScissor(0, 0, framebufferWidth, framebufferHeight);

				  glClearColor(0.2f, 0.2f, 0.2f, 0.f);
				  glClear(GL_COLOR_BUFFER_BIT);		      

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
		      		          
				  commands.windowResized = (commands.widthInPixels != (u32)viewportDim.x ||
							    commands.heightInPixels != (u32)viewportDim.y);
				  commands.widthInPixels = (u32)viewportDim.x;
				  commands.heightInPixels = (u32)viewportDim.y;
			  
				  glViewport((GLint)viewportMin.x, (GLint)viewportMin.y,
					     (GLsizei)viewportDim.x, (GLsizei)viewportDim.y);
				  glScissor((GLint)viewportMin.x, (GLint)viewportMin.y,
					    (GLsizei)viewportDim.x, (GLsizei)viewportDim.y);

				  GL_PRINT_ERROR("GL ERROR: %u at frame start\n");

				  double mouseX, mouseY;
				  glfwGetCursorPos(window, &mouseX, &mouseY);
				  newInput->mouseState.position = (V2(mouseX, (r64)framebufferHeight - mouseY) -
								   viewportMin);

				  if(plugin.renderNewFrame)
				    {
				      plugin.renderNewFrame(&pluginMemory, oldInput, &commands);
				      glfwSetCursorState(window, commands.cursorState);				      
				      renderCommands(&commands);
#if BUILD_DEBUG
				      for(String8Node *node = pluginMemory.logger->log.first; node; node = node->next)
					{
					  String8 string = node->string;
					  if(string.str)
					    {  
					      fwrite(string.str, sizeof(*string.str), string.size, stdout);
					      fprintf(stdout, "\n");
					    }
					}
				     
				      ZERO_STRUCT(&pluginMemory.logger->log);
				      arenaEnd(pluginMemory.logger->logArena);
#endif

				      if(commands.outputAudioDeviceChanged || commands.inputAudioDeviceChanged)
					{
					  ma_device_stop(&maDevice);
					  ma_device_uninit(&maDevice);

					  if(commands.outputAudioDeviceChanged)
					    {
					      maPlaybackIndex = commands.selectedOutputAudioDeviceIndex;
					      maConfig.playback.pDeviceID = &maPlaybackInfos[maPlaybackIndex].id;
					    }

					  if(commands.inputAudioDeviceChanged)
					    {
					      maCaptureIndex = commands.selectedInputAudioDeviceIndex;
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

			      threadDestroy(&audioThread);
			      ma_device_stop(&maDevice);
			      ma_device_uninit(&maDevice);
			    }

			  ma_pcm_rb_uninit(&maOutputRingBuffer);
			  ma_pcm_rb_uninit(&maInputRingBuffer);
			}		      
		    }

		  ma_context_uninit(&maContext);
		}
	      
	      // onnxState.api->ReleaseSession(onnxState.session);
	      // onnxState.api->ReleaseEnv(onnxState.env);

	      free(audioBufferDataInput);
	      free(audioBufferDataOutput);	    
	      free(audioBuffer.midiBuffer);
	    }	    
	    
	  glfwDestroyCursor(standardCursor);
	  glfwDestroyCursor(hResizeCursor);
	  glfwDestroyCursor(vResizeCursor);
	  glfwDestroyCursor(handCursor);
	  glfwDestroyCursor(textCursor);
	  	 	  
#if BUILD_DEBUG
	  fprintf(stderr, "freeing loggerMemory\n");
	  free(loggerMemory);
#endif
	  free(pluginMemory.memory);	  
	  free(loadMemory);
	  
	  glfwDestroyWindow(window);	  
	}

      glfwTerminate();
    }

  free(stringMemory);
  
  return(result);
}
