#define WINDOW_LENGTH 1024

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

struct Grain
{
  Grain* next;
  Grain* prev;

  r32* start[2];
  WindowType window;
  
  s32 samplesToPlay;
  u32 length;
  r32 lengthInv;
  //bool onFreeList;
};

struct GrainManager
{
  Arena* grainAllocator;

  u32 grainCount;
  Grain* grainPlayList;
  Grain* grainFreeList;
  
  //GrainBuffer *grainBuffer;
  AudioRingBuffer *grainBuffer;
  /*We calculate what the interonset times are and set them here. We start this var at 0, so in the first
    call to the synthesize function, we make a new grain, and set current_iot to whatever we compute it to be.*/
  //s32 current_iot;
  u32 samplesProcessedSinceLastSeed;
  
  r32* windowBuffer[4];
};
#if 0
struct GrainViewNode
{
  GrainViewNode *next;

  u32 readIndex;
};

struct GrainBufferViewNode
{
  volatile GrainBufferViewNode *next;
  GrainBufferViewNode *nextFree;

  u32 bufferReadIndex;
  u32 bufferWriteIndex;  
  r32 *bufferSamples[2];

  u32 grainCount;
  GrainViewNode *head;
};

struct GrainStateView
{
  volatile GrainBufferViewNode *head;
  volatile GrainBufferViewNode *tail;

  Arena *arena;  

  GrainBufferViewNode *grainBufferViewFreelist;
  GrainViewNode *grainViewFreelist;
};
#else
struct GrainViewEntry
{
  //u32 readIndex;
  u32 startIndex;
  u32 endIndex;
};

struct GrainBufferViewEntry
{
  //u32 bufferReadIndex;
  //u32 bufferWriteIndex;
  //r32 *bufferSamples[2];
  u32 sampleCount;
  u32 sampleCapacity;
  r32 *bufferSamples[2];

  u32 grainCount;
  GrainViewEntry grainViews[32];
};

struct GrainStateView
{
  volatile u32 readIndex;  
  volatile u32 writeIndex;
  volatile u32 entriesQueued;

  GrainBufferViewEntry views[32];

  /* r32 *viewSamples[2]; */
  /* u32 viewBufferReadIndex; */
  /* u32 viewBufferWriteIndex; */
  AudioRingBuffer viewBuffer;
};
#endif
