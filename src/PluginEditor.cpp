//#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "render.cpp"

using namespace juce::gl;

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
  newInput->mousePositionX = 2.f*((r32)mousePosition.getX()/(r32)getWidth()) - 1.f;
  newInput->mousePositionY = -2.f*((r32)mousePosition.getY()/(r32)getHeight()) + 1.f;
}

void
AudioPluginAudioProcessorEditor::mouseDrag(const juce::MouseEvent &event)
{
  juce::Point<int> mousePosition = event.getPosition();
  newInput->mousePositionX = 2.f*((r32)mousePosition.getX()/(r32)getWidth()) - 1.f;
  newInput->mousePositionY = -2.f*((r32)mousePosition.getY()/(r32)getHeight()) + 1.f;
}

void
AudioPluginAudioProcessorEditor::mouseDown(const juce::MouseEvent &event)
{
  if(event.mods.isLeftButtonDown())
    {
      newInput->mouseButtons[MouseButton_left].endedDown = true;
      ++newInput->mouseButtons[MouseButton_left].halfTransitionCount;
    }
  if(event.mods.isRightButtonDown())
    {
      newInput->mouseButtons[MouseButton_right].endedDown = true;
      ++newInput->mouseButtons[MouseButton_right].halfTransitionCount;
    }
}

void
AudioPluginAudioProcessorEditor::mouseUp(const juce::MouseEvent &event)
{
  // NOTE: JUCE is a very intuitive and well-designed framework that everyone should use without question
  if(event.mods.isLeftButtonDown())
    {      
      newInput->mouseButtons[MouseButton_left].endedDown = false;
      ++newInput->mouseButtons[MouseButton_left].halfTransitionCount;
    }
  if(event.mods.isRightButtonDown())
    {     
      newInput->mouseButtons[MouseButton_right].endedDown = false;
      ++newInput->mouseButtons[MouseButton_right].halfTransitionCount;
    }
}

void
AudioPluginAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel)
{
  if(wheel.deltaY != 0)
    {
      newInput->mouseScrollDelta += wheel.deltaY;
    }
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
      newInput->mouseButtons[buttonIndex].halfTransitionCount = 0;
      newInput->mouseButtons[buttonIndex].endedDown =
	oldInput->mouseButtons[buttonIndex].endedDown;
    }
}

void
AudioPluginAudioProcessorEditor::openGLContextClosing(void)
{
}
