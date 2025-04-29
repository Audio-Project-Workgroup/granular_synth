// the core audio-visual functionality of our plugin is implemented here

/* NOTE:
   - elements are passed to the renderer in pixel space (where (0, 0) is the bottom-left of the
     drawing region, and (windowWidthInPixels, windowHeightInPixels) is the top-right).
     
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
     - interface for selecting files to load at runtime
     - interface for remapping midi cc channels with parameters
     - display the state of grains and the grain buffer (wip)
     - make panel resizing not jank

   - Parameters (high priority):
     - more parameters
     - make parameters visible to the daw to enable parameter modification via automation
     - parameter display number -> processing number transformation for nonlinear parameters

   - File Loading: (on hold)
     - more audio file formats (eg flac, ogg, mp3)
     - speed up reads (ie SIMD)
     - better samplerate conversion (ie FFT method (TODO: speed up fft and czt))
     - load in batches to pass to ML model (maybe)     

   - MIDI (high priority):
     - cc channel - parameter remapping at runtime

   - Grains (high priority):
     - more grain parameters    
     - add grain post-processing effects like pitch-shifting, time-stretching and stereo-widening

   - Misc:
     - speed up fft (aligned loads/stores, different radix?)
     - pull native executable audio loop into its own thread
     - work queues?
     - allocate hardware ring buffer?
*/  

//#include <stdio.h>

#include "plugin.h"

#include "fft_test.cpp"
#include "midi.cpp"
#include "ui_layout.cpp"
#include "file_granulator.cpp"
#include "internal_granulator.cpp"

PlatformAPI globalPlatform;
PluginLogger *globalLogger;

inline void
printButtonState(ButtonState button, char *name)
{
  printf("%s: %s, %s\n", name,
	 wasPressed(button) ? "pressed" : "not pressed",
	 isDown(button) ? "down" : "up");
}

static PluginState *
getPluginState(PluginMemory *memoryBlock)
{
  void *memory = memoryBlock->memory;
  PluginState *pluginState = 0;
  if(memory)
    {      
      pluginState = (PluginState *)memory;
    }

  return(pluginState);
}

