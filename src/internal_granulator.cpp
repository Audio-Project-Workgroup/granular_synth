static r32
getRandomStereoPosition(r32 spreadAmount)
{
  // Generate a random value between -1 and 1 using a normal distribution
  // Higher spreadAmount = wider stereo field

  // Simple random number between -1 and 1 (using two calls to rand() for better distribution)
  // TODO: replace rand() with custom prng for better performance, uniformity, and consistency across platforms
  // r32 randVal = ((((r32)gsRand() / (r32)RAND_MAX) * 2.0f - 1.0f) * 0.5f +
  // 		 (((r32)gsRand() / (r32)RAND_MAX) * 2.0f - 1.0f)) * 0.5f;
  RangeR32 range = {-0.5f, 0.5f};
  r32 randVal = gsRand(range) + gsRand(range);

  // Scale by spread parameter (0 = mono, 1 = full stereo)
  return randVal * spreadAmount;
}

static void
initializeWindows(GrainManager* grainManager)
{
  r32 windowLengthF = (r32)WINDOW_LENGTH;
  r32 windowLengthInv = 1.f/windowLengthF;
  
  r32 *hannBuffer = grainManager->windowBuffer[WindowShape_hann];
  r32 *sineBuffer = grainManager->windowBuffer[WindowShape_sine];
  r32 *triangleBuffer = grainManager->windowBuffer[WindowShape_triangle];
  r32 *rectangularBuffer = grainManager->windowBuffer[WindowShape_rectangle];
  for(u32 sample = 0; sample < WINDOW_LENGTH; ++sample)
    {
      *hannBuffer++ = 0.5f * (1.f - gsCos(GS_TAU * sample * windowLengthInv));
      *sineBuffer++ = gsSin(GS_PI * sample * windowLengthInv);
      *triangleBuffer++ = (1.f - gsAbs((sample - (windowLengthF - 1.f) / 2.f) / ((windowLengthF - 1.f) / 2.f)));
      *rectangularBuffer++ = 1.f;
    }
}

static AudioRingBuffer
initializeGrainBuffer(PluginState *pluginState, u32 bufferCount)
{
  AudioRingBuffer grainBuffer = {};
  grainBuffer.capacity = bufferCount;
  grainBuffer.samples[0] = arenaPushArray(pluginState->permanentArena, bufferCount, r32,
					  arenaFlagsNoZeroAlign(4*sizeof(r32)));
  grainBuffer.samples[1] = arenaPushArray(pluginState->permanentArena, bufferCount, r32,
					  arenaFlagsNoZeroAlign(4*sizeof(r32)));

  r32 offset = pluginReadFloatParameter(&pluginState->parameters[PluginParameter_offset]);
  u32 offsetSamples = (u32)offset;
  grainBuffer.writeIndex = offsetSamples;
  grainBuffer.readIndex = 0;
  //grainBuffer.writeSpeed = 1.f;
  //grainBuffer.readSpeed = 1.f;

  return(grainBuffer);
}

static GrainManager
initializeGrainManager(PluginState *pluginState)
{
  GrainManager result = {};
  result.grainBuffer = &pluginState->grainBuffer;
  
  result.windowBuffer[0] = arenaPushArray(pluginState->permanentArena, WINDOW_LENGTH, r32,
					  arenaFlagsNoZeroAlign(4*sizeof(r32)));
  result.windowBuffer[1] = arenaPushArray(pluginState->permanentArena, WINDOW_LENGTH, r32,
					  arenaFlagsNoZeroAlign(4*sizeof(r32)));
  result.windowBuffer[2] = arenaPushArray(pluginState->permanentArena, WINDOW_LENGTH, r32,
					  arenaFlagsNoZeroAlign(4*sizeof(r32)));
  result.windowBuffer[3] = arenaPushArray(pluginState->permanentArena, WINDOW_LENGTH, r32,
					  arenaFlagsNoZeroAlign(4*sizeof(r32)));
  initializeWindows(&result);
  
  result.grainAllocator = pluginState->audioArena;
  
  result.firstPlayingGrain = 0;
  result.lastPlayingGrain = 0;
  result.grainCount = 0;
  result.grainFreeList = 0;
  // result.grainPlayList = arenaPushStruct(result.grainAllocator, Grain);
  // result.grainPlayList->next = result.grainPlayList;
  // result.grainPlayList->prev = result.grainPlayList;

  return(result);
}

