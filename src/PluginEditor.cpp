//#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "render.cpp"

using namespace juce::gl;

inline void
juceProcessButtonPress(ButtonState *newState, bool pressed)
{
  newState->endedDown = pressed;
  ++newState->halfTransitionCount;
}

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
  juce::ignoreUnused(processorRef);
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
  setSize(400, 300);
  setResizable(true, true);
  
  glContext.setRenderer(this);
  glContext.setContinuousRepainting(true);
  glContext.attachTo(*this);
  
  editorWidth = getWidth();
  editorHeight = getHeight();

  targetAspectRatio = 16.f/9.f;
  r32 editorAspectRatio = (r32)editorWidth/(r32)editorHeight;
  if(editorAspectRatio < targetAspectRatio)
    {
      // NOTE: width constrained
      displayDim.x = (r32)editorWidth;
      displayDim.y = displayDim.x/targetAspectRatio;
      displayMin = V2(0, ((r32)editorHeight - displayDim.y)*0.5f);
    }
  else
    {
      // NOTE: height constrained
      displayDim.y = (r32)editorHeight;
      displayDim.x = displayDim.y*targetAspectRatio;
      displayMin = V2(((r32)editorWidth - displayDim.x)*0.5f, 0);
    }

  commands = {};
  commands.generateNewTextures = true;
  commands.widthInPixels = (u32)displayDim.x;
  commands.heightInPixels = (u32)displayDim.y; 

  //commands = {};
  //commands.vertexCapacity = 512;
  //commands.indexCapacity = 1024;
  //commands.vertices = (Vertex *)calloc(commands.vertexCapacity, sizeof(Vertex));
  //commands.indices = (u32 *)calloc(commands.indexCapacity, sizeof(u32));

  setWantsKeyboardFocus(true);
  setMouseClickGrabsKeyboardFocus(true);
  //grabKeyboardFocus();

  inputLock = 0;
  pluginInput[0] = {};
  pluginInput[1] = {};
  newInput = &pluginInput[0];
  oldInput = &pluginInput[1];
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
  glContext.detach();
  //free(commands.vertices);
  //free(commands.indices);
}

//==============================================================================
void AudioPluginAudioProcessorEditor::
paint(juce::Graphics& g)
{
  // (Our component is opaque, so we must completely fill the background with a solid colour)
  //g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

  //g.fillAll(juce::Colours::white);

  //g.setColour(juce::Colours::black);
  //g.setFont(15.0f);
  //g.drawFittedText("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::
resized()
{
  editorWidth = getWidth();
  editorHeight = getHeight();
  r32 targetAspectRatio = 16.f/9.f;
  r32 editorAspectRatio = (r32)editorWidth/(r32)editorHeight;

  if(editorAspectRatio < targetAspectRatio)
    {
      // NOTE: width constrained
      displayDim.x = (r32)editorWidth;
      displayDim.y = displayDim.x/targetAspectRatio;
      displayMin = V2(0, ((r32)editorHeight - displayDim.y)*0.5f);
    }
  else
    {
      // NOTE: height constrained
      displayDim.y = (r32)editorHeight;
      displayDim.x = displayDim.y*targetAspectRatio;
      displayMin = V2(((r32)editorWidth - displayDim.x)*0.5f, 0);
    }

  commands.windowResized = true;
  commands.widthInPixels = (u32)displayDim.x;
  commands.heightInPixels = (u32)displayDim.y;
}

void AudioPluginAudioProcessorEditor::
mouseMove(const juce::MouseEvent &event)
{
  while(atomicCompareAndSwap(&inputLock, 0, 1)) {}
  
  juce::Point<int> mousePosition = event.getPosition();
  newInput->mouseState.position = V2(mousePosition.getX(), editorHeight - mousePosition.getY()) - displayMin;
  
  while(atomicCompareAndSwap(&inputLock, 1, 0)) {}
  
  setMouseCursor(editorMouseCursor);
}

void AudioPluginAudioProcessorEditor::
mouseDrag(const juce::MouseEvent &event)
{
  while(atomicCompareAndSwap(&inputLock, 0, 1)) {}
  
  juce::Point<int> mousePosition = event.getPosition();
  newInput->mouseState.position = V2(mousePosition.getX(), editorHeight - mousePosition.getY()) - displayMin;

  if(!(isDown(newInput->mouseState.buttons[MouseButton_left]) ||
       isDown(newInput->mouseState.buttons[MouseButton_right])))
    {
      int breakme = 5;
    }
  
  while(atomicCompareAndSwap(&inputLock, 1, 0)) {}

  setMouseCursor(editorMouseCursor);  
}

void AudioPluginAudioProcessorEditor::
mouseDown(const juce::MouseEvent &event)
{
  while(atomicCompareAndSwap(&inputLock, 0, 1)) {}
  
  if(event.mods.isLeftButtonDown())
    {
      juceProcessButtonPress(&newInput->mouseState.buttons[MouseButton_left], true);      
    }
  if(event.mods.isRightButtonDown())
    {
      juceProcessButtonPress(&newInput->mouseState.buttons[MouseButton_right], true);
    }
  
  while(atomicCompareAndSwap(&inputLock, 1, 0)) {}
}

void AudioPluginAudioProcessorEditor::
mouseUp(const juce::MouseEvent &event)
{
  // NOTE: JUCE is a very intuitive and well-designed framework that everyone should use without question
  while(atomicCompareAndSwap(&inputLock, 0, 1)) {}
  
  if(event.mods.isLeftButtonDown())
    {
      juceProcessButtonPress(&newInput->mouseState.buttons[MouseButton_left], false);
    }
  if(event.mods.isRightButtonDown())
    {
      juceProcessButtonPress(&newInput->mouseState.buttons[MouseButton_right], false);
    }

  while(atomicCompareAndSwap(&inputLock, 1, 0)) {}
}

void AudioPluginAudioProcessorEditor::
mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel)
{
  while(atomicCompareAndSwap(&inputLock, 0, 1)) {}
  
  if(wheel.deltaY != 0)
    {
      newInput->mouseState.scrollDelta += wheel.deltaY;
    }

  while(atomicCompareAndSwap(&inputLock, 1, 0)) {}
}

