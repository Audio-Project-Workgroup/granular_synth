// the core audio-visual functionality of our plugin is implemented here

/* NOTE:
   - elements are passed to the renderer in pixel space (where (0, 0) is the bottom-left of the
     screen, and (windowWidthInPixels, windowHeightInPixels) is the top-right).
     
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
     state parameters in a thread-safe way. (WIP)

   - Image loading for text rendering and a prettier UI (DONE)

   - Make the audioProcess() do granular synth things

   - Either finish the Midi message parsing code, or pass the relevant information from the host layer

   - Integrate with the ML system
*/  

#include <stdio.h>

#include "common.h"
#include "plugin.h"

#include "ui_layout.cpp"

PlatformAPI globalPlatform;

inline void
printButtonState(ButtonState button, char *name)
{
  printf("%s: %s, %s\n", name,
	 wasPressed(button) ? "pressed" : "not pressed",
	 isDown(button) ? "down" : "up");
}

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
	  globalPlatform = memoryBlock->platformAPI;

	  pluginState->permanentArena = arenaBegin((u8 *)memory + sizeof(PluginState), MEGABYTES(256));
	  pluginState->frameArena = arenaSubArena(&pluginState->permanentArena, MEGABYTES(2));
	  pluginState->loadArena = arenaSubArena(&pluginState->permanentArena, MEGABYTES(128));

#if 0 // NOTE: fft sample rate conversion workbench
	  r32 sourceSignalFreq = 4.f;
	  u32 sourceSignalLength = 4410;
	  u32 destSignalLength = 4800;
	  u32 maxSignalLength = MAX(sourceSignalLength, destSignalLength);
	  r32 conversionFactor = (r32)destSignalLength/(r32)sourceSignalLength;	  
	  
	  r32 *sourceSignal = arenaPushArray(&pluginState->permanentArena, sourceSignalLength, r32);   
	  for(u32 i = 0; i < sourceSignalLength; ++i)
	    {
	      r32 angle = M_TAU*sourceSignalFreq*i/(r32)sourceSignalLength;
	      sourceSignal[i] = Sin(angle);
	    }
	  
	  r32 *compSignal = arenaPushArray(&pluginState->permanentArena, destSignalLength, r32);
	  r32 *destSignal = arenaPushArray(&pluginState->permanentArena, destSignalLength, r32);
	  for(u32 i = 0; i < destSignalLength; ++i)
	    {
	      r32 angle = M_TAU*sourceSignalFreq*i/(r32)destSignalLength;
	      compSignal[i] = Sin(angle);
	    }	  

	  u32 destBatchSize = 1024;
	  u32 sourceBatchSize = (u32)((r32)destBatchSize/conversionFactor);
	  u32 maxBatchSize = MAX(sourceBatchSize, destBatchSize);
	  u32 numBatches = ((destSignalLength + destBatchSize - 1) & ~(destBatchSize - 1))/destBatchSize;

	  TemporaryMemory cztScratch = arenaBeginTemporaryMemory(&pluginState->loadArena,
								 16*maxSignalLength*sizeof(c64));

	  r32 *source = sourceSignal;
	  r32 *dest = destSignal;
	  c64 *cztDest = arenaPushArray(&pluginState->permanentArena, maxSignalLength, c64);
	  r32 *imagScratch = arenaPushArray(&pluginState->permanentArena, destSignalLength, r32);
	  for(u32 batch = 0; batch < numBatches; ++batch)
	    {	      	      
	      //czt(cztDest, sourceSignal, sourceSignalLength, (Arena *)&cztScratch);
	      czt(cztDest, source, sourceBatchSize, (Arena *)&cztScratch);
	      arenaEnd((Arena *)&cztScratch, true);

	      if(sourceSignalLength < destSignalLength)
		{
		  for(u32 i = sourceBatchSize/2 + 1; i < destBatchSize; ++i)
		    {
		      cztDest[i] = C64(0, 0);
		    }

		  c64 *midpoint = cztDest + sourceBatchSize/2;
		  c64 *write = cztDest + destBatchSize - sourceBatchSize/2;
		  for(u32 i = 0; i < sourceBatchSize/2; ++i)
		    {
		      c64 sourceVal = *(midpoint - i);
		      if(i == 0) sourceVal *= 0.5f;

		      *(midpoint - i) = conversionFactor*sourceVal;
		      *write++ = conversionFactor*conjugateC64(sourceVal);
		    }
		}
	      else if(sourceSignalLength > destSignalLength)
		{
		  for(u32 i = destBatchSize; i < sourceBatchSize; ++i)
		    {
		      cztDest[i] = C64(0, 0);
		    }
	      
		  c64 *read = cztDest + destBatchSize/2;
		  c64 *write = read;
		  *write++ = C64(0, 0);
		  --read;
		  for(u32 i = 1; i < destBatchSize/2; ++i)
		    {
		      c64 readVal = *read;
		      readVal *= conversionFactor;
		  
		      *write++ = conjugateC64(readVal);
		      *read-- = readVal;
		    }
		}
	  	      
	      //iczt(destSignal, imagScratch, cztDest, destSignalLength, (Arena *)&cztScratch);
	      iczt(dest, imagScratch, cztDest, destBatchSize, (Arena *)&cztScratch);

	      source += sourceBatchSize;
	      dest += destBatchSize;

	      cztDest += maxBatchSize;
	      imagScratch += destBatchSize;
	    }
	  
	  arenaEndTemporaryMemory(&cztScratch);
	  //(void *)&sourceSignalFreq;

	  for(u32 i = 0; i < destSignalLength; ++i)
	    {
	      r32 compVal = compSignal[i];
	      r32 destVal = destSignal[i];
	      r32 diff = Abs(compVal - destVal);
	      r32 err = diff/(compVal + 0.0001f);
	      printf("err[%u]: %.6f\n", i, err);
	    }
