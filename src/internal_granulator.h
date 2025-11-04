#define WINDOW_LENGTH 1024

struct Grain
{
  Grain* next;
  Grain* prev;

  u32 readIndex;
  r32 windowParam;
  
  s32 samplesToPlay;
  u32 length;
  r32 lengthInv;
  r32 stereoPosition;

  u32 startSampleIndex; // NOTE: the sample index in the processing loop at which we start processing this grain
  b32 isFinished;
};

struct GrainManager
{
  Arena* grainAllocator;

  u32 grainCount;
  Grain *firstPlayingGrain;
  Grain *lastPlayingGrain;

  Grain* grainFreeList;
  
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

