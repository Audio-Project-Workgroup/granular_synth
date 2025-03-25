struct GrainBuffer
{
  r32* samples[2];

  u32 writeIndex;
  u32 readIndex;

  u32 bufferSize;
};

enum WindowType {
  HANN = 1,
  SINE = 2,
  TRIANGLE = 3,
  RECTANGULAR = 4
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
  /*We calculate what the interonset times are and set them here. We start this var at 0, so in the first
  call to the synthesize function, we make a new grain, and set current_iot to whatever we compute it to be.*/
  s32 current_iot;
  r32* windowBuffer[4];
};


