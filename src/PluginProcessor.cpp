#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "platform.h"
#include "onnx.cpp"

PLATFORM_READ_ENTIRE_FILE(juceReadEntireFile)
{
  ReadFileResult result = {};

  juce::String filepath = juce::String(BUILD_DIR) + juce::String("/") + juce::String(filename);
  juce::Logger::writeToLog(filepath);
  
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
	      result.contents[result.contentsSize++] = 0; // NOTE: null termination
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

PLATFORM_WRITE_ENTIRE_FILE(juceWriteEntireFile)
{ 
  juce::String filepath = juce::String(BUILD_DIR) + juce::String("/") + juce::String(filename);
  juce::Logger::writeToLog(filepath);
  
  juce::File file(filepath);  
  juce::FileOutputStream outputStream(filepath);
  if(outputStream.openedOk())
    {
      if(outputStream.write(fileMemory, fileSize))
	{
	  // NOTE: nothing to do
	}
      else
	{
	  juce::Logger::writeToLog("ERROR: failed to write file: " +
				   filepath + ": " +
				   outputStream.getStatus().getErrorMessage());
	}
    }
  else
    {
      juce::Logger::writeToLog("ERROR: failed to open file: " +
				   filepath + ": " +
				   outputStream.getStatus().getErrorMessage());
    }
}

PLATFORM_FREE_FILE_MEMORY(juceFreeFileMemory)
{
  arenaPopSize(allocator, file.contentsSize);  
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
  pluginMemoryBlockSizeInBytes = MEGABYTES(512);
  pluginMemory = {};

  pluginCode = {};
  
  audioBuffer = {};
  
  pluginLogger = {};
  loggerMemory = nullptr;
  loggerArena = {};

  pluginParameters = nullptr;

  resourcesReleased = false;

  for(u32 parameterIndex = PluginParameter_none + 1; parameterIndex < PluginParameter_count; ++parameterIndex)
    {
      PluginParameterInitData paramInit = pluginParameterInitData[parameterIndex];
      if(paramInit.min != paramInit.max)
	{
	  juce::AudioParameterFloat *vstParameter =
	    new juce::AudioParameterFloat(juce::String("parameter") + juce::String(parameterIndex),
					  juce::String(paramInit.name),
					  paramInit.min, paramInit.max, paramInit.init);
	  vstParameters.push_back(vstParameter);
	  addParameter(vstParameter);
	}
    }
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{  
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::
getName() const
{
  return(JucePlugin_Name);
}

bool AudioPluginAudioProcessor::
acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
  return(true);
#else
  return(false);
#endif
}

bool AudioPluginAudioProcessor::
producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
  return(true);
#else
  return(false);
#endif
}

bool AudioPluginAudioProcessor::
isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
  return(true);
#else
  return(false);
#endif
}

double AudioPluginAudioProcessor::
getTailLengthSeconds() const
{
  return(0.0);
}

