#define INTERNAL_SAMPLE_RATE (48000)

#include "common.h"

inline void
logString(char *string)
{
  for(;;)
    {    
      if(globalPlatform.atomicCompareAndSwap(&globalLogger->mutex, 0, 1) == 0)
	{
	  stringListPush(globalLogger->logArena, &globalLogger->log, STR8_CSTR(string));

	  if(globalLogger->log.totalSize >= globalLogger->maxCapacity)
	    {
	      ZERO_STRUCT(&globalLogger->log);
	      arenaEnd(globalLogger->logArena);
	    }

	  globalPlatform.atomicStore(&globalLogger->mutex, 0);
	  break;
	}
    }
}

inline void
logFormatString(char *format, ...)
{
  va_list vaArgs;
  va_start(vaArgs, format);

  for(;;)
    {    
      if(globalPlatform.atomicCompareAndSwap(&globalLogger->mutex, 0, 1) == 0)
	{   
	  stringListPushFormatV(globalLogger->logArena, &globalLogger->log, format, vaArgs);

	  if(globalLogger->log.totalSize >= globalLogger->maxCapacity)
	    {
	      ZERO_STRUCT(&globalLogger->log);
	      arenaEnd(globalLogger->logArena);
	    }

	  globalPlatform.atomicStore(&globalLogger->mutex, 0);
	  break;
	}
    }
}

#include "plugin_parameters.h"
#include "file_granulator.h"
#include "file_formats.h"
#include "ui_layout.h"
#include "plugin_ui.h"
#include "plugin_render.h"
#include "ring_buffer.h"
#include "internal_granulator.h"

enum PluginMode
{
  PluginMode_editor,
  PluginMode_menu,
};

struct PlayingSound
{  
  LoadedSound sound;
  r32 samplesPlayed;  
};

INTROSPECT
struct PluginState
{
  PluginFloatParameter parameters[PluginParameter_count];

  u64 osTimerFreq;
  Arena grainArena;
  Arena permanentArena;
  Arena frameArena;
  Arena framePermanentArena;
  Arena loadArena;

  PluginHost pluginHost;
  PluginMode pluginMode;

  String8 outputDeviceNames[32];
  u32 outputDeviceCount;
  u32 selectedOutputDeviceIndex;

  String8 inputDeviceNames[32];
  u32 inputDeviceCount;
  u32 selectedInputDeviceIndex;

  LoadedGrainPackfile loadedGrainPackfile;
  FileGrainState silo;

  r64 phasor;
  r32 freq;
  PluginBooleanParameter soundIsPlaying;
  //PluginFloatParameter parameters[PluginParameter_count];

  PlayingSound loadedSound;
  LoadedBitmap testBitmap;
  LoadedFont testFont;

  //UILayout layout;
  UIContext uiContext;
  UIPanel *rootPanel;
  UIPanel *menuPanel;

  GrainManager grainManager;
  //GrainBuffer grainBuffer;
  AudioRingBuffer grainBuffer;
  GrainStateView grainStateView;

  volatile u32 initializationMutex;
  bool initialized;
};

