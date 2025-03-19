// #include <stdio.h>
// #include <cassert>

#define MIDI_VERBOSE // ERASE WHEN DONE
// example function for NoteOn
static inline r32
hertzFromMidiNoteNumber(u8 noteNumber)
{
r32 result = 440.f;
result *= powf(powf(2.f, 1.f/12.f), (r32)((s32)noteNumber - 69));
return(result);
}

namespace midi {

    // Function pointer type for MIDI handlers
	typedef void (*MidiHandler)(uint8_t channel, uint8_t* data, uint8_t len, PluginState *pluginState);

    // Functions for each MIDI command
    void NoteOff(uint8_t channel, uint8_t* data, uint8_t len, PluginState* pluginState) {
        assert(len == 2);
        uint8_t key = data[0];
        uint8_t velocity = data[1];
        pluginState->freq = hertzFromMidiNoteNumber(key);
        pluginSetFloatParameter(&pluginState->volume, 0.0);
        printf("Note Off: Channel %d Key %d Velocity %d\n", channel, key, velocity);
    }

    void NoteOn(uint8_t channel, uint8_t* data, uint8_t len, PluginState* pluginState) {
        assert(len == 2);
        uint8_t key = data[0];
        uint8_t velocity = data[1];

        pluginState->freq = hertzFromMidiNoteNumber(key);
        pluginSetFloatParameter(&pluginState->volume, (r32)velocity / 127.f);
        printf("Note On: Channel %d Key %d Velocity %d\n", channel, key, velocity);
    }

    void Aftertouch(uint8_t channel, uint8_t* data, uint8_t len, PluginState* pluginState) {
        assert(len == 2);
        uint8_t key = data[0];
        uint8_t touch = data[1];
        pluginState->freq = hertzFromMidiNoteNumber(key);
        pluginSetFloatParameter(&pluginState->volume, (r32)touch / 127.f);
        printf("Aftertouch: Channel %d Key %d Touch %d\n", channel, key, touch);
    }

    void ContinuousController(uint8_t channel, uint8_t* data, uint8_t len, PluginState* pluginState) {
        assert(len == 2);
        uint8_t controller = data[0]; 
        uint8_t value = data[1]; 
        int paramIndex = ccParamTable[controller];

        r32 min = pluginState->parameters[paramIndex].range.min;
        r32 max = pluginState->parameters[paramIndex].range.max;
        r32 normalizedValue = ((max - min) * (r32)value / 127.f) + min;

        pluginSetFloatParameter(&pluginState->parameters[paramIndex], normalizedValue);

        printf("Continuous Controller: Channel %d Controller %d Value %d the normalized value is: %.2f\n", channel, controller, value, normalizedValue);
    }

    void PatchChange(uint8_t channel, uint8_t* data, uint8_t len, PluginState* pluginState) {
        assert(len == 1);
        uint8_t instument = data[0];
        printf("Patch Change: Channel %d instument %d\n", channel, instument);
    }

    void ChannelPressure(uint8_t channel, uint8_t* data, uint8_t len, PluginState *pluginState) {
        assert(len == 1);
        uint8_t pressure = data[0];  // Controller number (data[1])
        printf("Channel Pressure: Channel %d pressure %d\n",channel,pressure);
    }

    void PitchBend(uint8_t channel, uint8_t* data, uint8_t len, PluginState* pluginState) {

        assert(len == 2);
        uint8_t lsb = data[0];
        uint8_t msb = data[1];
        s16 pitchbenddata = (static_cast<s16>(msb) << 7) | lsb;

        pitchbenddata -= 8192;

        r32 pitchRange = 2.0f; //Semitones range
        r32 normalizedBend = (r32)pitchbenddata / 8192.0f; // Change made so we work in [-1,1] range
        r32 pitchFactor = powf(2.0f, normalizedBend * pitchRange / 12.0f); // semitones calculation 2^(ST/12)

        //pluginState->freq[channel] = pluginState->freq[channel] * pitchFactor;
        printf("Pitch Bend: Channel %d pitchFactor %d \n", channel, pitchFactor);
    }

    void SystemMessages(uint8_t channel, uint8_t* data, uint8_t len, PluginState *pluginState) {
        printf("System Messages");
    }

    // Define the MIDI command table as an array of function pointers
    MidiHandler midiCommandTable[8] {
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
    void processMidiCommand(uint8_t commandByte, uint8_t* data, uint8_t len, PluginState *pluginState) {

        uint8_t channel = commandByte & 0x0F; // take the last 4 bits
        
        commandByte &= 0xF0; // take the first 4 bits
        commandByte >>= 4; // shift the bits to the right
        commandByte &= 0b0111; // take the last 3 bits

        if (midiCommandTable[commandByte] == nullptr) {
            printf("Unknown MIDI command");
            return;
        }

        midiCommandTable[commandByte](channel,data,len,pluginState); // commandByte & 0xF0 will zero down the last 4 bits
    }

    void parseMidiMessage(PluginAudioBuffer *audioBuffer, PluginState *pluginState){
        u8 *atMidiBuffer = audioBuffer->midiBuffer;
        
        if(audioBuffer->midiMessageCount){

    #ifdef MIDI_VERBOSE
            printf("ALL BYTES : ");
            size_t numberOfBytesAheadToPrint = 10;
            for (size_t i = 0; i < numberOfBytesAheadToPrint; ++i) {
                printf("0x%02x ", (unsigned char)atMidiBuffer[i]);
            }
            printf("\n");
    #endif			
            // get the bytes to read
            MidiHeader *header = (MidiHeader *)atMidiBuffer;
            atMidiBuffer += sizeof(MidiHeader); // forward atMidiBuffer pointer past the header byte
            u8 bytesToRead = header->messageLength;

            // // get the timestamp
            // // u32 timeStamp= *atMidiBuffer;
            u32 timeStamp = *((u32*)atMidiBuffer);
            atMidiBuffer += sizeof(u32); // forward atMidiBuffer pointer past the command byte
            bytesToRead-=4;
            
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
            printf("AFTER PARSING : 0x%x | %d timeStamp(ms passed) | %d bytes to read | data points to 0x%x | midiBuffer ptr points to 0x%x\n", 
                (int)commandByte,
                timeStamp, 
                (int)bytesToRead, 
                (int)data[0], 
                (int)atMidiBuffer[0]);
            printf("\n\n");
    #endif
            --audioBuffer->midiMessageCount;
        }
    }
}