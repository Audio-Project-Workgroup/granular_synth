#include "PluginProcessor.h"
#include "PluginEditor.h"

//#include "platform.h"
//#include "onnx.cpp"

static juce::String globalVstBaseDirectory;

//PLATFORM_READ_ENTIRE_FILE(juceReadEntireFile)
static Buffer
juceReadEntireFile(char *filename, Arena *allocator)
{
  Buffer result = {};

  juce::String filepath = globalVstBaseDirectory + "/" + juce::String(filename);
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
	  result.size = fileLength;

	  auto bytesToRead = fileLength;
	  while(bytesToRead)
	    {
	      auto bytesRead = fileStream.read(dest, bytesToRead);
	      if(fileStream.getStatus().failed())
		{
		  juce::Logger::writeToLog("ERROR: filestream read failed");
		  result.contents = nullptr;
		  result.size = 0;
		  break;
		}
	      
	      dest += bytesRead;
	      bytesToRead -= bytesRead;
	    }      

	  if(result.contents)
	    {
	      result.contents[result.size] = 0; // NOTE: null termination
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

//PLATFORM_WRITE_ENTIRE_FILE(juceWriteEntireFile)
static void
juceWriteEntireFile(char *filename, Buffer file)
{ 
  juce::String filepath = globalVstBaseDirectory + "/" + juce::String(filename);
  juce::Logger::writeToLog(filepath);
  
  //juce::File file(filepath);  
  juce::FileOutputStream outputStream(filepath);
  if(outputStream.openedOk())
    {
      if(outputStream.write(file.contents, file.size))
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

//PLATFORM_FREE_FILE_MEMORY(juceFreeFileMemory)
static void
juceFreeFileMemory(Buffer file, Arena *allocator)
{
  arenaPopSize(allocator, file.size);  
}

// TODO: pull this code that's copied from main.cpp out into a shared file
#define ARENA_MIN_ALLOCATION_SIZE KILOBYTES(64)

static Arena*
gsArenaAcquire(usz size)
{
  usz allocSize = MAX(size, ARENA_MIN_ALLOCATION_SIZE);

  void *base = platformAllocateMemory(allocSize);
  Arena *result = (Arena*)base;
  result->current = result;
  result->prev = 0;
  result->base = 0;
  result->capacity = allocSize;
  result->pos = ARENA_HEADER_SIZE;

  return(result);
}

static void
gsArenaDiscard(Arena *arena)
{
  platformFreeMemory(arena, arena->capacity);
}

static r32
platformRand(RangeR32 range)
{
  int randVal = rand();
  r32 rand01 = (r32)randVal / (r32)RAND_MAX;
  r32 result = mapToRange(rand01, range);
  return(result);
}

static void
gsCopyMemory(void *dest, void *src, usz size)
{
  memcpy(dest, src, size);
}

static void
gsSetMemory(void *dest, int value, usz size)
{
  memset(dest, value, size);
}

//==============================================================================
DetectivePervert::DetectivePervert(AudioPluginAudioProcessor &p)
  : processorRef(p)
{
  juce::Logger::writeToLog("detective pervert reporting for duty");
}

void DetectivePervert::
parameterValueChanged(int parameterIndex, float newValue)
{
  if(!processorRef.ignoreParameterChange)
    {
      juce::Logger::writeToLog("detective pervert heard a parameter value change: " +
			       juce::String(parameterIndex) + " " +
			       juce::String(newValue));

      PluginAudioBuffer *audioBuffer = &processorRef.audioBuffer;
      u32 writeIndex = audioBuffer->parameterValueQueueWriteIndex;

      ParameterValueQueueEntry *entry = audioBuffer->parameterValueQueueEntries + writeIndex;
      entry->index = processorRef.vstParameterIndexTo_pluginParameterIndex[parameterIndex];
      entry->value.asFloat = newValue;

      u32 queuedCount = atomicLoad(&audioBuffer->queuedCount);
      while(atomicCompareAndSwap(&audioBuffer->queuedCount, queuedCount, queuedCount + 1) != queuedCount)
	{
	  queuedCount = atomicLoad(&audioBuffer->queuedCount);
	}

      audioBuffer->parameterValueQueueWriteIndex =
	(writeIndex + 1) % ARRAY_COUNT(audioBuffer->parameterValueQueueEntries);
    }
}

void DetectivePervert::
parameterGestureChanged(int parameterIndex, bool gestureIsStarting)
{
  juce::Logger::writeToLog("detective pervert heard a parameter gesture change");
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
		   ),
    atlasImage(juce::ImageFileFormat::loadFrom(BinaryData::test_atlas_png,
					       BinaryData::test_atlas_pngSize)),
    atlasImageBitmapData(atlasImage, juce::Image::BitmapData::readOnly)
{
  pluginMemoryBlockSizeInBytes = MEGABYTES(512);
  pluginMemory = {};

  pluginCode = {};
  
  audioBuffer = {};

#if BUILD_LOGGING
  pluginLogger = {};
  loggerMemory = nullptr;
  loggerArena = {};
#endif

  pluginParameters = nullptr;
  parameterListener = new DetectivePervert(*this);

  resourcesReleased = false;

  ignoreParameterChange = false;

  juce::File vstFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
  juce::File platformContentsDirectory = vstFile.getParentDirectory();
  juce::File contentsDirectory = platformContentsDirectory.getParentDirectory();
  juce::File resourcesDirectory = contentsDirectory.getChildFile("Resources");
  juce::Logger::writeToLog("platform contents directory: " + platformContentsDirectory.getFullPathName());
  juce::Logger::writeToLog("contents directory: " + contentsDirectory.getFullPathName());
  juce::Logger::writeToLog("resources directory: " + resourcesDirectory.getFullPathName());

  globalVstBaseDirectory = platformContentsDirectory.getFullPathName();  

  pathToVST = vstFile.getFullPathName();  
  pathToPlugin = platformContentsDirectory.getFullPathName() + "/" + juce::String(DYNAMIC_PLUGIN_FILE_NAME);
  pathToData = resourcesDirectory.getFullPathName() + "/data";
  
  juce::Logger::writeToLog("path to vst: " + pathToVST);
  juce::Logger::writeToLog("if the above path is the path to your daw, then there's a problem!");
  
  juce::Logger::writeToLog("path to plugin: " + pathToPlugin);
  juce::Logger::writeToLog("path to data: " + pathToData);

  for(u32 parameterIndex = PluginParameter_none + 1; parameterIndex < PluginParameter_count; ++parameterIndex)
    {
      PluginParameterInitData paramInit = pluginParameterInitData[parameterIndex];
      if(paramInit.min != paramInit.max)
	{
	  VstParameter *vstParameter = new VstParameter();
	  vstParameter->id = juce::String("parameter") + juce::String(parameterIndex);
	  vstParameter->name = juce::String(paramInit.name);
	  vstParameter->parameter = 
	    new juce::AudioParameterFloat(vstParameter->id, vstParameter->name,		  
					  paramInit.min, paramInit.max, paramInit.init);
	  
	  vstParameters.push_back(vstParameter);
	  addParameter(vstParameter->parameter);
	  vstParameter->parameter->addListener(parameterListener);

	  vstParameterIndexTo_pluginParameterIndex.insert(
							  {vstParameter->parameter->getParameterIndex(),
							   parameterIndex});
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
  // const ORTCHAR_T *modelPath = ORT_TSTR_ON_MACRO(ONNX_MODEL_PATH);
  // juce::Logger::writeToLog(modelPath);

  //onnxState = {};
  //onnxState = onnxInitializeState(modelPath);

  //pluginMemory.memory			     = calloc(pluginMemoryBlockSizeInBytes, 1);
  pluginMemory.host			     = PluginHost_daw;
  // pluginMemory.platformAPI.gsReadEntireFile  = juceReadEntireFile;
  // pluginMemory.platformAPI.gsWriteEntireFile = juceWriteEntireFile;
  // pluginMemory.platformAPI.gsFreeFileMemory  = juceFreeFileMemory;
  pluginMemory.platformAPI.gsReadEntireFile  = platformReadEntireFile;
  pluginMemory.platformAPI.gsWriteEntireFile = platformWriteEntireFile;
  pluginMemory.platformAPI.gsFreeFileMemory  = platformFreeFileMemory;
  pluginMemory.platformAPI.gsGetPathToModule = platformGetPathToModule;
  
  //pluginMemory.platformAPI.runModel = platformRunModel;

  pluginMemory.platformAPI.gsRand = platformRand;
  pluginMemory.platformAPI.gsAbs  = fabsf;
  pluginMemory.platformAPI.gsSqrt = sqrtf;
  pluginMemory.platformAPI.gsSin  = sinf;
  pluginMemory.platformAPI.gsCos  = cosf;
  pluginMemory.platformAPI.gsPow  = powf;

  pluginMemory.platformAPI.gsAllocateMemory = platformAllocateMemory;
  pluginMemory.platformAPI.gsFreeMemory	    = platformFreeMemory;
  pluginMemory.platformAPI.gsCopyMemory	    = gsCopyMemory;
  pluginMemory.platformAPI.gsSetMemory	    = gsSetMemory;
  pluginMemory.platformAPI.gsArenaAcquire   = gsArenaAcquire;
  pluginMemory.platformAPI.gsArenaDiscard   = gsArenaDiscard;

  pluginMemory.platformAPI.gsAtomicLoad			  = atomicLoad;
  pluginMemory.platformAPI.gsAtomicStore		  = atomicStore;
  pluginMemory.platformAPI.gsAtomicAdd			  = atomicAdd;
  pluginMemory.platformAPI.gsAtomicCompareAndSwap	  = atomicCompareAndSwap;
  pluginMemory.platformAPI.gsAtomicCompareAndSwapPointers = atomicCompareAndSwapPointers;  

#if BUILD_LOGGING
  usz loggerMemorySize = KILOBYTES(128);
  loggerMemory = calloc(loggerMemorySize, 1);
  loggerArena = arenaBegin(loggerMemory, loggerMemorySize);

  pluginLogger.logArena = &loggerArena;
  pluginLogger.maxCapacity = loggerMemorySize/8;
  pluginMemory.logger = &pluginLogger;
#endif

  //juce::String pluginPath(DYNAMIC_PLUGIN_PATH);

  // NOTE: JUCE's platform-independent DynamicLibrary abstraction doesn't provide an interface
  //       for getting error messages.
  if(libPlugin.open(pathToPlugin))
    {
      pluginCode.pluginAPI.gsRenderNewFrame = (GS_RenderNewFrame*)libPlugin.getFunction("gsRenderNewFrame");
      if(!pluginCode.pluginAPI.gsRenderNewFrame)
	{
	  juce::Logger::writeToLog("failed to load function: gsRenderNewFrame");
	}
      
      pluginCode.pluginAPI.gsAudioProcess = (GS_AudioProcess*)libPlugin.getFunction("gsAudioProcess");
      if(!pluginCode.pluginAPI.gsAudioProcess)
	{
	  juce::Logger::writeToLog("failed to load function: gsAudioProcess");
	}

      pluginCode.pluginAPI.gsInitializePluginState = (GS_InitializePluginState*)libPlugin.getFunction("gsInitializePluginState");
      if(!pluginCode.pluginAPI.gsInitializePluginState)
	{
	  juce::Logger::writeToLog("failed to load function: gsInitializePluginState");
	}     
    }
  else
    {
      juce::Logger::writeToLog("failed to open dynamic library");
    }
  if(!pluginCode.pluginAPI.gsRenderNewFrame || !pluginCode.pluginAPI.gsAudioProcess || !pluginCode.pluginAPI.gsInitializePluginState)
    {      
      pluginCode.pluginAPI.gsRenderNewFrame = nullptr;
      pluginCode.pluginAPI.gsAudioProcess = nullptr;
      pluginCode.pluginAPI.gsInitializePluginState = nullptr;
    }  

  pluginMemory.pluginHandle = libPlugin.getNativeHandle();
  void *pluginState = (void*)pluginCode.pluginAPI.gsInitializePluginState(&pluginMemory);
  pluginParameters = (PluginFloatParameter*)pluginState;    
  
  audioBuffer.inputFormat = audioBuffer.outputFormat = AudioFormat_r32;
  audioBuffer.inputSampleRate = audioBuffer.outputSampleRate = sampleRate;
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
      //free(pluginMemory.memory);
#if BUILD_LOGGING
      free(loggerMemory);
#endif
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
  
  if(pluginCode.pluginAPI.gsAudioProcess)
    {
      audioBuffer.framesToWrite = buffer.getNumSamples();      
      pluginCode.pluginAPI.gsAudioProcess(&pluginMemory, &audioBuffer);

      // NOTE: debug
      // juce::Logger::writeToLog(juce::String("logger used: ") +
      // 			       juce::String(pluginLogger.log.totalSize));
      // juce::Logger::writeToLog(juce::String("logger capacity: ") +
      // 			       juce::String(pluginLogger.maxCapacity));
    }

  ignoreParameterChange = true;
  for(u32 parameterIndex = 0; parameterIndex < vstParameters.size(); ++parameterIndex)
    {      
      u32 pluginParameterIndex = vstParameterIndexTo_pluginParameterIndex[parameterIndex];
      PluginFloatParameter *pluginParameter = pluginParameters + pluginParameterIndex;
      VstParameter *vstParameter = vstParameters[parameterIndex];

      ParameterValue pluginParameterValue = {};
      pluginParameterValue.asInt = atomicLoad(&pluginParameter->currentValue.asInt);
      *vstParameter->parameter = pluginParameterValue.asFloat;

      int interacting = atomicLoad(&pluginParameter->interacting);
      if(interacting)
	{
	  if(!vstParameter->openChangeGesture)
	    {
	      vstParameter->parameter->beginChangeGesture();
	      vstParameter->openChangeGesture = true;
	    }	  
	}
      else if(vstParameter->openChangeGesture)
	{
	  vstParameter->openChangeGesture = false;
	  vstParameter->parameter->endChangeGesture();
	}
      
      juce::Logger::writeToLog("vstParameter " + juce::String(parameterIndex) + ": " +
			       juce::String(vstParameter->parameter->get()));
      juce::Logger::writeToLog("pluginParameter " + juce::String(pluginParameterIndex) + ": " +
			       juce::String(pluginParameterValue.asFloat));
    }
  ignoreParameterChange = false;  
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

  
#if 0  
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
#else
  juce::MemoryOutputStream os(destData, true);
  if(pluginParameters)
    {  
      for(auto parameter : vstParameters)
	{
	  float parameterVal = parameter->parameter->get();
	  os.writeFloat(parameterVal);
	}
    }
#endif
}

void AudioPluginAudioProcessor::
setStateInformation(const void* data, int sizeInBytes)
{
  // You should use this method to restore your parameters from this memory block,
  // whose contents will have been created by the getStateInformation() call.
  //juce::ignoreUnused(data, sizeInBytes);

#if 0
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
#else
  int index = 0;
  juce::MemoryInputStream is(data, (usz)sizeInBytes, false);
  for(auto parameter : vstParameters)
    {
      float newValue = is.readFloat();
      ignoreParameterChange = true;
      *parameter->parameter = newValue;
      ignoreParameterChange = false;

      PluginFloatParameter *pluginParameter = pluginParameters + vstParameterIndexTo_pluginParameterIndex[index];
      float newValuePercentage = percentageFromRange(newValue, pluginParameter->range);
      parameterListener->parameterValueChanged(index++, newValuePercentage);
    }
#endif
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor*
JUCE_CALLTYPE createPluginFilter()
{
  return(new AudioPluginAudioProcessor());
}
