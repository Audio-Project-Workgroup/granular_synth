// the core audio-visual functionality of our plugin is implemented here

/* NOTE:
   - elements are passed to the renderer in OpenGL clip-coordinates ((-1, -1) is the bottom-left of the
     screen, (1, 1) is the top-right).
     
   - the mouse position is in these coordinates as well, for ease of intersection testing
   
   - the renderNewFrame() and audioProcess() functions are hot-reloadable, i.e. you modify the code,
     recompile, and perceive those changes in the running application automatically.
     
     - currently, only the simple host application implements hot-reloading. This is intended to be a
       development feature, and is not needed in the release build.
       
     - initializePluginState() is currently only called at startup. If you modify this function and
       recompile at run-time, you will not see those changes since the function has already been called.
       
     - modifying the arguments to those functions, or adding fields to PluginState, will likely crash the
       application, and introduce some unusual and undesirable behavior otherwise.
*/

/* TODO:
   - The interface code is ad-hoc, ugly, does not support binding to a keyboard, midi controller,
     or screen reader, and assumes renderNewFrame() and audioProcess() are called sequentially
     on the same thread (which they are in the simple host, but not in the release build).
     We need a system for creating a hierarchy of UI elements, which can be navigated with the keyboard
     and can pass information to a screen reader, and having elements bound to state parameters and
     midi channels/keyboard keys. This system then needs to interface with a system for updating and reading
     state parameters in a thread-safe way.

   - Image loading for text rendering and a prettier UI

   - Make the audioProcess() do granular synth things

   - Either finish the Midi message parsing code, or pass the relevant information from the host layer

   - Integrate with the ML system
*/  

#include <stdio.h>

#include "common.h"
#include "plugin.h"

PlatformAPI platform;

static PluginState *
initializePluginState(PluginMemory *memoryBlock)
{
  void *memory = memoryBlock->memory;
  PluginState *pluginState = 0;
  if(memory)
    {      
      pluginState = (PluginState *)memory;
      if(!pluginState->initialized)
	{
	  platform = memoryBlock->platformAPI;

	  pluginState->permanentArena = arenaBegin((u8 *)memory + sizeof(PluginState), MEGABYTES(256));
	  pluginState->frameArena = arenaSubArena(&pluginState->permanentArena, MEGABYTES(2));
      
	  pluginState->phasor = 0.f;
	  pluginState->freq = 440.f;
	  pluginState->volume = 0.8f;

	  pluginState->mouseP = V3(0, 0, 0);
	  pluginState->lastMouseP = V3(0, 0, 0);
	  pluginState->mouseLClickP = V2(0, 0);
	  pluginState->mouseRClickP = V2(0, 0);

	  pluginState->loadedSound.sound = loadWav("../data/fingertips.wav", &pluginState->permanentArena);
	  pluginState->loadedSound.samplesPlayed = 0;
	  pluginState->loadedSound.isPlaying = false;

	  pluginState->initialized = true;
	}
    }
  
  return(pluginState);
}

