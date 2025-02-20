static GrainPackfile
beginGrainPackfile(Arena *allocator)
{
  GrainPackfile result = {};
  result.allocator = allocator;
  result.storage = arenaPushSize(allocator, 0);  

  return(result);
}

static void
addSoundToGrainPackfile(GrainPackfile *packfile, LoadedSound *sound)
{
  u32 soundGrainCount = sound->sampleCount/GRAIN_LENGTH;
  if(sound->sampleCount % GRAIN_LENGTH) ++soundGrainCount;

  // NOTE: assuming we can read an integer multiple of GRAIN_LENGTH samples from these;
  r32 *srcSamplesL = sound->samples[0];
  r32 *srcSamplesR = sound->samples[1];
  for(u32 grainIndex = 0; grainIndex < soundGrainCount; ++grainIndex)
    {
      GrainPackfileEntry *entry = arenaPushStruct(packfile->allocator, GrainPackfileEntry);
      COPY_ARRAY(entry->grainSamples[0], srcSamplesL, GRAIN_LENGTH, r32);
      COPY_ARRAY(entry->grainSamples[1], srcSamplesR, GRAIN_LENGTH, r32);

      // TODO: investigate batching inputs to the model
      void *tagVector = globalPlatform.runModel(entry->grainSamples[0], GRAIN_LENGTH);
      COPY_ARRAY(entry->grainTagVector, tagVector, TAG_LENGTH, r32);      
      
      srcSamplesL += GRAIN_LENGTH;
      srcSamplesR += GRAIN_LENGTH;
      ++packfile->grainCount;
    }
}

static void
writePackfileToDisk(GrainPackfile *packfile, char *filename)
{ 
  usz arenaStartUsed = packfile->allocator->used;
  GrainPackfileHeader *header = arenaPushStruct(packfile->allocator, GrainPackfileHeader);
  GrainPackfileTag *tags = arenaPushArray(packfile->allocator, packfile->grainCount, GrainPackfileTag);
  GrainPackfileGrain *grains = arenaPushArray(packfile->allocator, packfile->grainCount, GrainPackfileGrain);
  usz packfileSize = packfile->allocator->used - arenaStartUsed;
  
  header->fileSize = packfileSize;
  header->grainCount = packfile->grainCount;
  header->grainLength = GRAIN_LENGTH; 
  header->tagOffset = (u8 *)tags - (u8 *)header;
  header->grainOffset = (u8 *)grains - (u8 *)header;

  GrainPackfileEntry *entries = (GrainPackfileEntry *)packfile->storage;
  for(usz grainIndex = 0; grainIndex < header->grainCount; ++grainIndex)
    {
      GrainPackfileEntry *entry = entries + grainIndex;
      GrainPackfileTag *tag = tags + grainIndex;
      GrainPackfileGrain *grain = grains + grainIndex;
      
      tag->startSampleIndex = grainIndex;//grain - grains;
      COPY_ARRAY(tag->vector, entry->grainTagVector, TAG_LENGTH, r32);
      COPY_ARRAY(grain->samples, entry->grainSamples, GRAIN_CHANNEL_LENGTH, r32);
    }
   
  globalPlatform.writeEntireFile(filename, header, packfileSize);
}

static LoadedGrainPackfile
loadGrainPackfile(char *filename, Arena *allocator)
{
  LoadedGrainPackfile result = {};  
  
  ReadFileResult readResult = globalPlatform.readEntireFile(filename, allocator);
  if(readResult.contents)
    {
      GrainPackfileHeader *header = (GrainPackfileHeader *)readResult.contents;      
      result.packfileMemory = readResult.contents;
      result.grainCount = header->grainCount;
      result.grainLength = header->grainLength;
      result.tags = (GrainPackfileTag *)((u8 *)result.packfileMemory + header->tagOffset);
      result.samples = (r32 *)((u8 *)result.packfileMemory + header->grainOffset);
    }

  return(result);
}

static FileGrainState
initializeFileGrainState(Arena *allocator)
{
  FileGrainState result = {};
  result.allocator = allocator;
  result.playlistSentinel = arenaPushStruct(result.allocator, PlayingGrain);
  result.playlistSentinel->nextPlaying = result.playlistSentinel;
  result.playlistSentinel->prevPlaying = result.playlistSentinel;

  return(result);
}

