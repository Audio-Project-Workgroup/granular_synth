// the core audio-visual functionality of our plugin is implemented here

/* NOTE:
   - elements are passed to the renderer in pixel space (where (0, 0) is the bottom-left of the
     drawing region, and (windowWidthInPixels, windowHeightInPixels) is the top-right).
     
   - the mouse position is in these coordinates as well, for ease of intersection testing
   
   - the renderNewFrame() and audioProcess() functions are hot-reloadable, i.e. you modify the code,
     recompile, and perceive those changes in the running application automatically.
     
     - only the simple host application implements hot-reloading. This is intended to be a
       development feature, and is not needed in the release build.
       
     - initializePluginState() is called once at startup. If you modify this function and
       recompile at run-time, you will not see those changes since the function has already been called.
       
     - modifying the arguments to those functions, or adding fields to PluginState, will likely crash the
       application, and introduce some unusual and undesirable behavior otherwise.
*/

/**
   TODO FOR DEMO 11/12-NOV-2025
   ORDERED FROM HIGHEST PRIORITY TO LOWEST PRIORITY
 
 * FIX FUNCTIONALITY BUGS:
   -(FIXED) `gsAudioProcess()` sometimes crashes
            (bad memory access on ouput or grain mix buffer)
   -(FIXED) LOUD SCREECH WHEN COMPILING IN NON DEBUG MODE
   -(FIXED) a few hundred bytes read-only data is occasionaly overwritten around
            address 0x500. symptoms include control labels and log strings being
            garbage, and sometimes the size knob has the wrong texture.
   -(FIXED) the `__memory_used` value sometimes drops from its stable value of
            around 5 MB to ~ 200 KB for a single frame, and then goes back to normal
   -(FIXED?) the audio process appears to occasionally underflow
   -(FIXED) microphone audio input sometimes sounds choppy
            (index.js, granade_audio.js, wasm_glue.cpp)
   -(web)   test on firefox/safari/mobile
   -(FIXED) prevent too much grain view data from being queued up, overflowing the render buffer
            (can happen when device selection menu is open, plugin editor window is closed, or
	    user is in a different browser tab).
	    (internal_granulator.h, internal_granulator.cpp, plugin.cpp)
	   
 * FIX GRAPHICAL BUGS:
   -(FIXED) editor should render at a constant aspect ratio of 16:9, adding
            black bars if necessary (as in native/vst targets)
   -(FIXED?) fix sort order so that knobs aren't cut off
   -(FIXED) align knobs to center

 * (DONE) INFO (MERGE PULL REQUEST):
   -(web) put relevant readme info in the web page (description, controls, etc.)

   -----------------------------------------------------------------------------
   |                             LINE OF NECESSITY                             |
   -----------------------------------------------------------------------------

 * NEW FEATURES:
   -(all) pitch-shift/time-stretch controls
   -(all) parameter modulation
   -(all) input level control
   -(DONE) control tooltips
   -(all) double-click control to reset to default value
   -(web) keyboard driven ui
      
 * TECH DEBT:
   -(vst/exe) resurrect vst target, make sure all platforms still work
   -(vst) figure out how to automatically copy the vst folder on windows
   -(all) merge build scripts into a single `build.bat` and `build.sh`, with
          a CLI for selecting targets and options, e.g.
	  ```build --target=vst --debug```
   -(all) split function declarations and definitions into separate .h and .cpp
          files, compile the common implementation once, and link into each
	  target, to hopefully speed up compilation time
   -(all) simplify/optimize ui system

 * PERFORMANCE OPTIMIZATION:
   -(all) profile
   -(all) vectorize copying audio samples to output/from input
   -(all) transpose loop order in grain process ()
   -(all) check for false-sharing on cache lines with contended locks
   -(web) implement simple math functions, to avoid a round-trip to javascript
   
 */

/* TODO:
   - UI:
     - improve font rendering
     - screen reader support
     - allow specification of horizontal vs. vertical slider, or knob for draggable elements
     - collapsable element groups, sized relative to children
     - tooltips when hovering (eg show numeric parameter value)
     - interface for selecting files to load at runtime
     - interface for remapping midi cc channels with parameters
     - display the state of grains and the grain buffer (wip)

   - Parameters:
     - make parameters visible to fl studio last tweaked automation menu
     - per-grain level
     - modulation

   - State:
     - full state (de)serialization (not just parameters, also grain buffer)

   - File Loading: (on hold)
     - more audio file formats (eg flac, ogg, mp3)
     - speed up reads (ie SIMD)
     - better samplerate conversion (ie FFT method (TODO: speed up fft and czt))
     - load in batches to pass to ML model

   - MIDI:
     - cc channel - parameter remapping at runtime

   - Grains:
     - pitch-shift and time-stretch grains

   - Misc:
     - profile the code
     - static analysis
     - speed up fft (aligned loads/stores, different radix?)
     - general optimization (simd everywhere)
     - work queues?
     - allocate hardware ring buffer?
*/

/*
  NEW ISSUES: WEB PORT:
  - knob alignment values seem to be slightly off. they are not quite rotating about the center
  - the volume fader is not moving along the whole range
  - text rendering is off. there are some black lines, and glyphs are not aligned to the baseline
  - the ui system sucks ass
  - logging system sometimes crashes
  - sometimes not enough quad capacity
 */

#include "plugin.h"

//#include "fft_test.cpp"
#include "midi.cpp"
#include "ui_layout.cpp"
//#include "file_granulator.cpp"
#include "internal_granulator.cpp"

#if BUILD_LOGGING
PluginLogger *globalLogger;
#endif

inline void
printButtonState(ButtonState button, char *name)
{
  logFormatString("%s: %s, %s\n", name,
		  wasPressed(button) ? "pressed" : "not pressed",
		  isDown(button) ? "down" : "up");
}

static PluginState *globalPluginState = 0;

