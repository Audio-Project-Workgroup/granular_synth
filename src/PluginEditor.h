#pragma once

#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"

#define JUCE_GL

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor,	
					      public juce::OpenGLRenderer
{
public:
  explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor&);
  ~AudioPluginAudioProcessorEditor() override;
  
  //==============================================================================
  void paint(juce::Graphics&) override;
  void resized() override;

  void mouseMove(const juce::MouseEvent &event) override;
  void mouseDrag(const juce::MouseEvent &event) override;
  void mouseDown(const juce::MouseEvent &event) override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

  void newOpenGLContextCreated(void) override;
  void renderOpenGL(void) override;
  void openGLContextClosing(void) override;
  
private:

  AudioPluginAudioProcessor& processorRef;
  juce::OpenGLContext glContext;
  RenderCommands commands;  
    
  PluginInput pluginInput[2];
  PluginInput *newInput;
  PluginInput *oldInput;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
