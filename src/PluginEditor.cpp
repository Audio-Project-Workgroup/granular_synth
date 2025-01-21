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
  
  glContext.setRenderer(this);
  glContext.setContinuousRepainting(true);
  glContext.attachTo(*this);

  commands = {};
  commands.generateNewTextures = true;
  commands.widthInPixels = getWidth();
  commands.heightInPixels = getHeight();

  //commands = {};
  //commands.vertexCapacity = 512;
  //commands.indexCapacity = 1024;
  //commands.vertices = (Vertex *)calloc(commands.vertexCapacity, sizeof(Vertex));
  //commands.indices = (u32 *)calloc(commands.indexCapacity, sizeof(u32));

  setWantsKeyboardFocus(true);
  setMouseClickGrabsKeyboardFocus(true);
  //grabKeyboardFocus();
  
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
void
AudioPluginAudioProcessorEditor::paint(juce::Graphics& g)
{
  // (Our component is opaque, so we must completely fill the background with a solid colour)
  //g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

  //g.fillAll(juce::Colours::white);

  //g.setColour(juce::Colours::black);
  //g.setFont(15.0f);
  //g.drawFittedText("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void
AudioPluginAudioProcessorEditor::resized()
{
  commands.widthInPixels = getWidth();
  commands.heightInPixels = getHeight();
}

void
AudioPluginAudioProcessorEditor::mouseMove(const juce::MouseEvent &event)
{
  juce::Point<int> mousePosition = event.getPosition();
  newInput->mouseState.position.x = mousePosition.getX();//2.f*((r32)mousePosition.getX()/(r32)getWidth()) - 1.f;
  newInput->mouseState.position.y = getHeight() - mousePosition.getY();//-2.f*((r32)mousePosition.getY()/(r32)getHeight()) + 1.f;
}

void
AudioPluginAudioProcessorEditor::mouseDrag(const juce::MouseEvent &event)
{
  juce::Point<int> mousePosition = event.getPosition();
  newInput->mouseState.position.x = mousePosition.getX();//2.f*((r32)mousePosition.getX()/(r32)getWidth()) - 1.f;
  newInput->mouseState.position.y = getHeight() - mousePosition.getY();//-2.f*((r32)mousePosition.getY()/(r32)getHeight()) + 1.f;
}

void
AudioPluginAudioProcessorEditor::mouseDown(const juce::MouseEvent &event)
{
  if(event.mods.isLeftButtonDown())
    {
      juceProcessButtonPress(&newInput->mouseState.buttons[MouseButton_left], true);
      //newInput->mouseButtons[MouseButton_left].endedDown = true;
      //++newInput->mouseButtons[MouseButton_left].halfTransitionCount;
    }
  if(event.mods.isRightButtonDown())
    {
      juceProcessButtonPress(&newInput->mouseState.buttons[MouseButton_right], true);
      //newInput->mouseButtons[MouseButton_right].endedDown = true;
      //++newInput->mouseButtons[MouseButton_right].halfTransitionCount;
    }
}

void
AudioPluginAudioProcessorEditor::mouseUp(const juce::MouseEvent &event)
{
  // NOTE: JUCE is a very intuitive and well-designed framework that everyone should use without question
  if(event.mods.isLeftButtonDown())
    {
      juceProcessButtonPress(&newInput->mouseState.buttons[MouseButton_left], false);
      //newInput->mouseButtons[MouseButton_left].endedDown = false;
      //++newInput->mouseButtons[MouseButton_left].halfTransitionCount;
    }
  if(event.mods.isRightButtonDown())
    {
      juceProcessButtonPress(&newInput->mouseState.buttons[MouseButton_right], false);
      //newInput->mouseButtons[MouseButton_right].endedDown = false;
      //++newInput->mouseButtons[MouseButton_right].halfTransitionCount;
    }
}

void
AudioPluginAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel)
{
  if(wheel.deltaY != 0)
    {
      newInput->mouseState.scrollDelta += wheel.deltaY;
    }
}

bool AudioPluginAudioProcessorEditor::
keyPressed(const juce::KeyPress &key)
{
  bool result = false;
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

  return(result);
}

void
AudioPluginAudioProcessorEditor::newOpenGLContextCreated(void)
{
}

void
AudioPluginAudioProcessorEditor::renderOpenGL(void)
{    
  glClearColor(0.2f, 0.2f, 0.2f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT);

  if(processorRef.pluginCode.renderNewFrame)
    {
      processorRef.pluginCode.renderNewFrame(&processorRef.pluginMemory, newInput, &commands);
      renderCommands(&commands);
    }

  PluginInput *temp = newInput;
  newInput = oldInput;
  oldInput = temp;

  for(u32 buttonIndex = 0; buttonIndex < MouseButton_count; ++buttonIndex)
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
}

void
AudioPluginAudioProcessorEditor::openGLContextClosing(void)
{
}