EXPORT_FUNCTION PluginState*
gsInitializePluginState(PluginMemory *memoryBlock)
{
  PluginState *pluginState = 0;
  if(!globalPluginState)
    {
      //globalPlatform = memoryBlock->platformAPI;
#if !defined(HOST_LAYER)
#define X(name, ret, args) gs##name = memoryBlock->platformAPI.gs##name;
      PLATFORM_API_XLIST
#undef X
#endif

#if BUILD_LOGGING
      globalLogger = memoryBlock->logger;
#endif

      Arena *permanentArena = gsArenaAcquire(MEGABYTES(1));
      pluginState = arenaPushStruct(permanentArena, PluginState);  
      pluginState->permanentArena = permanentArena;

      pluginState->osTimerFreq = memoryBlock->osTimerFreq;
      pluginState->pluginHost = memoryBlock->host;
      pluginState->pluginMode = PluginMode_editor;

      // TODO: maybe these initial sizes can be tuned for fewer allocation calls
      pluginState->frameArena = gsArenaAcquire(MEGABYTES(1));
      //pluginState->framePermanentArena = gsArenaAcquire(0);
      pluginState->audioArena = gsArenaAcquire(MEGABYTES(1));

      if(pluginState->pluginHost == PluginHost_executable ||
	 pluginState->pluginHost == PluginHost_daw)
	{
	  pluginState->pathToPlugin =
	    gsGetPathToModule(memoryBlock->pluginHandle, (void *)gsInitializePluginState,
			      pluginState->permanentArena);
	}
      // NOTE: parameter initialization
      // pluginState->phasor = 0.f;
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

      initializeFloatParameter(&pluginState->parameters[PluginParameter_pan],
			       pluginParameterInitData[PluginParameter_pan]);

      initializeFloatParameter(&pluginState->parameters[PluginParameter_offset],
			       pluginParameterInitData[PluginParameter_offset]);

      // NOTE: devices
      pluginState->outputDeviceCount = memoryBlock->outputDeviceCount;
      pluginState->selectedOutputDeviceIndex = memoryBlock->selectedOutputDeviceIndex;
      for(u32 i = 0; i < pluginState->outputDeviceCount; ++i)
	{
	  pluginState->outputDeviceNames[i] =
	    arenaPushString(pluginState->permanentArena, memoryBlock->outputDeviceNames[i]);
	}
	      
      pluginState->inputDeviceCount = memoryBlock->inputDeviceCount;
      pluginState->selectedInputDeviceIndex = memoryBlock->selectedInputDeviceIndex;
      for(u32 i = 0; i < pluginState->inputDeviceCount; ++i)
	{
	  pluginState->inputDeviceNames[i] =
	    arenaPushString(pluginState->permanentArena, memoryBlock->inputDeviceNames[i]);
	}

      // NOTE: grain buffer initialization
      u32 grainBufferCount = 48000;
      pluginState->grainBuffer = initializeGrainBuffer(pluginState, grainBufferCount);
      pluginState->grainManager = initializeGrainManager(pluginState);

      // NOTE: grain view initialization
      GrainStateView *grainStateView = &pluginState->grainStateView;
      grainStateView->viewWriteIndex = 1;
      grainStateView->viewBuffer.capacity = grainBufferCount;
      grainStateView->viewBuffer.samples[0] =
	arenaPushArray(pluginState->permanentArena, grainStateView->viewBuffer.capacity, r32,
		       arenaFlagsNoZeroAlign(4*sizeof(r32)));
      grainStateView->viewBuffer.samples[1] =
	arenaPushArray(pluginState->permanentArena, grainStateView->viewBuffer.capacity, r32,
		       arenaFlagsNoZeroAlign(4*sizeof(r32)));
      grainStateView->viewBuffer.readIndex = pluginState->grainBuffer.readIndex;
      grainStateView->viewBuffer.writeIndex = pluginState->grainBuffer.writeIndex;

      u32 grainViewSampleCapacity = 4096;
      for(u32 i = 0; i < ARRAY_COUNT(grainStateView->views); ++i)
	{
	  GrainBufferViewEntry *view = grainStateView->views + i;
	  view->sampleCapacity = grainViewSampleCapacity;
	  view->bufferSamples[0] =
	    arenaPushArray(pluginState->permanentArena, grainViewSampleCapacity, r32);
	  view->bufferSamples[1] =
	    arenaPushArray(pluginState->permanentArena, grainViewSampleCapacity, r32);
	}

      // NOTE: file loading, embedded grain caching
      pluginState->soundIsPlaying.value = false;
      // pluginState->loadedSound.sound =
      // 	loadWav(pluginState->permanentArena, STR8_LIT("../data/fingertips_44100_PCM_16.wav"));
      // pluginState->loadedSound.samplesPlayed = 0;// + pluginState->start_pos);
#if 0
      char *fingertipsPackfilename = "../data/fingertips.grains";
      TemporaryMemory packfileMemory = arenaBeginTemporaryMemory(&pluginState->loadArena, MEGABYTES(64));
      GrainPackfile fingertipsGrains = beginGrainPackfile((Arena *)&packfileMemory);
      addSoundToGrainPackfile(&fingertipsGrains, &pluginState->loadedSound.sound);
      writePackfileToDisk(&fingertipsGrains, fingertipsPackfilename);
      arenaEndTemporaryMemory(&packfileMemory);

      pluginState->loadedGrainPackfile = loadGrainPackfile(fingertipsPackfilename,
							   &pluginState->permanentArena);
      pluginState->silo = initializeFileGrainState(&pluginState->permanentArena);
#endif

#define X(name) pluginState->name = PLUGIN_ASSET(name);
      PLUGIN_ASSET_XLIST;
#undef X
      // pluginState->nullTexture =
      // 	makeBitmap(pluginState->permanentArena, 1920, 1080, 0xFFFFFFFF);
      // // TODO: maybe have some kind of x macro list for the bitmap loading
      // pluginState->editorReferenceLayout =
      // 	loadBitmap(DATA_PATH"BMP/NEWGRANADE_UI_POSITIONSREFERENCE.bmp", pluginState->permanentArena);
      // pluginState->editorSkin =
      // 	loadBitmap(DATA_PATH"BMP/TREE.bmp", pluginState->permanentArena);
      // pluginState->pomegranateKnob =
      // 	loadBitmap(DATA_PATH"BMP/POMEGRANATE_BUTTON.bmp", pluginState->permanentArena,
      // 		   V2(0, -0.03f));
      // pluginState->pomegranateKnobLabel =
      // 	loadBitmap(DATA_PATH"BMP/POMEGRANATE_BUTTON_WHITEMARKERS.bmp", pluginState->permanentArena,
      // 		   V2(0, 0));
      // pluginState->halfPomegranateKnob =
      // 	loadBitmap(DATA_PATH"BMP/HALFPOMEGRANATE_BUTTON.bmp", pluginState->permanentArena,
      // 		   V2(-0.005f, -0.035f));
      // pluginState->halfPomegranateKnobLabel =
      // 	loadBitmap(DATA_PATH"BMP/HALFPOMEGRANATE_BUTTON_WHITEMARKERS.bmp", pluginState->permanentArena,
      // 		   V2(0, 0));
      // pluginState->densityKnob =
      // 	loadBitmap(DATA_PATH"BMP/DENSITYPOMEGRANATE_BUTTON.bmp", pluginState->permanentArena,
      // 		   V2(0.01f, -0.022f));
      // pluginState->densityKnobShadow =
      // 	loadBitmap(DATA_PATH"BMP/DENSITYPOMEGRANATE_BUTTON_SHADOW.bmp", pluginState->permanentArena);
      // pluginState->densityKnobLabel =
      // 	loadBitmap(DATA_PATH"BMP/DENSITYPOMEGRANATE_BUTTON_WHITEMARKERS.bmp", pluginState->permanentArena,
      // 		   V2(0, 0));
      // pluginState->levelBar =
      // 	loadBitmap(DATA_PATH"BMP/LEVELBAR.bmp", pluginState->permanentArena);
      // pluginState->levelFader =
      // 	loadBitmap(DATA_PATH"BMP/LEVELBAR_SLIDINGLEVER.bmp", pluginState->permanentArena,
      // 		   V2(0, -0.416f));
      // pluginState->grainViewBackground =
      // 	loadBitmap(DATA_PATH"BMP/GREENFRAME_RECTANGLE.bmp", pluginState->permanentArena);
      // pluginState->grainViewOutline =
      // 	loadBitmap(DATA_PATH"BMP/GREENFRAME.bmp", pluginState->permanentArena);

      // RangeU32 characterRange = {32, 127}; // NOTE: from SPACE up to (but not including) DEL
      // pluginState->agencyBold =
      // 	loadFont(pluginState->permanentArena, STR8_LIT(DATA_PATH"FONT/AGENCYB.ttf"),
      // 		 characterRange, 36.f);
      
      // TODO: get this data from asset packing/codegen
      // pluginState->agencyBold.characterRange = {32, 127};
      // pluginState->agencyBold.verticalAdvance = 0.f;
      // pluginState->agencyBold.glyphs = PLUGIN_ASSET(AgencyBold_32);
      pluginState->agencyBold = &fontAgencyBold;
      // TODO: set vertical advance!!!

      // NOTE: ui initialization
      pluginState->uiContext =
	// uiInitializeContext(pluginState->frameArena, pluginState->framePermanentArena,
	// 		    &pluginState->agencyBold);
	uiInitializeContext(pluginState->frameArena, pluginState->permanentArena,
			    pluginState->agencyBold);

      // TODO: our editor interface doesn't use resizable panels,
      //       so it doesn't make sense to still be using them
      pluginState->rootPanel =
	arenaPushStruct(pluginState->permanentArena, UIPanel,
			arenaFlagsZeroNoAlign());
      pluginState->rootPanel->sizePercentOfParent = 1.f;
      pluginState->rootPanel->splitAxis = UIAxis_y;
      pluginState->rootPanel->name = STR8_LIT("editor");
      pluginState->rootPanel->color = V4(1, 1, 1, 1);
      pluginState->rootPanel->texture = pluginState->editorSkin;
      //pluginState->rootPanel->texture = &pluginState->editorReferenceLayout;

      pluginState->menuPanel = arenaPushStruct(pluginState->permanentArena, UIPanel,
					       arenaFlagsZeroNoAlign());
      pluginState->menuPanel->sizePercentOfParent = 1.f;
      pluginState->menuPanel->splitAxis = UIAxis_x;

      v4 menuBackgroundColor = colorV4FromU32(0x080C1CFF);
      UIPanel *currentParentPanel = pluginState->menuPanel;
      UIPanel *menuLeft = makeUIPanel(currentParentPanel, pluginState->permanentArena,
				      UIAxis_x, 0.5f, STR8_LIT("menu left"),
				      pluginState->null, menuBackgroundColor);
      UIPanel *menuRight = makeUIPanel(currentParentPanel, pluginState->permanentArena,
				       UIAxis_x, 0.5f, STR8_LIT("menu right"),
				       pluginState->null, menuBackgroundColor);
      UNUSED(menuLeft);
      UNUSED(menuRight);

      pluginState->mouseTooltipLayout = 0;
	      
#if 0
#if 1
      r32 *testModelInput = pluginState->loadedSound.sound.samples[0];
      //s64 testModelInputSampleCount = pluginState->loadedSound.sound.sampleCount;
      s64 testModelInputSampleCount = 2400; // TODO: feed the whole file
#else	  
      ReadFileResult testInputFile = globalPlatform.readEntireFile(DATA_PATH"/test_input.data",
								   &pluginState->permanentArena);
      ReadFileResult testOutputFile = globalPlatform.readEntireFile(DATA_PATH"/test_output.data",
								    &pluginState->permanentArena);
      r32 *testModelInput = (r32 *)testInputFile.contents;
      s64 testModelInputSampleCount = (testInputFile.contentsSize - 1)/(2*sizeof(r32));
#endif
      void *outputData = globalPlatform.runModel(testModelInput, testModelInputSampleCount);
      r32 *outputFloat = (r32 *)outputData;
#endif

      pluginState->initialized = true;
      globalPluginState = pluginState;
    }  
  return(pluginState);
}

