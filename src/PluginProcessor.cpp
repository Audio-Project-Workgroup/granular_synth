#include "PluginProcessor.h"
#include "PluginEditor.h"

PLATFORM_READ_ENTIRE_FILE(juceReadEntireFile)
{
  ReadFileResult result = {};

  // TODO: this is my(ry's) local working directory. We need to give JUCE a uniform location to look for files.
  juce::String filepath = "~/Documents/C/GLFW_miniaudio_JUCE_test/data/" + juce::String(filename);
  juce::File file(filepath);  
  if(file.existsAsFile())
    {
      juce::FileInputStream fileStream(file);
      if(fileStream.openedOk())
	{
	  auto fileLength = fileStream.getTotalLength();
      
	  u8 *dest = arenaPushSize(allocator, fileLength + 1);
	  result.contents = dest;
	  result.contentsSize = fileLength;

	  auto bytesToRead = fileLength;
	  while(bytesToRead)
	    {
	      auto bytesRead = fileStream.read(dest, bytesToRead);
	      if(fileStream.getStatus().failed())
		{
		  juce::Logger::writeToLog("ERROR: filestream read failed");
		  result.contents = nullptr;
		  result.contentsSize = 0;
		  break;
		}
	      
	      dest += bytesRead;
	      bytesToRead -= bytesRead;
	    }      

	  if(result.contents)
	    {
	      result.contents[result.contentsSize++] = ' '; // NOTE: null termination
	    }
	}
      else
	{
	  juce::Logger::writeToLog("ERROR: filestream failed to open");
	}
    }
  else
    {
      juce::Logger::writeToLog("ERROR: " + filepath + " does not exist");
    }

  return(result);
}

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
  : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
		   .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
#endif
		   .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
		   )
{  
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{  
}

//==============================================================================
const juce::String
AudioPluginAudioProcessor::getName() const
{
  return(JucePlugin_Name);
}

bool
AudioPluginAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
  return(true);
#else
  return(false);
#endif
}

bool
AudioPluginAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
  return(true);
#else
  return(false);
#endif
}

bool
AudioPluginAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
  return(true);
#else
  return(false);
#endif
}

double
AudioPluginAudioProcessor::getTailLengthSeconds() const
{
  return(0.0);
}

int
AudioPluginAudioProcessor::getNumPrograms()
{
  return(1);   // NB: some hosts don't cope very well if you tell them there are 0 programs,
               // so this should be at least 1, even if you're not really implementing programs.
}

int
AudioPluginAudioProcessor::getCurrentProgram()
{
  return(0);
}

void
AudioPluginAudioProcessor::setCurrentProgram(int index)
{
  juce::ignoreUnused(index);
}

const juce::String
AudioPluginAudioProcessor::getProgramName(int index)
{
  juce::ignoreUnused(index);
  const juce::String result = {};
  return(result);
}

void
AudioPluginAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
  juce::ignoreUnused(index, newName);
}

//==============================================================================
void
AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
  juce::Logger::writeToLog("prepareToPlay called");

  pluginMemory = {};
  pluginMemory.memory = calloc(MEGABYTES(512), 1);
  pluginMemory.platformAPI.readEntireFile = juceReadEntireFile;  

  // TODO: you probably have to tell your shell to search for dynamic libaries in
  //       [your_local_project_directory]/build. We shouldn't require end-users to do that.
#ifdef __WIN32__
  juce::String pluginFilename = "plugin.dll";
#else
  juce::String pluginFilename = "plugin.so";
#endif

  // NOTE: JUCE's platform-independent DynamicLibrary abstraction doesn't provide an interface
  //       for getting error messages. If they haven't implemented that after 20 years, it must
  //       be really hard, right?
  if(libPlugin.open(pluginFilename))
    {
      pluginCode.renderNewFrame = (RenderNewFrame *)libPlugin.getFunction("renderNewFrame");
      if(!pluginCode.renderNewFrame)
	{
	  juce::Logger::writeToLog("failed to load function: renderNewFrame");
	}
      pluginCode.audioProcess = (AudioProcess *)libPlugin.getFunction("audioProcess");
      if(!pluginCode.audioProcess)
	{
	  juce::Logger::writeToLog("failed to load function: audioProcess");
	}     
    }
  else
    {
      juce::Logger::writeToLog("failed to open dynamic library");
    }

  if(!pluginCode.renderNewFrame || !pluginCode.audioProcess)
    {      
      pluginCode.renderNewFrame = nullptr;
      pluginCode.audioProcess = nullptr;
    }

  audioBuffer = {};
  audioBuffer.format = AudioFormat_r32;
  audioBuffer.sampleRate = sampleRate;
  audioBuffer.channels = 1;
  audioBuffer.buffer = calloc(samplesPerBlock, 2*sizeof(float));
  audioBuffer.midiBuffer = (u8 *)calloc(KILOBYTES(1), 1);
}

void
AudioPluginAudioProcessor::releaseResources()
{  
  free(audioBuffer.buffer);
  free(audioBuffer.midiBuffer);
  free(pluginMemory.memory);  
  libPlugin.close(); 
}

bool
AudioPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return(true);
#else
  // This is the place where you check if the layout is supported.
  // In this template code we only support mono or stereo.
  // Some plugin hosts, such as certain GarageBand versions, will only
  // load plugins that support stereo bus layouts.
  if(layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return(false);

  // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
  if(layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return(false);
#endif
  
  return(true);
#endif
}

void
AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
  audioBuffer.midiMessageCount = 0;
  u8 *atMidiBuffer = audioBuffer.midiBuffer;
  for(const auto metadata : midiMessages)
    {
      auto message = metadata.getMessage();
      int messageLength = message.getRawDataSize();
      
      MidiHeader *messageHeader = (MidiHeader *)atMidiBuffer;
      messageHeader->messageLength = messageLength;      
      atMidiBuffer += sizeof(MidiHeader);
      
      memcpy(atMidiBuffer, message.getRawData(), messageLength);
      atMidiBuffer += messageLength;
      ++audioBuffer.midiMessageCount;      
    }

  juce::ScopedNoDenormals noDenormals;
  auto totalNumInputChannels  = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();
  
  for(auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());  

  auto *lChannel = buffer.getWritePointer(0);
  auto *rChannel = buffer.getWritePointer(1);

  if(pluginCode.audioProcess)
    {
      audioBuffer.framesToWrite = buffer.getNumSamples();      
      pluginCode.audioProcess(&pluginMemory, &audioBuffer);
      
      memcpy(lChannel, audioBuffer.buffer, sizeof(*lChannel)*buffer.getNumSamples());
      memcpy(rChannel, audioBuffer.buffer, sizeof(*rChannel)*buffer.getNumSamples());
    }  
}

//==============================================================================
bool
AudioPluginAudioProcessor::hasEditor() const
{
  return(true); // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor*
AudioPluginAudioProcessor::createEditor()
{
  return(new AudioPluginAudioProcessorEditor(*this));
}

//==============================================================================
void
AudioPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
  // You should use this method to store your parameters in the memory block.
  // You could do that either as raw data, or use the XML or ValueTree classes
  // as intermediaries to make it easy to save and load complex data.
  juce::ignoreUnused(destData);
}

void
AudioPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
  // You should use this method to restore your parameters from this memory block,
  // whose contents will have been created by the getStateInformation() call.
  juce::ignoreUnused(data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor*
JUCE_CALLTYPE createPluginFilter()
{
  return(new AudioPluginAudioProcessor());
}