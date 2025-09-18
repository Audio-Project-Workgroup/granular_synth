static r32
getRandomStereoPosition(r32 spreadAmount)
{
  // Generate a random value between -1 and 1 using a normal distribution
  // Higher spreadAmount = wider stereo field

  // Simple random number between -1 and 1 (using two calls to rand() for better distribution)
  // TODO: replace rand() with custom prng for better performance, uniformity, and consistency across platforms
  r32 randVal = ((((r32)rand() / (r32)RAND_MAX) * 2.0f - 1.0f) * 0.5f +
		 (((r32)rand() / (r32)RAND_MAX) * 2.0f - 1.0f)) * 0.5f;

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
      *hannBuffer++ = 0.5f * (1.f - Cos(GS_TAU * sample * windowLengthInv));
      *sineBuffer++ = Sin(M_PI * sample * windowLengthInv);
      *triangleBuffer++ = (1.f - Abs((sample - (windowLengthF - 1.f) / 2.f) / ((windowLengthF - 1.f) / 2.f)));
      *rectangularBuffer++ = 1.f;
    }
}

static AudioRingBuffer
initializeGrainBuffer(PluginState *pluginState, u32 bufferCount)
{
  AudioRingBuffer grainBuffer = {};
  //GrainBuffer grainBuffer = {};
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
  
  result.grainAllocator = pluginState->grainArena;
  result.grainPlayList = arenaPushStruct(result.grainAllocator, Grain);
  result.grainPlayList->next = result.grainPlayList;
  result.grainPlayList->prev = result.grainPlayList;

  return(result);
}

static void
makeNewGrain(GrainManager* grainManager, u32 grainSize, r32 windowParam, r32 spread)
{
  Grain* result;
  //TODO: Add some logic that could update based off of position so it doesn't just re-use the same grains over n over
  if (grainManager->grainFreeList)
    {
      result = grainManager->grainFreeList;
      grainManager->grainFreeList = result->next;
      //result->next = 0;
    }
  else
    {
      result = arenaPushStruct(grainManager->grainAllocator, Grain);
    }

  AudioRingBuffer* buffer = grainManager->grainBuffer;
  result->start[0] = buffer->samples[0] + buffer->readIndex;
  result->start[1] = buffer->samples[1] + buffer->readIndex;
    
  result->samplesToPlay = grainSize;
  result->length = grainSize;
  result->lengthInv = (r32)1/grainSize;
  result->windowParam = MIN(windowParam, (WindowShape_count - 1));

  result->stereoPosition = getRandomStereoPosition(spread);

  DLINKED_LIST_APPEND(grainManager->grainPlayList, result);

  grainManager->samplesProcessedSinceLastSeed = 0;
  ++grainManager->grainCount;
}

static void
destroyGrain(GrainManager* grainManager, Grain* grain)
{
  --grainManager->grainCount;

  DLINKED_LIST_REMOVE(grainManager->grainPlayList, grain);
  STACK_PUSH(grainManager->grainFreeList, grain);
  grain->prev = 0;
}

inline r32
getWindowVal(GrainManager* grainManager, r32 samplesPlayedFrac, r32 windowParam) 
{
  r32 tablePosition = samplesPlayedFrac * (r32)(WINDOW_LENGTH - 1);
  u32 tableIndex = (u32)tablePosition;
  r32 indexFrac = tablePosition - tableIndex;

  u32 windowIndex = (u32)windowParam;
  r32 windowFrac = windowParam - windowIndex;

  ASSERT(tableIndex < WINDOW_LENGTH);
  ASSERT(((tableIndex + 1) < WINDOW_LENGTH) || (indexFrac == 0));
  ASSERT(windowIndex + 1 < ARRAY_COUNT(grainManager->windowBuffer));

  r32 floor0 = grainManager->windowBuffer[windowIndex][tableIndex];
  r32 ceil0 = grainManager->windowBuffer[windowIndex][tableIndex + 1];
  r32 floor1 = grainManager->windowBuffer[windowIndex + 1][tableIndex];
  r32 ceil1 = grainManager->windowBuffer[windowIndex + 1][tableIndex + 1];

  // NOTE: bilinear blend
  r32 windowVal0 = lerp(floor0, ceil0, indexFrac);
  r32 windowVal1 = lerp(floor1, ceil1, indexFrac);
  r32 result = lerp(windowVal0, windowVal1, windowFrac);

  return(result);
}