extern "C"
RENDER_NEW_FRAME(renderNewFrame)
{
  PluginState *pluginState = initializePluginState(memory);
  if(pluginState)
    {
      //renderCommands->arena = &pluginState->frameArena;      

      pluginState->mouseP.x = (r32)input->mousePositionX;
      pluginState->mouseP.y = (r32)input->mousePositionY;
      pluginState->mouseP.z += (r32)input->mouseScrollDelta;
#if 0
      printf("mouseP: (%.2f, %.2f, %.2f)\n",
	     pluginState->mouseP.x, pluginState->mouseP.y, pluginState->mouseP.z);
      printf("mouseLeft: %s, %s\n",
	     wasPressed(input->mouseButtons[MouseButton_left]) ? "pressed" : "not pressed",
	     isDown(input->mouseButtons[MouseButton_left]) ? "down" : "up");
      printf("mouseRight: %s, %s\n",
	     wasPressed(input->mouseButtons[MouseButton_right]) ? "pressed" : "not pressed",	     
	     isDown(input->mouseButtons[MouseButton_right]) ? "down" : "up");
#endif

      v2 dMouseP = pluginState->mouseP.xy - pluginState->lastMouseP.xy;

      if(wasPressed(input->mouseButtons[MouseButton_left]))
	{
	  pluginState->mouseLClickP = pluginState->mouseP.xy;
	}
      if(wasPressed(input->mouseButtons[MouseButton_right]))
	{
	  pluginState->mouseRClickP = pluginState->mouseP.xy;
	}      
      
      v4 defaultColor = V4(1, 1, 1, 1);
      v4 hoverColor = V4(1, 1, 0, 1);
      v4 activeColor = V4(0.8, 0.8, 0, 1);

      v2 playButtonDim = V2(0.1f, 0.1f);
      Rect2 playButton = rectCenterDim(V2(0.5f, 0.5f), playButtonDim);
      renderPushQuad(renderCommands, playButton, V4(0, 0, 1, 1));

      v4 playColor = defaultColor;
      if(isInRectangle(playButton, pluginState->mouseP.xy))
	{
	  if(isDown(input->mouseButtons[MouseButton_left]))
	    {
	      playColor = activeColor;
	      pluginState->loadedSound.isPlaying = true;
	    }
	  else
	    {
	      playColor = hoverColor;
	    }
	}
      renderPushVertex(renderCommands, playButton.min + V2(0.2f*playButtonDim.x, 0.2f*playButtonDim.y), V2(0, 0), playColor);
      renderPushVertex(renderCommands, playButton.min + V2(0.2f*playButtonDim.x, 0.8f*playButtonDim.y), V2(0, 0), playColor);
      renderPushVertex(renderCommands, playButton.min + V2(0.8f*playButtonDim.x, 0.5f*playButtonDim.y), V2(0, 0), playColor);

      v2 faderBarCenter = V2(-0.8f, 0);
      v2 faderBarDim = V2(0.02f, 1.5f);
      Rect2 faderBar = rectCenterDim(faderBarCenter, faderBarDim);      
      renderPushQuad(renderCommands, faderBar, V4(0, 0, 0, 1));

      Rect2 fader = rectCenterDim(faderBarCenter + V2(0, (pluginState->volume - 0.5f)*faderBarDim.y),
				  V2(0.1f, 0.1f));
      v4 faderColor = defaultColor;
      if(isInRectangle(fader, pluginState->mouseP.xy))
	{
	  if(isDown(input->mouseButtons[MouseButton_left]))
	    {
	      faderColor = activeColor;
	      pluginState->volume += dMouseP.y;	      
	    }
	  else
	    {
	      faderColor = hoverColor;	      
	    }
	}      
      renderPushQuad(renderCommands, fader, faderColor);

      pluginState->lastMouseP = pluginState->mouseP;
    }
}

enum MidiStatus : u32
{
  MidiStatus_noteOff = 0x80,
  MidiStatus_noteOn = 0x90,
  MidiStatus_aftertouch = 0xA0,
  MidiStatus_continuousController = 0xB0,
  MidiStatus_patchChange = 0xC0,
  MidiStatus_channelPressure = 0xD0,
  MidiStatus_pitchBend = 0xE0,
  MidiStatus_sysEx = 0xF0,
};

static inline r32
hertzFromMidiNoteNumber(u32 noteNumber)
{
  r32 result = 440.f;
  result *= powf(powf(2.f, 1.f/12.f), (r32)((s32)noteNumber - 69));

  return(result);
}