#endif
	  
	  pluginState->phasor = 0.f;
	  pluginState->freq = 440.f;	  
	  pluginState->volume.currentValue = 0.8f;
	  pluginState->volume.targetValue = 0.8f;
	  pluginState->volume.range = makeRange(0, 1);
	  pluginState->soundIsPlaying.value = false;

	  pluginState->loadedSound.sound = loadWav("../data/fingertips_44100_PCM_16.wav",
						   &pluginState->loadArena, &pluginState->permanentArena);
	  pluginState->loadedSound.samplesPlayed = 0;

	  pluginState->testBitmap = loadBitmap("../data/signal_z.bmp", &pluginState->permanentArena);

	  RangeU32 characterRange = {32, 127}; // NOTE: from SPACE up to (but not including) DEL
	  pluginState->testFont = loadFont("../data/arial.ttf", &pluginState->loadArena,
					   &pluginState->permanentArena, characterRange, 32.f);

	  pluginState->layout = uiInitializeLayout(&pluginState->frameArena, &pluginState->permanentArena,
						   &pluginState->testFont);

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
      renderBeginCommands(renderCommands, &pluginState->frameArena);

      // TODO: maybe make a logging system?
#if 0
      printf("mouseP: (%.2f, %.2f)\n",
	     input->mouseState.position.x, input->mouseState.position.y);
      printButtonState(input->mouseState.buttons[MouseButton_left], "left");
      printButtonState(input->mouseState.buttons[MouseButton_right], "right");     
#endif

#if 0
      printButtonState(input->keyboardState.keys[KeyboardButton_tab], "tab");
      printButtonState(input->keyboardState.keys[KeyboardButton_backspace], "backspace");
#endif

      //v2 dMouseP = pluginState->mouseP.xy - pluginState->lastMouseP.xy;      
      
      v4 defaultColor = V4(1, 1, 1, 1);
      v4 hoverColor = V4(1, 1, 0, 1);
      v4 activeColor = V4(0.8f, 0.8f, 0, 1);

      u32 windowWidth = renderCommands->widthInPixels;
      u32 windowHeight = renderCommands->heightInPixels;
      LoadedFont *font = &pluginState->testFont;