EXPORT_FUNCTION void
gsRenderNewFrame(PluginMemory *memory, PluginInput *input, RenderCommands *renderCommands) 
{ 
  gsInitializePluginState(memory);
  if(globalPluginState)
    {
      // NOTE: DEBUG
#if OS_WASM && BUILD_LOGGING
      {
#define X(name) DEBUG_POINTER_CHECK(name);
	DEBUG_POINTER_XLIST
#undef X
      }
#endif

      PluginState *pluginState = globalPluginState;
      //renderBeginCommands(renderCommands, &pluginState->frameArena);
      TemporaryMemory scratch = arenaGetScratch(0, 0);

#if 0
      logFormatString("mouseP: (%.2f, %.2f)", input->mouseState.position.x, input->mouseState.position.y);
      logFormatString("mouseLeft: %s, %s",
		      wasPressed(input->mouseState.buttons[MouseButton_left]) ? "pressed" : "not pressed",
		      isDown(input->mouseState.buttons[MouseButton_left]) ? "down" : "up");
      logFormatString("mouseRight: %s, %s",
		      wasPressed(input->mouseState.buttons[MouseButton_right]) ? "pressed" : "not pressed",
		      isDown(input->mouseState.buttons[MouseButton_right]) ? "down" : "up");
      
      printButtonState(input->keyboardState.keys[KeyboardButton_tab], "tab");
      printButtonState(input->keyboardState.keys[KeyboardButton_backspace], "backspace");
#endif
      
      u32 windowWidth = renderCommands->widthInPixels;
      u32 windowHeight = renderCommands->heightInPixels;
      v2 windowDim = V2((r32)windowWidth, (r32)windowHeight);
      Rect2 screenRect = rectMinDim(V2(0, 0), windowDim);

      // NOTE: UI layout
      UIContext *uiContext = &pluginState->uiContext;
      uiContextNewFrame(uiContext, &input->mouseState, &input->keyboardState, renderCommands->windowResized);

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
	      Rect2 panelRect = uiComputeRectFromPanel(panel, screenRect, scratch.arena);	      

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
			}
		    }

		  renderPushUILayout(renderCommands, panelLayout);
		  uiEndLayout(panelLayout);
		}
	    }
	}
      else
	{
	  // TODO: our editor interface doesn't use resizable panels,
	  //       so it doesn't make sense to still be using them

	  // NOTE: non-leaf ui
	  for(UIPanel *panel = pluginState->rootPanel; panel; panel = uiPanelIteratorDepthFirstPreorder(panel).next)
	    {
	      Rect2 panelRect = uiComputeRectFromPanel(panel, screenRect, scratch.arena);
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
	      Rect2 panelRect = uiComputeRectFromPanel(panel, screenRect, scratch.arena);	  
	  	  
	      // NOTE: build ui
	      if(!panel->first)
		{
		  UILayout *panelLayout = &panel->layout;
		  uiBeginLayout(panelLayout, uiContext, panelRect, panel->color, panel->texture);
		  //uiBeginLayout(panelLayout, uiContext, panelRect, V4(0, 0, 0, 1), panel->texture);
		  uiPushLayoutOffsetSizeType(panelLayout, UISizeType_percentOfParent);
		  uiPushLayoutDimSizeType(panelLayout, UISizeType_percentOfParent);

		  // renderPushQuad(renderCommands, panelRect, panel->texture, 0,
		  // 		 RENDER_LEVEL(background));

		  v2 panelDim = getDim(panelRect);
		  v2 elementTextScale = 0.0008f*panelDim.x*V2(1.f, 1.f);

		  r32 knobDimPOP = 0.235f;
		  v2 knobLabelOffset = V2(0.02f, 0.06f);
		  v2 knobLabelDim = V2(0.96f, 1);
		  v2 knobClickableOffset = V2(0.1f, 0.2f);
		  v2 knobClickableDim = V2(0.8f, 0.7f);
		  //v2 knobTextOffset = hadamard(V2(0, 0.04f), panelDim);
		  r32 knobDragLength = 0.35f*panelDim.y;

#if 0
		  // NOTE: play (for now)
		  {
		    v2 playOffsetPOP = V2(0.05f, 0.6f);
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
#endif

		  v4 mouseTooltipBackgroundColor = V4(1, 1, 0.5f, 1);
		  v4 mouseTooltipTextColor = V4(0.055f, 0.055f, 0.098f, 1);
		  r32 mouseTooltipTextScale = 0.7f;

		  // TODO: a lot of these per-element calculations could be
		  //       condensed into loops over parameters (or an X-Macro)
		  //       if we stored more data in some array (or X-List)
		  //       (e.g. position, texture, tooltip label, ...)
		  UIComm parameterComms[PluginParameter_count];

		  // NOTE: get comms from all elements
		  {
		    // NOTE: volume
		    {
		      v2 volumeOffsetPOP = V2(0.857f, 0.43f);
		      v2 volumeDimPOP = V2(0.2f, 0.46f);
		      v2 volumeClickableOffset = V2(0.3f, 0);
		      v2 volumeClickableDim = V2(0.4f, 1.f);
		      v2 volumeTextOffset = hadamard(V2(0.001f, -0.045f), panelDim);
		      parameterComms[PluginParameter_volume] =
			uiMakeSlider(panelLayout, STR8_LIT("LEVEL"),
				     volumeOffsetPOP, volumeDimPOP, 0.5f,
				     &pluginState->parameters[PluginParameter_volume],
				     volumeClickableOffset, volumeClickableDim,
				     volumeTextOffset, elementTextScale,
				     pluginState->levelBar, pluginState->levelFader, 0,
				     V4(1, 1, 1, 1));
		    }

		    // NOTE: density
		    {
		      r32 densityDimPOP = 0.235f;
		      v2 densityOffsetPOP = V2(0.45f, 0.06f);
		      v2 densitySizePOP = densityDimPOP*V2(1, 1);
		      v2 densityClickableOffset = V2(0.f, 0.1f);
		      v2 densityClickableDim = V2(1.f, 0.9f);
		      v2 densityTextOffset = hadamard(V2(0, -0.015f), panelDim);
		      parameterComms[PluginParameter_density] =
			uiMakeKnob(panelLayout, STR8_LIT("DENSITY"),
				   densityOffsetPOP, densitySizePOP, 1.f,
				   &pluginState->parameters[PluginParameter_density],
				   V2(-0.02f, 0.02f), V2(1.02f, 1),
				   densityClickableOffset, densityClickableDim,
				   densityTextOffset, elementTextScale,
				   pluginState->densityKnob, pluginState->densityKnobLabel,
				   V4(1, 1, 1, 1));
		    }

		    // NOTE: spread
		    {
		      v2 spreadOffsetPOP = V2(-0.001f, 0.067f);
		      v2 spreadSizePOP = knobDimPOP*V2(1, 1);
		      v2 spreadTextOffset = hadamard(V2(0, 0.038f), panelDim);
		      parameterComms[PluginParameter_spread] =
			uiMakeKnob(panelLayout, STR8_LIT("SPREAD"),
				   spreadOffsetPOP, spreadSizePOP, 1.f,
				   &pluginState->parameters[PluginParameter_spread],
				   knobLabelOffset, knobLabelDim,
				   knobClickableOffset, knobClickableDim,
				   spreadTextOffset, elementTextScale,
				   pluginState->pomegranateKnob, pluginState->pomegranateKnobLabel,
				   V4(1, 1, 1, 1));
		    }

		    // NOTE: offset
		    {
		      v2 offsetOffsetPOP = V2(0.1285f, 0.004f);
		      v2 offsetSizePOP = knobDimPOP*V2(1, 1);
		      v2 offsetTextOffset = hadamard(V2(0.002f, 0.038f), panelDim);
		      parameterComms[PluginParameter_offset] =
			uiMakeKnob(panelLayout, STR8_LIT("OFFSET"),
				   offsetOffsetPOP, offsetSizePOP, 1.f,
				   &pluginState->parameters[PluginParameter_offset],
				   knobLabelOffset, knobLabelDim,
				   knobClickableOffset, knobClickableDim,
				   offsetTextOffset, elementTextScale,
				   pluginState->pomegranateKnob, pluginState->pomegranateKnobLabel,
				   V4(1, 1, 1, 1));
		    }

		    // NOTE: size
		    {
		      v2 sizeOffsetPOP = V2(0.239f, 0.109f);
		      v2 sizeDimPOP = knobDimPOP*V2(1, 1);
		      v2 sizeTextOffset = hadamard(V2(0.002f, 0.038f), panelDim);
		      parameterComms[PluginParameter_size] =
			uiMakeKnob(panelLayout, STR8_LIT("SIZE"), sizeOffsetPOP, sizeDimPOP, 1.f,
				   &pluginState->parameters[PluginParameter_size],
				   knobLabelOffset, knobLabelDim,
				   knobClickableOffset, knobClickableDim,
				   sizeTextOffset, elementTextScale,
				   pluginState->pomegranateKnob, pluginState->pomegranateKnobLabel,
				   V4(1, 1, 1, 1));
		    }

		    // NOTE: mix
		    {
		      r32 mixDimPOP = 0.235f;
		      v2 mixOffsetPOP = V2(0.613f, 0.131f);
		      v2 mixSizePOP = mixDimPOP*V2(1, 1);
		      v2 mixLabelOffset = V2(0.02f, 0.068f);
		      v2 mixTextOffset = hadamard(V2(0.003f, 0.042f), panelDim);
		      parameterComms[PluginParameter_mix] =
			uiMakeKnob(panelLayout, STR8_LIT("MIX"), mixOffsetPOP, mixSizePOP, 1.f,
				   &pluginState->parameters[PluginParameter_mix],
				   mixLabelOffset, knobLabelDim,
				   knobClickableOffset, knobClickableDim,
				   mixTextOffset, elementTextScale,
				   pluginState->halfPomegranateKnob,
				   pluginState->halfPomegranateKnobLabel,
				   V4(1, 1, 1, 1));
		    }

		    // NOTE: pan
		    {
		      v2 panOffsetPOP = V2(0.727f, 0.0008f);
		      v2 panSizePOP = knobDimPOP * V2(1, 1);
		      v2 panLabelOffset = V2(0.03f, 0.065f);
		      v2 panTextOffset = hadamard(V2(0.003f, 0.043f), panelDim);
		      parameterComms[PluginParameter_pan] =
			uiMakeKnob(panelLayout, STR8_LIT("PAN"), panOffsetPOP, panSizePOP, 1.f,
				   &pluginState->parameters[PluginParameter_pan],
				   panLabelOffset, knobLabelDim,
				   knobClickableOffset, knobClickableDim,
				   panTextOffset, elementTextScale,
				   pluginState->halfPomegranateKnob,
				   pluginState->halfPomegranateKnobLabel,
				   V4(1, 1, 1, 1));
		    }

		    // NOTE: window
		    {
		      v2 windowOffsetPOP = V2(0.854f, 0.063f);
		      v2 windowSizePOP = knobDimPOP*V2(1, 1);
		      v2 windowLabelOffset = V2(0.03f, 0.065f);
		      v2 windowTextOffset = hadamard(V2(0.003f, 0.02f), panelDim);
		      parameterComms[PluginParameter_window] =
			uiMakeKnob(panelLayout, STR8_LIT("WINDOW"), windowOffsetPOP, windowSizePOP, 1.f,
				   &pluginState->parameters[PluginParameter_window],
				   windowLabelOffset, knobLabelDim,
				   knobClickableOffset, knobClickableDim,
				   windowTextOffset, elementTextScale,
				   pluginState->halfPomegranateKnob,
				   pluginState->halfPomegranateKnobLabel,
				   V4(1, 1, 1, 1));
		    }
		  }
		  
		  // NOTE: modify parameters if they are interacting		  
		  {
		    // NOTE: volume
		    { 
		    UIComm volume = parameterComms[PluginParameter_volume];
		    if(volume.flags & UICommFlag_dragging)
		      {
			uiContext->interactingElement = volume.element->hashKey;
			gsAtomicStore(&volume.element->fParam->interacting, 1);
		      
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
		    else
		      {
			gsAtomicStore(&volume.element->fParam->interacting, 0);
			if(uiHashKeysAreEqual(uiContext->interactingElement, volume.element->hashKey))
			  {
			    ZERO_STRUCT(&uiContext->interactingElement);
			  }
		      }
		    }

		    // NOTE: density
		    {
		      UIComm density = parameterComms[PluginParameter_density];
		      if(density.flags & UICommFlag_dragging)
		      {
			uiContext->interactingElement = density.element->hashKey;
			gsAtomicStore(&density.element->fParam->interacting, 1);

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
		    else
		      {
			gsAtomicStore(&density.element->fParam->interacting, 0);
			if(uiHashKeysAreEqual(uiContext->interactingElement, density.element->hashKey))
			  {  
			    ZERO_STRUCT(&uiContext->interactingElement);
			  }
		      }
		    }

		    // NOTE: spread
		    {
		      UIComm spread = parameterComms[PluginParameter_spread];
		      if(spread.flags & UICommFlag_dragging)
		      {
			uiContext->interactingElement = spread.element->hashKey;
			gsAtomicStore(&spread.element->fParam->interacting, 1);

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
		    else
		      {
			gsAtomicStore(&spread.element->fParam->interacting, 0);
			if(uiHashKeysAreEqual(uiContext->interactingElement, spread.element->hashKey))
			  {
			    ZERO_STRUCT(&uiContext->interactingElement);
			  }
		      }
		    }

		    // NOTE: offset
		    {
		      UIComm offset = parameterComms[PluginParameter_offset];
		      if(offset.flags & UICommFlag_dragging)
		      {
			uiContext->interactingElement = offset.element->hashKey;
			gsAtomicStore(&offset.element->fParam->interacting, 1);

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
		    else
		      {
			gsAtomicStore(&offset.element->fParam->interacting, 0);
			if(uiHashKeysAreEqual(uiContext->interactingElement, offset.element->hashKey))
			  {
			    ZERO_STRUCT(&uiContext->interactingElement);
			  }
		      }
		    }

		    // NOTE: size
		    {
		      UIComm size = parameterComms[PluginParameter_size];
		      if(size.flags & UICommFlag_dragging)
		      {
			uiContext->interactingElement = size.element->hashKey;
			gsAtomicStore(&size.element->fParam->interacting, 1);
		      
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
		    else
		      {
			gsAtomicStore(&size.element->fParam->interacting, 0);
			if(uiHashKeysAreEqual(uiContext->interactingElement, size.element->hashKey))
			  {
			    ZERO_STRUCT(&uiContext->interactingElement);
			  }
		      }
		    }

		    // NOTE: mix
		    {
		      UIComm mix = parameterComms[PluginParameter_mix];
		      if(mix.flags & UICommFlag_dragging)
		      {
			uiContext->interactingElement = mix.element->hashKey;
			gsAtomicStore(&mix.element->fParam->interacting, 1);

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
		    else
		      {
			gsAtomicStore(&mix.element->fParam->interacting, 0);
			if(uiHashKeysAreEqual(uiContext->interactingElement, mix.element->hashKey))
			  {  
			    ZERO_STRUCT(&uiContext->interactingElement);
			  }
		      }
		    }

		    // NOTE: pan
		    {
		      UIComm pan = parameterComms[PluginParameter_pan];
		      if(pan.flags & UICommFlag_dragging)
		      {
			uiContext->interactingElement = pan.element->hashKey;
			gsAtomicStore(&pan.element->fParam->interacting, 1);

			if(pan.flags & UICommFlag_leftDragging)
			  {
			    v2 dragDelta = uiGetDragDelta(pan.element);
			    r32 newpan = (pan.element->fParamValueAtClick +
					  dragDelta.y / knobDragLength * getLength(pan.element->fParam->range));

			    pluginSetFloatParameter(pan.element->fParam, newpan);
			  }
			else if(pan.flags & UICommFlag_minusPressed)
			  {
			    r32 oldpan = pluginReadFloatParameter(pan.element->fParam);
			    r32 newpan = oldpan - 0.02f * getLength(pan.element->fParam->range);

			    pluginSetFloatParameter(pan.element->fParam, newpan);
			  }
			else if(pan.flags & UICommFlag_plusPressed)
			  {
			    r32 oldpan = pluginReadFloatParameter(pan.element->fParam);
			    r32 newpan = oldpan + 0.02f * getLength(pan.element->fParam->range);

			    pluginSetFloatParameter(pan.element->fParam, newpan);
			  }
		      }
		    else
		      {
			gsAtomicStore(&pan.element->fParam->interacting, 0);
			if(uiHashKeysAreEqual(uiContext->interactingElement, pan.element->hashKey))
			  {  
			    ZERO_STRUCT(&uiContext->interactingElement);
			  }
		      }
		    }

		    // NOTE: window
		    {
		      UIComm window = parameterComms[PluginParameter_window];
		      if(window.flags & UICommFlag_dragging)
		      {
			uiContext->interactingElement = window.element->hashKey;
			gsAtomicStore(&window.element->fParam->interacting, 1);

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
		    else
		      {
			gsAtomicStore(&window.element->fParam->interacting, 0);
			if(uiHashKeysAreEqual(uiContext->interactingElement, window.element->hashKey))
			  {  
			    ZERO_STRUCT(&uiContext->interactingElement);
			  }
		      }
		    }
		  }

#if PLUGIN_PARAMETER_TOOLTIPS
		  // NOTE: draw tooltips
		  {
		    // NOTE: volume
		    {
		      UIComm volume = parameterComms[PluginParameter_volume];
		      b32 volumeIsInteracting =
			uiHashKeysAreEqual(uiContext->interactingElement, volume.element->hashKey);
		      b32 volumeIsHovering = ((volume.flags & UICommFlag_hovering) &&
					      uiHashKeyIsNull(uiContext->interactingElement));
		      if(volumeIsInteracting || volumeIsHovering)
			{
			  ASSERT(pluginState->mouseTooltipLayout == 0);
			  pluginState->mouseTooltipLayout =
			    arenaPushStruct(pluginState->frameArena, UILayout, arenaFlagsZeroNoAlign());

			  String8 volumeTooltipMessage =
			    arenaPushStringFormat(pluginState->frameArena,
						  "volume: %.2f dB",
						  pluginReadFloatParameter(volume.element->fParam));
			  v2 messageRectMin = V2(MAX(uiContext->mouseP.x,
						     volume.element->region.max.x),
						 uiContext->mouseP.y);
			  v2 messageRectDim =
			    (getTextDim(uiContext->font, volumeTooltipMessage, mouseTooltipTextScale) +
			     V2(10, uiContext->font->verticalAdvance * 0.5f));
			  if(messageRectMin.x + messageRectDim.x >= renderCommands->widthInPixels)
			    {
			      messageRectMin.x = volume.element->region.min.x - messageRectDim.x;
			    }
			  Rect2 messageRect = rectMinDim(messageRectMin,
							 messageRectDim);
		      
			  uiBeginLayout(pluginState->mouseTooltipLayout, uiContext,
					messageRect, mouseTooltipBackgroundColor,
					PLUGIN_ASSET(null), 1);
			  pluginState->mouseTooltipLayout->regionRemaining.min += V2(5, 5);
			  uiMakeTextElement(pluginState->mouseTooltipLayout,
					    volumeTooltipMessage,
					    mouseTooltipTextScale, mouseTooltipTextColor);
		      
			}
		    }

		    // NOTE: density
		    {
		      UIComm density = parameterComms[PluginParameter_density];
		      b32 densityIsInteracting =
			uiHashKeysAreEqual(uiContext->interactingElement, density.element->hashKey);
		      b32 densityIsHovering = ((density.flags & UICommFlag_hovering) &&
					       uiHashKeyIsNull(uiContext->interactingElement));
		      if(densityIsInteracting || densityIsHovering)
			{
			  ASSERT(pluginState->mouseTooltipLayout == 0);
			  pluginState->mouseTooltipLayout =
			    arenaPushStruct(pluginState->frameArena, UILayout, arenaFlagsZeroNoAlign());

			  String8 densityTooltipMessage =
			    arenaPushStringFormat(pluginState->frameArena,
						  "density: %.2f",
						  pluginReadFloatParameter(density.element->fParam));
			  v2 messageRectMin = V2(MAX(uiContext->mouseP.x,
						     density.element->region.max.x),
						 uiContext->mouseP.y);
			  v2 messageRectDim =
			    (getTextDim(uiContext->font, densityTooltipMessage, mouseTooltipTextScale) +
			     V2(10, uiContext->font->verticalAdvance * 0.5f));
			  if(messageRectMin.x + messageRectDim.x >= renderCommands->widthInPixels)
			    {
			      messageRectMin.x = density.element->region.min.x - messageRectDim.x;
			    }
			  Rect2 messageRect = rectMinDim(messageRectMin,
							 messageRectDim);
		      
			  uiBeginLayout(pluginState->mouseTooltipLayout, uiContext,
					messageRect, mouseTooltipBackgroundColor,
					PLUGIN_ASSET(null), 1);
			  pluginState->mouseTooltipLayout->regionRemaining.min += V2(5, 5);
			  uiMakeTextElement(pluginState->mouseTooltipLayout,
					    densityTooltipMessage,
					    mouseTooltipTextScale, mouseTooltipTextColor);
		      
			}
		    }

		    // NOTE: spread
		    {
		      UIComm spread = parameterComms[PluginParameter_spread];
		      b32 spreadIsInteracting =
			uiHashKeysAreEqual(uiContext->interactingElement, spread.element->hashKey);
		      b32 spreadIsHovering = ((spread.flags & UICommFlag_hovering) &&
					      uiHashKeyIsNull(uiContext->interactingElement));
		      if(spreadIsInteracting || spreadIsHovering)
			{
			  ASSERT(pluginState->mouseTooltipLayout == 0);
			  pluginState->mouseTooltipLayout =
			    arenaPushStruct(pluginState->frameArena, UILayout, arenaFlagsZeroNoAlign());

			  String8 spreadTooltipMessage =
			    arenaPushStringFormat(pluginState->frameArena,
						  "spread: %.2f",
						  pluginReadFloatParameter(spread.element->fParam));
			  v2 messageRectMin = V2(MAX(uiContext->mouseP.x,
						     spread.element->region.max.x),
						 uiContext->mouseP.y);
			  v2 messageRectDim =
			    (getTextDim(uiContext->font, spreadTooltipMessage, mouseTooltipTextScale) +
			     V2(10, uiContext->font->verticalAdvance * 0.5f));
			  if(messageRectMin.x + messageRectDim.x >= renderCommands->widthInPixels)
			    {
			      messageRectMin.x = spread.element->region.min.x - messageRectDim.x;
			    }
			  Rect2 messageRect = rectMinDim(messageRectMin,
							 messageRectDim);
		      
			  uiBeginLayout(pluginState->mouseTooltipLayout, uiContext,
					messageRect, mouseTooltipBackgroundColor,
					PLUGIN_ASSET(null), 1);
			  pluginState->mouseTooltipLayout->regionRemaining.min += V2(5, 5);
			  uiMakeTextElement(pluginState->mouseTooltipLayout,
					    spreadTooltipMessage,
					    mouseTooltipTextScale, mouseTooltipTextColor);
		      
			}
		    }

		    // NOTE: offset
		    {
		      UIComm offset = parameterComms[PluginParameter_offset];
		      b32 offsetIsInteracting =
			uiHashKeysAreEqual(uiContext->interactingElement, offset.element->hashKey);
		      b32 offsetIsHovering = ((offset.flags & UICommFlag_hovering) &&
					      uiHashKeyIsNull(uiContext->interactingElement));
		      if(offsetIsInteracting || offsetIsHovering)
			{
			  ASSERT(pluginState->mouseTooltipLayout == 0);
			  pluginState->mouseTooltipLayout =
			    arenaPushStruct(pluginState->frameArena, UILayout, arenaFlagsZeroNoAlign());

			  String8 offsetTooltipMessage =
			    arenaPushStringFormat(pluginState->frameArena,
						  "offset: %.2f",
						  pluginReadFloatParameter(offset.element->fParam));
			  v2 messageRectMin = V2(MAX(uiContext->mouseP.x,
						     offset.element->region.max.x),
						 uiContext->mouseP.y);
			  v2 messageRectDim =
			    (getTextDim(uiContext->font, offsetTooltipMessage, mouseTooltipTextScale) +
			     V2(10, uiContext->font->verticalAdvance * 0.5f));
			  if(messageRectMin.x + messageRectDim.x >= renderCommands->widthInPixels)
			    {
			      messageRectMin.x = offset.element->region.min.x - messageRectDim.x;
			    }
			  Rect2 messageRect = rectMinDim(messageRectMin,
							 messageRectDim);
		      
			  uiBeginLayout(pluginState->mouseTooltipLayout, uiContext,
					messageRect, mouseTooltipBackgroundColor,
					PLUGIN_ASSET(null), 1);
			  pluginState->mouseTooltipLayout->regionRemaining.min += V2(5, 5);
			  uiMakeTextElement(pluginState->mouseTooltipLayout,
					    offsetTooltipMessage,
					    mouseTooltipTextScale, mouseTooltipTextColor);
		      
			}
		    }

		    // NOTE: size
		    {
		      UIComm size = parameterComms[PluginParameter_size];
		      b32 sizeIsInteracting =
			uiHashKeysAreEqual(uiContext->interactingElement, size.element->hashKey);
		      b32 sizeIsHovering = ((size.flags & UICommFlag_hovering) &&
					    uiHashKeyIsNull(uiContext->interactingElement));
		      if(sizeIsInteracting || sizeIsHovering)
			{
			  ASSERT(pluginState->mouseTooltipLayout == 0);
			  pluginState->mouseTooltipLayout =
			    arenaPushStruct(pluginState->frameArena, UILayout, arenaFlagsZeroNoAlign());

			  String8 sizeTooltipMessage =
			    arenaPushStringFormat(pluginState->frameArena,
						  "size: %.2f",
						  pluginReadFloatParameter(size.element->fParam));
			  v2 messageRectMin = V2(MAX(uiContext->mouseP.x,
						     size.element->region.max.x),
						 uiContext->mouseP.y);
			  v2 messageRectDim =
			    (getTextDim(uiContext->font, sizeTooltipMessage, mouseTooltipTextScale) +
			     V2(10, uiContext->font->verticalAdvance * 0.5f));
			  if(messageRectMin.x + messageRectDim.x >= renderCommands->widthInPixels)
			    {
			      messageRectMin.x = size.element->region.min.x - messageRectDim.x;
			    }
			  Rect2 messageRect = rectMinDim(messageRectMin,
							 messageRectDim);
		      
			  uiBeginLayout(pluginState->mouseTooltipLayout, uiContext,
					messageRect, mouseTooltipBackgroundColor,
					PLUGIN_ASSET(null), 1);
			  pluginState->mouseTooltipLayout->regionRemaining.min += V2(5, 5);
			  uiMakeTextElement(pluginState->mouseTooltipLayout,
					    sizeTooltipMessage,
					    mouseTooltipTextScale, mouseTooltipTextColor);
		      
			}
		    }

		    // NOTE: mix
		    {
		      UIComm mix = parameterComms[PluginParameter_mix];
		      b32 mixIsInteracting =
			uiHashKeysAreEqual(uiContext->interactingElement, mix.element->hashKey);
		      b32 mixIsHovering = ((mix.flags & UICommFlag_hovering) &&
					   uiHashKeyIsNull(uiContext->interactingElement));
		      if(mixIsInteracting || mixIsHovering)
			{
			  ASSERT(pluginState->mouseTooltipLayout == 0);
			  pluginState->mouseTooltipLayout =
			    arenaPushStruct(pluginState->frameArena, UILayout, arenaFlagsZeroNoAlign());

			  String8 mixTooltipMessage =
			    arenaPushStringFormat(pluginState->frameArena,
						  "mix: %.2f",
						  pluginReadFloatParameter(mix.element->fParam));
			  v2 messageRectMin = V2(MAX(uiContext->mouseP.x,
						     mix.element->region.max.x),
						 uiContext->mouseP.y);
			  v2 messageRectDim =
			    (getTextDim(uiContext->font, mixTooltipMessage, mouseTooltipTextScale) +
			     V2(10, uiContext->font->verticalAdvance * 0.5f));
			  if(messageRectMin.x + messageRectDim.x >= renderCommands->widthInPixels)
			    {
			      messageRectMin.x = mix.element->region.min.x - messageRectDim.x;
			    }
			  Rect2 messageRect = rectMinDim(messageRectMin,
							 messageRectDim);
		      
			  uiBeginLayout(pluginState->mouseTooltipLayout, uiContext,
					messageRect, mouseTooltipBackgroundColor,
					PLUGIN_ASSET(null), 1);
			  pluginState->mouseTooltipLayout->regionRemaining.min += V2(5, 5);
			  uiMakeTextElement(pluginState->mouseTooltipLayout,
					    mixTooltipMessage,
					    mouseTooltipTextScale, mouseTooltipTextColor);
		      
			}
		    }

		    // NOTE: pan
		    {
		      UIComm pan = parameterComms[PluginParameter_pan];
		      b32 panIsInteracting =
			uiHashKeysAreEqual(uiContext->interactingElement, pan.element->hashKey);
		      b32 panIsHovering = ((pan.flags & UICommFlag_hovering) &&
					   uiHashKeyIsNull(uiContext->interactingElement));
		      if(panIsInteracting || panIsHovering)
			{
			  ASSERT(pluginState->mouseTooltipLayout == 0);
			  pluginState->mouseTooltipLayout =
			    arenaPushStruct(pluginState->frameArena, UILayout, arenaFlagsZeroNoAlign());

			  String8 panTooltipMessage =
			    arenaPushStringFormat(pluginState->frameArena,
						  "pan: %.2f",
						  pluginReadFloatParameter(pan.element->fParam));
			  v2 messageRectMin = V2(MAX(uiContext->mouseP.x,
						     pan.element->region.max.x),
						 uiContext->mouseP.y);
			  v2 messageRectDim =
			    (getTextDim(uiContext->font, panTooltipMessage, mouseTooltipTextScale) +
			     V2(10, uiContext->font->verticalAdvance * 0.5f));
			  if(messageRectMin.x + messageRectDim.x >= renderCommands->widthInPixels)
			    {
			      messageRectMin.x = pan.element->region.min.x - messageRectDim.x;
			    }
			  Rect2 messageRect = rectMinDim(messageRectMin,
							 messageRectDim);
		      
			  uiBeginLayout(pluginState->mouseTooltipLayout, uiContext,
					messageRect, mouseTooltipBackgroundColor,
					PLUGIN_ASSET(null), 1);
			  pluginState->mouseTooltipLayout->regionRemaining.min += V2(5, 5);
			  uiMakeTextElement(pluginState->mouseTooltipLayout,
					    panTooltipMessage, mouseTooltipTextScale, mouseTooltipTextColor);
		      
			}
		    }

		    // NOTE: window
		    {
		      UIComm window = parameterComms[PluginParameter_window];
		      b32 windowIsInteracting =
			uiHashKeysAreEqual(uiContext->interactingElement, window.element->hashKey);
		      b32 windowIsHovering = ((window.flags & UICommFlag_hovering) &&
					      uiHashKeyIsNull(uiContext->interactingElement));
		      if(windowIsInteracting || windowIsHovering)
			{
			  ASSERT(pluginState->mouseTooltipLayout == 0);
			  pluginState->mouseTooltipLayout =
			    arenaPushStruct(pluginState->frameArena, UILayout, arenaFlagsZeroNoAlign());

			  String8 windowTooltipMessage =
			    arenaPushStringFormat(pluginState->frameArena,
						  "window: %.2f",
						  pluginReadFloatParameter(window.element->fParam));
			  v2 messageRectMin = V2(MAX(uiContext->mouseP.x,
						     window.element->region.max.x),
						 uiContext->mouseP.y);
			  v2 messageRectDim =
			    (getTextDim(uiContext->font, windowTooltipMessage, mouseTooltipTextScale) +
			     V2(10, uiContext->font->verticalAdvance * 0.5f));
			  if(messageRectMin.x + messageRectDim.x >= renderCommands->widthInPixels)
			    {
			      messageRectMin.x = window.element->region.min.x - messageRectDim.x;
			    }
			  Rect2 messageRect = rectMinDim(messageRectMin,
							 messageRectDim);
		      
			  uiBeginLayout(pluginState->mouseTooltipLayout, uiContext,
					messageRect, mouseTooltipBackgroundColor,
					PLUGIN_ASSET(null), 1);
			  pluginState->mouseTooltipLayout->regionRemaining.min += V2(5, 5);
			  uiMakeTextElement(pluginState->mouseTooltipLayout,
					    windowTooltipMessage,
					    mouseTooltipTextScale, mouseTooltipTextColor);
		      
			}
		    }
		  }
#endif
		  
		  // NOTE: grain view
		  v2 drawRegionDim = getDim(panelLayout->regionRemaining);
		  v2 viewDim = hadamard(V2(0.53f, 0.25f), drawRegionDim);
		  v2 viewMin = hadamard(V2(0.24f, 0.55f), drawRegionDim);
		  Rect2 viewRect = rectMinDim(viewMin, viewDim);

		  v2 dim = hadamard(V2(0.843f, 0.62f), viewDim);
		  v2 min = viewMin + hadamard(V2(0.075f, 0.2f), viewDim);
		  //Rect2 rect = rectMinDim(min, dim);

		  r32 middleBarThickness = 4.f;
		  Rect2 middleBar = rectMinDim(min + V2(0, 0.5f*dim.y),
					       V2(dim.x, middleBarThickness));

		  v2 outlineOffset = hadamard(V2(0, 0.002f), viewDim);

		  renderPushQuad(renderCommands, viewRect, pluginState->grainViewBackground, 0,
				 RENDER_LEVEL(grainViewBackground));
		  renderPushQuad(renderCommands, middleBar, pluginState->null, 0.f,
				 RENDER_LEVEL(grainViewMiddleBar), V4(0, 0, 0, 1));
		  renderPushQuad(renderCommands, rectOffset(viewRect, outlineOffset),
				 pluginState->grainViewOutline, 0,
				 RENDER_LEVEL(grainViewBorder));
		 
		  v2 upperRegionMin = min + V2(0, 0.5f*(dim.y + middleBarThickness));
		  v2 lowerRegionMin = min;
		  v2 regionDim = V2(dim.x, 0.5f*dim.y - middleBarThickness);
		  v2 lowerRegionMiddle = lowerRegionMin + V2(0, 0.5f*regionDim.y);
		  v2 upperRegionMiddle = upperRegionMin + V2(0, 0.5f*regionDim.y);

		  u32 grainBufferCapacity = pluginState->grainBuffer.capacity;
		  GrainStateView *grainStateView = &pluginState->grainStateView;
		  AudioRingBuffer *grainViewBuffer = &grainStateView->viewBuffer;
		  
		  u32 viewEntryReadIndex = grainStateView->viewReadIndex;
		  u32 entriesQueued = gsAtomicLoad(&grainStateView->entriesQueued);
		  //u32 entriesToProcess = entiresQueued % ARRAY_COUNT(grainStateView->view);
		  logFormatString("entriesQueued: %u", entriesQueued);
		  //logFormatString("entriesToProcess: %u", entriesToProcess);

		  for(u32 entryIndex = 0; entryIndex < entriesQueued; ++entryIndex)
		    {		     		  
		      GrainBufferViewEntry *view = (grainStateView->views +
						    ((viewEntryReadIndex + entryIndex) %
						     ARRAY_COUNT(grainStateView->views)));

		      // NOTE: update view buffer with new view data
		      writeSamplesToAudioRingBuffer(grainViewBuffer,
						    view->bufferSamples[0], view->bufferSamples[1],
						    view->sampleCount);
		      
		      u32 bufferReadIndex = view->bufferReadIndex;
		      u32 bufferWriteIndex = view->bufferWriteIndex;

		      // NOTE: display view read and write positions
		      r32 barThickness = 2.f;
		      u32 readIndex = bufferReadIndex;
		      r32 readPosition = (r32)readIndex/(r32)grainBufferCapacity;
		      r32 readBarPosition = readPosition*dim.x;
		      Rect2 readBar = rectMinDim(min + V2(readBarPosition, 0.f),
						 V2(barThickness, dim.y));
		      renderPushQuad(renderCommands, readBar, pluginState->null, 0.f,
				     RENDER_LEVEL(grainViewMarker), V4(1, 0, 0, 1));
		      
		      u32 writeIndex = bufferWriteIndex;
		      r32 writePosition = (r32)writeIndex/(r32)grainBufferCapacity;
		      r32 writeBarPosition = writePosition*dim.x;
		      Rect2 writeBar = rectMinDim(min + V2(writeBarPosition, 0.f),
						  V2(barThickness, dim.y));
		      renderPushQuad(renderCommands, writeBar, pluginState->null, 0.f,
				     RENDER_LEVEL(grainViewMarker), V4(1, 1, 1, 1));

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
			  renderPushQuad(renderCommands, grainStartBar, pluginState->null, 0.f,
					 RENDER_LEVEL(grainViewMarker), grainWindowColor);
			  renderPushQuad(renderCommands, grainEndBar, pluginState->null, 0.f,
					 RENDER_LEVEL(grainViewMarker), grainWindowColor);
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
		      renderPushQuad(renderCommands, sampleLBar, pluginState->null, 0.f,
				     RENDER_LEVEL(grainViewSignal), V4(0, 1, 0, 1));
		      renderPushQuad(renderCommands, sampleRBar, pluginState->null, 0.f,
				     RENDER_LEVEL(grainViewSignal), V4(0, 1, 0, 1));

		      lastSampleIndex = sampleIndex;
		    }

		  grainStateView->viewReadIndex =
		    (viewEntryReadIndex + entriesQueued) % ARRAY_COUNT(grainStateView->views);
		  u32 oldEntriesQueued = entriesQueued;
		  while(gsAtomicCompareAndSwap(&grainStateView->entriesQueued,
							    entriesQueued,
							    entriesQueued - oldEntriesQueued) != entriesQueued)
		    {
		      entriesQueued = gsAtomicLoad(&grainStateView->entriesQueued);
		    }		    
	      
		  renderPushUILayout(renderCommands, panelLayout);
		  uiEndLayout(panelLayout);

		  if(pluginState->mouseTooltipLayout)
		    {
		      renderPushUILayout(renderCommands, pluginState->mouseTooltipLayout);
		      uiEndLayout(pluginState->mouseTooltipLayout);
		      pluginState->mouseTooltipLayout = 0;
		    }
		}	  
	    }
	}
      
      arenaReleaseScratch(scratch);
      uiContextEndFrame(uiContext);
      
      arenaEnd(pluginState->frameArena);

      //logFormatString("permanent arena used %zu bytes", pluginState->permanentArena->pos);
    }  
}

EXPORT_FUNCTION void
gsAudioProcess(PluginMemory *memory, PluginAudioBuffer *audioBuffer)
{
  UNUSED(memory);

  if(globalPluginState)
    {
      PluginState *pluginState = globalPluginState;
      if(pluginState->initialized)
	{
	  TemporaryMemory scratch = arenaGetScratch(0, 0);

	  // NOTE: dequeue host-driven parameter value changes
	  u32 queuedCount = gsAtomicLoad(&audioBuffer->queuedCount);
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
	      while(gsAtomicCompareAndSwap(&audioBuffer->queuedCount, queuedCount, queuedCount - entriesRead) != queuedCount)
		{
		  queuedCount = gsAtomicLoad(&audioBuffer->queuedCount);
		}
	    }	  

	  // NOTE: copy plugin input audio to the grain buffer
	  u32 framesToRead = audioBuffer->framesToWrite;      
	  r32 inputBufferReadSpeed = ((audioBuffer->inputSampleRate != 0) ?
				      (r32)INTERNAL_SAMPLE_RATE/(r32)audioBuffer->inputSampleRate : 1.f);
	  r32 scaledFramesToRead = inputBufferReadSpeed*framesToRead;
	  UNUSED(scaledFramesToRead);
      
	  AudioRingBuffer *gbuff = &pluginState->grainBuffer;
	  //PlayingSound *loadedSound = &pluginState->loadedSound;
	  logFormatString("samples to read: %lu", framesToRead);

	  r32 *inputMixBuffersArray = arenaPushArray(scratch.arena, 2*framesToRead, r32,
						     arenaFlagsZeroAlign(4*sizeof(r32)));
	  r32 *inputMixBuffers[2] = {};
	  inputMixBuffers[0] = inputMixBuffersArray + (0 * framesToRead);
	  inputMixBuffers[1] = inputMixBuffersArray + (1 * framesToRead);

	  const void *genericInputFrames[2] = {};
	  genericInputFrames[0] = audioBuffer->inputBuffer[0];
	  genericInputFrames[1] = audioBuffer->inputBuffer[1];

	  for(u32 frameIndex = 0; frameIndex < framesToRead; ++frameIndex)
	    {		
	      bool soundIsPlaying = pluginReadBooleanParameter(&pluginState->soundIsPlaying);       
	      // r32 currentTime = (r32)loadedSound->samplesPlayed + inputBufferReadSpeed*(r32)frameIndex;
	      // u32 soundReadIndex = (u32)currentTime;
	      // r32 soundReadFrac = currentTime - (r32)soundReadIndex;		

	      for(u32 channelIndex = 0; channelIndex < audioBuffer->inputChannels; ++channelIndex)
		{
		  r32 mixedVal = 0.f;		   
		  if(soundIsPlaying)		       
		    {			
		      // if(currentTime < loadedSound->sound.sampleCount)
		      // 	{
		      // 	  r32 loadedSoundSample0 = loadedSound->sound.samples[channelIndex][soundReadIndex];
		      // 	  r32 loadedSoundSample1 = loadedSound->sound.samples[channelIndex][soundReadIndex + 1];
		      // 	  r32 loadedSoundSample = lerp(loadedSoundSample0, loadedSoundSample1, soundReadFrac);

		      // 	  mixedVal += 0.5f*loadedSoundSample;
		      // 	}
		      // else
		      // 	{		      
		      // 	  pluginSetBooleanParameter(&pluginState->soundIsPlaying, false);
		      // 	  loadedSound->samplesPlayed = 0;// + pluginState->start_pos);
		      // 	}		  
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

		      // default: {ASSERT(!"invalid input format");} break;
		    default: {} break;
		    }
	      
		  inputMixBuffers[channelIndex][frameIndex] = mixedVal;		   
		}
	    }
      
	  bool soundIsPlaying = pluginReadBooleanParameter(&pluginState->soundIsPlaying);
	  if(soundIsPlaying)
	    {
	      //loadedSound->samplesPlayed += scaledFramesToRead;
	    }

	  writeSamplesToAudioRingBuffer(gbuff, inputMixBuffers[0], inputMixBuffers[1], framesToRead);
	  //arenaEndTemporaryMemory(&inputMixerMemory);

	  // NOTE: grain playback
	  GrainManager* gManager = &pluginState->grainManager;
	  u32 framesToWrite = audioBuffer->framesToWrite;

	  // NOTE: parameter updates
	  r32 *paramValsArray = arenaPushArray(scratch.arena, 8 * framesToWrite, r32,
					       arenaFlagsZeroNoAlign());
	  r32 *volumeParamVals	= paramValsArray + (0 * framesToWrite);
	  r32 *densityParamVals = paramValsArray + (1 * framesToWrite);
	  r32 *panParamVals	= paramValsArray + (2 * framesToWrite);
	  r32 *sizeParamVals	= paramValsArray + (3 * framesToWrite);
	  r32 *windowParamVals	= paramValsArray + (4 * framesToWrite);
	  r32 *spreadParamVals	= paramValsArray + (5 * framesToWrite);
	  r32 *mixParamVals	= paramValsArray + (6 * framesToWrite);
	  r32 *offsetParamVals  = paramValsArray + (7 * framesToWrite);
	  for(u32 frameIdx = 0; frameIdx < framesToWrite; ++frameIdx)
	    {
	      volumeParamVals[frameIdx] =
		pluginUpdateFloatParameter(pluginState->parameters + PluginParameter_volume);
	      densityParamVals[frameIdx] =
		pluginUpdateFloatParameter(pluginState->parameters + PluginParameter_density);
	      panParamVals[frameIdx] =
		pluginUpdateFloatParameter(pluginState->parameters + PluginParameter_pan);
	      sizeParamVals[frameIdx] =
		pluginUpdateFloatParameter(pluginState->parameters + PluginParameter_size);
	      windowParamVals[frameIdx] =
		pluginUpdateFloatParameter(pluginState->parameters + PluginParameter_window);
	      spreadParamVals[frameIdx] =
		pluginUpdateFloatParameter(pluginState->parameters + PluginParameter_spread);
	      mixParamVals[frameIdx] =
		pluginUpdateFloatParameter(pluginState->parameters + PluginParameter_mix);
	      offsetParamVals[frameIdx] =
		pluginUpdateFloatParameter(pluginState->parameters + PluginParameter_offset);
	    }     

	  u32 targetOffset = (u32)offsetParamVals[0];
	  r32 *grainMixBuffersArray = arenaPushArray(scratch.arena, 2 * framesToWrite, r32,
						     arenaFlagsZeroNoAlign());
	  r32 *grainMixBuffers[2] = {};
	  grainMixBuffers[0] = grainMixBuffersArray + (0 * framesToWrite);
	  grainMixBuffers[1] = grainMixBuffersArray + (1 * framesToWrite);
	  synthesize(grainMixBuffers[0], grainMixBuffers[1],
		     gManager, &pluginState->grainStateView,
		     densityParamVals, sizeParamVals, windowParamVals, spreadParamVals,
		     targetOffset,
		     framesToWrite);
	  logFormatString("grainMixBuffers = %p, %p",
			  grainMixBuffers[0], grainMixBuffers[1]);
	  // NOTE: DEBUG
	  usz grainMixBuffersIntL = INT_FROM_PTR(grainMixBuffers[0]);
	  usz grainMixBuffersIntR = INT_FROM_PTR(grainMixBuffers[1]);
#if OS_WASM
	  DEBUG_POINTER_INITIALIZE(audio__grainMixBuffersL, grainMixBuffers[0]);
	  DEBUG_POINTER_INITIALIZE(audio__grainMixBuffersR, grainMixBuffers[1]);
#endif

	  // NOTE: audio output
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
	  genericOutputFrames[0] = (void*)audioBuffer->outputBuffer[0];
	  genericOutputFrames[1] = (void*)audioBuffer->outputBuffer[1];     
	  logFormatString("genericOutputFrames = %p, %p",
			  genericOutputFrames[0], genericOutputFrames[1]);
	  // NOTE: DEBUG
	  usz genericOutputFramesIntL = INT_FROM_PTR(genericOutputFrames[0]);
	  usz genericOutputFramesIntR = INT_FROM_PTR(genericOutputFrames[1]);
	  UNUSED(genericOutputFramesIntL);
	  UNUSED(genericOutputFramesIntR);

	  u8 *atMidiBuffer = audioBuffer->midiBuffer;

	  logFormatString("samples to write: %lu", audioBuffer->framesToWrite);
	  for(u32 frameIndex = 0; frameIndex < audioBuffer->framesToWrite; ++frameIndex)
	    {
	      atMidiBuffer = midi::parseMidiMessage(atMidiBuffer, pluginState,
						    audioBuffer->midiMessageCount, frameIndex);

	      r32 volume = formatVolumeFactor * volumeParamVals[frameIndex];
	      r32 mixParam = mixParamVals[frameIndex];
	      r32 panner = panParamVals[frameIndex];
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
		  if(grainMixBuffersIntL != INT_FROM_PTR(grainMixBuffers[0]) ||
		     grainMixBuffersIntR != INT_FROM_PTR(grainMixBuffers[1]))
		    {
		      logFormatString("grain mix buffers (%p, %p) got fucked up "
				      "at sample %u, channel %u",
				      grainMixBuffers[0], grainMixBuffers[1],
				      frameIndex, channelIndex);
		    }

		  r32 grainVal = grainMixBuffers[channelIndex][frameIndex];
		  r32 leftGrainVal = grainMixBuffers[0][frameIndex];
		  r32 rightGrainVal = grainMixBuffers[1][frameIndex];
		  r32 stereoWidth = spreadParamVals[frameIndex];
		  if (channelIndex < 2) { // Make sure we only process left and right channels
		    //r32 widthVal = stereoWidth * 0.5f;
		    r32 tmp = 1.0f / MAX(1.0f + stereoWidth, 2.0f);
		    r32 coef_M = 1.0f * tmp;
		    r32 coef_S = stereoWidth * tmp;

		    r32 mid = (leftGrainVal + rightGrainVal) * coef_M;
		    r32 sides = (rightGrainVal - leftGrainVal) * coef_S;

		    // Update grain value based on channel
		    if (channelIndex == 0) {
		      grainVal = mid - sides; // Left channel
		      grainVal = grainVal * (1 - panner);
		    }
		    else {
		      grainVal = mid + sides; // Right channel
		      grainVal = grainVal * (1 + panner);
		    }
		  }
#endif
	      	      
		  mixedVal += lerp(inputMixBuffers[channelIndex][frameIndex], grainVal, mixParam);
		  //logFormatString("mixedVal: %.2f", mixedVal);

		  // NOTE: we can't quite do the same thing here as we do for
		  //       the grain mix buffers because these pointers are
		  //       incremented each sample
		  // if(genericOutputFramesIntL != INT_FROM_PTR(genericOutputFrames[0]) ||
		  //    genericOutputFramesIntR != INT_FROM_PTR(genericOutputFrames[1]))
		  //   {
		  //     logFormatString("generic output frames (%p, %p) got fucked up "
		  // 		      "at sample %u, channel %u",
		  // 		      genericOutputFrames[0], genericOutputFrames[1],
		  // 		      frameIndex, channelIndex);
		  //   }

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
	    }

	  arenaReleaseScratch(scratch);
	}
    }
}
