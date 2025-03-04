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

   - UI:
     - screen reader support
     - bitmap assets for ui elements
     - allow specification of horizontal vs. vertical slider, or knob for draggable elements
     - render element name, with control over position (ie above, below, left, right)
     - collapsable element groups, sized relative to children
     - tooltips when hovering (eg show numeric parameter value)
     - resizable regions
     - specify render order
     - interface for selecting files to load at runtime

   - Parameters:
     - come up with what parameters we want to have
     - make parameter reads/updates atomic
     - put parameters in an array (or multiple), for keyboard/midi mapping

   - File Loading:
     - more audio file formats (eg flac, ogg, mp3)
     - speed up reads (ie SIMD)
     - better samplerate conversion (ie FFT method (TODO: speed up fft and czt))
     - load in batches to pass to ML model (maybe)     

   - MIDI:
     - pass timestamps to the midi buffer, and read new messages based on timestamps
     - map midi channels to parameters
     - handle more message codes

   - Make the audioProcess() do granular synth things
*/  

#include <stdio.h>

#include "common.h"
#include "plugin.h"

#include "midi.cpp"
#include "ui_layout.cpp"
#include "file_granulator.cpp"

PlatformAPI globalPlatform;

inline void
printButtonState(ButtonState button, char *name)
{
  printf("%s: %s, %s\n", name,
	 wasPressed(button) ? "pressed" : "not pressed",
	 isDown(button) ? "down" : "up");
}

