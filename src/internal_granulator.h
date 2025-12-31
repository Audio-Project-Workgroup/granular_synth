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

struct SamplePair
{
  r32 left;
  r32 right;
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
  //r32 *bufferSamples[2];
  SamplePair *bufferSamples;

  u32 grainCount;
  GrainViewEntry grainViews[32];
};

struct GrainStateView
{
  u32 viewReadIndex;
  u32 viewWriteIndex;
  volatile u32 entriesQueued;

  GrainBufferViewEntry views[32];

  //AudioRingBuffer viewBuffer;
  SamplePair *viewBufferSamples;
  u32 viewBufferCount;
  u32 viewBufferWriteIndex;
  u32 viewBufferReadIndex;
};

struct GrainManager
{
  BufferStream self; // NOTE: must always be the first member (so we can do casting tricks)
  BufferStream *sampleSource;

  Arena *grainAllocator;
  Arena *refillArena;

  PluginFloatParameter *parameters;

  GrainStateView *grainStateView;

  u32 grainCount;
  Grain *firstPlayingGrain;
  Grain *lastPlayingGrain;

  Grain *grainFreeList;

  //AudioRingBuffer *grainBuffer;
  SamplePair *grainBufferSamples;
  u32 grainBufferCount;
  u32 readIndex;
  u32 writeIndex;

  u32 samplesProcessedSinceLastSeed;

  r32 *windowBuffer[WindowShape_count];
};