int AudioPluginAudioProcessor::
getNumPrograms()
{
  return(1);   // NB: some hosts don't cope very well if you tell them there are 0 programs,
               // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::
getCurrentProgram()
{
  return(0);
}

void AudioPluginAudioProcessor::
setCurrentProgram(int index)
{
  juce::ignoreUnused(index);
}

const juce::String AudioPluginAudioProcessor::
getProgramName(int index)
{
  juce::ignoreUnused(index);
  const juce::String result = {};
  return(result);
}

void AudioPluginAudioProcessor::
changeProgramName(int index, const juce::String& newName)
{
  juce::ignoreUnused(index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::
prepareToPlay(double sampleRate, int samplesPerBlock)
{
  juce::Logger::writeToLog("prepareToPlay called");
  
  const ORTCHAR_T *modelPath = ORT_TSTR_ON_MACRO(ONNX_MODEL_PATH);
  juce::Logger::writeToLog(modelPath);

  //onnxState = {};
  onnxState = onnxInitializeState(modelPath);
  
  pluginMemory.memory = calloc(pluginMemoryBlockSizeInBytes, 1);
  pluginMemory.host = PluginHost_daw;
  pluginMemory.platformAPI.readEntireFile = juceReadEntireFile;
  pluginMemory.platformAPI.writeEntireFile = juceWriteEntireFile;
  pluginMemory.platformAPI.freeFileMemory = juceFreeFileMemory;
  //pluginMemory.platformAPI.readEntireFile = platformReadEntireFile;
  //pluginMemory.platformAPI.writeEntireFile = platformWriteEntireFile;
  //pluginMemory.platformAPI.freeFileMemory = platformFreeFileMemory;
  pluginMemory.platformAPI.runModel = platformRunModel;

  pluginMemory.platformAPI.atomicLoad = atomicLoad;
  //pluginMemory.platformAPI.atomicLoadPointer = atomicLoadPointer;
  pluginMemory.platformAPI.atomicStore = atomicStore;
  pluginMemory.platformAPI.atomicAdd = atomicAdd;
  pluginMemory.platformAPI.atomicCompareAndSwap = atomicCompareAndSwap;
  pluginMemory.platformAPI.atomicCompareAndSwapPointers = atomicCompareAndSwapPointers;

  // pluginMemory.platformAPI.wideLoadFloats	 = wideLoadFloats;
  // pluginMemory.platformAPI.wideLoadInts		 = wideLoadInts;
  // pluginMemory.platformAPI.wideSetConstantFloats = wideSetConstantFloats;
  // pluginMemory.platformAPI.wideSetConstantInts	 = wideSetConstantInts;
  // pluginMemory.platformAPI.wideStoreFloats	 = wideStoreFloats;
  // pluginMemory.platformAPI.wideStoreInts	 = wideStoreInts;
  // pluginMemory.platformAPI.wideAddFloats	 = wideAddFloats;
  // pluginMemory.platformAPI.wideAddInts		 = wideAddInts;
  // pluginMemory.platformAPI.wideSubFloats	 = wideSubFloats;
  // pluginMemory.platformAPI.wideSubInts		 = wideSubInts;
  // pluginMemory.platformAPI.wideMulFloats	 = wideMulFloats;
  // pluginMemory.platformAPI.wideMulInts		 = wideMulInts;

  usz loggerMemorySize = KILOBYTES(128);
  loggerMemory = calloc(loggerMemorySize, 1);
  loggerArena = arenaBegin(loggerMemory, loggerMemorySize);

  pluginLogger.logArena = &loggerArena;
  pluginLogger.maxCapacity = loggerMemorySize/8;
  pluginMemory.logger = &pluginLogger;

  juce::String pluginPath(DYNAMIC_PLUGIN_PATH);
  juce::Logger::writeToLog(pluginPath);  

  // NOTE: JUCE's platform-independent DynamicLibrary abstraction doesn't provide an interface
  //       for getting error messages. If they haven't implemented that after 20 years, it must
  //       be really hard, right?

  if(libPlugin.open(pluginPath))
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

      pluginCode.initializePluginState = (InitializePluginState *)libPlugin.getFunction("initializePluginState");
      if(!pluginCode.audioProcess)
	{
	  juce::Logger::writeToLog("failed to load function: initializePluginState");
	}     
    }
  else
    {
      juce::Logger::writeToLog("failed to open dynamic library");
    }
  if(!pluginCode.renderNewFrame || !pluginCode.audioProcess || !pluginCode.initializePluginState)
    {      
      pluginCode.renderNewFrame = nullptr;
      pluginCode.audioProcess = nullptr;
      pluginCode.initializePluginState = nullptr;
    }

  pluginCode.initializePluginState(&pluginMemory);
  pluginParameters = (PluginFloatParameter *)pluginMemory.memory;    
  for(u32 parameterIndex = PluginParameter_none + 1; parameterIndex < PluginParameter_count; ++parameterIndex)
    {
      PluginFloatParameter *parameter = pluginParameters + parameterIndex;      
      if(parameter->range.min != parameter->range.max)
	{	  
	}
    }
  
  audioBuffer.inputFormat = audioBuffer.outputFormat = AudioFormat_r32;
  audioBuffer.inputSampleRate = audioBuffer.outputSampleRate = sampleRate;
  //audioBuffer.channels = 1;
  //audioBuffer.buffer = calloc(samplesPerBlock, 2*sizeof(float));
  audioBuffer.midiBuffer = (u8 *)calloc(KILOBYTES(1), 1); 
}

void AudioPluginAudioProcessor::
releaseResources()
{
  // NOTE: when an instance of the plugin is deleted, this function gets called multiple times, because the DAW
  //       wants to be *super careful* and make sure all the resources were freed for real, so we have to keep 
  //       track of whether or not we *actually* have to free our resources, otherwise the DAW will crash itself
  //       by double-freeing resources.
  //       there is, of course, no need to reinvent the wheel that is the audio software toolchain, since
  //       everyone involved in its creation was very clever and thought through everything they did perfectly
  if(!resourcesReleased)
    {
      //free(audioBuffer.buffer);
      free(audioBuffer.midiBuffer);
      free(pluginMemory.memory);
      free(loggerMemory);
      libPlugin.close();
      resourcesReleased = true;
    }
}

bool AudioPluginAudioProcessor::
isBusesLayoutSupported(const BusesLayout& layouts) const
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

void AudioPluginAudioProcessor::
processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
  audioBuffer.midiMessageCount = 0;
  u8 *atMidiBuffer = audioBuffer.midiBuffer;
  for(const auto metadata : midiMessages)
    {
      auto message = metadata.getMessage();
      int messageLength = message.getRawDataSize();
      r64 messageTimestamp = message.getTimeStamp();

      MidiHeader *messageHeader = (MidiHeader *)atMidiBuffer;
      // TODO: we probably don't need to have the message length in the header anymore
      messageHeader->messageLength = messageLength;
      messageHeader->timestamp = (u64)messageTimestamp;
      atMidiBuffer += sizeof(MidiHeader);
            
      memcpy(atMidiBuffer, message.getRawData(), messageLength);
      atMidiBuffer += messageLength;

      ++audioBuffer.midiMessageCount;     
    }

  juce::ScopedNoDenormals noDenormals;
  
  audioBuffer.inputChannels  = MIN(getTotalNumInputChannels(),  ARRAY_COUNT(audioBuffer.inputBuffer));  
  audioBuffer.outputChannels = MIN(getTotalNumOutputChannels(), ARRAY_COUNT(audioBuffer.outputBuffer));
  audioBuffer.inputStride = audioBuffer.outputStride = sizeof(r32);
  
  audioBuffer.inputBuffer[0] = buffer.getReadPointer(0);
  audioBuffer.inputBuffer[1] = buffer.getReadPointer(1);

  audioBuffer.outputBuffer[0] = buffer.getWritePointer(0);
  audioBuffer.outputBuffer[1] = buffer.getWritePointer(1);
  
  if(pluginCode.audioProcess)
    {
      audioBuffer.framesToWrite = buffer.getNumSamples();      
      pluginCode.audioProcess(&pluginMemory, &audioBuffer);

      // NOTE: debug
      juce::Logger::writeToLog(juce::String("logger used: ") +
			       juce::String(pluginLogger.log.totalSize));
      juce::Logger::writeToLog(juce::String("logger capacity: ") +
			       juce::String(pluginLogger.maxCapacity));
    }  
}

//==============================================================================
bool AudioPluginAudioProcessor::
hasEditor() const
{
  return(true); // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::
createEditor()
{
  return(new AudioPluginAudioProcessorEditor(*this));
}

//==============================================================================
void AudioPluginAudioProcessor::
getStateInformation(juce::MemoryBlock& destData)
{
  // You should use this method to store your parameters in the memory block.
  // You could do that either as raw data, or use the XML or ValueTree classes
  // as intermediaries to make it easy to save and load complex data.
  //juce::ignoreUnused(destData);

  //destData.copyFrom(pluginMemory.memory, 0, pluginMemoryBlockSizeInBytes);
  
  // TODO: this is a shit ton of data, and it's really slow to write this out
  //       (slower than it needs to be. idk wtf juce is doing). So we should be more clever about what
  //       parts of the state we choose to save and restore
  juce::String stateStorageFilename(BUILD_DIR"/granular_synth_state.test");
  juce::MemoryOutputStream os(destData, true);  
  if(pluginMemory.memory)
    {
      os.writeInt(pluginMemoryBlockSizeInBytes);
      //os.write(pluginMemory.memory, pluginMemoryBlockSizeInBytes);
      os.writeString(stateStorageFilename);
      
      juce::File stateStorageFile(stateStorageFilename);
      if(!stateStorageFile.existsAsFile())
	{
	  stateStorageFile.create();
	}
      if(stateStorageFile.existsAsFile())
	{
	  juce::FileOutputStream fos(stateStorageFile);
	  if(fos.openedOk())
	    {
	      if(!fos.write(pluginMemory.memory, pluginMemoryBlockSizeInBytes))
		{
		  juce::Logger::writeToLog("ERROR: failed to write to file: " +
					   stateStorageFilename + ": " + 
					   fos.getStatus().getErrorMessage());
		}
	    }
	}
    }
}

void AudioPluginAudioProcessor::
setStateInformation(const void* data, int sizeInBytes)
{
  // You should use this method to restore your parameters from this memory block,
  // whose contents will have been created by the getStateInformation() call.
  //juce::ignoreUnused(data, sizeInBytes);

  //juce::MemoryBlock srcData = juce::MemoryBlock(data, sizeInBytes);
  //srcData.copyTo(pluginMemory.memory, pluginMemoryBlockSIzeInBytes);
  
  // ASSERT(sizeInBytes <= pluginMemoryBlockSizeInBytes);
  // memcpy(pluginMemory.memory, data, sizeInBytes);

  // TODO: idk if this function ever gets called
  juce::MemoryInputStream is(data, (usz)sizeInBytes, false);
  int blockSize = is.readInt();
  juce::String stateStorageFilename = is.readString();
  if(blockSize == pluginMemoryBlockSizeInBytes)
    {
      if(pluginMemory.memory)
	{
	  // if(is.getNumBytesRemaining() == pluginMemoryBlockSizeInBytes)
	  //   {
	  //     is.read(pluginMemory.memory, pluginMemoryBlockSizeInBytes);
	  //   }
	  // else
	  //   {
	  //     juce::Logger::writeToLog("setStateInformation() received inconsistent block size");
	  //   }
	  juce::File stateStorageFile(stateStorageFilename);
	  if(stateStorageFile.existsAsFile())
	    {
	      juce::FileInputStream fis(stateStorageFile);
	      if(fis.openedOk())
		{
		  if(!fis.read(pluginMemory.memory, pluginMemoryBlockSizeInBytes))
		    {
		      juce::Logger::writeToLog("ERROR: failed to read from file: " +
					       stateStorageFilename + ": " +
					       fis.getStatus().getErrorMessage());
		    }
		}
	    }
	}
      else if(blockSize == 0)
	{
	  juce::Logger::writeToLog("setStateInformation() received empty input block");
	}
      else
	{
	  juce::Logger::writeToLog("setStateInformation() received invalid block size");
	}
    }
}
//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor*
JUCE_CALLTYPE createPluginFilter()
{
  return(new AudioPluginAudioProcessor());
}
