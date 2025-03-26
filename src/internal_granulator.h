struct GrainBuffer
{
  r32* samples[2];

  u32 writeIndex;
  u32 readIndex;
  /* r32 writePosition; */
  /* r32 readPosition; */

  u32 bufferCount;
};

inline u32
grainBufferSetReadPos(u32 write_pos, u32 bufferSize)
{
  // TODO: dunno why this is the way that it is
  s32 scratch_rpos = write_pos - 1000;

  if (scratch_rpos < 0) {
    scratch_rpos += bufferSize;
  } else {
    scratch_rpos %= bufferSize;
  }
    
  return scratch_rpos;
}

enum WindowType {
  HANN = 1,
  SINE = 2,
  TRIANGLE = 3,
  RECTANGULAR = 4
};

struct Grain
{
  Grain* next;
  Grain* prev;

  r32* start[2];
  WindowType window;
  
  s32 samplesToPlay;
  u32 length;
  
  //bool onFreeList;
};

struct GrainManager
{
  Arena* grainAllocator;

  u32 grainCount;
  Grain* grainPlayList;
  Grain* grainFreeList;
  
  GrainBuffer *grainBuffer;
  /*We calculate what the interonset times are and set them here. We start this var at 0, so in the first
    call to the synthesize function, we make a new grain, and set current_iot to whatever we compute it to be.*/
  //s32 current_iot;
  u32 samplesProcessedSinceLastSeed;
  
  r32* windowBuffer[4];
};
