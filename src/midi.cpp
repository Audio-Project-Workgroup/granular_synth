#define MIDI_VERBOSE // ERASE WHEN DONE

// TODO: for whatever reason, my debugger doesn't show the printf logs when loading with the vst :(
//       maybe i'll add a system for displaying debug logs (and some actual string processing too) -Ry

static inline r32
hertzFromMidiNoteNumber(u8 noteNumber)
{
  r32 result = 440.f;
  result *= powf(powf(2.f, 1.f/12.f), (r32)((s32)noteNumber - 69));
  
  return(result);
}

namespace midi {  
  // NOTE: using a macro so that if you wanna change the signature (ie remove 'len') then you do it once here
#define MIDI_HANDLER(name) void (name)(u8 channel, u8* data, u8 len, PluginState *pluginState)
  typedef MIDI_HANDLER(MidiHandler); // Function pointer type for MIDI handlers

  // Functions for each MIDI command  
  static MIDI_HANDLER(NoteOff)
  {
    //     assert(len == 2);
    u8 key = data[0];
    u8 velocity = data[1];
    UNUSED(velocity);

    pluginState->freq = hertzFromMidiNoteNumber(key);
    pluginSetFloatParameter(&pluginState->parameters[PluginParameter_volume], 0.0);
    
#if BUILD_DEBUG
    stringListPushFormat(globalLogger->logArena, &globalLogger->log,
			 "Note Off: Channel %u Key %u Velocity %u\n", channel, key, velocity);
#endif
  }
  
  static MIDI_HANDLER(NoteOn)
  {
    //     assert(len == 2);
    u8 key = data[0];
    u8 velocity = data[1];

    pluginState->freq = hertzFromMidiNoteNumber(key);
    // TODO: it would be more "correct" to check the volume range here
    pluginSetFloatParameter(&pluginState->parameters[PluginParameter_volume], (r32)velocity / 127.f);
    
#if BUILD_DEBUG
    stringListPushFormat(globalLogger->logArena, &globalLogger->log,
			 "Note On: Channel %u Key %u Velocity %u\n", channel, key, velocity);
#endif
  }

  static MIDI_HANDLER(Aftertouch)
  {
    //     assert(len == 2);
    u8 key = data[0];
    u8 touch = data[1];
    
    pluginState->freq = hertzFromMidiNoteNumber(key);
    // TODO: it would be more "correct" to check the volume range here
    pluginSetFloatParameter(&pluginState->parameters[PluginParameter_volume], (r32)touch / 127.f);

#if BUILD_DEBUG
    stringListPushFormat(globalLogger->logArena, &globalLogger->log,
			 "Aftertouch: Channel %u Key %u Touch %u\n", channel, key, touch);
#endif
  }
  
  static MIDI_HANDLER(ContinuousController)
  {
    //     assert(len == 2);
    u8 controller = data[0]; 
    u8 value = data[1];
    
    int paramIndex = ccParamTable[controller];
    r32 min = pluginState->parameters[paramIndex].range.min;
    r32 max = pluginState->parameters[paramIndex].range.max;
    r32 normalizedValue = ((max - min) * (r32)value / 127.f) + min;

    pluginSetFloatParameter(&pluginState->parameters[paramIndex], normalizedValue);

#if BUILD_DEBUG
    stringListPushFormat(globalLogger->logArena, &globalLogger->log,
			 "Continuous Controller: Channel %u Controller %u Value %u the normalized value is: %.2f\n", channel, controller, value, normalizedValue);
#endif			 
  }

  static MIDI_HANDLER(PatchChange)
  {
    //     assert(len == 1);
    u8 instrument = data[0];
    UNUSED(instrument);
    
#if BUILD_DEBUG
    stringListPushFormat(globalLogger->logArena, &globalLogger->log,
			 "Patch Change: Channel %u instrument %u\n", channel, instrument);
#endif
  }

  //void ChannelPressure(u8 channel, u8* data, u8 len, PluginState *pluginState) {
  static MIDI_HANDLER(ChannelPressure)
  {
    //     assert(len == 1);
    u8 pressure = data[0];  // Controller number (data[1])
    UNUSED(pressure);

#if BUILD_DEBUG
    stringListPushFormat(globalLogger->logArena, &globalLogger->log,
			 "Channel Pressure: Channel %u pressure %u\n", channel, pressure);
#endif
  }

  //void PitchBend(u8 channel, u8* data, u8 len, PluginState* pluginState) {
  static MIDI_HANDLER(PitchBend)
  {
    //
    assert(len == 2);
    u8 lsb = data[0];
    u8 msb = data[1];
    
    s16 pitchbenddata = (static_cast<s16>(msb) << 7) | lsb;
    pitchbenddata -= 8192;

    r32 pitchRange = 2.0f; //Semitones range
    r32 normalizedBend = (r32)pitchbenddata / 8192.0f; // Change made so we work in [-1,1] range
    r32 pitchFactor = powf(2.0f, normalizedBend * pitchRange / 12.0f); // semitones calculation 2^(ST/12)
    UNUSED(pitchFactor);

    //pluginState->freq[channel] = pluginState->freq[channel] * pitchFactor;
#if BUILD_DEBUG
    stringListPushFormat(globalLogger->logArena, &globalLogger->log,
			 "Pitch Bend: Channel %u pitchFactor %.2f\n", channel, pitchFactor);
#endif
  }

