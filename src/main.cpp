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

#include <GLFW/glfw3.h>
//#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "render.cpp"

static PluginInput *newInput;

static inline void
glfwProcessButtonPress(ButtonState *newState, bool pressed)
{
  ASSERT(newState->endedDown != pressed);
  newState->endedDown = pressed;
  ++newState->halfTransitionCount;
}

static void
glfwKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

static void
glfwMouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
  if(button == GLFW_MOUSE_BUTTON_LEFT)
    {
      glfwProcessButtonPress(&newInput->mouseButtons[MouseButton_left],
			     action == GLFW_PRESS);
    }
  else if(button == GLFW_MOUSE_BUTTON_RIGHT)
    {
      glfwProcessButtonPress(&newInput->mouseButtons[MouseButton_right],
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
  int result = 0;
  
  if(glfwInit())
    {      
      GLFWwindow *window = glfwCreateWindow(640, 480, "glfw_miniaudio", NULL, NULL);
      if(window)	
	{
	  // input setup
	  
	  PluginInput pluginInput[2] = {};
	  newInput = &pluginInput[0];
	  PluginInput *oldInput = &pluginInput[1];

	  glfwSetKeyCallback(window, glfwKeyCallback);
	  glfwSetMouseButtonCallback(window, glfwMouseButtonCallback);
	  glfwMakeContextCurrent(window);

	  glfwSwapInterval(1);

	  // memory/grahics setup

	  PluginMemory pluginMemory = {};
	  pluginMemory.memory = calloc(MEGABYTES(512), 1);
	  pluginMemory.platformAPI.readEntireFile = platformReadEntireFile;

	  RenderCommands commands = {};
	  commands.vertexCapacity = 512;
	  commands.indexCapacity = 1024;
	  commands.vertices = (Vertex *)calloc(commands.vertexCapacity, sizeof(Vertex));
	  commands.indices = (u32 *)calloc(commands.indexCapacity, sizeof(u32));	  

	  // plugin setup
	  
#ifdef __WIN32__
	  char *pluginName = "../build/plugin.dll";
#else
	  char *pluginName = "../build/plugin.so";
#endif
	  PluginCode plugin = loadPluginCode(pluginName);

	  // audio setup

	  u32 audioBufferFrameCount = 48000/2;
	  void *audioBufferData = calloc(audioBufferFrameCount, CHANNELS*sizeof(s16));

	  PluginAudioBuffer audioBuffer = {};
	  audioBuffer.format = AudioFormat_s16;
	  audioBuffer.sampleRate = SR;
	  audioBuffer.channels = CHANNELS;
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

		  while(!glfwWindowShouldClose(window))
		    {
		      // reload plugin
		      
		      time_t newWriteTime = getLastWriteTime(pluginName);
		      if(newWriteTime != plugin.lastWriteTime)
			{
			  unloadPluginCode(&plugin);
			  for(u32 tryIndex = 0; !plugin.isValid && (tryIndex < 10); ++tryIndex)
			    {
			      plugin = loadPluginCode(pluginName);
			      msecWait(5);
			    }			  
			}		      

		      // handle input
		      
		      for(u32 buttonIndex = 0; buttonIndex < MouseButton_count; ++buttonIndex)
			{
			  newInput->mouseButtons[buttonIndex].halfTransitionCount = 0;
			  newInput->mouseButtons[buttonIndex].endedDown =
			    oldInput->mouseButtons[buttonIndex].endedDown;
			}

		      int windowWidth, windowHeight;
		      glfwGetWindowSize(window, &windowWidth, &windowHeight);

		      double mouseX, mouseY;
		      glfwGetCursorPos(window, &mouseX, &mouseY);
		      newInput->mousePositionX = 2.0*mouseX/(r64)windowWidth - 1.0;
		      newInput->mousePositionY = -2.0*mouseY/(r64)windowHeight + 1.0;

		      // render new frame

		      int width, height;
		      glfwGetFramebufferSize(window, &width, &height);
		      glViewport(0, 0, width, height);

		      glClearColor(0.2f, 0.2f, 0.2f, 0.f);
		      glClear(GL_COLOR_BUFFER_BIT);

		      if(plugin.renderNewFrame)
			{
			  plugin.renderNewFrame(&pluginMemory, newInput, &commands);
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
			  ma_result result = ma_pcm_rb_acquire_write(&maRingBuffer, &framesToWrite, &writePtr);
			  if(result == MA_SUCCESS)
			    {
			      if(plugin.audioProcess)
				{
				  audioBuffer.buffer = writePtr;
				  audioBuffer.framesToWrite = framesToWrite;
				  plugin.audioProcess(&pluginMemory, &audioBuffer);
				}

			      result = ma_pcm_rb_commit_write(&maRingBuffer, framesToWrite);
			      if(result == MA_SUCCESS)
				{
				  //fprintf(stdout, "wrote %u frames\n", framesToWrite);
				  framesRemaining -= framesToWrite;
				}
			      else
				{
				  fprintf(stderr, "ERROR: ma_pcm_rb_commit_write failed: %s\n",
					  ma_result_description(result));
				}
			    }
			  else
			    {
			      fprintf(stderr, "ERROR: ma_pcm_rb_acquire_write failed: %s\n",
				      ma_result_description(result));
			    }
			}

		      PluginInput *temp = newInput;
		      newInput = oldInput;
		      oldInput = temp;
		    }

		  ma_device_uninit(&maDevice);
		}

	      ma_pcm_rb_uninit(&maRingBuffer);
	    }
	  
	   glfwDestroyWindow(window);
	}
      
      glfwTerminate();
    }

  return(result);
}