bool AudioPluginAudioProcessorEditor::
keyPressed(const juce::KeyPress &key)
{
  bool result = false;
  while(atomicCompareAndSwap(&inputLock, 0, 1)) {}
  
  if(key == juce::KeyPress::tabKey)
    {
      result = true;
      juceProcessButtonPress(&newInput->keyboardState.keys[KeyboardButton_tab], result);
      juce::Logger::writeToLog("tab pressed");
    }
  else if(key == juce::KeyPress::backspaceKey)
    {
      result = true;
      juceProcessButtonPress(&newInput->keyboardState.keys[KeyboardButton_backspace], result);
      juce::Logger::writeToLog("backspace pressed");
    }
  else if(key == juce::KeyPress::returnKey)
    {
      result = true;
      juceProcessButtonPress(&newInput->keyboardState.keys[KeyboardButton_enter], result);
    }
  else if(key.getKeyCode() == '-')
    {
      result = true;
      juceProcessButtonPress(&newInput->keyboardState.keys[KeyboardButton_minus], result);
    }
  else if(key.getKeyCode() == '=')
    {
      result = true;
      juceProcessButtonPress(&newInput->keyboardState.keys[KeyboardButton_equal], result);
    }
  /*
  else if(key == juce::KeyPress::upKey)
    {
      result = true;
      juceProcessButtonPress(&newInput->keyboardState.keys[KeyboardButton_up], result);
    }
  else if(key == juce::KeyPress::downKey)
    {
      result = true;
      juceProcessButtonPress(&newInput->keyboardState.keys[KeyboardButton_down], result);
    }
  */

  juce::ModifierKeys modifiers = key.getModifiers();
  if(modifiers.isShiftDown())
    {
      result = true;
      juceProcessButtonPress(&newInput->keyboardState.modifiers[KeyboardModifier_shift], result);
    }
  else if(modifiers.isCtrlDown())
    {
      result = true;
      juceProcessButtonPress(&newInput->keyboardState.modifiers[KeyboardModifier_control], result);
    }
  else if(modifiers.isAltDown())
    {
      result = true;
      juceProcessButtonPress(&newInput->keyboardState.modifiers[KeyboardModifier_alt], result);
    }

  while(atomicCompareAndSwap(&inputLock, 1, 0)) {}
  return(result);
}

void AudioPluginAudioProcessorEditor::
newOpenGLContextCreated(void)
{
  glDebugMessageControl(GL_DEBUG_SOURCE_API,
			GL_DEBUG_TYPE_OTHER,
			GL_DEBUG_SEVERITY_NOTIFICATION,
			0,
			0,
			GL_FALSE);
  
  glEnable(GL_TEXTURE_2D);  
  glEnable(GL_SCISSOR_TEST);
}