//static PluginState *
//initializePluginState(PluginMemory *memoryBlock)
extern "C"
INITIALIZE_PLUGIN_STATE(initializePluginState)
{
  void *memory = memoryBlock->memory;
  PluginState *pluginState = 0;
  if(memory)
    {      
      pluginState = (PluginState *)memory;
      if(!pluginState->initialized)
	{
	  //if(globalPlatform.syncCompareAndSwap(&pluginState->initializationMutex, 0, 1) == 1)
	    {
	      globalPlatform = memoryBlock->platformAPI;

	      pluginState->osTimerFreq = memoryBlock->osTimerFreq;

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
	      pluginState->volume.range = makeRange(0.f, 1.f);
	  
	      pluginState->density.currentValue = 1.f;
	      pluginState->density.targetValue = 1.f;
	      pluginState->density.range = makeRange(0.f, 20.f);
	  
	      pluginState->soundIsPlaying.value = false;

	      pluginState->loadedSound.sound = loadWav("../data/fingertips_44100_PCM_16.wav",
						       &pluginState->loadArena, &pluginState->permanentArena);
	      pluginState->loadedSound.samplesPlayed = 0;

	      char *fingertipsPackfilename = "../data/fingertips.grains";
	      TemporaryMemory packfileMemory = arenaBeginTemporaryMemory(&pluginState->loadArena, MEGABYTES(64));
	      GrainPackfile fingertipsGrains = beginGrainPackfile((Arena *)&packfileMemory);
	      addSoundToGrainPackfile(&fingertipsGrains, &pluginState->loadedSound.sound);
	      writePackfileToDisk(&fingertipsGrains, fingertipsPackfilename);
	      arenaEndTemporaryMemory(&packfileMemory);

	      pluginState->loadedGrainPackfile = loadGrainPackfile(fingertipsPackfilename,
								   &pluginState->permanentArena);
	      pluginState->silo = initializeFileGrainState(&pluginState->permanentArena);	  

	      //pluginState->testBitmap = loadBitmap("../data/signal_z.bmp", &pluginState->permanentArena);

	      RangeU32 characterRange = {32, 127}; // NOTE: from SPACE up to (but not including) DEL
	      pluginState->testFont = loadFont("../data/arial.ttf", &pluginState->loadArena,
					       &pluginState->permanentArena, characterRange, 32.f);

	      pluginState->uiContext = uiInitializeContext(&pluginState->frameArena, &pluginState->permanentArena,
							   &pluginState->testFont);
	      //pluginState->layout = uiInitializeLayout(&pluginState->frameArena, &pluginState->permanentArena,
	      //				       &pluginState->testFont);

	      pluginState->rootPanel = arenaPushStruct(&pluginState->permanentArena, UIPanel);
	      pluginState->rootPanel->sizePercentOfParent = 1.f;
	      pluginState->rootPanel->splitAxis = UIAxis_y;	  

	      UIPanel *currentParentPanel = pluginState->rootPanel;
	      UIPanel *bottom = makeUIPanel(currentParentPanel, &pluginState->permanentArena,
					    UIAxis_x, 0.2f, "bottom", V4(0.094f, 0.149f, 0.102f, 1));
	      UIPanel *top = makeUIPanel(currentParentPanel, &pluginState->permanentArena,
					 UIAxis_x, 0.8f, "top");
	      UNUSED(bottom);
	      	      
	      currentParentPanel = top;
	      UIPanel *left = makeUIPanel(currentParentPanel, &pluginState->permanentArena,
					  UIAxis_x, 0.5f, "left", V4(0.094f, 0.102f, 0.149f, 1));
	      UIPanel *right = makeUIPanel(currentParentPanel, &pluginState->permanentArena,
					   UIAxis_x, 0.5f, "right", V4(0.149f, 0.102f, 0.094f, 1));
	      UNUSED(left);
	      UNUSED(right);	      	      
#if 0
#if 1
	      r32 *testModelInput = pluginState->loadedSound.sound.samples[0];
	      //s64 testModelInputSampleCount = pluginState->loadedSound.sound.sampleCount;
	      s64 testModelInputSampleCount = 2400; // TODO: feed the whole file
#else	  
	      ReadFileResult testInputFile = globalPlatform.readEntireFile("../data/test_input.data",
									   &pluginState->permanentArena);
	      ReadFileResult testOutputFile = globalPlatform.readEntireFile("../data/test_output.data",
									    &pluginState->permanentArena);
	      r32 *testModelInput = (r32 *)testInputFile.contents;
	      s64 testModelInputSampleCount = (testInputFile.contentsSize - 1)/(2*sizeof(r32));
#endif
	      void *outputData = globalPlatform.runModel(testModelInput, testModelInputSampleCount);
	      r32 *outputFloat = (r32 *)outputData;
#endif

	      pluginState->initialized = true;
	    }
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
      
      u32 windowWidth = renderCommands->widthInPixels;
      u32 windowHeight = renderCommands->heightInPixels;
      v2 windowDim = V2((r32)windowWidth, (r32)windowHeight);
      Rect2 screenRect = rectMinDim(V2(0, 0), windowDim);

      // NOTE: UI layout
#if 1
      //UILayout *layout = &pluginState->layout;
      //uiBeginLayout(layout, V2(windowWidth, windowHeight), &input->mouseState, &input->keyboardState);
      UIContext *uiContext = &pluginState->uiContext;
      uiContextNewFrame(uiContext, &input->mouseState, &input->keyboardState, renderCommands->windowResized);
      
      TemporaryMemory scratchMemory = arenaBeginTemporaryMemory(&pluginState->frameArena, KILOBYTES(128));           
      
      // NOTE: non-leaf ui
      for(UIPanel *panel = pluginState->rootPanel; panel; panel = uiPanelIteratorDepthFirstPreorder(panel).next)
	{
	  Rect2 panelRect = uiComputeRectFromPanel(panel, screenRect, (Arena *)&scratchMemory);
	  v2 panelDim = getDim(panelRect);

	  if(panel->first)
	    {
	      UILayout *panelLayout = &panel->layout;
	      uiBeginLayout(panelLayout, uiContext, panelRect);
	  
	      for(UIPanel *child = panel->first; child && child->next; child = child->next)
		{
		  Rect2 childRect = uiComputeChildPanelRect(child, panelRect);
		  Rect2 boundaryRect = childRect;
		  UIAxis splitAxis = panel->splitAxis;
		  boundaryRect.min.E[splitAxis] = boundaryRect.max.E[splitAxis];
		  boundaryRect.min.E[splitAxis] -= 2;
		  boundaryRect.max.E[splitAxis] += 2;
	      
		  UIComm hresize = uiMakeBox(&panel->layout, "hresize", boundaryRect,
					     UIElementFlag_clickable |
					     UIElementFlag_drawBackground |
					     UIElementFlag_drawBorder);
		  if(isInRectangle(boundaryRect, uiContext->mouseP.xy))
		    {
		      renderChangeCursorState(renderCommands,
					      (splitAxis == UIAxis_x) ? CursorState_hArrow : CursorState_vArrow);
		    }
		  UIPanel *minChild = child;
		  UIPanel *maxChild = child->next;
		  if(hresize.flags & UICommFlag_pressed)
		    {
		      uiStoreDragData(hresize.element,
				      V2(minChild->sizePercentOfParent, maxChild->sizePercentOfParent));
		    }
		  if(hresize.flags & UICommFlag_dragging)
		    {
		      renderChangeCursorState(renderCommands,
					      splitAxis == UIAxis_x ? CursorState_hArrow : CursorState_vArrow);
		  
		      v2 dragData = uiLoadDragData(hresize.element);
		      v2 dragDelta = uiGetDragDelta(hresize.element);
		      printf("dragData: (%.2f, %.2f)\n", dragData.x, dragData.y);
		      printf("dragDelta: (%.2f, %.2f)\n", dragDelta.x, dragDelta.y);
		  
		      r32 minChildPercentPreDrag = dragData.x;
		      r32 maxChildPercentPreDrag = dragData.y;
		      r32 minChildPixelsPreDrag = minChildPercentPreDrag*panelDim.E[splitAxis];
		      r32 maxChildPixelsPreDrag = maxChildPercentPreDrag*panelDim.E[splitAxis];
		      r32 minChildPixelsPostDrag = minChildPixelsPreDrag + dragDelta.E[splitAxis];
		      r32 maxChildPixelsPostDrag = maxChildPixelsPreDrag - dragDelta.E[splitAxis];
		      r32 minChildPercentPostDrag = minChildPixelsPostDrag/panelDim.E[splitAxis];
		      r32 maxChildPercentPostDrag = maxChildPixelsPostDrag/panelDim.E[splitAxis];
		      minChild->sizePercentOfParent = clampToRange(minChildPercentPostDrag, 0, 1);
		      maxChild->sizePercentOfParent = clampToRange(maxChildPercentPostDrag, 0, 1);
	
		      //v2 mouseDelta = (isInRectangle(panelRect, layout->mouseP.xy) ?
		      //		   (layout->lastMouseP.xy - layout->mouseP.xy) : V2(0, 0));
		      //printf("mouseDelta: (%.2f, %.2f)\n", mouseDelta.x, mouseDelta.y);
		      //r32 sizeChange = mouseDelta.E[child->splitAxis]/windowDim.E[child->splitAxis];
		      //child->sizePercentOfParent -= sizeChange;
		      //child->next->sizePercentOfParent += sizeChange;
		    }

		  //renderPushQuad(renderCommands, boundaryRect, 0, 0, RenderLevel_front);
		}

	      renderPushUILayout(renderCommands, panelLayout);
	      uiEndLayout(panelLayout);
	    }
	}
      
      // NOTE: leaf ui
      for(UIPanel *panel = pluginState->rootPanel; panel; panel = uiPanelIteratorDepthFirstPreorder(panel).next)
	{
	  // NOTE: calculate rectangles
	  Rect2 panelRect = uiComputeRectFromPanel(panel, screenRect, (Arena *)&scratchMemory);	  
	  	  
	  // NOTE: build ui
	  if(!panel->first)
	    {
	      //printf("panelRect:\n  min: (%.2f, %.2f)\n  max: (%.2f, %.2f)\n",
	      //    panelRect.min.x, panelRect.min.y, panelRect.max.x, panelRect.max.y);
	      
	      renderPushQuad(renderCommands, panelRect, 0, 0, RenderLevel_background, panel->color);
	      
	      UILayout *panelLayout = &panel->layout;
	      uiBeginLayout(panelLayout, uiContext, panelRect);

	      //printf("layoutRootRect:\n  min: (%.2f, %.2f)\n  max: (%.2f, %.2f)\n",
	      //     panelLayout->root->region.min.x, panelLayout->root->region.min.y,
	      //     panelLayout->root->region.max.x, panelLayout->root->region.max.y);

	      UIComm text = uiMakeTextElement(panelLayout, (char *)panel->name, 0.2f, 0.f);
	      UNUSED(text);

	      // TODO: don't use strings to check which panel we are in
	      if(stringsAreEqual(panel->name, (u8 *)"left"))
		{
		  UIComm volume = uiMakeSlider(panelLayout, "volume", V2(0.1f, 0.1f), V2(0.1f, 0.5f),
					       &pluginState->volume, V4(0.8f, 0.8f, 0.8f, 1));

		  if(volume.flags & UICommFlag_hovering)
		    {
		      volume.element->color = hadamard(volume.element->color, V4(1, 1, 0, 1));
		    }

		  r32 oldVolume = pluginReadFloatParameter(volume.element->fParam);
		  r32 newVolume = oldVolume;
		  if(volume.flags & UICommFlag_dragging)
		    {
		      if(volume.flags & UICommFlag_leftDragging)
			{	
			  v2 dragDelta = uiGetDragDelta(volume.element);
			  printf("dragDelta: (%.2f, %.2f)\n", dragDelta.x, dragDelta.y);
		      
			  newVolume = volume.element->fParamValueAtClick + dragDelta.y/getDim(volume.element->region).y;
			}
		      else if(volume.flags & UICommFlag_minusPressed)
			{
			  newVolume -= 0.02f;
			}
		      else if(volume.flags & UICommFlag_plusPressed)
			{
			  newVolume += 0.02f;
			}
		    }		  

		  pluginSetFloatParameter(volume.element->fParam, newVolume);
		}
	      else if(stringsAreEqual(panel->name, (u8 *)"right"))
		{
		  //
		  // play
		  //
		  UIComm play = uiMakeButton(panelLayout, "play", V2(0.7f, 0.7f), V2(0.2f, 0.2f),
					     &pluginState->soundIsPlaying, V4(0, 0, 1, 1));

		  bool oldPlay = pluginReadBooleanParameter(play.element->bParam);
		  bool newPlay = oldPlay;
		  if(play.flags & UICommFlag_pressed)
		    {
		      newPlay = !oldPlay;
		      // TODO: stop doing the queue here: we don't want to have to synchronize grain (de)queueing
		      //       across the audio and video threads (once we actually have them on their own threads)
		      queueAllGrainsFromFile(&pluginState->silo, &pluginState->loadedGrainPackfile);
		    }

		  pluginSetBooleanParameter(play.element->bParam, newPlay);

		  //
		  // density
		  //
		  UIComm density = uiMakeKnob(panelLayout, "density", V2(0.7f, 0.3f), V2(0.2f, 0.2f),
					      &pluginState->density, V4(0, 1, 0, 1));
		  
		  r32 oldDensity = pluginReadFloatParameter(density.element->fParam);
		  r32 newDensity = oldDensity;
		  printf("oldDensity: %.2f\n", oldDensity);		  
		  if(density.flags & UICommFlag_dragging)
		    {
		      v2 dragDelta = uiGetDragDelta(density.element);
		      printf("dragDelta: (%.2f, %.2f)\n", dragDelta.x, dragDelta.y);
		      
		      newDensity = density.element->fParamValueAtClick + 0.1f*dragDelta.y;
		      printf("newDensity: %.2f\n", newDensity);
		    }

		  pluginSetFloatParameter(density.element->fParam, newDensity);
		}

	      renderPushUILayout(renderCommands, panelLayout);
	      uiEndLayout(panelLayout);
	    }	  
	}

      arenaEndTemporaryMemory(&scratchMemory);
      uiContextEndFrame(uiContext);
#else
      UILayout *layout = &pluginState->layout;
      uiBeginLayout(layout, V2(windowWidth, windowHeight), &input->mouseState, &input->keyboardState);
      
#if 0
      printf("\nlayout:\n  mouseP: (%.2f, %.2f)\n  left pressed: %s\n  left released: %s\n  left down: %s\n",
	     layout->mouseP.x, layout->mouseP.y,
	     layout->leftButtonPressed ? "true" : "false",
	     layout->leftButtonReleased ? "true" : "false",
	     layout->leftButtonDown ? "true" : "false");
#endif
      
      UIComm title = uiMakeTextElement(layout, "I Will Become a Granular Synthesizer!", 0.75f, 0.f);
      (void)title;
      
      UIComm play = uiMakeButton(layout, "play", V2(0.75f, 0.75f), V2(0.1f, 0.1f),
				     &pluginState->soundIsPlaying, V4(0, 0, 1, 1));
      if(play.flags & UICommFlag_pressed)
	{	  
	  ASSERT(play.element->parameterType = UIParameter_boolean);
	  pluginSetBooleanParameter(play.element->bParam, true);

	  // TODO: stop doing the queue here: we don't want to have to synchronize grain (de)queueing across
	  //       the audio and video threads (once we actually have them on their own threads)
	  //r64 currentTimestamp = (r64)globalPlatform.getCurrentTimestamp()/(r64)pluginState->osTimerFreq;
	  //printf("grainQueue: %llu\n", currentTimestamp);
	  queueAllGrainsFromFile(&pluginState->silo, &pluginState->loadedGrainPackfile);
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
      if(volume.flags & UICommFlag_dragging)
	{
	  Rect2 volumeRegion = volume.element->region;
	  v2 regionDim = getDim(volumeRegion);
	  
	  ASSERT(volume.element->parameterType == UIParameter_float);
	  r32 oldVal = pluginReadFloatParameter(volume.element->fParam);
	  r32 newVal = oldVal;
	  if(volume.flags & UICommFlag_leftDragging)
	    {  
	      v2 clippedMouseP = clipToRect(layout->mouseP.xy, volumeRegion);	  
	      newVal = (clippedMouseP.y - volumeRegion.min.y)/regionDim.y;
	    }
	  else if(volume.flags & UICommFlag_minusPressed)
	    {
	      newVal -= 0.02f*getLength(volume.element->fParam->range);
	    }
	  else if(volume.flags & UICommFlag_plusPressed)
	    {
	      newVal += 0.02f*getLength(volume.element->fParam->range);
	    }
#if 0
	  printf("mouseP: (%.2f, %.2f)\n", layout->mouseP.x, layout->mouseP.y);
	  printf("clippedMouseP: (%.2f, %.2f)\n", clippedMouseP.x, clippedMouseP.y);	  
	  printf("newVolume: %.2f\n", newVal);
	  printf("volume dim: %.2f\n", getDim(volume.element->region).y);
#endif
	  
	  pluginSetFloatParameter(volume.element->fParam, newVal);
	}

      UIComm density = uiMakeKnob(layout, "density", V2(0.75f, 0.5f), V2(0.1f, 0.1f),
				  &pluginState->density, V4(0, 1, 0, 1));
      if(density.flags & UICommFlag_dragging)
	{
	  //Rect2 knobDragRect = rectCenterDim(density.element->mouseClickedP - V2(20, 0), V2(5, 50));	  
	  //renderPushQuad(renderCommands, knobDragRect, 0, 0, V4(0, 0, 0, 1));
	  
	  r32 oldValue = density.element->fParamValueAtClick;	  
	  v2 dragDelta = layout->mouseP.xy - density.element->mouseClickedP;	  
	  r32 newValue = oldValue + 0.1f*dragDelta.y;
#if 0
	  printf("layoutMouseP: (%.2f, %.2f)\n", layout->mouseP.x, layout->mouseP.y);
	  printf("mouseClickedP: (%.2f, %.2f)\n", density.element->mouseClickedP.x, density.element->mouseClickedP.y);
	  printf("oldValue: %.2f\n", oldValue);
	  printf("newValue: %.2f\n", newValue);
#endif
	  
	  ASSERT(density.element->parameterType == UIParameter_float);
	  pluginSetFloatParameter(density.element->fParam, newValue);
	}

      UIComm paramList = uiMakeCollapsibleList(layout, "parameters", V2(200, 100), V2(0.2f, 0.f));
      if(paramList.flags & UICommFlag_clicked)
	{
	  paramList.element->showChildren = !paramList.element->showChildren;
	  
	  uiPushParentElement(layout, paramList.element);
	  {
	    uiMakeKnob(layout, "squamblicity", V2(0, 0), V2(0.1f, 0.1f), 0, V4(1, 1, 0, 1));
	    uiMakeKnob(layout, "blatherousness", V2(0.2f, 0), V2(0.1f, 0.1f), 0, V4(0, 1, 1, 1));
	  }
	  uiPopParentElement(layout);
	}

      // NOTE: keyboard navigation
      if(wasPressed(input->keyboardState.keys[KeyboardButton_tab]))
	{
	  layout->selectedElementOrdinal = (layout->selectedElementOrdinal + 1) % (layout->elementCount + 1);	  
	  layout->selectedElement = {};
	}
      
      if(wasPressed(input->keyboardState.keys[KeyboardButton_backspace]))
	{
	  if(layout->selectedElementOrdinal)
	    {
	      --layout->selectedElementOrdinal;
	    }	  

	  layout->selectedElement = {};
	}

      //printf("element ordinal: %u\n", layout->selectedElementOrdinal);
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
      
      if(selectedElement)
	{	
	  layout->selectedElement = selectedElement->hashKey;
	}      
      
      //uiPrintLayout(layout);
      renderPushUILayout(renderCommands, layout);
      uiEndLayout(layout);
#endif
      
      arenaEnd(&pluginState->frameArena);
      printf("permanent arena used %zu bytes\n", pluginState->permanentArena.used);
    }  
}

extern "C"
AUDIO_PROCESS(audioProcess)
{  
  PluginState *pluginState = initializePluginState(memory);  
  if(pluginState->initialized)
    {

      r64 nFreq = M_TAU*pluginState->freq/(r64)audioBuffer->sampleRate;

      bool soundIsPlaying = pluginReadBooleanParameter(&pluginState->soundIsPlaying);
      PlayingSound *loadedSound = &pluginState->loadedSound;
      r32 *loadedSoundSamples[2] = {};
      if(soundIsPlaying)
	{
	  loadedSoundSamples[0] = loadedSound->sound.samples[0] + (u32)loadedSound->samplesPlayed;
	  loadedSoundSamples[1] = loadedSound->sound.samples[1] + (u32)loadedSound->samplesPlayed;
	}
      
      r32 *grainMixBuffers[2];
      u32 framesToWrite = audioBuffer->framesToWrite;
      //r64 grainMixTimestamp = (r64)globalPlatform.getCurrentTimestamp()/(r64)pluginState->osTimerFreq;
      //printf("\ngrainMixTimestamp: %llu\n", grainMixTimestamp);            

      r32 formatVolumeFactor = 1.f;
      u32 destSampleRate = audioBuffer->sampleRate;
      r32 sampleRateRatio = (r32)INTERNAL_SAMPLE_RATE/(r32)destSampleRate;
      switch(audioBuffer->format)
	{
	case AudioFormat_s16:
	  {
	    formatVolumeFactor = 32000.f;
	  } break;
	case AudioFormat_r32: break;
	default: { ASSERT(!"invalid audio format"); } break;
	}
            
      TemporaryMemory grainMixerMemory = arenaBeginTemporaryMemory(&pluginState->permanentArena, KILOBYTES(128));
      u32 scaledFramesToWrite = (u32)(sampleRateRatio*framesToWrite) + 1;
      grainMixBuffers[0] = arenaPushArray((Arena *)&grainMixerMemory, scaledFramesToWrite, r32,
					  arenaFlagsZeroNoAlign());
      grainMixBuffers[1] = arenaPushArray((Arena *)&grainMixerMemory, scaledFramesToWrite, r32,
					  arenaFlagsZeroNoAlign());
      mixPlayingGrains(grainMixBuffers[0], grainMixBuffers[1],
		       1.f, scaledFramesToWrite, &pluginState->silo);

      void *genericAudioFrames = audioBuffer->buffer;      
      for(u32 frameIndex = 0; frameIndex < audioBuffer->framesToWrite; ++frameIndex)
	{

		midi::parseMidiMessage(audioBuffer, pluginState);

	  r32 volume = 0.1f*formatVolumeFactor*pluginReadFloatParameter(&pluginState->volume);	  	  
		
	  pluginState->phasor += nFreq;
	  if(pluginState->phasor > M_TAU) pluginState->phasor -= M_TAU;
				  
	  r32 sinVal = sin(pluginState->phasor);
	  for(u32 channelIndex = 0; channelIndex < audioBuffer->channels; ++channelIndex)
	    {
	      r32 mixedVal = 0.5f*sinVal;
	      
	      r32 grainReadPosition = sampleRateRatio*frameIndex;
	      u32 grainReadIndex = (u32)grainReadPosition;
	      r32 grainReadFrac = grainReadPosition - grainReadIndex;
	      
	      r32 firstGrainVal = grainMixBuffers[channelIndex][grainReadIndex];
	      r32 nextGrainVal = grainMixBuffers[channelIndex][grainReadIndex + 1];
	      r32 grainVal = lerp(firstGrainVal, nextGrainVal, grainReadFrac);
	      mixedVal += 0.5f*grainVal;

	      soundIsPlaying = pluginReadBooleanParameter(&pluginState->soundIsPlaying);
	      if(soundIsPlaying)		       
		{
		  r32 soundReadPosition = sampleRateRatio*frameIndex;
		  if((loadedSound->samplesPlayed + soundReadPosition + 1) < loadedSound->sound.sampleCount)
		    {		      
		      u32 soundReadIndex = (u32)soundReadPosition;
		      r32 soundReadFrac = soundReadPosition - soundReadIndex;

		      r32 firstSoundVal = loadedSoundSamples[channelIndex][soundReadIndex];
		      r32 nextSoundVal = loadedSoundSamples[channelIndex][soundReadIndex + 1];
		      r32 loadedSoundVal = lerp(firstSoundVal, nextSoundVal, soundReadFrac);
		      //mixedVal += 0.5f*loadedSoundVal;
		      (void)loadedSoundVal;
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

		default: ASSERT(!"ERROR: invalid audio format");
		}
	    }

	  pluginUpdateFloatParameter(&pluginState->volume);
	  pluginUpdateFloatParameter(&pluginState->density);
	}

      arenaEndTemporaryMemory(&grainMixerMemory);

      soundIsPlaying = pluginReadBooleanParameter(&pluginState->soundIsPlaying);
      if(soundIsPlaying)
	{
	  loadedSound->samplesPlayed += sampleRateRatio*audioBuffer->framesToWrite;
	}      
    }
}