extern "C"
AUDIO_PROCESS(audioProcess)
{  
  PluginState *pluginState = initializePluginState(memory);  
  if(pluginState->initialized)
    {
      // TODO: This midi parsing is janky and bad. Passing the message length before each message is unnecessary
      u8 *atMidiBuffer = audioBuffer->midiBuffer;
      while(audioBuffer->midiMessageCount)
	{
	  MidiHeader *header = (MidiHeader *)atMidiBuffer;
	  atMidiBuffer += sizeof(MidiHeader);

	  u32 bytesToRead = header->messageLength;
	  if(bytesToRead >= 1)
	    {
	      u32 midiStatus = *atMidiBuffer & 0xF0;
	      u32 midiChannelNumber = *atMidiBuffer & 0x0F;
	      ++atMidiBuffer;
	      --bytesToRead;

	      switch(midiStatus)
		{				  
		case MidiStatus_noteOn:
		  {
		    ASSERT(bytesToRead == 2);
		    u32 noteNumber = *atMidiBuffer++;
		    u32 noteVelocity = *atMidiBuffer++;
		    pluginState->freq = hertzFromMidiNoteNumber(noteNumber);
		    pluginState->volume = (r32)noteVelocity/127.f;
		  } break;
		  
		default:
		  {
		    while(bytesToRead)
		      {
			++atMidiBuffer;
			--bytesToRead;
		      }
		  } break;
		}
	    }

	  --audioBuffer->midiMessageCount;
	}

      r32 nFreq = M_TAU*pluginState->freq/(r32)audioBuffer->sampleRate;

      PlayingSound *loadedSound = &pluginState->loadedSound;
      s16 *loadedSoundSamples[2] = {};
      if(loadedSound->isPlaying)
	{
	  loadedSoundSamples[0] = loadedSound->sound.samples[0] + loadedSound->samplesPlayed;
	  loadedSoundSamples[1] = loadedSound->sound.samples[loadedSound->sound.channelCount - 1] + loadedSound->samplesPlayed;
	}

      // TODO: there's too much code that's identical in the two switch cases
      switch(audioBuffer->format)
	{
	case AudioFormat_r32:
	  {
	    r32 *audioFrames = (r32 *)audioBuffer->buffer;
	    for(u64 i = 0; i < audioBuffer->framesToWrite; ++i)
	      {
		pluginState->phasor += nFreq;
		if(pluginState->phasor > M_TAU) pluginState->phasor -= M_TAU;
				  
		r32 sinVal = pluginState->volume*sinf(pluginState->phasor);
		for(u32 j = 0; j < audioBuffer->channels; ++j)
		  {
		    r32 mixedVal = 0.5f*sinVal;
		    if(pluginState->loadedSound.isPlaying)		       
		      {
			if((loadedSound->samplesPlayed + i) < loadedSound->sound.sampleCount)
			  {
			    r32 loadedSoundVal = pluginState->volume*(r32)loadedSoundSamples[j][i]/32000.f;
			    mixedVal += 0.5f*loadedSoundVal;
			  }
			else
			  {
			    loadedSound->isPlaying = false;
			    loadedSound->samplesPlayed = 0;
			  }
		      }
		    
		    *audioFrames++ = mixedVal;
		  }
	      }
	  } break;

	case AudioFormat_s16:
	  {
	    s16 *audioFrames = (s16 *)audioBuffer->buffer;
	    for(u64 i = 0; i < audioBuffer->framesToWrite; ++i)
	      {
		pluginState->phasor += nFreq;
		if(pluginState->phasor > M_TAU) pluginState->phasor -= M_TAU;
				  
		r32 sinVal = 32000.f*pluginState->volume*sinf(pluginState->phasor);
		for(u32 j = 0; j < audioBuffer->channels; ++j)
		  {
		    r32 mixedVal = 0.5f*sinVal;
		    if(pluginState->loadedSound.isPlaying)		       
		      {
			if((loadedSound->samplesPlayed + i) < loadedSound->sound.sampleCount)
			  {
			    r32 loadedSoundVal = pluginState->volume*(r32)loadedSoundSamples[j][i];
			    mixedVal += 0.5f*loadedSoundVal;
			  }
			else
			  {
			    loadedSound->isPlaying = false;
			    loadedSound->samplesPlayed = 0;
			  }
		      }
		    
		    *audioFrames++ = (s16)mixedVal;
		  }		
	      }
	  } break;

	default: { ASSERT(!"invalid default case"); } break;
	}

      if(loadedSound->isPlaying)
	{
	  loadedSound->samplesPlayed += audioBuffer->framesToWrite;
	}
    }
}