extern "C"
INITIALIZE_PLUGIN_STATE(initializePluginState)
{
  globalPlatform = memoryBlock->platformAPI;
  globalLogger = memoryBlock->logger;

  void *memory = memoryBlock->memory;
  PluginState *pluginState = 0;
  if(memory)
    {      
      pluginState = (PluginState *)memory;
      if(!pluginState->initialized)
	{
	    {
	      pluginState->osTimerFreq = memoryBlock->osTimerFreq;
	      pluginState->pluginHost = memoryBlock->host;
	      pluginState->pluginMode = PluginMode_editor;

	      pluginState->permanentArena = arenaBegin((u8 *)memory + sizeof(PluginState), MEGABYTES(512));
	      pluginState->frameArena = arenaSubArena(&pluginState->permanentArena, MEGABYTES(64));
	      pluginState->framePermanentArena = arenaSubArena(&pluginState->permanentArena, MEGABYTES(64));
	      pluginState->loadArena = arenaSubArena(&pluginState->permanentArena, MEGABYTES(128));
	      pluginState->grainArena = arenaSubArena(&pluginState->permanentArena, KILOBYTES(32));
	      	      	     
	      TemporaryMemory fftTestMemory = arenaBeginTemporaryMemory(&pluginState->loadArena, KILOBYTES(1));
	      bool fftTestResult = fftTest((Arena *)&fftTestMemory);
	      UNUSED(fftTestResult);
	      arenaEndTemporaryMemory(&fftTestMemory);

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

	      // NOTE: parameter initialization
	      pluginState->phasor = 0.f;
	      pluginState->freq = 440.f;
	      
	      initializeFloatParameter(&pluginState->parameters[PluginParameter_volume], 
				       pluginParameterInitData[PluginParameter_volume], decibelsToAmplitude);
	      
	      initializeFloatParameter(&pluginState->parameters[PluginParameter_density],
				       pluginParameterInitData[PluginParameter_density], densityTransform);
	      
	      //avail range: [0, 3], default value: 0
	      initializeFloatParameter(&pluginState->parameters[PluginParameter_window], 
				       pluginParameterInitData[PluginParameter_window]);
	      
	      //avail range: [0, 16000], default value: 2600
	      initializeFloatParameter(&pluginState->parameters[PluginParameter_size], 
				       pluginParameterInitData[PluginParameter_size]);
	      
	      initializeFloatParameter(&pluginState->parameters[PluginParameter_spread],
						pluginParameterInitData[PluginParameter_spread]);
	      
	      initializeFloatParameter(&pluginState->parameters[PluginParameter_mix],
						pluginParameterInitData[PluginParameter_mix]);
	      
	      initializeFloatParameter(&pluginState->parameters[PluginParameter_offset],
						pluginParameterInitData[PluginParameter_offset]);

	      // NOTE: devices
	      pluginState->outputDeviceCount = memoryBlock->outputDeviceCount;
	      pluginState->selectedOutputDeviceIndex = memoryBlock->selectedOutputDeviceIndex;
	      for(u32 i = 0; i < pluginState->outputDeviceCount; ++i)
		{
		  pluginState->outputDeviceNames[i] =
		    arenaPushString(&pluginState->permanentArena, memoryBlock->outputDeviceNames[i]);
		}
	      
	      pluginState->inputDeviceCount = memoryBlock->inputDeviceCount;
	      pluginState->selectedInputDeviceIndex = memoryBlock->selectedInputDeviceIndex;
	      for(u32 i = 0; i < pluginState->inputDeviceCount; ++i)
		{
		  pluginState->inputDeviceNames[i] =
		    arenaPushString(&pluginState->permanentArena, memoryBlock->inputDeviceNames[i]);
		}

	      // NOTE: grain buffer initialization
	      u32 grainBufferCount = 48000;
	      pluginState->grainBuffer = initializeGrainBuffer(pluginState, grainBufferCount);
	      pluginState->grainManager = initializeGrainManager(pluginState);

	      // NOTE: grain view initialization
	      GrainStateView *grainStateView = &pluginState->grainStateView;
	      grainStateView->viewWriteIndex = 1;
	      grainStateView->viewBuffer.capacity = grainBufferCount;
	      grainStateView->viewBuffer.samples[0] = arenaPushArray(&pluginState->permanentArena,
								     grainStateView->viewBuffer.capacity, r32,
								     arenaFlagsNoZeroAlign(4*sizeof(r32)));
	      grainStateView->viewBuffer.samples[1] = arenaPushArray(&pluginState->permanentArena,
								     grainStateView->viewBuffer.capacity, r32,
								     arenaFlagsNoZeroAlign(4*sizeof(r32)));
	      grainStateView->viewBuffer.readIndex = pluginState->grainBuffer.readIndex;
	      grainStateView->viewBuffer.writeIndex = pluginState->grainBuffer.writeIndex;

	      u32 grainViewSampleCapacity = 4096;
	      for(u32 i = 0; i < ARRAY_COUNT(grainStateView->views); ++i)
		{
		  GrainBufferViewEntry *view = grainStateView->views + i;
		  view->sampleCapacity = grainViewSampleCapacity;
		  view->bufferSamples[0] = arenaPushArray(&pluginState->permanentArena, grainViewSampleCapacity, r32);
		  view->bufferSamples[1] = arenaPushArray(&pluginState->permanentArena, grainViewSampleCapacity, r32);
		}

	      // NOTE: file loading, embedded grain caching
	      pluginState->soundIsPlaying.value = false;
	      pluginState->loadedSound.sound = loadWav("../data/fingertips_44100_PCM_16.wav",
						       &pluginState->loadArena, &pluginState->permanentArena);	      
	      pluginState->loadedSound.samplesPlayed = 0;// + pluginState->start_pos);

	      char *fingertipsPackfilename = "../data/fingertips.grains";
	      TemporaryMemory packfileMemory = arenaBeginTemporaryMemory(&pluginState->loadArena, MEGABYTES(64));
	      GrainPackfile fingertipsGrains = beginGrainPackfile((Arena *)&packfileMemory);
	      addSoundToGrainPackfile(&fingertipsGrains, &pluginState->loadedSound.sound);
	      writePackfileToDisk(&fingertipsGrains, fingertipsPackfilename);
	      arenaEndTemporaryMemory(&packfileMemory);

	      pluginState->loadedGrainPackfile = loadGrainPackfile(fingertipsPackfilename,
								   &pluginState->permanentArena);
	      pluginState->silo = initializeFileGrainState(&pluginState->permanentArena);	  	      

	      pluginState->nullTexture = makeBitmap(&pluginState->permanentArena, 1920, 1080, 0xFFFFFFFF);
	      pluginState->editorReferenceLayout =
		loadBitmap("../data/BMP/GRANADE_UI_POSITIONSREFERENCE.bmp", &pluginState->permanentArena);
	      pluginState->editorSkin =
		loadBitmap("../data/BMP/TREE.bmp", &pluginState->permanentArena);
	      pluginState->pomegranateKnob =
		loadBitmap("../data/BMP/POMEGRANATE_BUTTON.bmp", &pluginState->permanentArena,
			   V2(0, -0.03f));
	      pluginState->pomegranateKnobLabel =
		loadBitmap("../data/BMP/POMEGRANATE_BUTTON_WHITEMARKERS.bmp", &pluginState->permanentArena,
			   V2(0, 0));
	      pluginState->halfPomegranateKnob =
		loadBitmap("../data/BMP/HALFPOMEGRANATE_BUTTON.bmp", &pluginState->permanentArena,
			   V2(-0.005f, -0.035f));
	      pluginState->halfPomegranateKnobLabel =
		loadBitmap("../data/BMP/HALFPOMEGRANATE_BUTTON_WHITEMARKERS.bmp", &pluginState->permanentArena,
			   V2(0, 0));
	      pluginState->densityKnob =
		loadBitmap("../data/BMP/DENSITYPOMEGRANATE_BUTTON.bmp", &pluginState->permanentArena,
			   V2(0.01f, -0.022f));
	      pluginState->densityKnobShadow =
		loadBitmap("../data/BMP/DENSITYPOMEGRANATE_BUTTON_SHADOW.bmp", &pluginState->permanentArena);
	      pluginState->densityKnobLabel =
		loadBitmap("../data/BMP/DENSITYPOMEGRANATE_BUTTON_WHITEMARKERS.bmp", &pluginState->permanentArena,
			   V2(0, 0));
	      pluginState->levelBar =
		loadBitmap("../data/BMP/LEVELBAR.bmp", &pluginState->permanentArena);
	      pluginState->levelFader =
		loadBitmap("../data/BMP/LEVELBAR_SLIDINGLEVER.bmp", &pluginState->permanentArena,
			   V2(0, -0.416f));
	      pluginState->grainViewBackground =
		loadBitmap("../data/BMP/GREENFRAME_RECTANGLE.bmp", &pluginState->permanentArena);
	      pluginState->grainViewOutline =
		loadBitmap("../data/BMP/GREENFRAME.bmp", &pluginState->permanentArena);

	      RangeU32 characterRange = {32, 127}; // NOTE: from SPACE up to (but not including) DEL
	      pluginState->testFont = loadFont("../data/arial.ttf", &pluginState->loadArena,
					       &pluginState->permanentArena, characterRange, 32.f);
	      pluginState->agencyBold = loadFont("../data/FONT/AGENCYB.ttf", &pluginState->loadArena,
					       &pluginState->permanentArena, characterRange, 32.f);

	      // NOTE: ui initialization
	      pluginState->uiContext =
		uiInitializeContext(&pluginState->frameArena, &pluginState->framePermanentArena,
				    &pluginState->agencyBold);

	      pluginState->rootPanel = arenaPushStruct(&pluginState->permanentArena, UIPanel,
						       arenaFlagsZeroNoAlign());
	      pluginState->rootPanel->sizePercentOfParent = 1.f;
	      pluginState->rootPanel->splitAxis = UIAxis_y;
	      pluginState->rootPanel->name = STR8_LIT("editor");
	      pluginState->rootPanel->color = V4(1, 1, 1, 1);
	      //pluginState->rootPanel->texture = &pluginState->editorSkin;
	      pluginState->rootPanel->texture = &pluginState->editorReferenceLayout;
#if 0 
	      UIPanel *currentParentPanel = pluginState->rootPanel;
	      UIPanel *bottom = makeUIPanel(currentParentPanel, &pluginState->permanentArena,
					    UIAxis_x, 0.2f, STR8_LIT("bottom"), V4(0.094f, 0.149f, 0.102f, 1));
	      UIPanel *top = makeUIPanel(currentParentPanel, &pluginState->permanentArena,
					 UIAxis_x, 0.8f, STR8_LIT("top"));
	      UNUSED(bottom);
	      	      
	      currentParentPanel = top;
	      UIPanel *left = makeUIPanel(currentParentPanel, &pluginState->permanentArena,
					  UIAxis_x, 0.5f, STR8_LIT("left"), V4(0.094f, 0.102f, 0.149f, 1));
	      UIPanel *right = makeUIPanel(currentParentPanel, &pluginState->permanentArena,
					   UIAxis_x, 0.5f, STR8_LIT("right"), V4(0.149f, 0.102f, 0.094f, 1));
	      UNUSED(left);
	      UNUSED(right);
#endif
	      pluginState->menuPanel = arenaPushStruct(&pluginState->permanentArena, UIPanel,
						       arenaFlagsZeroNoAlign());
	      pluginState->menuPanel->sizePercentOfParent = 1.f;
	      pluginState->menuPanel->splitAxis = UIAxis_x;

	      v4 menuBackgroundColor = colorV4FromU32(0x080C1CFF);
	      UIPanel *currentParentPanel = pluginState->menuPanel;
	      UIPanel *menuLeft = makeUIPanel(currentParentPanel, &pluginState->permanentArena,
					      UIAxis_x, 0.5f, STR8_LIT("menu left"),
					      &pluginState->nullTexture, menuBackgroundColor);
	      UIPanel *menuRight = makeUIPanel(currentParentPanel, &pluginState->permanentArena,
					       UIAxis_x, 0.5f, STR8_LIT("menu right"),
					       &pluginState->nullTexture, menuBackgroundColor);
	      UNUSED(menuLeft);
	      UNUSED(menuRight);
	      
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
      renderBeginCommands(renderCommands, &pluginState->frameArena);
     
      logFormatString("mouseP: (%.2f, %.2f)", input->mouseState.position.x, input->mouseState.position.y);
      logFormatString("mouseLeft: %s, %s",
		      wasPressed(input->mouseState.buttons[MouseButton_left]) ? "pressed" : "not pressed",
		      isDown(input->mouseState.buttons[MouseButton_left]) ? "down" : "up");
      logFormatString("mouseRight: %s, %s",
		      wasPressed(input->mouseState.buttons[MouseButton_right]) ? "pressed" : "not pressed",
		      isDown(input->mouseState.buttons[MouseButton_right]) ? "down" : "up");
      
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
      UIContext *uiContext = &pluginState->uiContext;
      uiContextNewFrame(uiContext, &input->mouseState, &input->keyboardState, renderCommands->windowResized);
      
      TemporaryMemory scratchMemory = arenaBeginTemporaryMemory(&pluginState->frameArena, KILOBYTES(128));

      if(pluginState->pluginHost == PluginHost_executable)
	{
	  if(uiContext->escPressed)
	    {
	      pluginState->pluginMode = (PluginMode)!pluginState->pluginMode;
	    }
	}

      // NOTE: standalone i/o device selection      
      if(pluginState->pluginMode == PluginMode_menu)
	{
	  for(UIPanel *panel = pluginState->menuPanel; panel; panel = uiPanelIteratorDepthFirstPreorder(panel).next)
	    {
	      Rect2 panelRect = uiComputeRectFromPanel(panel, screenRect, (Arena *)&scratchMemory);	      

	      if(!panel->first)
		{
		  UILayout *panelLayout = &panel->layout;
		  uiBeginLayout(panelLayout, uiContext, panelRect, panel->color, panel->texture);		 

		  r32 textScale = 0.7f;
		  v4 baseTextColor = colorV4FromU32(0xFFDEADFF);
		  v4 hoveringTextColor = colorV4FromU32(0x98F5FFFF);
		  v4 selectedTextColor = colorV4FromU32(0xFFD700FF);

		  if(stringsAreEqual(panel->name, STR8_LIT("menu left")))
		    {
		      for(u32 outputDeviceIndex = 0;
			  outputDeviceIndex < pluginState->outputDeviceCount;
			  ++outputDeviceIndex)
			{
			  bool isSelected = (outputDeviceIndex == pluginState->selectedOutputDeviceIndex);
			  v4 textColor =  isSelected ? selectedTextColor : baseTextColor;
			  String8 outputDeviceName = pluginState->outputDeviceNames[outputDeviceIndex];
#if 1
			  UIComm outputDevice = uiMakeSelectableTextElement(panelLayout, outputDeviceName,
									    textScale, textColor);
			  if(outputDevice.flags & UICommFlag_hovering)
			    {
			      if(!isSelected) outputDevice.element->color = hoveringTextColor;
			    }
			  if(outputDevice.flags & UICommFlag_pressed)
			    {
			      pluginState->selectedOutputDeviceIndex = outputDeviceIndex;

			      renderCommands->outputAudioDeviceChanged = true;
			      renderCommands->selectedOutputAudioDeviceIndex = outputDeviceIndex;
			    }
#endif
			}
		    }
		  else if(stringsAreEqual(panel->name, STR8_LIT("menu right")))
		    {
		      for(u32 inputDeviceIndex = 0;
			  inputDeviceIndex < pluginState->inputDeviceCount;
			  ++inputDeviceIndex)
			{
			  bool isSelected = (inputDeviceIndex == pluginState->selectedInputDeviceIndex);
			  v4 textColor = isSelected ? selectedTextColor : baseTextColor;
			  String8 inputDeviceName = pluginState->inputDeviceNames[inputDeviceIndex];
#if 1
			  UIComm inputDevice = uiMakeSelectableTextElement(panelLayout, inputDeviceName,
									   textScale, textColor);
			  if(inputDevice.flags & UICommFlag_hovering)
			    {
			      if(!isSelected) inputDevice.element->color = hoveringTextColor;
			    }
			  if(inputDevice.flags & UICommFlag_pressed)
			    {
			      pluginState->selectedInputDeviceIndex = inputDeviceIndex;

			      renderCommands->inputAudioDeviceChanged = true;
			      renderCommands->selectedInputAudioDeviceIndex = inputDeviceIndex;
			    }
#endif
			}
		    }

		  renderPushUILayout(renderCommands, panelLayout);
		  uiEndLayout(panelLayout);
		}
	    }
	}
      else
	{
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

		      if(splitAxis == UIAxis_x)
			{
			  child->fringeFlags |= UIFringeFlag_right;
			  child->next->fringeFlags |= UIFringeFlag_left;
			}
		      else
			{
			  child->fringeFlags |= UIFringeFlag_top;
			  child->next->fringeFlags |= UIFringeFlag_bottom;
			}
		    
		      boundaryRect.min.E[splitAxis] = boundaryRect.max.E[splitAxis];
		      boundaryRect.min.E[splitAxis] -= 3;
		      boundaryRect.max.E[splitAxis] += 3;
	      
		      UIComm hresize = uiMakeBox(&panel->layout, STR8_LIT("hresize"), boundaryRect,
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
			  //printf("dragData: (%.2f, %.2f)\n", dragData.x, dragData.y);
			  //printf("dragDelta: (%.2f, %.2f)\n", dragDelta.x, dragDelta.y);
		  
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
			}	
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
		  // NOTE: background skin
		  //renderPushQuad(renderCommands, panelRect, panel->texture, 0, RenderLevel_background, panel->color);	      
		  UILayout *panelLayout = &panel->layout;
		  uiBeginLayout(panelLayout, uiContext, panelRect, panel->color, panel->texture);
		  uiPushLayoutOffsetSizeType(panelLayout, UISizeType_percentOfParent);
		  uiPushLayoutDimSizeType(panelLayout, UISizeType_percentOfParent);

		  // UIComm text = uiMakeTextElement(panelLayout, panel->name, 1.5);
		  // UNUSED(text);

#if 1
		  v2 panelDim = getDim(panelRect);
		  v2 elementTextScale = 0.0005f*panelDim.x*V2(1.f, 1.f);

		  // NOTE: volume
		  v2 volumeOffsetPOP = V2(0.87f, 0.17f);
		  v2 volumeDimPOP = V2(0.2f, 0.45f);
		  v2 volumeClickableOffset = V2(0.3f, 0);
		  v2 volumeClickableDim = V2(0.4f, 1.f);
		  v2 volumeTextOffset = hadamard(V2(0.001f, -0.022f), panelDim);
		  UIComm volume = uiMakeSlider(panelLayout, STR8_LIT("LEVEL"), volumeOffsetPOP, volumeDimPOP, 0.5f,
					       &pluginState->parameters[PluginParameter_volume],
					       volumeClickableOffset, volumeClickableDim,
					       volumeTextOffset, elementTextScale,
					       &pluginState->levelBar, &pluginState->levelFader, 0,
					       V4(1, 1, 1, 1));

		  if(volume.flags & UICommFlag_hovering)
		    {
		      volume.element->color = hadamard(volume.element->color, V4(1, 1, 0, 1));
		    }
		      
		  if(volume.flags & UICommFlag_dragging)
		    {
		      if(volume.flags & UICommFlag_leftDragging)
			{	
			  v2 dragDelta = uiGetDragDelta(volume.element);			  
		      
			  r32 newVolume =
			    (volume.element->fParamValueAtClick +
			     getLength(volume.element->fParam->range)*dragDelta.y/getDim(volume.element->region).y);
			      
			  pluginSetFloatParameter(volume.element->fParam, newVolume);
			}
		      else if(volume.flags & UICommFlag_minusPressed)
			{
			  r32 oldVolume = pluginReadFloatParameter(volume.element->fParam);
			  r32 newVolume = oldVolume - 0.02f*getLength(volume.element->fParam->range);

			  pluginSetFloatParameter(volume.element->fParam, newVolume);
			}
		      else if(volume.flags & UICommFlag_plusPressed)
			{
			  r32 oldVolume = pluginReadFloatParameter(volume.element->fParam);
			  r32 newVolume = oldVolume + 0.02f*getLength(volume.element->fParam->range);

			  pluginSetFloatParameter(volume.element->fParam, newVolume);
			}
		    }			

		  r32 knobDimPOP = 0.235f;
		  v2 knobLabelOffset = V2(0.02f, 0.06f);
		  v2 knobLabelDim = V2(0.96f, 1);
		  v2 knobClickableOffset = V2(0.1f, 0.2f);
		  v2 knobClickableDim = V2(0.8f, 0.7f);
		  v2 knobTextOffset = hadamard(V2(0, 0.04f), panelDim);
		  r32 knobDragLength = 0.35f*panelDim.y;

		  // NOTE: size
		  v2 sizeOffsetPOP = V2(0.239f, 0.109f);
		  v2 sizeDimPOP = knobDimPOP*V2(1, 1);		  
		  UIComm size =
		    uiMakeKnob(panelLayout, STR8_LIT("SIZE"), sizeOffsetPOP, sizeDimPOP, 1.f,
			       &pluginState->parameters[PluginParameter_size],
			       knobLabelOffset, knobLabelDim,
			       knobClickableOffset, knobClickableDim,
			       knobTextOffset, elementTextScale,
			       &pluginState->pomegranateKnob, &pluginState->pomegranateKnobLabel,
			       V4(1, 1, 1, 1));
		  if(size.flags & UICommFlag_dragging)
		    {
		      if(size.flags & UICommFlag_leftDragging)
			{			
			  v2 dragDelta = uiGetDragDelta(size.element);
			  // post processing to set the new grain size using a temporary workaround: 
			  // We scaled dragDelta.x by x20 to cover approximately the full grain-size scale across width resolution of the screen.
			  // i.e. dragging fully to the left reduces grain size and sets it to a value towards zero ..
			  // .. and dragging fully to the right sets the grain-size towards a value near its max value.
			  // @TODO fix temporary workaround		
			  r32 newGrainSize = (size.element->fParamValueAtClick +
					      dragDelta.y/knobDragLength*getLength(size.element->fParam->range));
					
			  pluginSetFloatParameter(size.element->fParam, newGrainSize);
			}
		      else if(size.flags & UICommFlag_minusPressed)
			{
			  r32 oldSize = pluginReadFloatParameter(size.element->fParam);
			  r32 newSize = oldSize - 0.02f*getLength(size.element->fParam->range);

			  pluginSetFloatParameter(size.element->fParam, newSize);
			}
		      else if(size.flags & UICommFlag_plusPressed)
			{
			  r32 oldSize = pluginReadFloatParameter(size.element->fParam);
			  r32 newSize = oldSize + 0.02f*getLength(size.element->fParam->range);

			  pluginSetFloatParameter(size.element->fParam, newSize);
			}
		    }		  

		  // NOTE: play (for now)
		  {
		    v2 playOffsetPOP = V2(0.8f, 0.65f);
		    v2 playSizePOP = knobDimPOP*V2(1, 1);
		    UIComm play = uiMakeButton(panelLayout, STR8_LIT("play"), playOffsetPOP, playSizePOP, 1.f,
					       &pluginState->soundIsPlaying, 0, V4(0, 0, 1, 1));
		    
		    if(play.flags & UICommFlag_pressed)
		      {
			bool oldPlay = pluginReadBooleanParameter(play.element->bParam);
			bool newPlay = !oldPlay;
			pluginSetBooleanParameter(play.element->bParam, newPlay);
			// TODO: stop doing the queue here: we don't want to have to synchronize grain (de)queueing
			//       across the audio and video threads (once we actually have them on their own threads)
			//queueAllGrainsFromFile(&pluginState->silo, &pluginState->loadedGrainPackfile);
		      }		    
		  }

		  // NOTE: spread
		  {
		    v2 spreadOffsetPOP = V2(-0.001f, 0.067f);
		    v2 spreadSizePOP = knobDimPOP*V2(1, 1);
		    UIComm spread = uiMakeKnob(panelLayout, STR8_LIT("SPREAD"), spreadOffsetPOP, spreadSizePOP, 1.f,
					       &pluginState->parameters[PluginParameter_spread],
					       knobLabelOffset, knobLabelDim,
					       knobClickableOffset, knobClickableDim,
					       knobTextOffset, elementTextScale,
					       &pluginState->pomegranateKnob, &pluginState->pomegranateKnobLabel,
					       V4(1, 1, 1, 1));
		    if(spread.flags & UICommFlag_dragging)
		      {
			if(spread.flags & UICommFlag_leftDragging)
			  {
			    v2 dragDelta = uiGetDragDelta(spread.element);
			    //printf("dragDelta: (%.2f, %.2f)\n", dragDelta.x, dragDelta.y);
			    r32 newSpread = (spread.element->fParamValueAtClick +
					     dragDelta.y/knobDragLength*getLength(spread.element->fParam->range));
			    pluginSetFloatParameter(spread.element->fParam, newSpread);
			  }
			else if(spread.flags & UICommFlag_minusPressed)
			  {
			    r32 oldSpread = pluginReadFloatParameter(spread.element->fParam);
			    r32 newSpread = oldSpread - 0.02f*getLength(spread.element->fParam->range);

			    pluginSetFloatParameter(spread.element->fParam, newSpread);
			  }
			else if(spread.flags & UICommFlag_plusPressed)
			  {
			    r32 oldSpread = pluginReadFloatParameter(spread.element->fParam);
			    r32 newSpread = oldSpread + 0.02f*getLength(spread.element->fParam->range);

			    pluginSetFloatParameter(spread.element->fParam, newSpread);
			  }
		      }
		  }

		  // NOTE: density
		  {
		    r32 densityDimPOP = 0.235f;
		    v2 densityOffsetPOP = V2(0.45f, 0.06f);
		    v2 densitySizePOP = densityDimPOP*V2(1, 1);
		    v2 densityClickableOffset = V2(0.f, 0.1f);
		    v2 densityClickableDim = V2(1.f, 0.9f);
		    v2 densityTextOffset = hadamard(V2(0, -0.015f), panelDim);

		    UIComm density = uiMakeKnob(panelLayout, STR8_LIT("DENSITY"),
						densityOffsetPOP, densitySizePOP, 1.f,
						&pluginState->parameters[PluginParameter_density],
						V2(-0.02f, 0.02f), V2(1.02f, 1),
						densityClickableOffset, densityClickableDim,
						densityTextOffset, elementTextScale,
						&pluginState->densityKnob, &pluginState->densityKnobLabel,
						V4(1, 1, 1, 1));
		    Rect2 densityRect = density.element->region;
		    v2 densityRectCenter = getCenter(densityRect);
		    renderPushQuad(renderCommands, rectCenterDim(densityRectCenter, V2(4, 4)), 0, 0, RenderLevel_front);
		    if(density.flags & UICommFlag_dragging)
		      {
			if(density.flags & UICommFlag_leftDragging)
			  {
			    v2 dragDelta = uiGetDragDelta(density.element);		      			    
			    r32 newDensity = (density.element->fParamValueAtClick +
					     dragDelta.y/knobDragLength*getLength(density.element->fParam->range));
			    
			    pluginSetFloatParameter(density.element->fParam, newDensity);			
			  }
			else if(density.flags & UICommFlag_minusPressed)
			  {
			    r32 oldDensity = pluginReadFloatParameter(density.element->fParam);
			    r32 newDensity = oldDensity - 0.02f*getLength(density.element->fParam->range);

			    pluginSetFloatParameter(density.element->fParam, newDensity);
			  }
			else if(density.flags & UICommFlag_plusPressed)
			  {
			    r32 oldDensity = pluginReadFloatParameter(density.element->fParam);
			    r32 newDensity = oldDensity + 0.02f*getLength(density.element->fParam->range);

			    pluginSetFloatParameter(density.element->fParam, newDensity);
			  }
		      }
		  }
		  
		  // NOTE: window
		  {
		    v2 windowOffsetPOP = V2(0.1f, 0.5f);
		    v2 windowSizePOP = knobDimPOP*V2(1, 1);
		    UIComm window = uiMakeKnob(panelLayout, STR8_LIT("WINDOW"), windowOffsetPOP, windowSizePOP, 1.f,
					       &pluginState->parameters[PluginParameter_window],
					       knobLabelOffset, knobLabelDim,
					       knobClickableOffset, knobClickableDim,
					       knobTextOffset, elementTextScale,
					       &pluginState->pomegranateKnob, &pluginState->pomegranateKnobLabel,
					       V4(1, 1, 1, 1));
		    if(window.flags & UICommFlag_dragging)
		      {
			if(window.flags & UICommFlag_leftDragging)
			  {
			    v2 dragDelta = uiGetDragDelta(window.element);		
			    r32 newWindow = (window.element->fParamValueAtClick +
					     dragDelta.y/knobDragLength*getLength(window.element->fParam->range));

			    pluginSetFloatParameter(window.element->fParam, newWindow);
			  }
			else if(window.flags & UICommFlag_minusPressed)
			  {
			    r32 oldWindow = pluginReadFloatParameter(window.element->fParam);
			    r32 newWindow = oldWindow - 0.02f*getLength(window.element->fParam->range);

			    pluginSetFloatParameter(window.element->fParam, newWindow);
			  }
			else if(window.flags & UICommFlag_plusPressed)
			  {
			    r32 oldWindow = pluginReadFloatParameter(window.element->fParam);
			    r32 newWindow = oldWindow + 0.02f*getLength(window.element->fParam->range);

			    pluginSetFloatParameter(window.element->fParam, newWindow);
			  }
		      }
		  }
		  
		  // NOTE: mix
		  {
		    r32 mixDimPOP = 0.235f;
		    v2 mixOffsetPOP = V2(0.6399f, 0.1121f);
		    v2 mixSizePOP = mixDimPOP*V2(1, 1);
		    UIComm mix =
		      uiMakeKnob(panelLayout, STR8_LIT("MIX"), mixOffsetPOP, mixSizePOP, 1.f,
				 &pluginState->parameters[PluginParameter_mix],
				 knobLabelOffset, knobLabelDim,
				 knobClickableOffset, knobClickableDim,
				 knobTextOffset, elementTextScale,
				 &pluginState->halfPomegranateKnob, &pluginState->halfPomegranateKnobLabel,
				 V4(1, 1, 1, 1));
		    Rect2 mixRect = mix.element->region;
		    v2 mixRectCenter = getCenter(mixRect);
		    renderPushQuad(renderCommands, rectCenterDim(mixRectCenter, V2(4, 4)), 0, 0, RenderLevel_front, V4(0, 0, 0, 1));
		    if(mix.flags & UICommFlag_dragging)
		      {
			if(mix.flags & UICommFlag_leftDragging)
			  {
			    v2 dragDelta = uiGetDragDelta(mix.element);
			    r32 newMix = (mix.element->fParamValueAtClick +
					     dragDelta.y/knobDragLength*getLength(mix.element->fParam->range));

			    pluginSetFloatParameter(mix.element->fParam, newMix);
			  }
			else if(mix.flags & UICommFlag_minusPressed)
			  {
			    r32 oldMix = pluginReadFloatParameter(mix.element->fParam);
			    r32 newMix = oldMix - 0.02f*getLength(mix.element->fParam->range);

			    pluginSetFloatParameter(mix.element->fParam, newMix);
			  }
			else if(mix.flags & UICommFlag_plusPressed)
			  {
			    r32 oldMix = pluginReadFloatParameter(mix.element->fParam);
			    r32 newMix = oldMix + 0.02f*getLength(mix.element->fParam->range);

			    pluginSetFloatParameter(mix.element->fParam, newMix);
			  }
		      }
		  }
		  
		  // NOTE: offset
		  {
		    v2 offsetOffsetPOP = V2(0.1285f, 0.004f);
		    v2 offsetSizePOP = knobDimPOP*V2(1, 1);
		    UIComm offset = uiMakeKnob(panelLayout, STR8_LIT("OFFSET"), offsetOffsetPOP, offsetSizePOP, 1.f,
					       &pluginState->parameters[PluginParameter_offset],
					       knobLabelOffset, knobLabelDim,
					       knobClickableOffset, knobClickableDim,
					       knobTextOffset, elementTextScale,
					       &pluginState->pomegranateKnob, &pluginState->pomegranateKnobLabel,
					       V4(1, 1, 1, 1));
		    Rect2 offsetRect = offset.element->region;
		    v2 offsetRectCenter = getCenter(offsetRect);
		    renderPushQuad(renderCommands, rectCenterDim(offsetRectCenter, V2(4, 4)), 0, 0, RenderLevel_front, V4(0, 0, 0, 1));
		    if(offset.flags & UICommFlag_dragging)
		      {
			if(offset.flags & UICommFlag_leftDragging)
			  {
			    v2 dragDelta = uiGetDragDelta(offset.element);
			    r32 newOffset = (offset.element->fParamValueAtClick +
					     dragDelta.y/knobDragLength*getLength(offset.element->fParam->range));

			    pluginSetFloatParameter(offset.element->fParam, newOffset);
			  }
			else if(offset.flags & UICommFlag_minusPressed)
			  {
			    r32 oldOffset = pluginReadFloatParameter(offset.element->fParam);
			    r32 newOffset = oldOffset - 0.02f*getLength(offset.element->fParam->range);

			    pluginSetFloatParameter(offset.element->fParam, newOffset);
			  }
			else if(offset.flags & UICommFlag_plusPressed)
			  {
			    r32 oldOffset = pluginReadFloatParameter(offset.element->fParam);
			    r32 newOffset = oldOffset + 0.02f*getLength(offset.element->fParam->range);

			    pluginSetFloatParameter(offset.element->fParam, newOffset);
			  }
		      }
		  }
		  
		  // NOTE: grain view		  
		  logString("\ndisplaying grain buffer\n");

		  v2 drawRegionDim = getDim(panelLayout->regionRemaining);
		  v2 viewDim = hadamard(V2(0.53f, 0.25f), drawRegionDim);
		  v2 viewMin = hadamard(V2(0.24f, 0.55f), drawRegionDim);
		  Rect2 viewRect = rectMinDim(viewMin, viewDim);
		  
		  renderPushRectOutline(renderCommands, viewRect, 2.f, RenderLevel_front, V4(1, 1, 1, 1));
		  renderPushQuad(renderCommands, viewRect, &pluginState->grainViewBackground, 0, RenderLevel_front);
		  renderPushQuad(renderCommands, viewRect, &pluginState->grainViewOutline, 0, RenderLevel_front);

		  v2 dim = hadamard(V2(0.843f, 0.62f), viewDim);
		  v2 min = viewMin + hadamard(V2(0.075f, 0.2f), viewDim);
		  Rect2 rect = rectMinDim(min, dim);
		  renderPushRectOutline(renderCommands, rect, 2.f, RenderLevel_front, V4(1, 1, 1, 1));
		  

		  r32 middleBarThickness = 4.f;
		  Rect2 middleBar = rectMinDim(min + V2(0, 0.5f*dim.y),
					       V2(dim.x, middleBarThickness));
		  renderPushQuad(renderCommands, middleBar, 0, 0.f, RenderLevel_front, V4(0, 0, 0, 1));

		  v2 upperRegionMin = min + V2(0, 0.5f*(dim.y + middleBarThickness));
		  v2 lowerRegionMin = min;
		  v2 regionDim = V2(dim.x, 0.5f*dim.y - middleBarThickness);
		  v2 lowerRegionMiddle = lowerRegionMin + V2(0, 0.5f*regionDim.y);
		  v2 upperRegionMiddle = upperRegionMin + V2(0, 0.5f*regionDim.y);

		  u32 grainBufferCapacity = pluginState->grainBuffer.capacity;
		  GrainStateView *grainStateView = &pluginState->grainStateView;
		  AudioRingBuffer *grainViewBuffer = &grainStateView->viewBuffer;
		  
		  u32 viewEntryReadIndex = grainStateView->viewReadIndex;
		  u32 entriesQueued = globalPlatform.atomicLoad(&grainStateView->entriesQueued);
		  logFormatString("entriesQueued: %u", entriesQueued);
		  
		  for(u32 entryIndex = 0; entryIndex < entriesQueued; ++entryIndex)
		    {		     		  
		      GrainBufferViewEntry *view = (grainStateView->views +
						    ((viewEntryReadIndex + entryIndex) %
						     ARRAY_COUNT(grainStateView->views)));

		      // NOTE: update view buffer with new view data
		      writeSamplesToAudioRingBuffer(grainViewBuffer,
						    view->bufferSamples[0], view->bufferSamples[1],
						    view->sampleCount);		      
		      // u32 bufferReadIndex = grainStateView->viewBuffer.readIndex;
		      // u32 bufferWriteIndex = grainStateView->viewBuffer.writeIndex;
		      u32 bufferReadIndex = view->bufferReadIndex;
		      u32 bufferWriteIndex = view->bufferWriteIndex;

		      // NOTE: display view read and write positions
		      r32 barThickness = 2.f;
		      u32 readIndex = bufferReadIndex;
		      r32 readPosition = (r32)readIndex/(r32)grainBufferCapacity;
		      r32 readBarPosition = readPosition*dim.x;
		      Rect2 readBar = rectMinDim(min + V2(readBarPosition, 0.f),
						 V2(barThickness, dim.y));
		      renderPushQuad(renderCommands, readBar, 0, 0.f, RenderLevel_front, V4(1, 0, 0, 1));
		      
		      u32 writeIndex = bufferWriteIndex;
		      r32 writePosition = (r32)writeIndex/(r32)grainBufferCapacity;
		      r32 writeBarPosition = writePosition*dim.x;
		      Rect2 writeBar = rectMinDim(min + V2(writeBarPosition, 0.f),
						  V2(barThickness, dim.y));
		      renderPushQuad(renderCommands, writeBar, 0, 0.f, RenderLevel_front, V4(1, 1, 1, 1));

		      // NOTE: display playing grain start and end positions
		      v4 grainWindowColors[] =
			{
			  //colorV4FromU32(0xFF4000FF),
			  colorV4FromU32(0xFF8000FF),
			  colorV4FromU32(0xFFBF00FF),
			  colorV4FromU32(0xFFFF00FF),
			  
			  colorV4FromU32(0xBFFF00FF),
			  colorV4FromU32(0x80FF00FF),
			  colorV4FromU32(0x40FF00FF),
			  colorV4FromU32(0x00FF00FF),

			  colorV4FromU32(0x00FF40FF),
			  colorV4FromU32(0x00FF80FF),
			  colorV4FromU32(0x00FFBFFF),
			  colorV4FromU32(0x00FFFFFF),

			  colorV4FromU32(0x00BFFFFF),
			  colorV4FromU32(0x0080FFFF),
			  colorV4FromU32(0x0040FFFF),
			  colorV4FromU32(0x0000FFFF),

			  colorV4FromU32(0x4000FFFF),
			  colorV4FromU32(0x8000FFFF),
			  colorV4FromU32(0xBF00FFFF),
			  colorV4FromU32(0xFF00FFFF),

			  colorV4FromU32(0xFF00BFFF),
			  colorV4FromU32(0xFF0080FF),
			  //colorV4FromU32(0xFF0040FF),
			};

		      for(u32 grainViewIndex = 0; grainViewIndex < view->grainCount; ++grainViewIndex)
			{
			  GrainViewEntry *grainView = view->grainViews + grainViewIndex;
			  //u32 grainReadIndex = grainView->readIndex;
			  u32 grainStartIndex = grainView->startIndex;
			  u32 grainEndIndex = grainView->endIndex;			  
			  r32 grainStartPosition = (r32)grainStartIndex/(r32)grainBufferCapacity;
			  r32 grainEndPosition = (r32)grainEndIndex/(r32)grainBufferCapacity;
			  r32 grainStartBarPosition = grainStartPosition*dim.x;
			  r32 grainEndBarPosition = grainEndPosition*dim.x;
			  Rect2 grainStartBar = rectMinDim(min + V2(grainStartBarPosition, 0.f),
							   V2(barThickness, dim.y));
			  Rect2 grainEndBar = rectMinDim(min + V2(grainEndBarPosition, 0.f),
							 V2(barThickness, dim.y));
			  v4 grainWindowColor = grainWindowColors[grainViewIndex];
			  renderPushQuad(renderCommands, grainStartBar, 0, 0.f, RenderLevel_front, grainWindowColor);
			  renderPushQuad(renderCommands, grainEndBar, 0, 0.f, RenderLevel_front, grainWindowColor);
			}
		    }
	  
		  r32 samplesPerPixel = (r32)grainBufferCapacity/dim.x;
		  u32 lastSampleIndex = 0;
		  u32 widthInPixels = (u32)dim.x;
		  for(u32 pixel = 0; pixel < widthInPixels; ++pixel)
		    {
		      r32 samplePosition = samplesPerPixel*pixel;
		      u32 sampleIndex = (u32)samplePosition;
		      r32 sampleL = 0.f;
		      r32 sampleR = 0.f;
		      for(u32 i = lastSampleIndex; i < sampleIndex; ++i)
			{
			  sampleL += grainViewBuffer->samples[0][i];
			  sampleR += grainViewBuffer->samples[1][i];
			}
		      sampleL /= samplesPerPixel;
		      sampleR /= samplesPerPixel;
		      		      
		      Rect2 sampleLBar = rectMinDim(lowerRegionMiddle + V2(pixel, 0.f),
						    V2(1.f, 0.5f*sampleL*regionDim.y));
		      Rect2 sampleRBar = rectMinDim(upperRegionMiddle + V2(pixel, 0.f),
						    V2(1.f, 0.5f*sampleR*regionDim.y));
		      renderPushQuad(renderCommands, sampleLBar, 0, 0.f, RenderLevel_front, V4(0, 1, 0, 1));
		      renderPushQuad(renderCommands, sampleRBar, 0, 0.f, RenderLevel_front, V4(0, 1, 0, 1));

		      lastSampleIndex = sampleIndex;
		    }

		  grainStateView->viewReadIndex =
		    (viewEntryReadIndex + entriesQueued) % ARRAY_COUNT(grainStateView->views);
		  u32 oldEntriesQueued = entriesQueued;
		  while(globalPlatform.atomicCompareAndSwap(&grainStateView->entriesQueued,
							    entriesQueued,
							    entriesQueued - oldEntriesQueued) != entriesQueued)
		    {
		      entriesQueued = globalPlatform.atomicLoad(&grainStateView->entriesQueued);
		    }		    
#else

		  // TODO: don't use strings to check which panel we are in
		  if(stringsAreEqual(panel->name, STR8_LIT("left")))
		    {
		      UIComm volume = uiMakeSlider(panelLayout, STR8_LIT("volume"), V2(30.f, 30.f), UIAxis_y, 0.6f,
						   &pluginState->parameters[PluginParameter_volume], V4(0.8f, 0.8f, 0.8f, 1));

		      renderPushRectOutline(renderCommands, volume.element->parent->region, 2, RenderLevel_front,
					    V4(1, 1, 0, 1));

		      if(volume.flags & UICommFlag_hovering)
			{
			  volume.element->color = hadamard(volume.element->color, V4(1, 1, 0, 1));
			}
		      
		      if(volume.flags & UICommFlag_dragging)
			{
			  if(volume.flags & UICommFlag_leftDragging)
			    {	
			      v2 dragDelta = uiGetDragDelta(volume.element);
			      //printf("dragDelta: (%.2f, %.2f)\n", dragDelta.x, dragDelta.y);
		      
			      r32 newVolume =
				(volume.element->fParamValueAtClick +
				 getLength(volume.element->fParam->range)*dragDelta.y/getDim(volume.element->region).y);
			      
			      pluginSetFloatParameter(volume.element->fParam, newVolume);
			    }
			  else if(volume.flags & UICommFlag_minusPressed)
			    {
			      r32 oldVolume = pluginReadFloatParameter(volume.element->fParam);
			      r32 newVolume = oldVolume - 0.02f;

			      pluginSetFloatParameter(volume.element->fParam, newVolume);
			    }
			  else if(volume.flags & UICommFlag_plusPressed)
			    {
			      r32 oldVolume = pluginReadFloatParameter(volume.element->fParam);
			      r32 newVolume = oldVolume + 0.02f;

			      pluginSetFloatParameter(volume.element->fParam, newVolume);
			    }
			}
			
		      // define a new knob for the size parameter
		      // we are on the left sub-UI
		      v2 offset = 20.f*V2(1, 1);
		      r32 sizePOP = 0.15f;
		      UIComm grainSizeKnob = uiMakeKnob(panelLayout, STR8_LIT("size"), offset, -sizePOP,
							&pluginState->parameters[PluginParameter_size], 
							V4(0.2f, 0.5f, 0.3f, 1));
		      if(grainSizeKnob.flags & UICommFlag_dragging)
			{
			  v2 dragDelta = uiGetDragDelta(grainSizeKnob.element);
			  // post processing to set the new grain size using a temporary workaround: 
			  // We scaled dragDelta.x by x20 to cover approximately the full grain-size scale across width resolution of the screen.
			  // i.e. dragging fully to the left reduces grain size and sets it to a value towards zero ..
			  // .. and dragging fully to the right sets the grain-size towards a value near its max value.
			  // @TODO fix temporary workaround
			  r32 newGrainSize = grainSizeKnob.element->fParamValueAtClick + 50.f*dragDelta.y;
					
			  pluginSetFloatParameter(grainSizeKnob.element->fParam, newGrainSize);
			}
		    }
		  else if(stringsAreEqual(panel->name, STR8_LIT("right")))
		    {
		      //
		      // play
		      //
		      {
			v2 dim = getDim(panelLayout->regionRemaining);
			UNUSED(dim);
			r32 sizePOP = 0.15f;		    
			r32 offsetPixels = 20.f;		    
			v2 offset = offsetPixels*V2(1, 1);
			UIComm play = uiMakeButton(panelLayout, STR8_LIT("play"), offset, -sizePOP,
						   &pluginState->soundIsPlaying, V4(0, 0, 1, 1));
		    
			if(play.flags & UICommFlag_pressed)
			  {
			    bool oldPlay = pluginReadBooleanParameter(play.element->bParam);
			    bool newPlay = !oldPlay;
			    pluginSetBooleanParameter(play.element->bParam, newPlay);
			    // TODO: stop doing the queue here: we don't want to have to synchronize grain (de)queueing
			    //       across the audio and video threads (once we actually have them on their own threads)
			    //queueAllGrainsFromFile(&pluginState->silo, &pluginState->loadedGrainPackfile);
			  }		    
		      }
		  
		      //
		      // spread knob
		      //
		      {
			v2 dim = getDim(panelLayout->regionRemaining);
			UNUSED(dim);
			r32 sizePOP = 0.15f;
			r32 offsetPixels = 20.f;
			v2 offset = offsetPixels*V2(1,1);
			//spread knob
			//v2 offsetSpread = offsetPixels * V2(1,2);
			UIComm spread = uiMakeKnob(panelLayout, STR8_LIT("Spread"), offset, -sizePOP,
						   &pluginState->parameters[PluginParameter_spread], V4(1, 1, 0, 1));
			if (spread.flags & UIElementFlag_turnable)
			  {
			    v2 dragDelta2 = uiGetDragDelta(spread.element);
			    //printf("dragDelta: (%.2f, %.2f)\n", dragDelta.x, dragDelta.y);
			    r32 spreader = spread.element->fParamValueAtClick + .01f * dragDelta2.y;
			    pluginSetFloatParameter(spread.element->fParam, spreader);
			  }
		      }
			
		      //
		      // density
		      //
		      {
			v2 dim = getDim(panelLayout->regionRemaining);
			UNUSED(dim);
			r32 sizePOP = 0.15f;
			r32 offsetPixels = 20.f;
			v2 offset = offsetPixels*V2(1,1);
			UIComm density = uiMakeKnob(panelLayout, STR8_LIT("density"), offset, -sizePOP,
						    &pluginState->parameters[PluginParameter_density], V4(0, 1, 0, 1));
			if(density.flags & UICommFlag_dragging)
			  {
			    v2 dragDelta = uiGetDragDelta(density.element);
			    //printf("dragDelta: (%.2f, %.2f)\n", dragDelta.x, dragDelta.y);
		      
			    r32 newDensity = density.element->fParamValueAtClick + 0.1f*dragDelta.y;
			    pluginSetFloatParameter(density.element->fParam, newDensity);
			    //printf("newDensity: %.2f\n", newDensity);
			  }		    
		      }
		      
		      // @TODO Refine UI for the window parameter ..
		      // .. what needs to be done is position the rotating knob into a more aligned spot in the UI frame .. 
		      // .. and implement a knob with distinct states instead of a knob with float states.
		      // Otherwise, define a new UI struct for it. hint: It needs a UI that will be used a toggling mechanism across its 4+ disctinct states.
		      // define a new knob for the window parameter
		      // we are on the right sub-UI
		      {
			v2 dim = getDim(panelLayout->regionRemaining);
			UNUSED(dim);
			r32 sizePOP = 0.15f;
			r32 offsetPixels = 20.f;
			v2 offset = offsetPixels*V2(1, 1);
			UIComm window = uiMakeKnob(panelLayout, STR8_LIT("window"), offset, -sizePOP,
						   &pluginState->parameters[PluginParameter_window], 
						   V4(1, 0, 1, 1));
			if(window.flags & UICommFlag_dragging)
			  {
			    v2 dragDelta = uiGetDragDelta(window.element);
			    r32 newWindow = window.element->fParamValueAtClick + 0.01f*dragDelta.y;

			    pluginSetFloatParameter(window.element->fParam, newWindow);
			  }		    
		      }
		    }
		  else if(stringsAreEqual(panel->name, STR8_LIT("bottom")))
		    {
		      logString("\ndisplaying grain buffer\n");

		      v2 dim = getDim(panelLayout->regionRemaining);
		      v2 min = panelLayout->regionRemaining.min;

		      r32 middleBarThickness = 4.f;
		      Rect2 middleBar = rectMinDim(min + V2(0, 0.5f*dim.y),
						   V2(dim.x, middleBarThickness));
		      renderPushQuad(renderCommands, middleBar, 0, 0.f, RenderLevel_front, V4(0, 0, 0, 1));

		      v2 upperRegionMin = min + V2(0, 0.5f*(dim.y + middleBarThickness));
		      v2 lowerRegionMin = min;
		      v2 regionDim = V2(dim.x, 0.5f*dim.y - middleBarThickness);
		      v2 lowerRegionMiddle = lowerRegionMin + V2(0, 0.5f*regionDim.y);
		      v2 upperRegionMiddle = upperRegionMin + V2(0, 0.5f*regionDim.y);

		      u32 grainBufferCapacity = pluginState->grainBuffer.capacity;
		      GrainStateView *grainStateView = &pluginState->grainStateView;
		      AudioRingBuffer *grainViewBuffer = &grainStateView->viewBuffer;
		  
		      u32 viewEntryReadIndex = grainStateView->readIndex;
		      u32 entriesQueued = globalPlatform.atomicLoad(&grainStateView->entriesQueued);
		      logFormatString("entriesQueued: %u", entriesQueued);
		  
		      for(u32 entryIndex = 0; entryIndex < entriesQueued; ++entryIndex)
			{
			  u32 bufferReadIndex = grainStateView->viewBuffer.readIndex;
			  u32 bufferWriteIndex = grainStateView->viewBuffer.writeIndex;
		  
			  GrainBufferViewEntry *view = (grainStateView->views +
							((viewEntryReadIndex + entryIndex) %
							 ARRAY_COUNT(grainStateView->views)));

			  // NOTE: update view buffer with new view data
			  writeSamplesToAudioRingBuffer(grainViewBuffer,
							view->bufferSamples[0], view->bufferSamples[1],
							view->sampleCount);

			  // NOTE: display view read and write positions
			  r32 barThickness = 2.f;
			  u32 readIndex = bufferReadIndex;
			  r32 readPosition = (r32)readIndex/(r32)grainBufferCapacity;
			  r32 readBarPosition = readPosition*dim.x;
			  Rect2 readBar = rectMinDim(min + V2(readBarPosition, 0.f),
						     V2(barThickness, dim.y));
			  renderPushQuad(renderCommands, readBar, 0, 0.f, RenderLevel_front, V4(1, 0, 0, 1));
		      
			  u32 writeIndex = bufferWriteIndex;
			  r32 writePosition = (r32)writeIndex/(r32)grainBufferCapacity;
			  r32 writeBarPosition = writePosition*dim.x;
			  Rect2 writeBar = rectMinDim(min + V2(writeBarPosition, 0.f),
						      V2(barThickness, dim.y));
			  renderPushQuad(renderCommands, writeBar, 0, 0.f, RenderLevel_front, V4(1, 1, 1, 1));

			  // NOTE: display playing grain start and end positions
#if 1
			  v4 grainWindowColors[] =
			    {
			      //colorV4FromU32(0xFF4000FF),
			      colorV4FromU32(0xFF8000FF),
			      colorV4FromU32(0xFFBF00FF),
			      colorV4FromU32(0xFFFF00FF),
			  
			      colorV4FromU32(0xBFFF00FF),
			      colorV4FromU32(0x80FF00FF),
			      colorV4FromU32(0x40FF00FF),
			      colorV4FromU32(0x00FF00FF),

			      colorV4FromU32(0x00FF40FF),
			      colorV4FromU32(0x00FF80FF),
			      colorV4FromU32(0x00FFBFFF),
			      colorV4FromU32(0x00FFFFFF),

			      colorV4FromU32(0x00BFFFFF),
			      colorV4FromU32(0x0080FFFF),
			      colorV4FromU32(0x0040FFFF),
			      colorV4FromU32(0x0000FFFF),

			      colorV4FromU32(0x4000FFFF),
			      colorV4FromU32(0x8000FFFF),
			      colorV4FromU32(0xBF00FFFF),
			      colorV4FromU32(0xFF00FFFF),

			      colorV4FromU32(0xFF00BFFF),
			      colorV4FromU32(0xFF0080FF),
			      //colorV4FromU32(0xFF0040FF),
			    };

			  for(u32 grainViewIndex = 0; grainViewIndex < view->grainCount; ++grainViewIndex)
			    {
			      GrainViewEntry *grainView = view->grainViews + grainViewIndex;
			      //u32 grainReadIndex = grainView->readIndex;
			      u32 grainStartIndex = grainView->startIndex;
			      u32 grainEndIndex = grainView->endIndex;			  
			      r32 grainStartPosition = (r32)grainStartIndex/(r32)grainBufferCapacity;
			      r32 grainEndPosition = (r32)grainEndIndex/(r32)grainBufferCapacity;
			      r32 grainStartBarPosition = grainStartPosition*dim.x;
			      r32 grainEndBarPosition = grainEndPosition*dim.x;
			      Rect2 grainStartBar = rectMinDim(min + V2(grainStartBarPosition, 0.f),
							       V2(barThickness, dim.y));
			      Rect2 grainEndBar = rectMinDim(min + V2(grainEndBarPosition, 0.f),
							     V2(barThickness, dim.y));
			      v4 grainWindowColor = grainWindowColors[grainViewIndex];
			      renderPushQuad(renderCommands, grainStartBar, 0, 0.f, RenderLevel_front, grainWindowColor);
			      renderPushQuad(renderCommands, grainEndBar, 0, 0.f, RenderLevel_front, grainWindowColor);
			    }
#endif
			}

#if 1		  
		      r32 samplesPerPixel = (r32)grainBufferCapacity/dim.x;
		      u32 lastSampleIndex = 0;
		      u32 widthInPixels = (u32)dim.x;
		      for(u32 pixel = 0; pixel < widthInPixels; ++pixel)
			{
			  r32 samplePosition = samplesPerPixel*pixel;
			  u32 sampleIndex = (u32)samplePosition;
			  r32 sampleL = 0.f;
			  r32 sampleR = 0.f;
			  for(u32 i = lastSampleIndex; i < sampleIndex; ++i)
			    {
			      sampleL += grainViewBuffer->samples[0][i];
			      sampleR += grainViewBuffer->samples[1][i];
			    }
			  sampleL /= samplesPerPixel;
			  sampleR /= samplesPerPixel;
		      		      
			  Rect2 sampleLBar = rectMinDim(lowerRegionMiddle + V2(pixel, 0.f),
							V2(1.f, 0.5f*sampleL*regionDim.y));
			  Rect2 sampleRBar = rectMinDim(upperRegionMiddle + V2(pixel, 0.f),
							V2(1.f, 0.5f*sampleR*regionDim.y));
			  renderPushQuad(renderCommands, sampleLBar, 0, 0.f, RenderLevel_front, V4(0, 1, 0, 1));
			  renderPushQuad(renderCommands, sampleRBar, 0, 0.f, RenderLevel_front, V4(0, 1, 0, 1));

			  lastSampleIndex = sampleIndex;
			}
#endif

		      grainStateView->readIndex =
			(viewEntryReadIndex + entriesQueued) % ARRAY_COUNT(grainStateView->views);
		      u32 oldEntriesQueued = entriesQueued;
		      while(globalPlatform.atomicCompareAndSwap(&grainStateView->entriesQueued,
								entriesQueued,
								entriesQueued - oldEntriesQueued) != entriesQueued)
			{
			  entriesQueued = globalPlatform.atomicLoad(&grainStateView->entriesQueued);
			}
		      // while(globalPlatform.atomicCompareAndSwap(&grainStateView->entriesQueued, entriesQueued, 0) !=
		      // 	    entriesQueued)
		      // 	{
		      // 	  entriesQueued = globalPlatform.atomicLoad(&grainStateView->entriesQueued);
		      // 	  grainStateView->readIndex =
		      // 	    (viewReadIndex + entriesQueued) % ARRAY_COUNT(grainStateView->views);
		      // 	}
		    }
#endif
	      
		  renderPushUILayout(renderCommands, panelLayout);
		  uiEndLayout(panelLayout);
		}	  
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
				   &pluginState->parameters[PluginParameter_volume],// makeRange(0.f, 1.f),
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
				  &pluginState->parameters[PluginParameter_density], V4(0, 1, 0, 1));
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
      //printf("permanent arena used %zu bytes\n", pluginState->permanentArena.used);
      //stringListPushFormat(globalLogger->logArena, &globalLogger->log, "permanent arena used %zu bytes\n", pluginState->permanentArena.used);
      logFormatString("permanent arena used %zu bytes", pluginState->permanentArena.used);
    }  
}

extern "C"
AUDIO_PROCESS(audioProcess)
{  
  PluginState *pluginState = getPluginState(memory);
  if(pluginState->initialized)
    {
      // NOTE: dequeue host-driven parameter value changes
      u32 queuedCount = globalPlatform.atomicLoad(&audioBuffer->queuedCount);
      if(queuedCount)
	{
	  u32 parameterValueQueueReadIndex = audioBuffer->parameterValueQueueReadIndex;
	  for(u32 entryIndex = 0; entryIndex < queuedCount; ++entryIndex)
	    {
	      ParameterValueQueueEntry *entry =
		audioBuffer->parameterValueQueueEntries + parameterValueQueueReadIndex;

	      PluginFloatParameter *parameter = pluginState->parameters + entry->index;
	      //r32 parameterRangeLen = getLength(parameter->range);
	      //r32 newVal = entry->value.asFloat*parameterRangeLen + parameter->range.min;
	      r32 newVal = mapToRange(entry->value.asFloat, parameter->range);
	      pluginSetFloatParameter(parameter, newVal);
	    }

	  audioBuffer->parameterValueQueueReadIndex =
	    (parameterValueQueueReadIndex + queuedCount) % ARRAY_COUNT(audioBuffer->parameterValueQueueEntries);

	  u32 entriesRead = queuedCount;
	  while(globalPlatform.atomicCompareAndSwap(&audioBuffer->queuedCount,
						    queuedCount, queuedCount - entriesRead) != queuedCount)
	    {
	      queuedCount = globalPlatform.atomicLoad(&audioBuffer->queuedCount);
	    }
	}
      
      // NOTE: copy plugin input audio to the grain buffer
      u32 framesToRead = audioBuffer->framesToWrite;      
      r32 inputBufferReadSpeed = ((audioBuffer->inputSampleRate != 0) ?
				  (r32)INTERNAL_SAMPLE_RATE/(r32)audioBuffer->inputSampleRate : 1.f);
      r32 scaledFramesToRead = inputBufferReadSpeed*framesToRead;
      
      AudioRingBuffer *gbuff = &pluginState->grainBuffer;
      PlayingSound *loadedSound = &pluginState->loadedSound;

      r32 *inputMixBuffers[2] = {};
      TemporaryMemory inputMixerMemory = arenaBeginTemporaryMemory(&pluginState->permanentArena, KILOBYTES(32));      
      inputMixBuffers[0] = arenaPushArray((Arena *)&inputMixerMemory, framesToRead, r32,
					  arenaFlagsZeroAlign(4*sizeof(r32)));
      inputMixBuffers[1] = arenaPushArray((Arena *)&inputMixerMemory, framesToRead, r32,
					  arenaFlagsZeroAlign(4*sizeof(r32)));

      const void *genericInputFrames[2] = {};
      genericInputFrames[0] = audioBuffer->inputBuffer[0];
      genericInputFrames[1] = audioBuffer->inputBuffer[1];

      for(u32 frameIndex = 0; frameIndex < framesToRead; ++frameIndex)
	{		
	  bool soundIsPlaying = pluginReadBooleanParameter(&pluginState->soundIsPlaying);       
	  r32 currentTime = (r32)loadedSound->samplesPlayed + inputBufferReadSpeed*(r32)frameIndex;
	  u32 soundReadIndex = (u32)currentTime;
	  r32 soundReadFrac = currentTime - (r32)soundReadIndex;		

	  for(u32 channelIndex = 0; channelIndex < audioBuffer->inputChannels; ++channelIndex)
	    {
	      r32 mixedVal = 0.f;		   
	      if(soundIsPlaying)		       
		{			
		  if(currentTime < loadedSound->sound.sampleCount)
		    {
		      r32 loadedSoundSample0 = loadedSound->sound.samples[channelIndex][soundReadIndex];
		      r32 loadedSoundSample1 = loadedSound->sound.samples[channelIndex][soundReadIndex + 1];
		      r32 loadedSoundSample = lerp(loadedSoundSample0, loadedSoundSample1, soundReadFrac);

		      mixedVal += 0.5f*loadedSoundSample;
		    }
		  else
		    {		      
		      pluginSetBooleanParameter(&pluginState->soundIsPlaying, false);
		      loadedSound->samplesPlayed = 0;// + pluginState->start_pos);
		    }		  
		}

	      switch(audioBuffer->inputFormat)
		{
		case AudioFormat_s16:
		  {
		    s16 *inputFrames = (s16 *)genericInputFrames[channelIndex];
		    r32 inputSample = (r32)*inputFrames/(r32)S16_MAX;		    
		    //ASSERT(isInRange(inputSample, -1.f, 1.f));
		    inputSample = clampToRange(inputSample, -1.f, 1.f);
		    mixedVal += 0.5f*inputSample;

		    genericInputFrames[channelIndex] = (u8 *)inputFrames + audioBuffer->inputStride;
		  } break;

		case AudioFormat_r32:
		  {
		    r32 *inputFrames = (r32 *)genericInputFrames[channelIndex];
		    r32 inputSample = *inputFrames;
		    //ASSERT(isInRange(inputSample, -1.f, 1.f));
		    inputSample = clampToRange(inputSample, -1.f, 1.f);
		    mixedVal += 0.5f*inputSample;		    

		    genericInputFrames[channelIndex] = (u8 *)inputFrames + audioBuffer->inputStride;
		  } break;

		default: {ASSERT(!"invalid input format");} break;
		}
	      
	      inputMixBuffers[channelIndex][frameIndex] = mixedVal;		   
	    }
	}
      
      bool soundIsPlaying = pluginReadBooleanParameter(&pluginState->soundIsPlaying);
      if(soundIsPlaying)
	{
	  loadedSound->samplesPlayed += scaledFramesToRead;
	}

      writeSamplesToAudioRingBuffer(gbuff, inputMixBuffers[0], inputMixBuffers[1], framesToRead);
      //arenaEndTemporaryMemory(&inputMixerMemory);

      // NOTE: grain playback
      GrainManager* gManager = &pluginState->grainManager;
      u32 framesToWrite = audioBuffer->framesToWrite;
            
      TemporaryMemory grainMixerMemory = arenaBeginTemporaryMemory(&pluginState->permanentArena, KILOBYTES(128));
      r32 *grainMixBuffers[2] = {};
      grainMixBuffers[0] = arenaPushArray((Arena *)&grainMixerMemory, framesToWrite, r32,
					  arenaFlagsZeroNoAlign());
      grainMixBuffers[1] = arenaPushArray((Arena *)&grainMixerMemory, framesToWrite, r32,
					  arenaFlagsZeroNoAlign());
      synthesize(grainMixBuffers[0], grainMixBuffers[1],
		 gManager, pluginState,
		 framesToWrite);

      // NOTE: audio output
      r64 nFreq = M_TAU*pluginState->freq/(r64)audioBuffer->outputSampleRate;                 

      r32 formatVolumeFactor = 1.f;      
      switch(audioBuffer->outputFormat)
	{
	case AudioFormat_s16:
	  {
	    formatVolumeFactor = 32000.f;
	  } break;
	case AudioFormat_r32: break;
	default: { ASSERT(!"invalid audio format"); } break;
	}
      
      void *genericOutputFrames[2] = {};
      genericOutputFrames[0] = audioBuffer->outputBuffer[0];
      genericOutputFrames[1] = audioBuffer->outputBuffer[1];     

      u8 *atMidiBuffer = audioBuffer->midiBuffer;

      for(u32 frameIndex = 0; frameIndex < audioBuffer->framesToWrite; ++frameIndex)
	{
	  atMidiBuffer = midi::parseMidiMessage(atMidiBuffer, pluginState,
						audioBuffer->midiMessageCount, frameIndex);

	  PluginFloatParameter *volumeParameter = pluginState->parameters + PluginParameter_volume;
	  r32 volumeRaw = pluginReadFloatParameter(volumeParameter);
	  r32 volume =  formatVolumeFactor*volumeParameter->processingTransform(volumeRaw);

	  r32 mixParam = pluginReadFloatParameter(&pluginState->parameters[PluginParameter_mix]);

	  pluginState->phasor += nFreq;
	  if(pluginState->phasor > M_TAU) pluginState->phasor -= M_TAU;
				  
	  r32 sinVal = sin(pluginState->phasor);
	  UNUSED(sinVal);
	  for(u32 channelIndex = 0; channelIndex < audioBuffer->outputChannels; ++channelIndex)
	    {
	      r32 mixedVal = 0.f;
	      
#if 0
	      // TODO: reading from the grain buffer does not have to do interpolation since it runs at the internal sample rate
	      r32 grainReadPosition = sampleRateRatio*frameIndex;
	      u32 grainReadIndex = (u32)grainReadPosition;
	      r32 grainReadFrac = grainReadPosition - grainReadIndex;

	      ASSERT(grainReadIndex + 1 < scaledFramesToWrite);
	      r32 firstGrainVal = grainMixBuffers[channelIndex][grainReadIndex];
	      r32 nextGrainVal = grainMixBuffers[channelIndex][grainReadIndex + 1];
	      r32 grainVal = lerp(firstGrainVal, nextGrainVal, grainReadFrac);
#else
	      r32 grainVal = grainMixBuffers[channelIndex][frameIndex];
	      r32 leftGrainVal = grainMixBuffers[0][frameIndex];
	      r32 rightGrainVal = grainMixBuffers[1][frameIndex];
	      r32 stereoWidth = pluginReadFloatParameter(&pluginState->parameters[PluginParameter_spread]);
	      if (channelIndex < 2) { // Make sure we only process left and right channels
		//r32 widthVal = stereoWidth * 0.5f;
		r32 tmp = 1.0f / fmaxf(1.0f + stereoWidth, 2.0f);
		r32 coef_M = 1.0f * tmp;
		r32 coef_S = stereoWidth * tmp;

		r32 mid = (leftGrainVal + rightGrainVal) * coef_M;
		r32 sides = (rightGrainVal - leftGrainVal) * coef_S;

		// Update grain value based on channel
		if (channelIndex == 0) {
		  grainVal = mid - sides; // Left channel
		}
		else {
		  grainVal = mid + sides; // Right channel
		}
	      }
#endif
	      
	      // mixedVal += 0.5f*grainVal;
	      // mixedVal += 0.5f*inputMixBuffers[channelIndex][frameIndex];
	      mixedVal += lerp(inputMixBuffers[channelIndex][frameIndex], grainVal, mixParam);
	     
	      switch(audioBuffer->outputFormat)
		{
		case AudioFormat_r32:
		  {
		    r32 *audioFrames = (r32 *)genericOutputFrames[channelIndex];
		    *audioFrames = volume*mixedVal;
		    genericOutputFrames[channelIndex] = (u8 *)audioFrames + audioBuffer->outputStride;
		  } break;
		case AudioFormat_s16:
		  {
		    s16 *audioFrames = (s16 *)genericOutputFrames[channelIndex];
		    *audioFrames = (s16)(volume*mixedVal);
		    genericOutputFrames[channelIndex] = (u8 *)audioFrames + audioBuffer->outputStride;
		  } break;

		default: ASSERT(!"ERROR: invalid audio output format");
		}
	    }

	  pluginUpdateFloatParameter(&pluginState->parameters[PluginParameter_volume]);
	  pluginUpdateFloatParameter(&pluginState->parameters[PluginParameter_density]);
	  pluginUpdateFloatParameter(&pluginState->parameters[PluginParameter_window]);
	  pluginUpdateFloatParameter(&pluginState->parameters[PluginParameter_size]);
	  pluginUpdateFloatParameter(&pluginState->parameters[PluginParameter_spread]);
	  pluginUpdateFloatParameter(&pluginState->parameters[PluginParameter_mix]);
	  pluginUpdateFloatParameter(&pluginState->parameters[PluginParameter_offset]);
	}

      arenaEndTemporaryMemory(&inputMixerMemory);
      arenaEndTemporaryMemory(&grainMixerMemory);      
    }
}