#if 1     
      UILayout *layout = &pluginState->layout;
      uiBeginLayout(layout, V2(windowWidth, windowHeight), &input->mouseState);
#if 0
      printf("\nlayout:\n  mouseP: (%.2f, %.2f)\n  left pressed: %s\n  left released: %s\n  left down: %s\n",
	     layout->mouseP.x, layout->mouseP.y,
	     layout->leftButtonPressed ? "true" : "false",
	     layout->leftButtonReleased ? "true" : "false",
	     layout->leftButtonDown ? "true" : "false");
#endif
      UIComm title = uiMakeTextElement(layout, "I Will Become a Granular Synthesizer!", 0.75f, 0.f);
      
      UIComm play = uiMakeButton(layout, "play", V2(0.75f, 0.75f), V2(0.1f, 0.1f),
				     &pluginState->soundIsPlaying, V4(0, 0, 1, 1));
      if(play.flags & UICommFlag_leftPressed)
	{
	  //pluginState->loadedSound.isPlaying = true;
	  ASSERT(play.element->parameterType = UIParameter_boolean);
	  pluginSetBooleanParameter(play.element->bParam, true);
	}
            
      UIComm volume = uiMakeSlider(layout, "volume", V2(0.1f, 0.1f), V2(0.1f, 0.75f),
				   &pluginState->volume,// makeRange(0.f, 1.f),
				   V4(0.8f, 0.8f, 0.8f, 1));
#if 0
      printf("\nvolume flags:\n %u(comm)\n %u(element)\n", volume.flags, volume.element->commFlags);
      printf("volume rect:\n  min: (%.2f, %.2f)\n  max: (%.2f, %.2f)\n",
	     volume.element->region.min.x, volume.element->region.min.y,
	     volume.element->region.max.x, volume.element->region.max.y);
