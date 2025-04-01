static void
initializeWindows(GrainManager* grainManager)
{
  r32 windowLengthF = (r32)WINDOW_LENGTH;
  r32 windowLengthInv = 1.f/windowLengthF;
  
  r32 *hannBuffer = grainManager->windowBuffer[HANN];
  r32 *sineBuffer = grainManager->windowBuffer[SINE];
  r32 *triangleBuffer = grainManager->windowBuffer[TRIANGLE];
  r32 *rectangularBuffer = grainManager->windowBuffer[RECTANGULAR];
  for(u32 sample = 0; sample < WINDOW_LENGTH; ++sample)
    {
      *hannBuffer++ = 0.5f * (1.f - Cos(M_TAU * sample * windowLengthInv));
      *sineBuffer++ = Sin(M_PI * sample * windowLengthInv);
      *triangleBuffer++ = (1.f - Abs((sample - (windowLengthF - 1.f) / 2.f) / ((windowLengthF - 1.f) / 2.f)));
      *rectangularBuffer++ = 1.f;
    }
}

static GrainBuffer
initializeGrainBuffer(PluginState *pluginState, u32 bufferCount)
{
  GrainBuffer grainBuffer = {};
  grainBuffer.bufferCount = bufferCount;
  grainBuffer.samples[0] = arenaPushArray(&pluginState->permanentArena, bufferCount, r32);
  grainBuffer.samples[1] = arenaPushArray(&pluginState->permanentArena, bufferCount, r32);
  grainBuffer.writeIndex = 0;
  grainBuffer.readIndex = grainBufferSetReadPos(60, bufferCount);

  return(grainBuffer);
}

static GrainManager
initializeGrainManager(PluginState *pluginState)
{
  GrainManager result = {};
  result.grainBuffer = &pluginState->grainBuffer;
  
  result.windowBuffer[0] = arenaPushArray(&pluginState->permanentArena, WINDOW_LENGTH, r32);
  result.windowBuffer[1] = arenaPushArray(&pluginState->permanentArena, WINDOW_LENGTH, r32);
  result.windowBuffer[2] = arenaPushArray(&pluginState->permanentArena, WINDOW_LENGTH, r32);
  result.windowBuffer[3] = arenaPushArray(&pluginState->permanentArena, WINDOW_LENGTH, r32);
  initializeWindows(&result);
  
  result.grainAllocator = &pluginState->grainArena;
  result.grainPlayList = arenaPushStruct(result.grainAllocator, Grain);
  result.grainPlayList->next = result.grainPlayList;
  result.grainPlayList->prev = result.grainPlayList;

  return(result);
}

static void
makeNewGrain(GrainManager* grainManager, u32 grainSize, WindowType windowParam)
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

  GrainBuffer* buffer = grainManager->grainBuffer;
  result->start[0] = buffer->samples[0] + buffer->readIndex;
  result->start[1] = buffer->samples[1] + buffer->readIndex;
    
  result->samplesToPlay = grainSize;
  result->length = grainSize;
  result->lengthInv = (r32)1/grainSize;
  result->window = windowParam;

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

#if 0
// TODO: use lookup tables for windowing, rather than having to branch so much
inline r32
applyWindow(r32 sample, u32 index, u32 length, WindowType window)
{
  switch (window) {
  case HANN:
    return sample * (0.5 * (1 - cos(2 * M_PI * index / length)));
  case SINE:
    return sample * sin((M_PI * index) / length);
  case RECTANGULAR:
    return sample * 1; 
  case TRIANGLE:
    return sample * (1.0 - Abs((index - (length - 1) / 2.0) / ((length - 1) / 2.0)));
  default:
    return sample;
  }
}
#endif

inline r32
getWindowVal(GrainManager* grainManager, r32 samplesPlayedFrac, WindowType window) 
{
  r32 tablePosition = samplesPlayedFrac * (r32)(WINDOW_LENGTH - 1);
  u32 tableIndex = (u32)tablePosition;
  r32 indexFrac = tablePosition - tableIndex;
  
  ASSERT(tableIndex < WINDOW_LENGTH);
  ASSERT(((tableIndex + 1) < WINDOW_LENGTH) || (indexFrac == 0));
  r32 floor = grainManager->windowBuffer[window][tableIndex];
  r32 ceil = grainManager->windowBuffer[window][tableIndex + 1];  
  r32 result = lerp(floor, ceil, indexFrac);

  return(result);
}

