// for writing to/loading from disk

struct GrainPackfile
{
  Arena *allocator;
  void *storage;

  usz grainCount;  
  //GrainPackfileTag *tags;
  //r32 *samples;
};

struct GrainPackfileTag
{
  usz startSampleIndex; // NOTE: refers to packfile->samples + startSampleIndex*GRAIN_CHANNEL_LENGTH
  r32 vector[FILE_TAG_LENGTH];  
};

struct GrainPackfileGrain
{
  r32 samples[FILE_GRAIN_CHANNEL_LENGTH];
};

struct GrainPackfileEntry
{
  //GrainPackfileTag tag;
  r32 grainTagVector[FILE_TAG_LENGTH];
  r32 grainSamples[FILE_GRAIN_CHANNELS][FILE_GRAIN_LENGTH];
};

#pragma pack(push, 1)
struct GrainPackfileHeader
{
  usz fileSize;

  usz grainCount;
  usz grainLength;
  
  usz tagOffset; // NOTE: offset from the start of the header
  usz grainOffset; // NOTE: offset from the start of the header
};
#pragma pack(pop)

struct LoadedGrainPackfile
{
  u8 *packfileMemory;

  u64 grainCount;
  u64 grainLength;
  
  GrainPackfileTag *tags;
  r32 *samples;
};

// for runtime use

struct PlayingGrain
{
  // NOTE: a grain is either queued, playing, or freed.
  PlayingGrain *nextPlaying; 
  PlayingGrain *prevPlaying;
  
  union
  {
    PlayingGrain *nextQueued;
    PlayingGrain *nextFree;
  };
  bool onFreeList;

  //r64 startTimestamp;
  u64 startSampleIndex;

  u32 samplesRemaining;
  r32 *samples[2];
};

struct FileGrainState
{
  Arena *allocator;
    
  u32 queuedGrainCount;
  u32 playingGrainCount;

  //u64 queuedSampleIndex;
  u64 samplesElapsedSinceLastQueue;
  
  PlayingGrain *grainQueue;
  PlayingGrain *playlistSentinel;
  
  PlayingGrain *freeList;
};

inline PlayingGrain *
reverseGrainQueueOrder(PlayingGrain *head)
{
  if(head->nextQueued)
    {
      PlayingGrain *rest = reverseGrainQueueOrder(head->nextQueued);
      head->nextQueued->nextQueued = head;
      head->nextQueued = 0;
      
      return(rest);
    }
  else
    {
      return(head);
    }
}

inline void
debugPrintQueuedGrainTimestamps(PlayingGrain *queue)
{
  u32 index = 0;
  for(PlayingGrain *grain = queue; grain; grain = grain->nextQueued, ++index)
    {
      //printf("%u: %" PRIu64 "\n", index, grain->startSampleIndex);
      logFormatString("%u: %llu\n", index, grain->startSampleIndex);
    }
}
