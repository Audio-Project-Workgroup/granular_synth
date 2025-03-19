// a simple host application that opens a window with an OpenGL context, allocates some memory,
// registers a callback with an audio playback device, and calls into a dynamic library to render
// audio and video.

/* TODO:
   - multithreading
   - audio output device selection
   - keyboard input
   - midi device input
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
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
#include "onnx.cpp"

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
  if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
  else if(key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
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

void
maDataCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{  
  ma_pcm_rb *buffer = (ma_pcm_rb *)pDevice->pUserData;
  if(buffer)
    {
      ma_uint32 framesRemaining = frameCount;
      while(framesRemaining)
	{
	  void *srcBuffer = 0;
	  ma_uint32 framesToRead = framesRemaining;
	  ma_result result = ma_pcm_rb_acquire_read(buffer, &framesToRead, &srcBuffer);
	  if(result == MA_SUCCESS)
	    {
	      usz bytesToRead = CHANNELS*framesToRead*sizeof(s16);
	      memcpy(pOutput, srcBuffer, bytesToRead);
	      
	      result = ma_pcm_rb_commit_read(buffer, framesToRead);
	      if(result == MA_SUCCESS)
		{
		  framesRemaining -= framesToRead;
		  pOutput = (u8 *)pOutput + bytesToRead;		 		 
		}
	      else
		{
		  //fprintf(stderr, "ERROR: ma_pcm_rb_commit_read failed: %s\n", ma_result_description(result));
		}
	    }
	  else
	    {
	      //fprintf(stderr, "ERROR: ma_pcm_rb_acquire_read failed: %s\n", ma_result_description(result));
	    }
	}     
    }
}

int
main(int argc, char **argv)
{
  printf("plugin path: %s\n", PLUGIN_PATH);
  int result = 0;
  
  if(glfwInit())
    {
      //glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
      //glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
      //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
      //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

      GLFWwindow *window = glfwCreateWindow(640, 480, "glfw_miniaudio", NULL, NULL);
      if(window)	
	{
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
	  
	  pluginMemory.platformAPI.readEntireFile  = platformReadEntireFile;
	  pluginMemory.platformAPI.freeFileMemory  = platformFreeFileMemory;
	  pluginMemory.platformAPI.writeEntireFile = platformWriteEntireFile;
	  
	  pluginMemory.platformAPI.runModel = platformRunModel;
	  
	  pluginMemory.platformAPI.getCurrentTimestamp = platformGetCurrentTimestamp;

	  pluginMemory.platformAPI.atomicLoad			= atomicLoad;
	  pluginMemory.platformAPI.atomicStore			= atomicStore;
	  pluginMemory.platformAPI.atomicAdd			= atomicAdd;
	  pluginMemory.platformAPI.atomicCompareAndSwap		= atomicCompareAndSwap;
	  pluginMemory.platformAPI.atomicCompareAndSwapPointers = atomicCompareAndSwapPointers;

	  // pluginMemory.platformAPI.wideLoadFloats	 = wideLoadFloats;
	  // pluginMemory.platformAPI.wideLoadInts		 = wideLoadInts;
	  // pluginMemory.platformAPI.wideSetConstantFloats = wideSetConstantFloats;
	  // pluginMemory.platformAPI.wideSetConstantInts	 = wideSetConstantInts;
	  // pluginMemory.platformAPI.wideStoreFloats	 = wideStoreFloats;
	  // pluginMemory.platformAPI.wideStoreInts	 = wideStoreInts;
	  // pluginMemory.platformAPI.wideAddFloats	 = wideAddFloats;
	  // pluginMemory.platformAPI.wideAddInts		 = wideAddInts;
	  // pluginMemory.platformAPI.wideSubFloats	 = wideSubFloats;
	  // pluginMemory.platformAPI.wideSubInts		 = wideSubInts;
	  // pluginMemory.platformAPI.wideMulFloats	 = wideMulFloats;
	  // pluginMemory.platformAPI.wideMulInts		 = wideMulInts;

	  RenderCommands commands = {};	  

	  // plugin setup

	  PluginCode plugin = loadPluginCode(PLUGIN_PATH);

	  // model setup

	  const ORTCHAR_T *modelPath = ORT_TSTR("../data/test_model.onnx");
	  onnxState = onnxInitializeState(modelPath);
	  
	  if(onnxState.session && onnxState.env)
	    {	      	      
	      // audio setup

	      u32 audioBufferFrameCount = 48000/2;
	      void *audioBufferData = calloc(audioBufferFrameCount, CHANNELS*sizeof(s16));

	      PluginAudioBuffer audioBuffer = {};
	      audioBuffer.outputFormat = AudioFormat_s16;
	      audioBuffer.outputSampleRate = SR;
	      audioBuffer.outputChannels = CHANNELS;
	      audioBuffer.outputStride = 2*sizeof(s16);
	      audioBuffer.midiMessageCount = 0; // TODO: send midi messages to the plugin
	      audioBuffer.midiBuffer = (u8 *)calloc(KILOBYTES(1), 1);
	  
	      ma_pcm_rb maRingBuffer;
	      if(ma_pcm_rb_init(ma_format_s16, CHANNELS, audioBufferFrameCount, audioBufferData, NULL, &maRingBuffer) == MA_SUCCESS)
		{
		  ASSERT(ma_pcm_rb_seek_write(&maRingBuffer, audioBufferFrameCount/10) == MA_SUCCESS);	     

		  // TODO: the host application is currently hard-coded to request an output device
		  //       which is 48kHz, stereo, and 16-bit-per-sample. Miniaudio can iterate over
		  //       output devices, so we should use that to make a device-selection interface.
		  ma_device_config maConfig = ma_device_config_init(ma_device_type_playback);
		  maConfig.playback.format = maRingBuffer.format;
		  maConfig.playback.channels = maRingBuffer.channels;
		  maConfig.sampleRate = SR;
		  maConfig.dataCallback = maDataCallback;
		  maConfig.pUserData = &maRingBuffer;

		  // TODO: this is about the lowest latency I could get on my machine while
		  //       avoiding buffer underflows. Putting the audioProcess() call on a
		  //       separate thread that doesn't have to wait for the monitor vblank
		  //       will almost certainly enable us to get lower latency, but I don't
		  //       want to introduce that extra complexity just yet. -Ry
		  r32 targetLatencyMs = 30.f;
		  u32 targetLatencySamples = (u32)(targetLatencyMs*(r32)SR/1000.f);

		  ma_device maDevice;
		  if(ma_device_init(NULL, &maConfig, &maDevice) == MA_SUCCESS)
		    {
		      ma_device_start(&maDevice);

		      // main loop

		      u64 frameElapsedTime = 0;
		      u64 lastAudioProcessCallTime = 0;
		      while(!glfwWindowShouldClose(window))
			{
			  u64 frameStartTime = readOSTimer();
			  
			  // reload plugin
		      
			  u64 newWriteTime = getLastWriteTimeU64(PLUGIN_PATH);
			  if(newWriteTime != plugin.lastWriteTime)
			    {
			      unloadPluginCode(&plugin);
			      for(u32 tryIndex = 0; !plugin.isValid && (tryIndex < 10); ++tryIndex)
				{
				  plugin = loadPluginCode(PLUGIN_PATH);
				  msecWait(5);
				}			  
			    }		      

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

			  // TODO: what's the difference between window size and framebuffer size? do we need both?
			  //int windowWidth, windowHeight;
			  //glfwGetWindowSize(window, &windowWidth, &windowHeight);
			  
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
			  newInput->mouseState.position = V2(mouseX, (r64)framebufferHeight - mouseY) - viewportMin;
			  //printf("mouseP: (%.2f, %.2f)\n", newInput->mouseState.position.x, newInput->mouseState.position.y);

			  if(plugin.renderNewFrame)
			    {
			      plugin.renderNewFrame(&pluginMemory, oldInput, &commands);
			      glfwSetCursorState(window, commands.cursorState);
			      renderCommands(&commands);
			    }

			  glfwSwapBuffers(window);
			  glfwPollEvents();		      	     		      

			  // process audio

			  s32 maRBPtrDistance = ma_pcm_rb_pointer_distance(&maRingBuffer);
			  //fprintf(stdout, "\nptr distance: %d\n", maRBPtrDistance);

			  // TODO: this timing code was hastily hacked together and should be thought out
			  //       more and made better
			  u32 framesRemaining = (maRBPtrDistance < 2*(s32)targetLatencySamples) ?
			    targetLatencySamples : 0;		      
			  while(framesRemaining)
			    {
			      void *writePtr = 0;
			      u32 framesToWrite = framesRemaining;
			      ma_result maResult = ma_pcm_rb_acquire_write(&maRingBuffer, &framesToWrite, &writePtr);
			      if(maResult == MA_SUCCESS)
				{
				  if(plugin.audioProcess)
				    {
				      u64 audioProcessCallTime = readOSTimer();
				      audioBuffer.millisecondsElapsedSinceLastCall =
					audioProcessCallTime - lastAudioProcessCallTime;
				      lastAudioProcessCallTime = audioProcessCallTime;

				      //audioBuffer.buffer = writePtr;
				      audioBuffer.outputBuffer[0] = writePtr;
				      audioBuffer.outputBuffer[1] = (u8 *)writePtr + sizeof(s16);
				      audioBuffer.framesToWrite = framesToWrite;
				      plugin.audioProcess(&pluginMemory, &audioBuffer);
				    }

				  maResult = ma_pcm_rb_commit_write(&maRingBuffer, framesToWrite);
				  if(maResult == MA_SUCCESS)
				    {
				      //fprintf(stdout, "wrote %u frames\n", framesToWrite);
				      framesRemaining -= framesToWrite;
				    }
				  else
				    {
				      fprintf(stderr, "ERROR: ma_pcm_rb_commit_write failed: %s\n",
					      ma_result_description(maResult));
				    }
				}
			      else
				{
				  fprintf(stderr, "ERROR: ma_pcm_rb_acquire_write failed: %s\n",
					  ma_result_description(maResult));
				}
			    }

			  PluginInput *temp = newInput;
			  newInput = oldInput;
			  oldInput = temp;

			  u64 frameEndTime = readOSTimer();
			  frameElapsedTime = frameEndTime - frameStartTime;
			}

		      ma_device_uninit(&maDevice);
		    }

		  ma_pcm_rb_uninit(&maRingBuffer);
		}

	      onnxState.api->ReleaseSession(onnxState.session);
	      onnxState.api->ReleaseEnv(onnxState.env);
	    }

	  glfwDestroyCursor(standardCursor);
	  glfwDestroyCursor(hResizeCursor);
	  glfwDestroyCursor(vResizeCursor);
	  glfwDestroyCursor(handCursor);
	  glfwDestroyCursor(textCursor);
	  
	  glfwDestroyWindow(window);
	}
      
      glfwTerminate();
    }

  return(result);
}
