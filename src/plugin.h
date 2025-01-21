#include "file_formats.h"
#include "ui_layout.h"
#include "plugin_render.h"

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
  Arena loadArena;
  
  r64 phasor;
  r32 freq;
  r32 volume;

  v3 mouseP;
  v3 lastMouseP;
  v2 mouseLClickP;
  v2 mouseRClickP;

  //usz wavDataSize;
  //u8 *wavData;
  PlayingSound loadedSound;
  LoadedBitmap testBitmap;
  LoadedFont testFont;

  UILayout layout;

  bool initialized;
};