// MIdi Continuous Controller Table 0-127
static PluginParameterEnum ccParamTable[128] = {
    PluginParameter_none,   // CC 0: Bank Select (followed by cc32 & Program Change)
    PluginParameter_none, // CC 1: Modulation Wheel (mapped to volume)
    PluginParameter_pitch,  // CC 2: Breath Controller (mapped to pitch)
    PluginParameter_none,   // CC 3: Undefined
    PluginParameter_none,   // CC 4: Foot Controller
    PluginParameter_none,   // CC 5: Portamento Time (Only use this for portamento time use cc65 to turn on/off)
    PluginParameter_none,   // CC 6: Data Entry MSB
    PluginParameter_volume, // CC 7: Channel Volume (mapped to volume)
    PluginParameter_none,   // CC 8: Balance
    PluginParameter_none,   // CC 9: Undefined
    PluginParameter_pan,   // CC 10: Pan
    PluginParameter_volume,   // CC 11: Expression Controller
    PluginParameter_none,   // CC 12: Effect Control 1 (MSB)
    PluginParameter_none,   // CC 13: Effect Control 2 (MSB)
    PluginParameter_none,   // CC 14: Undefined
    PluginParameter_none,   // CC 15: Undefined
    PluginParameter_none,   // CC 16: General Purpose Controller 1
    PluginParameter_none,   // CC 17: General Purpose Controller 2
    PluginParameter_none,   // CC 18: General Purpose Controller 3
    PluginParameter_none,   // CC 19: General Purpose Controller 4
    //22-31 are undefined, available for use by synths that let you assign controllers
    PluginParameter_none,   // CC 20: Undefined
    PluginParameter_none,   // CC 21: Undefined
    PluginParameter_none,   // CC 22: Undefined
    PluginParameter_none,   // CC 23: Undefined
    PluginParameter_none,   // CC 24: Undefined
    PluginParameter_none,   // CC 25: Undefined
    PluginParameter_none,   // CC 26: Undefined
    PluginParameter_none,   // CC 27: Undefined
    PluginParameter_none,   // CC 28: Undefined
    PluginParameter_none,   // CC 29: Undefined
    PluginParameter_none,   // CC 30: Undefined
    PluginParameter_none,   // CC 31: Undefined
    PluginParameter_none,   // CC 32: LSB for Control 0 (Bank Select)
    PluginParameter_none,   // CC 33: LSB for Control 1 (Modulation Wheel)
    PluginParameter_none,   // CC 34: LSB for Control 2 (Breath Controller)
    PluginParameter_none,   // CC 35: LSB for Control 3 (Undefined)
    PluginParameter_none,   // CC 36: LSB for Control 4 (Foot Controller)
    PluginParameter_none,   // CC 37: LSB for Control 5 (Portamento Time)
    PluginParameter_none,   // CC 38: LSB for Control 6 (Data Entry)
    PluginParameter_none,   // CC 39: LSB for Control 7 (Channel Volume)
    PluginParameter_none,   // CC 40: LSB for Control 8 (Balance)
    PluginParameter_none,   // CC 41: LSB for Control 9 (Undefined)
    PluginParameter_none,   // CC 42: LSB for Control 10 (Pan)
    PluginParameter_none,   // CC 43: LSB for Control 11 (Expression Controller)
    PluginParameter_none,   // CC 44: LSB for Control 12 (Effect Control 1)
    PluginParameter_none,   // CC 45: LSB for Control 13 (Effect Control 2)
    PluginParameter_none,   // CC 46: LSB for Control 14 (Undefined)
    PluginParameter_none,   // CC 47: LSB for Control 15 (Undefined)
    PluginParameter_none,   // CC 48: LSB for Control 16 (General Purpose Controller 1)
    PluginParameter_none,   // CC 49: LSB for Control 17 (General Purpose Controller 2)
    PluginParameter_none,   // CC 50: LSB for Control 18 (General Purpose Controller 3)
    PluginParameter_none,   // CC 51: LSB for Control 19 (General Purpose Controller 4)
    PluginParameter_none,   // CC 52: LSB for Control 20 (Undefined)
    PluginParameter_none,   // CC 53: LSB for Control 21 (Undefined)
    PluginParameter_none,   // CC 54: LSB for Control 22 (Undefined)
    PluginParameter_none,   // CC 55: LSB for Control 23 (Undefined)
    PluginParameter_none,   // CC 56: LSB for Control 24 (Undefined)
    PluginParameter_none,   // CC 57: LSB for Control 25 (Undefined)
    PluginParameter_none,   // CC 58: LSB for Control 26 (Undefined)
    PluginParameter_none,   // CC 59: LSB for Control 27 (Undefined)
    PluginParameter_none,   // CC 60: LSB for Control 28 (Undefined)
    PluginParameter_none,   // CC 61: LSB for Control 29 (Undefined)
    PluginParameter_none,   // CC 62: LSB for Control 30 (Undefined)
    PluginParameter_none,   // CC 63: LSB for Control 31 (Undefined)
    PluginParameter_none,   // CC 64: Hold Pedal (Sustain) Nearly every synth will react to 64 (sustain pedal)
    PluginParameter_none,   // CC 65: Portamento On/Off
    PluginParameter_none,   // CC 66: Sostenuto
    PluginParameter_none,   // CC 67: Soft Pedal
    PluginParameter_none,   // CC 68: Legato Footswitch
    PluginParameter_none,   // CC 69: Hold 2
    PluginParameter_none,   // CC 70: Sound Controller 1 Sound Variation
    PluginParameter_none,   // CC 71: Sound Controller 2 Resonance (Timbre)
    PluginParameter_none,   // CC 72: Sound Controller 3 (Release Time)
    PluginParameter_none,   // CC 73: Sound Controller 4 (Attack Time)
    PluginParameter_none,   // CC 74: Sound Controller 5 Frequency Cutoff (Brightness)
    PluginParameter_none,   // CC 75: Sound Controller 6 (Decay Time)
    PluginParameter_none,   // CC 76: Sound Controller 7 (Vibrato Rate)
    PluginParameter_none,   // CC 77: Sound Controller 8 (Vibrato Depth)
    PluginParameter_none,   // CC 78: Sound Controller 9 (Vibrato Delay)
    PluginParameter_none,   // CC 79: Sound Controller 10 (Undefined)
    PluginParameter_none,   // CC 80: General Purpose Controller 5
    PluginParameter_none,   // CC 81: General Purpose Controller 6
    PluginParameter_none,   // CC 82: General Purpose Controller 7
    PluginParameter_none,   // CC 83: General Purpose Controller 8
    PluginParameter_none,   // CC 84: Portamento Control
    PluginParameter_none,   // CC 85: Undefined
    PluginParameter_none,   // CC 86: Undefined
    PluginParameter_none,   // CC 87: Undefined
    PluginParameter_none,   // CC 88: High Resolution Velocity Prefix
    PluginParameter_none,   // CC 89: Undefined
    PluginParameter_none,   // CC 90: Undefined
    PluginParameter_none,   // CC 91: Effects 1 Depth (Reverb)
    PluginParameter_none,   // CC 92: Effects 2 Depth (Tremolo)
    PluginParameter_none,   // CC 93: Effects 3 Depth (Chorus)
    PluginParameter_none,   // CC 94: Effects 4 Depth (Detune)
    PluginParameter_none,   // CC 95: Effects 5 Depth (Phaser)
    //It's probably best not to use the group below for assigning controllers. 
    PluginParameter_none,   // CC 96: Data Increment
    PluginParameter_none,   // CC 97: Data Decrement
    PluginParameter_none,   // CC 98: Non-Registered Parameter Number LSB
    PluginParameter_none,   // CC 99: Non-Registered Parameter Number MSB
    PluginParameter_none,   // CC 100: Registered Parameter Number LSB
    PluginParameter_none,   // CC 101: Registered Parameter Number MSB
    PluginParameter_none,   // CC 102: Undefined
    PluginParameter_none,   // CC 103: Undefined
    PluginParameter_none,   // CC 104: Undefined
    PluginParameter_none,   // CC 105: Undefined
    PluginParameter_none,   // CC 106: Undefined
    PluginParameter_none,   // CC 107: Undefined
    PluginParameter_none,   // CC 108: Undefined
    PluginParameter_none,   // CC 109: Undefined
    PluginParameter_none,   // CC 110: Undefined
    PluginParameter_none,   // CC 111: Undefined
    PluginParameter_none,   // CC 112: Undefined
    PluginParameter_none,   // CC 113: Undefined
    PluginParameter_none,   // CC 114: Undefined
    PluginParameter_none,   // CC 115: Undefined
    PluginParameter_none,   // CC 116: Undefined
    PluginParameter_none,   // CC 117: Undefined
    PluginParameter_none,   // CC 118: Undefined
    PluginParameter_none,   // CC 119: Undefined
    //It's very important that you do not use these no matter what unless you want to invoke these functions
    PluginParameter_none,   // CC 120: All Sound Off
    PluginParameter_none,   // CC 121: Reset All Controllers
    PluginParameter_none,   // CC 122: Local Control On/Off (You might actually crash your keyboard if you use this one.)
    PluginParameter_none,   // CC 123: All Notes Off
    //You typically don't want your synths to change modes on you in the middle of making a song, so don't use these.
    PluginParameter_none,   // CC 124: Omni Mode Off
    PluginParameter_none,   // CC 125: Omni Mode On
    PluginParameter_none,   // CC 126: Mono Mode On
    PluginParameter_none    // CC 127: Poly Mode On
};