#endif
      if(volume.flags & UICommFlag_leftDragging)
	{
	  v2 dMouseP = layout->mouseP.xy - layout->lastMouseP.xy;	  
	  ASSERT(volume.element->parameterType == UIParameter_float);
	  //r32 *parameter = (r32 *)volume.element->data;	  
	  //r32 oldVal = *parameter;
	  r32 oldVal = pluginReadFloatParameter(volume.element->fParam);
	  //r32 newVal = clampToRange(oldVal + dMouseP.y/(r32)windowHeight, volume.element->range);
	  r32 newVal = oldVal + dMouseP.y/getDim(volume.element->region).y;
	  //*parameter = newVal;
	  pluginSetFloatParameter(volume.element->fParam, newVal);
	}

      if(wasPressed(input->keyboardState.keys[KeyboardButton_tab]))
	{
	  layout->selectedElementOrdinal = (layout->selectedElementOrdinal + 1) % (layout->elementCount + 1);

	  //layout->selectedElement->isActive = false;
	  layout->selectedElement = {};
	}
      
      if(wasPressed(input->keyboardState.keys[KeyboardButton_backspace]))
	{
	  if(layout->selectedElementOrdinal)
	    {
	      --layout->selectedElementOrdinal;
	    }	  

	  //layout->selectedElement->isActive = false;
	  layout->selectedElement = {};
	}

      UIElement *selectedElement = 0;
      for(u32 i = 0; i < layout->selectedElementOrdinal; ++i)
	{	  
	  if(selectedElement)
	    {
	      bool advanced = false;
	      if(selectedElement->first)
		{
		  if(selectedElement->first != ELEMENT_SENTINEL(selectedElement))
		    {
		      selectedElement = selectedElement->first;
		      advanced = true;
		    }
		}
	      
	      if(!advanced && selectedElement->next)
		{
		  if(selectedElement->next != ELEMENT_SENTINEL(selectedElement->parent))
		    {
		      selectedElement = selectedElement->next;
		      advanced = true;
		    }
		}
	      
	      ASSERT(advanced);
	    }
	  else
	    {
	      selectedElement = layout->root;
	    }
	}

      // TODO: merge this keyboard interaction codepath with the mouse interaction codepath
      if(selectedElement)
	{
	  selectedElement->lastFrameTouched = layout->frameIndex;
	  //selectedElement->isActive = true;

	  if(selectedElement->flags & UIElementFlag_draggable)
	    {	      
	      if(selectedElement->parameterType == UIParameter_float)
		{
		  r32 parameter = pluginReadFloatParameter(selectedElement->fParam);//(r32 *)selectedElement->data;
		  r32 newParameter = parameter;
		  if(wasPressed(input->keyboardState.keys[KeyboardButton_minus]))
		    {
		      newParameter = parameter - 0.02f*getLength(selectedElement->fParam->range);
		      //newParameter = clampToRange(newParameter, selectedElement->range);			  
		      //*parameter = newParameter;		      
		    }
		      
		  if(wasPressed(input->keyboardState.keys[KeyboardButton_equal]) &&
		     isDown(input->keyboardState.modifiers[KeyboardModifier_shift]))
		    {
		      newParameter = parameter + 0.02f*getLength(selectedElement->fParam->range);
		      //newParameter = clampToRange(newParameter, selectedElement->range);			  
		      //*parameter = newParameter;
		    }
		  
		  pluginSetFloatParameter(selectedElement->fParam, newParameter);
		}
	    }
	  else if(selectedElement->flags & UIElementFlag_clickable)
	    {	      
	      if(selectedElement->parameterType == UIParameter_boolean)
		{
		  bool parameter = pluginReadBooleanParameter(selectedElement->bParam);
		  if(wasPressed(input->keyboardState.keys[KeyboardButton_enter]))
		    {
		      //bool oldVal = *parameter;
		      bool newVal = !parameter;		      
		      //*parameter = newVal;
		      pluginSetBooleanParameter(selectedElement->bParam, newVal);
		    }
		}
	    }

	  //UIElement *persistentSelectedElement = uiCacheElement(selectedElement, layout);
	  //layout->selectedElement = persistentSelectedElement;
	  layout->selectedElement = selectedElement->hashKey;
	}      
      
      //uiPrintLayout(layout);
      renderPushUILayout(renderCommands, layout);
      uiEndLayout(layout);
      printf("permanent arena used %llu bytes\n", pluginState->permanentArena.used);