  //void SystemMessages(u8 channel, u8* data, u8 len, PluginState *pluginState) {
  static MIDI_HANDLER(SystemMessages)
  {
#if BUILD_DEBUG
    stringListPush(globalLogger->logArena, &globalLogger->log, STR8_LIT("System Messages"));
#endif			 
  }

  // Define the MIDI command table as an array of function pointers
  static MidiHandler *midiCommandTable[8] {
    NoteOff,
    NoteOn,
    Aftertouch,
    ContinuousController,
    PatchChange,
    ChannelPressure,
    PitchBend,
    SystemMessages
  };

  // Function to call the handler for a specific command
  static void processMidiCommand(u8 commandByte, u8* data, u8 len, PluginState *pluginState) {

    u8 channel = commandByte & 0x0F; // take the last 4 bits
        
    u8 commandTableIndex = commandByte & 0xF0; // take the first 4 bits
    commandTableIndex >>= 4; // shift the bits to the right
    commandTableIndex &= 0b0111; // take the last 3 bits

    // TODO: this can't happen
    if (midiCommandTable[commandTableIndex] == nullptr) {
      //printf("Unknown MIDI command\n");
      logString("Unknown MIDI command\n");
      return;
    }

    ASSERT(commandTableIndex < ARRAY_COUNT(midiCommandTable));
    midiCommandTable[commandTableIndex](channel, data, len, pluginState);
  }
 
  static u8 *parseMidiMessage(u8 *atMidiBufferInit, PluginState *pluginState, u32 &midiMessageCount, u32 sampleIndex){


    //u8 *atMidiBuffer = *atMidiBufferPtrInOut;
    u8 *atMidiBuffer = atMidiBufferInit;
        
    if(midiMessageCount){
#ifdef MIDI_VERBOSE
      //printf("ALL BYTES : ");
      logString("ALL BYTES : ");
      size_t numberOfBytesAheadToPrint = 15;
      for (size_t i = 0; i < numberOfBytesAheadToPrint; ++i) {
	//printf("0x%02x ", (unsigned char)atMidiBuffer[i]);
	logFormatString("0x%02x ", (unsigned char)atMidiBuffer[i]);
      }
      //printf("\n");
      logString("\n");
#endif
      
      // get header metadata to check if we should consume the message
      MidiHeader *header = (MidiHeader *)atMidiBuffer;      
      u8 bytesToRead = header->messageLength;
      u64 timestamp = header->timestamp;      
      if(sampleIndex >= timestamp)
	{
	  //logFormatString("midi message consumed:\n  timestamp: %zu\n  sampleIndex: %u",
	  //		  timestamp, sampleIndex);
	  atMidiBuffer += sizeof(MidiHeader); // forward atMidiBuffer pointer past the header
            
	  // get the command byte (command byte include both command and channel info within it)
	  u8 commandByte = *atMidiBuffer;
	  atMidiBuffer += sizeof(u8); // forward atMidiBuffer pointer past the command byte
	  --bytesToRead;

	  // get the rest of the data byte
	  u8 *data = atMidiBuffer;
	  atMidiBuffer += (bytesToRead*sizeof(u8));  // forward to the end of the message

	  // apply check for command and data 

	  // pass commandByte, the number of remaining bytes, and the pointer to them
	  processMidiCommand(commandByte, data, bytesToRead, pluginState);

#ifdef MIDI_VERBOSE
	  //printf("AFTER PARSING : 0x%x | %" PRIu64  " timeStamp(ms passed) | %d bytes to read | data points to 0x%x | midiBuffer ptr points to 0x%x\n",
	  // printf("AFTER PARSING : 0x%x | %llu timeStamp(ms passed) | %d bytes to read | data points to 0x%x | midiBuffer ptr points to 0x%x\n", 
	  // 	 (int)commandByte,
	  // 	 timestamp, 
	  // 	 (int)bytesToRead, 
	  // 	 (int)data[0], 
	  // 	 (int)atMidiBuffer[0]);
	  logFormatString("AFTER PARSING : 0x%x | %llu timeStamp(ms passed) | %d bytes to read | data points to 0x%x | midiBuffer ptr points to 0x%x\n", 
		 (int)commandByte,
		 timestamp, 
		 (int)bytesToRead, 
		 (int)data[0], 
		 (int)atMidiBuffer[0]);
	  //printf("\n\n");
	  logString("\n\n");
#endif
      
	  --midiMessageCount;
	  //*atMidiBufferPtrInOut = atMidiBuffer;    
	}
    }

    return(atMidiBuffer);
  }
}
