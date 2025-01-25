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

#include "file_formats.h"
#include "ui_layout.h"
#include "plugin_render.h"

struct PlayingSound
{  
  LoadedSound sound;
  u32 samplesPlayed;
  //bool isPlaying;
  //PluginBooleanParameter *isPlaying;
};

struct PluginState
{
  Arena permanentArena;
  Arena frameArena;
  Arena loadArena;
  
  r64 phasor;
  r32 freq;
  PluginFloatParameter volume;
  PluginBooleanParameter soundIsPlaying;

  //usz wavDataSize;
  //u8 *wavData;
  PlayingSound loadedSound;
  LoadedBitmap testBitmap;
  LoadedFont testFont;

  UILayout layout;

  bool initialized;
};