static void
queueAllGrainsFromFile(FileGrainState *grainState, LoadedGrainPackfile *source)
{
  // TODO: Right now, we queue all the grains from the supplied packfile to be played in sequential order,
  //       ie a grain is popped off the queue and put on the playlist as soon as the last grain on the playlist
  //       finishes, so that one and only one grain is playing at a time. The tags are not used at all.
  //       We should add the ability to queue grains according to tags, and control when grains should start playing
  ASSERT(grainState->queuedGrainCount == 0); 
  grainState->samplesElapsedSinceLastQueue = 0;
  u64 grainLength = source->grainLength; 
  //r64 internalSamplePeriod = 1.0/(r64)INTERNAL_SAMPLE_RATE;
  //r64 grainLengthInSeconds = internalSamplePeriod*(r64)grainLength;
  
  GrainPackfileTag *srcTags = source->tags;
  r32 *srcSamples = source->samples;  
  for(u64 grainIndex = 0; grainIndex < source->grainCount; ++grainIndex)
    {
      PlayingGrain *newGrain = 0;
      if(grainState->freeList)
	{
	  newGrain = grainState->freeList;
	  grainState->freeList = newGrain->nextFree;
	  newGrain->nextFree = 0;
	  newGrain->onFreeList = false;
	}
      else
	{
	  newGrain = arenaPushStruct(grainState->allocator, PlayingGrain);
	}

      newGrain->startSampleIndex = grainLength*grainIndex;//(r64)(grainLength*grainIndex)*internalSamplePeriod + callerTimestamp;            
      newGrain->samplesRemaining = grainLength;
      newGrain->samples[0] = srcSamples;
      newGrain->samples[1] = srcSamples + grainLength;

      // NOTE: put new grain at the head of the queue
      newGrain->nextQueued = grainState->grainQueue;
      grainState->grainQueue = newGrain;
      ++grainState->queuedGrainCount;

      ++srcTags; // TODO: how should we use the tags?
      srcSamples += 2*grainLength;
    }

  // NOTE: we reverse the queue, so that the grain with the earliest startSampleIndex is at the head of the queue,
  //       rather than at the end
  grainState->grainQueue = reverseGrainQueueOrder(grainState->grainQueue);
  debugPrintQueuedGrainTimestamps(grainState->grainQueue);
}

static void
mixPlayingGrains(r32 *destBufferLInit, r32 *destBufferRInit,
		 r32 volumeInit, u32 samplesToWrite,		
		 FileGrainState *grainState)
{
  r32 volume = volumeInit;

  r32 *destBufferL = destBufferLInit;
  r32 *destBufferR = destBufferRInit;
  PlayingGrain *nextGrainInQueue = grainState->grainQueue; 
  for(u32 sampleIndex = 0; sampleIndex < samplesToWrite; ++sampleIndex, ++grainState->samplesElapsedSinceLastQueue)
    {      
      if(nextGrainInQueue)
	{
	  //r64 currentTimestamp = sampleIndex*internalSamplePeriod + callerTimestamp;
	  //if(nextGrainInQueue->startTimestamp <= currentTimestamp)
	  if(grainState->samplesElapsedSinceLastQueue >= nextGrainInQueue->startSampleIndex)
	    {
	      // NOTE: dequeue the next grain and put it on the playlist	      
	      PlayingGrain *playlistSentinel = grainState->playlistSentinel;
	      nextGrainInQueue->nextPlaying = playlistSentinel;
	      nextGrainInQueue->prevPlaying = playlistSentinel->prevPlaying;
	      nextGrainInQueue->nextPlaying->prevPlaying = nextGrainInQueue;
	      nextGrainInQueue->prevPlaying->nextPlaying = nextGrainInQueue;
	      
	      nextGrainInQueue = nextGrainInQueue->nextQueued;
	      grainState->grainQueue = nextGrainInQueue;

	      --grainState->queuedGrainCount;
	      ++grainState->playingGrainCount;
	    }
	}
      
      // NOTE: loop over playing grains and mix their samples
      //for(PlayingGrain *grain = grainState->playlistSentinel;
      //	  grain && grain->nextPlaying != grainState->playlistSentinel;
      //	  grain = grain->nextPlaying)
      for(PlayingGrain *grain = grainState->playlistSentinel->nextPlaying;
	  grain != grainState->playlistSentinel;
	  grain = grain->nextPlaying)
	{
	  ASSERT(!grain->onFreeList);
 	  if(grain->samplesRemaining)	 
	    {
	      r32 sampleToWriteL = *grain->samples[0]++;
	      r32 sampleToWriteR = *grain->samples[1]++;	     

	      --grain->samplesRemaining;
	      *destBufferL += volume*sampleToWriteL;
	      *destBufferR += volume*sampleToWriteR;
	    }
	  else
	    {
	      // NOTE: remove from playing chain
	      grain->prevPlaying->nextPlaying = grain->nextPlaying;
	      grain->nextPlaying->prevPlaying = grain->prevPlaying;
	      //grain->nextPlaying = 0;
	      //grain->prevPlaying = 0;
	      
	      // NOTE: append to free list
	      grain->onFreeList = true;
	      grain->nextFree = grainState->freeList;
	      grainState->freeList = grain;

	      --grainState->playingGrainCount;
	    }	  
	}

      if(grainState->queuedGrainCount && !grainState->playingGrainCount)
	{
	  printf("FUCK! need grains!: %u\n", grainState->queuedGrainCount);
	}
      else if(grainState->playingGrainCount > 1)
	{
	  printf("FUCK! too many grains!: %u\n", grainState->playingGrainCount);
	}

      ++destBufferL;
      ++destBufferR;
    }  
}