// TODO: look up density and size parameters in this function, instead of passing fixed parameters
static void
synthesize(r32* destBufferLInit, r32* destBufferRInit,
	   GrainManager* grainManager, PluginState *pluginState,
	   u32 samplesToWrite, u32 grainSize)
{
  logString("\nsynthesize called\n");

  GrainBuffer *buffer = grainManager->grainBuffer;
  GrainStateView *grainStateView = &pluginState->grainStateView;

  // NOTE: queue a new view and fill out its data
  u32 viewWriteIndex = (grainStateView->writeIndex + 1) % ARRAY_COUNT(grainStateView->views);  
  u32 entriesQueued = globalPlatform.atomicLoad(&grainStateView->entriesQueued);
  GrainBufferViewEntry *newView = grainStateView->views + viewWriteIndex;  
  newView->grainCount = 0;
  //newView->bufferWriteIndex = buffer->writeIndex;
  //newView->bufferReadIndex = buffer->readIndex;
  ASSERT(samplesToWrite < newView->sampleCapacity);
  newView->sampleCount = samplesToWrite;

  ZERO_ARRAY(newView->bufferSamples[0], newView->sampleCapacity, r32);
  ZERO_ARRAY(newView->bufferSamples[1], newView->sampleCapacity, r32);

  u32 samplesToBufferEnd = buffer->bufferCount - buffer->readIndex;
  u32 samplesToCopy = MIN(samplesToBufferEnd, samplesToWrite);
  COPY_ARRAY(newView->bufferSamples[0], buffer->samples[0] + buffer->readIndex, samplesToCopy, r32);
  COPY_ARRAY(newView->bufferSamples[1], buffer->samples[1] + buffer->readIndex, samplesToCopy, r32);
  
  if(samplesToWrite > samplesToCopy)
    {
      u32 samplesToWriteAfterWrap = samplesToWrite - samplesToCopy;
      COPY_ARRAY(newView->bufferSamples[0] + samplesToBufferEnd, buffer->samples[0], samplesToWriteAfterWrap, r32);
      COPY_ARRAY(newView->bufferSamples[1] + samplesToBufferEnd, buffer->samples[1], samplesToWriteAfterWrap, r32);
    }
  
  for(Grain *grain = grainManager->grainPlayList->next;
      grain && grain != grainManager->grainPlayList;
      grain = grain->next)
    {      
      GrainViewEntry *grainView = newView->grainViews + newView->grainCount++;
      usz ptrDiff = (usz)grain->start[0] - (usz)buffer->samples[0];
      u32 readIndex = safeTruncateU64(ptrDiff)/sizeof(*grain->start[0]);
      grainView->endIndex = (readIndex + grain->samplesToPlay) % buffer->bufferCount;
      grainView->startIndex = ((grainView->endIndex >= grain->length) ?
			       (grainView->endIndex - grain->length) :
			       (buffer->bufferCount + grainView->endIndex - grain->length));      
    }

  // NOTE: process grains
  r32* destBufferL = destBufferLInit;
  r32* destBufferR = destBufferRInit;
  for (u32 sampleIndex = 0; sampleIndex < samplesToWrite; ++sampleIndex)
    {
#if 1
      r32 density = pluginReadFloatParameter(&pluginState->parameters[PluginParameter_density]);
      r32 iot = (r32)grainSize/density;
      r32 volume = MAX(1.f/density, 1.f); // TODO: how to adapt volume to prevent clipping?     
      if(grainManager->samplesProcessedSinceLastSeed >= iot)
	{
          makeNewGrain(grainManager, grainSize, HANN);         
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
	      ASSERT(buffer->samples[1] - buffer->samples[0] == buffer->bufferCount);
	      if(c_grain->start[0] == buffer->samples[1])
		{
		  c_grain->start[0] = buffer->samples[0];
		  c_grain->start[1] = buffer->samples[1];
		}
	      r32 sampleToWriteL = *c_grain->start[0]++;
	      r32 sampleToWriteR = *c_grain->start[1]++;

	      u32 samplesPlayed = c_grain->length - c_grain->samplesToPlay;
	      r32 samplesPlayedFrac = (r32)samplesPlayed*c_grain->lengthInv;
	      r32 windowVal = getWindowVal(grainManager, samplesPlayedFrac, c_grain->window);
	      outSampleL += windowVal*sampleToWriteL;
	      outSampleR += windowVal*sampleToWriteR;

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
      
      ++buffer->readIndex;
      buffer->readIndex %= buffer->bufferCount;
    
      *destBufferL++ = clampToRange(volume*outSampleL, -1.f, 1.f);
      *destBufferR++ = clampToRange(volume*outSampleR, -1.f, 1.f);
    }

  grainStateView->writeIndex = viewWriteIndex;
  while(globalPlatform.atomicCompareAndSwap(&grainStateView->entriesQueued, entriesQueued, entriesQueued + 1) !=
	entriesQueued)
    {
      entriesQueued = globalPlatform.atomicLoad(&grainStateView->entriesQueued);
    }
}