#else
      v2 textOutAt = V2(0, windowHeight);
      textOutAt = renderPushText(renderCommands, font, "", textOutAt);

      char *message = "I Will Become a Granular Synthesizer!";
      //Rectangle2 messageRect = getTextRectangle(font, message, windowWidth, windowHeight);
      //renderPushRectOutline(renderCommands, messageRect, 0.01f, V4(1, 1, 0, 1));
      //TextArgs textArgs = {};
      //textArgs.flags |= (TextFlag_centered | TextFlag_scaleAspect);
      //textArgs.hScale = 0.5f;
      textOutAt = renderPushText(renderCommands, font, message, textOutAt);

      renderPushQuad(renderCommands, rectCenterDim(V2(0, 0), V2(0.5f, 0.5f)), &pluginState->testBitmap);

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
      Vertex playVertex1 = makeVertex(playButton.min + V2(0.2f*playButtonDim.x, 0.2f*playButtonDim.y),
				      V2(0, 0), playColor);
      Vertex playVertex2 = makeVertex(playButton.min + V2(0.2f*playButtonDim.x, 0.8f*playButtonDim.y),
				      V2(0, 0), playColor);
      Vertex playVertex3 = makeVertex(playButton.min + V2(0.8f*playButtonDim.x, 0.5f*playButtonDim.y),
				      V2(0, 0), playColor);
      renderPushTriangle(renderCommands, playVertex1, playVertex2, playVertex3);

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
#endif

      //pluginState->lastMouseP = pluginState->mouseP;
      arenaEnd(&pluginState->frameArena);
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
      // TODO: move the midi loop inside the output loop, so that we don't only see the most recent midi message
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
		    pluginSetFloatParameter(&pluginState->volume, (r32)noteVelocity/127.f);
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

      r64 nFreq = M_TAU*pluginState->freq/(r64)audioBuffer->sampleRate;

      bool soundIsPlaying = pluginReadBooleanParameter(&pluginState->soundIsPlaying);
      PlayingSound *loadedSound = &pluginState->loadedSound;
      r32 *loadedSoundSamples[2] = {};
      if(soundIsPlaying)
	{
	  loadedSoundSamples[0] = loadedSound->sound.samples[0] + (u32)loadedSound->samplesPlayed;
	  loadedSoundSamples[1] = loadedSound->sound.samples[1] + (u32)loadedSound->samplesPlayed;
	}
      
      r32 formatVolumeFactor = 1.f;      
      switch(audioBuffer->format)
	{
	case AudioFormat_s16:
	  {
	    formatVolumeFactor = 32000.f;
	  } break;
	case AudioFormat_r32: break;
	default: { ASSERT(!"invalid audio format"); } break;
	}

      void *genericAudioFrames = audioBuffer->buffer;
      r32 sampleRateRatio = (r32)INTERNAL_SAMPLE_RATE/(r32)audioBuffer->sampleRate;      
      for(u32 i = 0; i < audioBuffer->framesToWrite; ++i)
	{
	  r32 volume = formatVolumeFactor*pluginReadFloatParameter(&pluginState->volume);
		
	  pluginState->phasor += nFreq;
	  if(pluginState->phasor > M_TAU) pluginState->phasor -= M_TAU;
				  
	  r32 sinVal = sin(pluginState->phasor);
	  for(u32 j = 0; j < audioBuffer->channels; ++j)
	    {
	      r32 mixedVal = 0.5f*sinVal;

	      soundIsPlaying = pluginReadBooleanParameter(&pluginState->soundIsPlaying);
	      if(soundIsPlaying)		       
		{
		  r32 soundReadPosition = sampleRateRatio*i;
		  if((loadedSound->samplesPlayed + soundReadPosition + 1) < loadedSound->sound.sampleCount)
		    {		      
		      u32 soundReadIndex = (u32)soundReadPosition;
		      r32 soundReadFrac = soundReadPosition - soundReadIndex;

		      r32 firstSoundVal = loadedSoundSamples[j][soundReadIndex];
		      r32 nextSoundVal = loadedSoundSamples[j][soundReadIndex + 1];
		      r32 loadedSoundVal = lerp(firstSoundVal, nextSoundVal, soundReadFrac);
		      mixedVal += 0.5f*loadedSoundVal;
		    }
		  else
		    {		      
		      pluginSetBooleanParameter(&pluginState->soundIsPlaying, false);
		      loadedSound->samplesPlayed = 0;
		    }
		}

	      switch(audioBuffer->format)
		{
		case AudioFormat_r32:
		  {
		    r32 *audioFrames = (r32 *)genericAudioFrames;
		    *audioFrames++ = volume*mixedVal;
		    genericAudioFrames = audioFrames;
		  } break;
		case AudioFormat_s16:
		  {
		    s16 *audioFrames = (s16 *)genericAudioFrames;
		    *audioFrames++ = (s16)(volume*mixedVal);
		    genericAudioFrames = audioFrames;
		  } break;
		}
	    }

	  pluginUpdateFloatParameter(&pluginState->volume);
	}

      soundIsPlaying = pluginReadBooleanParameter(&pluginState->soundIsPlaying);
      if(soundIsPlaying)
	{
	  loadedSound->samplesPlayed += sampleRateRatio*audioBuffer->framesToWrite;
	}      
    }
}