static void
synthesize(r32* destBufferLInit, r32* destBufferRInit,
	   GrainManager* grainManager, PluginState *pluginState,
	   u32 samplesToWrite)
{
  logString("\nsynthesize called\n");
  logFormatString("samplesToWrite: %lu", samplesToWrite);

  AudioRingBuffer *buffer = grainManager->grainBuffer;
  GrainStateView *grainStateView = &pluginState->grainStateView;

  u32 currentOffset = getAudioRingBufferOffset(buffer);
  u32 targetOffset = (u32)pluginReadFloatParameter(&pluginState->parameters[PluginParameter_offset]);
  r32 readPositionIncrement = ((r32)currentOffset - (r32)targetOffset)/(r32)samplesToWrite;
  logFormatString("currentOffset: %.2f", currentOffset);
  logFormatString("targetOffset: %.2f", targetOffset);
  logFormatString("readPositionIncrement: %.2f", readPositionIncrement);

  // NOTE: queue a new view and fill out its data
  u32 viewWriteIndex = (grainStateView->viewWriteIndex + 1) % ARRAY_COUNT(grainStateView->views);  
  u32 entriesQueued = globalPlatform.atomicLoad(&grainStateView->entriesQueued);
  GrainBufferViewEntry *newView = grainStateView->views + viewWriteIndex;  
  newView->grainCount = 0;
  ASSERT(samplesToWrite < newView->sampleCapacity);
  newView->sampleCount = samplesToWrite;

  ZERO_ARRAY(newView->bufferSamples[0], newView->sampleCapacity, r32);
  ZERO_ARRAY(newView->bufferSamples[1], newView->sampleCapacity, r32);

  readSamplesFromAudioRingBuffer(buffer, newView->bufferSamples[0], newView->bufferSamples[1], samplesToWrite, false);
  grainStateView->viewBuffer.readIndex = ((grainStateView->viewBuffer.readIndex + newView->sampleCount) %
					  grainStateView->viewBuffer.capacity);

  newView->bufferReadIndex = buffer->readIndex;
  newView->bufferWriteIndex = buffer->writeIndex;

  PluginFloatParameter *densityParam = pluginState->parameters + PluginParameter_density;

  for(Grain *grain = grainManager->grainPlayList->next;
      grain && grain != grainManager->grainPlayList;
      grain = grain->next)
    {      
      GrainViewEntry *grainView = newView->grainViews + newView->grainCount++;
      usz ptrDiff = (usz)grain->start[0] - (usz)buffer->samples[0];
      u32 readIndex = safeTruncateU64(ptrDiff)/sizeof(*grain->start[0]);
      grainView->endIndex = (readIndex + grain->samplesToPlay) % buffer->capacity;
      grainView->startIndex = ((grainView->endIndex >= grain->length) ?
			       (grainView->endIndex - grain->length) :
			       (buffer->capacity + grainView->endIndex - grain->length));      
    }

  // NOTE: process grains
  u32 startReadIndex = buffer->readIndex;
    
  r32* destBufferL = destBufferLInit;
  r32* destBufferR = destBufferRInit;  
  for (u32 sampleIndex = 0; sampleIndex < samplesToWrite; ++sampleIndex)
    {     
#if 1
      // TODO: update these parameters in this loop?
      u32 grainSize = (u32)pluginReadFloatParameter(&pluginState->parameters[PluginParameter_size]);      
      r32 windowParam = pluginReadFloatParameter(&pluginState->parameters[PluginParameter_window]);
      r32 densityRaw = pluginReadFloatParameter(densityParam);
      r32 density = densityParam->processingTransform(densityRaw);
      r32 spread = pluginReadFloatParameter(&pluginState->parameters[PluginParameter_spread]);      

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
	      
	      r32 panLeft = 1.0f - fmaxf(0.0f, c_grain->stereoPosition);
	      r32 panRight = 1.0f + fminf(0.0f, c_grain->stereoPosition);
          
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

  grainStateView->viewWriteIndex = viewWriteIndex;
  while(globalPlatform.atomicCompareAndSwap(&grainStateView->entriesQueued, entriesQueued, entriesQueued + 1) !=
	entriesQueued)
    {
      entriesQueued = globalPlatform.atomicLoad(&grainStateView->entriesQueued);
    }
}

