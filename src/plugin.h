#include "file_formats.h"

struct PlayingSound
{  
  LoadedSound sound;
  u32 samplesPlayed;
  bool isPlaying;
};

struct PluginState
{
  Arena permanentArena;
  Arena frameArena;
  
  r32 phasor;
  r32 freq;
  r32 volume;

  v3 mouseP;
  v3 lastMouseP;
  v2 mouseLClickP;
  v2 mouseRClickP;

  //usz wavDataSize;
  //u8 *wavData;
  PlayingSound loadedSound;

  bool initialized;
};