void AudioPluginAudioProcessorEditor::
renderOpenGL(void)
{
  
  glViewport(0, 0, editorWidth, editorHeight);
  glScissor(0, 0, editorWidth, editorHeight);

  glClearColor(0.2f, 0.2f, 0.2f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glViewport(displayMin.x, displayMin.y, displayDim.x, displayDim.y);
  glScissor(displayMin.x, displayMin.y, displayDim.x, displayDim.y);

  // NOTE: juce needs this blending stuff to be in the render function, aparently.
  //       no idea why, or if it's just a linux thing
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // NOTE: we have to put locks around input usage and input callback code, because juce doesn't give us a way of
  //       polling input at a particular time. We could avoid locks by having input callbacks write to a
  //       lock-free queue, which the rendering code would read from. But that's probably not necessary
  while(atomicCompareAndSwap(&inputLock, 0, 1)) {}
  if(processorRef.pluginCode.renderNewFrame)
    {      
      // juce::Logger::writeToLog("left: down=" +
      // 			       juce::String((int)isDown(newInput->mouseState.buttons[MouseButton_left])) +
      // 			       " pressed=" +
      // 			       juce::String((int)wasPressed(newInput->mouseState.buttons[MouseButton_left])));
      processorRef.pluginCode.renderNewFrame(&processorRef.pluginMemory, newInput, &commands);

      // NOTE: it's so cool that you can't set the mouse cursor here and have to defer setting it in a mouse callback
      switch(commands.cursorState)
	{
	case CursorState_default:
	  {
	    editorMouseCursor = juce::MouseCursor::NormalCursor;
	  } break;
	case CursorState_hArrow:
	  {	   
	    editorMouseCursor = juce::MouseCursor::LeftRightResizeCursor;
	  } break;
	case CursorState_vArrow:
	  {
	    editorMouseCursor = juce::MouseCursor::UpDownResizeCursor;
	  } break;
	case CursorState_hand:
	  {
	    editorMouseCursor = juce::MouseCursor::DraggingHandCursor;
	  } break;
	case CursorState_text:
	  {
	    editorMouseCursor = juce::MouseCursor::IBeamCursor;
	  } break;
	}
      
      renderCommands(&commands);
      //repaint();

      PluginLogger *pluginLogger = processorRef.pluginMemory.logger;
      for(;;)
	{
	  if(atomicCompareAndSwap(&pluginLogger->mutex, 0, 1) == 0)
	    {	    
	      for(String8Node *node = pluginLogger->log.first; node; node = node->next)
		{
		  String8 string = node->string;
		  if(string.str)
		    {  
		      juce::Logger::writeToLog(juce::String((const char *)string.str, string.size));
		    }
		}

	      //pluginLogger->log.first = 0;
	      ZERO_STRUCT(&pluginLogger->log);
	      arenaEnd(pluginLogger->logArena);
	      
	      atomicStore(&pluginLogger->mutex, 0);
	      break;
	    }
	}
      
    }
  
  juce::Logger::writeToLog(juce::String("inputs swapped"));  
  
  PluginInput *temp = newInput;
  newInput = oldInput;
  oldInput = temp;

  newInput->mouseState.position = oldInput->mouseState.position;
  for(u32 buttonIndex = 0; buttonIndex < MouseButton_COUNT; ++buttonIndex)
    {
      newInput->mouseState.buttons[buttonIndex].halfTransitionCount = 0;
      newInput->mouseState.buttons[buttonIndex].endedDown =
	oldInput->mouseState.buttons[buttonIndex].endedDown;
      oldInput->mouseState.buttons[buttonIndex].endedDown = false;
    }
  for(u32 keyIndex = 0; keyIndex < KeyboardButton_COUNT; ++keyIndex)
    {
      newInput->keyboardState.keys[keyIndex].halfTransitionCount = 0;
      newInput->keyboardState.keys[keyIndex].endedDown =
	oldInput->keyboardState.keys[keyIndex].endedDown;
      oldInput->keyboardState.keys[keyIndex].endedDown = false;
    }
  for(u32 modifierIndex = 0; modifierIndex < KeyboardModifier_COUNT; ++modifierIndex)
    {
      newInput->keyboardState.modifiers[modifierIndex].halfTransitionCount = 0;
      newInput->keyboardState.modifiers[modifierIndex].endedDown =
	oldInput->keyboardState.modifiers[modifierIndex].endedDown;
      oldInput->keyboardState.modifiers[modifierIndex].endedDown = false;
    }
  
  while(atomicCompareAndSwap(&inputLock, 1, 0)) {}
}

void AudioPluginAudioProcessorEditor::
openGLContextClosing(void)
{
}