static void
makeNewGrain(GrainManager* grainManager, u32 grainSize, r32 windowParam, r32 spread, u32 sampleIndex)
{
  Grain* result = grainManager->grainFreeList;
  if(result)
    {
      //grainManager->grainFreeList = result->next;
      STACK_POP(grainManager->grainFreeList);
    }
  else
    {
      result = arenaPushStruct(grainManager->grainAllocator, Grain);
    }
  ASSERT(result);

  AudioRingBuffer* buffer = grainManager->grainBuffer;
  // result->start[0] = buffer->samples[0] + buffer->readIndex;
  // result->start[1] = buffer->samples[1] + buffer->readIndex;
  result->readIndex = buffer->readIndex;
    
  result->samplesToPlay = grainSize;
  result->length = grainSize;
  result->lengthInv = (r32)1/grainSize;
  result->windowParam = MIN(windowParam, (WindowShape_count - 1));

  result->stereoPosition = getRandomStereoPosition(spread);

  result->startSampleIndex = sampleIndex;
  result->isFinished = 0;

  //DLINKED_LIST_APPEND(grainManager->grainPlayList, result);
  DLL_PUSH_BACK(grainManager->firstPlayingGrain, grainManager->lastPlayingGrain, result);

  grainManager->samplesProcessedSinceLastSeed = 0;
  ++grainManager->grainCount;
  
  logFormatString("created a grain. grain count is now %u", grainManager->grainCount);
}

static void
destroyGrain(GrainManager* grainManager, Grain* grain)
{
  ASSERT(grain->isFinished);
  // DLINKED_LIST_REMOVE(grainManager->grainPlayList, grain);
  DLL_REMOVE(grainManager->firstPlayingGrain, grainManager->lastPlayingGrain, grain);
  STACK_PUSH(grainManager->grainFreeList, grain);
  grain->prev = 0;
  --grainManager->grainCount;
  
  logFormatString("destroyed a grain. grain count is now %u", grainManager->grainCount);
}

inline r32
getWindowVal(GrainManager* grainManager, r32 samplesPlayedFrac, r32 windowParam) 
{
  r32 tablePosition = samplesPlayedFrac * (r32)(WINDOW_LENGTH - 1);
  u32 tableIndex = (u32)tablePosition;
  r32 indexFrac = tablePosition - tableIndex;

  u32 windowIndex = (u32)windowParam;
  r32 windowFrac = windowParam - windowIndex;

  u32 tableIndex0 = tableIndex;
  u32 tableIndex1 = MIN(tableIndex + 1, WINDOW_LENGTH - 1);

  u32 windowIndex0 = windowIndex;
  u32 windowIndex1 = MIN(windowIndex + 1, ARRAY_COUNT(grainManager->windowBuffer) - 1);

  // ASSERT(tableIndex < WINDOW_LENGTH);
  // ASSERT(((tableIndex + 1) < WINDOW_LENGTH) || (indexFrac == 0));
  // ASSERT(windowIndex + 1 < ARRAY_COUNT(grainManager->windowBuffer));
  ASSERT(tableIndex0 < WINDOW_LENGTH && tableIndex1 < WINDOW_LENGTH);
  ASSERT(windowIndex0 < ARRAY_COUNT(grainManager->windowBuffer) && windowIndex1 < ARRAY_COUNT(grainManager->windowBuffer));

  r32 floor0 = grainManager->windowBuffer[windowIndex0][tableIndex0];
  r32 ceil0  = grainManager->windowBuffer[windowIndex0][tableIndex1];
  r32 floor1 = grainManager->windowBuffer[windowIndex1][tableIndex0];
  r32 ceil1  = grainManager->windowBuffer[windowIndex1][tableIndex1];

  // NOTE: bilinear blend
  r32 windowVal0 = lerp(floor0, ceil0, indexFrac);
  r32 windowVal1 = lerp(floor1, ceil1, indexFrac);
  r32 result = lerp(windowVal0, windowVal1, windowFrac);

  return(result);
}

