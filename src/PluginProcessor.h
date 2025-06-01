#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "context.h"
#include "common.h"
static String8 basePath;
#include "platform.h"

struct VstParameter
{
  juce::AudioParameterFloat *parameter;
  juce::String name;
  juce::String id;
  bool openChangeGesture;
};

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
  //==============================================================================
  AudioPluginAudioProcessor();
  ~AudioPluginAudioProcessor() override;
  
  //==============================================================================
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using AudioProcessor::processBlock;
  
  //==============================================================================
  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;
  
  //==============================================================================
  const juce::String getName() const override;
  
  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;
  
  //==============================================================================
  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;
  
  //==============================================================================
  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;
  
  //==============================================================================
  PluginCode pluginCode;
  PluginMemory pluginMemory;
  PluginAudioBuffer audioBuffer;
  
  class DetectivePervert *parameterListener;
  std::map<int, int> vstParameterIndexTo_pluginParameterIndex;
  std::vector<VstParameter*> vstParameters;
  PluginFloatParameter *pluginParameters;
  bool ignoreParameterChange;

private:
  juce::String pathToVST;
  juce::String pathToPlugin;
  juce::String pathToData;
  juce::DynamicLibrary libPlugin;    
  
  PluginLogger pluginLogger;
  void *loggerMemory;
  Arena loggerArena;
  
  void *audioBufferMemory;
  bool resourcesReleased;

  int pluginMemoryBlockSizeInBytes;

  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};

//================================================================================
class DetectivePervert final : public juce::AudioProcessorParameter::Listener
{
public:
  DetectivePervert(AudioPluginAudioProcessor&);
  void parameterValueChanged(int parameterIndex, float newValue) override;
  void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;
  
private:
  AudioPluginAudioProcessor &processorRef;

};
