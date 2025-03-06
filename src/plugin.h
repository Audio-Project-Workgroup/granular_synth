#define INTERNAL_SAMPLE_RATE (48000)

struct PluginBooleanParameter
{
  bool value;
};

inline bool
pluginReadBooleanParameter(PluginBooleanParameter *param)
{
  return(param->value);
}

inline void
pluginSetBooleanParameter(PluginBooleanParameter *param, bool value)
{
  param->value = value;
}

struct PluginFloatParameter
{
  RangeR32 range;

  //r32 oldValue;
  r32 currentValue;
  r32 targetValue;
  r32 dValue;
};

inline r32
pluginReadFloatParameter(PluginFloatParameter *param)
{
  return(param->currentValue);
}

inline void
pluginSetFloatParameter(PluginFloatParameter *param, r32 value, r32 changeTimeMS = 10)
{
  //param->oldValue = param->currentValue;
  r32 changeTimeSamples = 0.001f*changeTimeMS*(r32)INTERNAL_SAMPLE_RATE;
  param->targetValue = clampToRange(value, param->range);
  param->dValue = (param->targetValue - param->currentValue)/changeTimeSamples;
}

inline void
pluginUpdateFloatParameter(PluginFloatParameter *param)
{
  // NOTE: should be called once per sample, per parameter
  r32 err = 0.001f;
  if(Abs(param->targetValue - param->currentValue) > err)
    {
      param->currentValue += param->dValue;
    }
}

enum PluginParameterType
{
  PluginParameterType_float,
  PluginParamterType_bool,
};

struct PluginParameter
{
  PluginParameterType type;

  union
  {
    PluginFloatParameter floatParam;
    PluginBooleanParameter boolParam;
  };
};

#include "file_granulator.h"
#include "file_formats.h"
#include "ui_layout.h"
#include "plugin_render.h"

struct PlayingSound
{  
  LoadedSound sound;
  r32 samplesPlayed;  
};

struct GrainBuffer
{
    r32* samples;
    
    u32 writeIndex;
    u32 readIndex;

    u32 bufferSize;
};

struct Grain
{
    r32* start;
    u32 samplesToPlay;

    Grain* next;
    Grain* prev;
};

struct GrainManager
{
    Arena* grainAllocator;
    Grain* grainPlayList;
    GrainBuffer* grainBuffer;
    u32 grainCount;

    Grain* grainFreeList;
};

void
makeNewGrain(GrainManager* grainManager, u32 grainSize)
{
    Grain* result;
    if (grainManager->grainFreeList)
    {
        result = grainManager->grainFreeList;
        grainManager->grainFreeList = result->next;
        result->next = 0;
    }
    else
    {
        result = arenaPushStruct(grainManager->grainAllocator, Grain);
    }

    GrainBuffer* buffer = grainManager->grainBuffer;
    result->start = buffer->samples + buffer->readIndex;
    result->samplesToPlay = grainSize;

    result->next = grainManager->grainPlayList;
    grainManager->grainPlayList = result;

    ++grainManager->grainCount;
}

void
destroyGrain(GrainManager* grainManager, Grain* grain)
{
    --grainManager->grainCount;
    grain->prev->next = grain->next;
    grain->next = grainManager->grainFreeList;
    grainManager->grainFreeList = grain;
}

u32 setReadPos(u32 write_pos, u32 bufferSize) {
    u32 scratch_rpos = write_pos - 2400;

    if (scratch_rpos < 0) {
        scratch_rpos += bufferSize;
    }
    else {
        scratch_rpos %= bufferSize;
    }
    
    return scratch_rpos;

}
struct PluginState
{
  Arena grainArena;
  Arena permanentArena;
  Arena frameArena;
  Arena loadArena; 
  
  r64 phasor;
  r32 freq;
  PluginFloatParameter volume;
  PluginBooleanParameter soundIsPlaying;

    PlayingSound loadedSound;
    LoadedBitmap testBitmap;
    LoadedFont testFont;

    UILayout layout;

    GrainManager GrainManager;
    GrainBuffer* gbuff;
    bool initialized;
};

/*
    Frequency of time between new grains and old grains. Ok so what I could do
*/
#

