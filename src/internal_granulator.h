struct GrainBuffer
{
  r32* samples[2];

  u32 writeIndex;
  u32 readIndex;

  u32 bufferSize;
};

enum WindowType {
  HANN,
  SINE,
  TRIANGLE,
  RECTANGULAR
};

struct Grain
{
  Grain* next;
  Grain* prev; // TODO: prev pointer unused

  r32* start[2];
  WindowType window;
  
  s32 samplesToPlay;
  u32 length;

  // TODO: these fields are probably unnecessary
  u32 rewrap_counter;
  u32 samplesTillRewrap;
  
  bool onFreeList;
};

struct GrainManager
{
  Arena* grainAllocator;

  u32 grainCount;
  Grain* grainPlayList;
  Grain* grainFreeList;
  
  GrainBuffer* grainBuffer;    

  r32 internal_clock;  
};