static void
synthesize(r32* destBufferLInit, r32* destBufferRInit,
	   GrainManager* grainManager, GrainStateView *grainStateView,//PluginState *pluginState,
	   r32 *densityParamVals, r32 *sizeParamVals, r32 *windowParamVals, r32 *spreadParamVals,
	   u32 targetOffset,
	   u32 samplesToWrite)
{    

  AudioRingBuffer *buffer = grainManager->grainBuffer;

  u32 currentOffset = getAudioRingBufferOffset(buffer);
  r32 readPositionIncrement = ((r32)currentOffset - (r32)targetOffset)/(r32)samplesToWrite;
#if 0
  logFormatString("currentOffset: %.2f", currentOffset);
  logFormatString("targetOffset: %.2f", targetOffset);
  logFormatString("readPositionIncrement: %.2f", readPositionIncrement);
#endif

  // NOTE: queue a new view and fill out its data
  u32 viewWriteIndex = (grainStateView->viewWriteIndex + 1) % ARRAY_COUNT(grainStateView->views);  
  u32 entriesQueued = gsAtomicLoad(&grainStateView->entriesQueued);
  GrainBufferViewEntry *newView = grainStateView->views + viewWriteIndex;  
  newView->grainCount = 0;
  ASSERT(samplesToWrite < newView->sampleCapacity);
  newView->sampleCount = samplesToWrite;

  ZERO_ARRAY(newView->bufferSamples[0], newView->sampleCapacity, r32);
  ZERO_ARRAY(newView->bufferSamples[1], newView->sampleCapacity, r32);

  readSamplesFromAudioRingBuffer(buffer, newView->bufferSamples[0], newView->bufferSamples[1], samplesToWrite, false);
  grainStateView->viewBuffer.readIndex =
    ((grainStateView->viewBuffer.readIndex + newView->sampleCount) %
     grainStateView->viewBuffer.capacity);

  newView->bufferReadIndex = buffer->readIndex;
  newView->bufferWriteIndex = buffer->writeIndex;

  // for(Grain *grain = grainManager->grainPlayList->next;
  //     grain && grain != grainManager->grainPlayList;
  //     grain = grain->next)
  for(Grain *grain = grainManager->firstPlayingGrain; grain; grain = grain->next)
    {      
      GrainViewEntry *grainView = newView->grainViews + newView->grainCount++;
      // usz ptrDiff = (usz)grain->start[0] - (usz)buffer->samples[0];
      // u32 readIndex = safeTruncateU64(ptrDiff)/sizeof(*grain->start[0]);
      // grainView->endIndex = (readIndex + grain->samplesToPlay) % buffer->capacity;
      // grainView->startIndex = ((grainView->endIndex >= grain->length) ?
      // 			       (grainView->endIndex - grain->length) :
      // 			       (buffer->capacity + grainView->endIndex - grain->length));     
      grainView->endIndex = (grain->readIndex + grain->samplesToPlay) % buffer->capacity;
      grainView->startIndex = ((grainView->endIndex >= grain->length) ?
			       (grainView->endIndex - grain->length) :
			       (buffer->capacity + grainView->endIndex - grain->length));
    }

  // NOTE: process grains  
#if 1

  // NOTE: create new grains
  u32 startReadIndex = buffer->readIndex;
  for(u32 sampleIndex = 0; sampleIndex < samplesToWrite; ++sampleIndex)
  {
    r32 windowParam = windowParamVals[sampleIndex];
    u32 grainSize = (u32)sizeParamVals[sampleIndex];
    r32 density = densityParamVals[sampleIndex];
    r32 spread = spreadParamVals[sampleIndex];

    r32 iot = (r32)grainSize/density;
    if(grainManager->samplesProcessedSinceLastSeed >= iot)
    {
      makeNewGrain(grainManager, grainSize, windowParam, spread, sampleIndex);
      logFormatString("creating grain at sample %u", sampleIndex);
    }
    
    ++grainManager->samplesProcessedSinceLastSeed;

    r32 newReadPosition = (r32)startReadIndex + readPositionIncrement*(r32)(sampleIndex + 1);
    buffer->readIndex = (u32)newReadPosition;
    buffer->readIndex %= buffer->capacity;
  }

  // NOTE: process playing grains 
  for(Grain *grain = grainManager->firstPlayingGrain; grain; grain = grain->next)
  {
    ASSERT(!grain->isFinished);
    for(u32 sampleIndex = grain->startSampleIndex; sampleIndex < samplesToWrite; ++sampleIndex)
    {
      // r32 windowParam = windowParamVals[sampleIndex];
      // u32 grainSize = (u32)sizeParamVals[sampleIndex];
      r32 density = densityParamVals[sampleIndex];
      // r32 spread = spreadParamVals[sampleIndex];

      // r32 iot = (r32)grainSize/density;
      r32 volume = MAX(1.f/density, 1.f); // TODO: how to adapt volume to prevent clipping?

      if(grain->samplesToPlay)
      {	       
	r32 grainSampleL = buffer->samples[0][grain->readIndex];
	r32 grainSampleR = buffer->samples[1][grain->readIndex];

	u32 samplesPlayed = grain->length - grain->samplesToPlay;
	r32 samplesPlayedFrac = (r32)samplesPlayed*grain->lengthInv;
	r32 windowVal = getWindowVal(grainManager, samplesPlayedFrac, grain->windowParam);
	      
	r32 panL = 1.0f - MAX(0.0f, grain->stereoPosition);
	r32 panR = 1.0f + MIN(0.0f, grain->stereoPosition);
          
	r32 outSampleL = windowVal * panL * grainSampleL;
	r32 outSampleR = windowVal * panR * grainSampleR;

	destBufferLInit[sampleIndex] += outSampleL;
	destBufferRInit[sampleIndex] += outSampleR;

	++grain->readIndex;
	grain->readIndex %= buffer->capacity;
	--grain->samplesToPlay;
      }
      else
      {
	grain->isFinished = 1;
	logFormatString("destroying grain at sample %u", sampleIndex);
	break;
      }
    }
  }

  // NOTE: remove finished grains from playlist
  for(Grain *grain = grainManager->firstPlayingGrain; grain;)
  {
    Grain *next = grain->next;
    if(grain->isFinished)
    {
      destroyGrain(grainManager, grain);
    }
    else
    {
      grain->startSampleIndex = 0;
    }
    grain = next;
  }

#else
  u32 startReadIndex = buffer->readIndex;
  
  r32* destBufferL = destBufferLInit;
  r32* destBufferR = destBufferRInit;  
  for (u32 sampleIndex = 0; sampleIndex < samplesToWrite; ++sampleIndex)
    {     
#if 1
      r32 windowParam = windowParamVals[sampleIndex];
      u32 grainSize = (u32)sizeParamVals[sampleIndex];
      r32 density = densityParamVals[sampleIndex];
      r32 spread = spreadParamVals[sampleIndex];

      r32 iot = (r32)grainSize/density;
      r32 volume = MAX(1.f/density, 1.f); // TODO: how to adapt volume to prevent clipping?     
      if(grainManager->samplesProcessedSinceLastSeed >= iot)
	{
	  makeNewGrain(grainManager, grainSize, windowParam, spread);
	}      

      r32 outSampleL = 0.f;
      r32 outSampleR = 0.f;
      for (Grain* c_grain = grainManager->grainPlayList->next;
	   c_grain && c_grain != grainManager->grainPlayList;
	   ) // TODO: it's weird not having the increment be in the loop statement
        {	  
	  if (c_grain->samplesToPlay > 0)
	    {
	      // NOTE: since the left and right channels of the grain buffer are stored contiguously, we can
	      //       compare the left grain read pointer to the right grain buffer pointer to check if we
	      //       need to wrap the grain reading
	      ASSERT(buffer->samples[1] - buffer->samples[0] == buffer->capacity);
	      if(c_grain->start[0] == buffer->samples[1])
		{
		  c_grain->start[0] = buffer->samples[0];
		  c_grain->start[1] = buffer->samples[1];
		}
	      r32 sampleToWriteL = *c_grain->start[0]++;
	      r32 sampleToWriteR = *c_grain->start[1]++;

	      u32 samplesPlayed = c_grain->length - c_grain->samplesToPlay;
	      r32 samplesPlayedFrac = (r32)samplesPlayed*c_grain->lengthInv;
	      r32 windowVal = getWindowVal(grainManager, samplesPlayedFrac, c_grain->windowParam);
	      
	      r32 panLeft = 1.0f - MAX(0.0f, c_grain->stereoPosition);
	      r32 panRight = 1.0f + MIN(0.0f, c_grain->stereoPosition);
          
	      outSampleL += windowVal * sampleToWriteL * panLeft;
	      outSampleR += windowVal * sampleToWriteR * panRight;

	      --c_grain->samplesToPlay;	    
	      c_grain = c_grain->next;
	    }
	  else
	    {
	      Grain *oldGrain = c_grain;
	      c_grain = c_grain->next;
	      destroyGrain(grainManager, oldGrain);
	    }
        }
#else
      ASSERT(buffer->readIndex < buffer->bufferCount);
      *destBufferL = volume*buffer->samples[0][buffer->readIndex];
      *destBufferR = volume*buffer->samples[0][buffer->readIndex];
#endif
      
#if 0
      if(grainManager->grainCount == 0)
	{
	  logString("NEED GRAINS!");
	}
      else if(grainManager->grainCount > 1) // NOCHECKIN
	{
	  logFormatString("TOO MANY GRAINS!\n  count: %u\n  sample: %u\n  readIndex: %u\n",
			  grainManager->grainCount, sampleIndex, buffer->readIndex);
	}
#endif
      
      ++grainManager->samplesProcessedSinceLastSeed;
      
      //++buffer->readIndex;
      r32 newReadPosition = (r32)startReadIndex + readPositionIncrement*(r32)(sampleIndex + 1);
      buffer->readIndex = (u32)newReadPosition;
      buffer->readIndex %= buffer->capacity;

      // r32 wetSampleL = volume*outSampleL;
      // r32 wetSampleR = volume*outSampleR;

      // r32 drySampleL = buffer->samples[0][buffer->readIndex];
      // r32 drySampleR = buffer->samples[1][buffer->readIndex];

      // r32 mixedSampleL = lerp(drySampleL, wetSampleL, mix);
      // r32 mixedSampleR = lerp(drySampleR, wetSampleR, mix);

      *destBufferL++ = clampToRange(volume*outSampleL, -1.f, 1.f);
      *destBufferR++ = clampToRange(volume*outSampleR, -1.f, 1.f);
      // *destBufferL++ = clampToRange(mixedSampleL, -1.f, 1.f);
      // *destBufferR++ = clampToRange(mixedSampleR, -1.f, 1.f);
    }
#endif

  grainStateView->viewWriteIndex = viewWriteIndex;
  u32 newEntriesQueued = (entriesQueued + 1) % ARRAY_COUNT(grainStateView->views);
  while(gsAtomicCompareAndSwap(&grainStateView->entriesQueued, entriesQueued, newEntriesQueued) != entriesQueued)
    {
      entriesQueued = gsAtomicLoad(&grainStateView->entriesQueued);
      newEntriesQueued = (entriesQueued + 1) % ARRAY_COUNT(grainStateView->views);
    }
}

