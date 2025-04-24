#define WINDOW_LENGTH 1024

/* inline u32 */
/* grainBufferSetReadPos(u32 write_pos, u32 bufferSize) */
/* { */
/*   // TODO: dunno why this is the way that it is */
/*   s32 scratch_rpos = write_pos - 1000; */

/*   if (scratch_rpos < 0) { */
/*     scratch_rpos += bufferSize; */
/*   } else { */
/*     scratch_rpos %= bufferSize; */
/*   } */
    
/*   return scratch_rpos; */
/* } */

struct Grain
{
  Grain* next;
  Grain* prev;

  r32* start[2];
  //WindowType window;
  r32 windowParam;
  
  s32 samplesToPlay;
  u32 length;
  r32 lengthInv;
  //bool onFreeList;
  r32 stereoPosition;
};

/* struct GrainBuffer */
/* { */
/*   r32 *samples[2]; */
/*   u32 capacity; */

/*   r32 writePosition; */
/*   r32 readPosition; */

/*   r32 writeSpeed; */
/*   r32 readSpeed; */
/* } */

struct GrainManager
{
  Arena* grainAllocator;

  u32 grainCount;
  Grain* grainPlayList;
  Grain* grainFreeList;
  
  //GrainBuffer *grainBuffer;
  AudioRingBuffer *grainBuffer;  
  u32 samplesProcessedSinceLastSeed;
  
  r32* windowBuffer[WindowShape_count];
};

struct GrainViewEntry
{
  u32 startIndex;
  u32 endIndex;
};

struct GrainBufferViewEntry
{
  u32 bufferReadIndex;
  u32 bufferWriteIndex;

  u32 sampleCount;
  u32 sampleCapacity;
  r32 *bufferSamples[2];

  u32 grainCount;
  GrainViewEntry grainViews[32];
};

struct GrainStateView
{
  u32 viewReadIndex;  
  u32 viewWriteIndex;
  volatile u32 entriesQueued;

  GrainBufferViewEntry views[32];
  
  AudioRingBuffer viewBuffer;
};

