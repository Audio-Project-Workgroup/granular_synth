static r32
getRandomStereoPosition(r32 spreadAmount)
{
  // Generate a random value between -1 and 1 using a normal distribution
  // Higher spreadAmount = wider stereo field

  // Simple random number between -1 and 1 (using two calls to rand() for better distribution)
  // TODO: replace rand() with custom prng for better performance, uniformity, and consistency across platforms
  // r32 randVal = ((((r32)gsRand() / (r32)RAND_MAX) * 2.0f - 1.0f) * 0.5f +
  //             (((r32)gsRand() / (r32)RAND_MAX) * 2.0f - 1.0f)) * 0.5f;
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

  return(grainBuffer);
}

static void
makeNewGrain(GrainManager* grainManager, u32 grainSize, r32 windowParam, r32 spread, u32 sampleIndex)
{
  Grain* result = grainManager->grainFreeList;
  if(result)
    {
      STACK_POP(grainManager->grainFreeList);
    }
  else
    {
      result = arenaPushStruct(grainManager->grainAllocator, Grain);
    }
  ASSERT(result);

  // AudioRingBuffer* buffer = grainManager->grainBuffer;
  // result->readIndex = buffer->readIndex;
  result->readIndex = grainManager->readIndex;

  result->samplesToPlay = grainSize;
  result->length = grainSize;
  result->lengthInv = (r32)1/grainSize;
  result->windowParam = MIN(windowParam, (WindowShape_count - 1));

  result->stereoPosition = getRandomStereoPosition(spread);

  result->startSampleIndex = sampleIndex;
  result->isFinished = 0;

  DLL_PUSH_BACK(grainManager->firstPlayingGrain, grainManager->lastPlayingGrain, result);

  grainManager->samplesProcessedSinceLastSeed = 0;
  ++grainManager->grainCount;

  logFormatString("created a grain. grain count is now %u", grainManager->grainCount);
}

static void
destroyGrain(GrainManager* grainManager, Grain* grain)
{
  ASSERT(grain->isFinished);
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
synthesize(SamplePair *destSamples,//r32* destBufferLInit, r32* destBufferRInit,
           GrainManager* grainManager,//, GrainStateView *grainStateView,
           //u32 targetOffset,
           u32 samplesToWrite)
{

//  AudioRingBuffer *buffer = grainManager->grainBuffer;
  GrainStateView *grainStateView = grainManager->grainStateView;

  PluginFloatParameter *densityParam = grainManager->parameters + PluginParameter_density;
  PluginFloatParameter *sizeParam = grainManager->parameters + PluginParameter_size;
  PluginFloatParameter *windowParam = grainManager->parameters + PluginParameter_window;
  PluginFloatParameter *spreadParam = grainManager->parameters + PluginParameter_spread;
  PluginFloatParameter *offsetParam = grainManager->parameters + PluginParameter_offset;

  u32 targetOffset = pluginReadFloatParameter(offsetParam);
  //u32 currentOffset = getAudioRingBufferOffset(buffer);
  u32 currentOffset =
    (grainManager->writeIndex > grainManager->readIndex ?
     grainManager->writeIndex - grainManager->readIndex :
     grainManager->grainBufferCount + grainManager->writeIndex - grainManager->readIndex);
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

  // ZERO_ARRAY(newView->bufferSamples[0], newView->sampleCapacity, r32);
  // ZERO_ARRAY(newView->bufferSamples[1], newView->sampleCapacity, r32);
  ZERO_ARRAY(newView->bufferSamples, newView->sampleCapacity, SamplePair);

  //readSamplesFromAudioRingBuffer(buffer, newView->bufferSamples[0], newView->bufferSamples[1], samplesToWrite, false);
  // NOTE: fill view buffer with grain buffer samples
  {
    u32 samplesToBufferEnd = grainManager->grainBufferCount - grainManager->readIndex;
    u32 samplesToCopy = MIN(samplesToBufferEnd, samplesToWrite);
    COPY_ARRAY(newView->bufferSamples, grainManager->grainBufferSamples + grainManager->readIndex,
               samplesToCopy, SamplePair);
    if(samplesToCopy < samplesToWrite)
    {
      u32 samplesRemaining = samplesToWrite - samplesToCopy;
      COPY_ARRAY(newView->bufferSamples + samplesToCopy, grainManager->grainBufferSamples,
                 samplesRemaining, SamplePair);
    }
  }

  // grainStateView->viewBuffer.readIndex =
  //   ((grainStateView->viewBuffer.readIndex + newView->sampleCount) %
  //    grainStateView->viewBuffer.capacity);
    grainStateView->viewBufferReadIndex =
    ((grainStateView->viewBufferReadIndex + newView->sampleCount) %
     grainStateView->viewBufferCount);

  // newView->bufferReadIndex = buffer->readIndex;
  // newView->bufferWriteIndex = buffer->writeIndex;
  newView->bufferReadIndex = grainManager->readIndex;
  newView->bufferWriteIndex = grainManager->writeIndex;

  for(Grain *grain = grainManager->firstPlayingGrain; grain; grain = grain->next)
  {
    GrainViewEntry *grainView = newView->grainViews + newView->grainCount++;
    grainView->endIndex = (grain->readIndex + grain->samplesToPlay) % grainManager->grainBufferCount;//buffer->capacity;
    grainView->startIndex = ((grainView->endIndex >= grain->length) ?
                             (grainView->endIndex - grain->length) :
                             (// buffer->capacity
                               grainManager->grainBufferCount
                              + grainView->endIndex - grain->length));
  }

  // NOTE: process grains
  {
    // NOTE: create new grains
    u32 startReadIndex = grainManager->readIndex;//buffer->readIndex;
    for(u32 sampleIndex = 0; sampleIndex < samplesToWrite; ++sampleIndex)
    {
      r32 window = pluginUpdateFloatParameter(windowParam);
      u32 grainSize   = (u32)pluginUpdateFloatParameter(sizeParam);
      r32 density     = pluginUpdateFloatParameter(densityParam);
      r32 spread      = pluginUpdateFloatParameter(spreadParam);

      r32 iot = (r32)grainSize/density;
      if(grainManager->samplesProcessedSinceLastSeed >= iot)
      {
        makeNewGrain(grainManager, grainSize, window, spread, sampleIndex);
        logFormatString("creating grain at sample %u", sampleIndex);
      }

      ++grainManager->samplesProcessedSinceLastSeed;

      r32 newReadPosition = (r32)startReadIndex + readPositionIncrement*(r32)(sampleIndex + 1);
      //buffer->readIndex = (u32)newReadPosition;
      grainManager->readIndex = (u32)newReadPosition;
      grainManager->readIndex %= grainManager->grainBufferCount;
      //buffer->readIndex %= buffer->capacity;
    }

    // NOTE: process playing grains
    for(Grain *grain = grainManager->firstPlayingGrain; grain; grain = grain->next)
    {
      ASSERT(!grain->isFinished);
      for(u32 sampleIndex = grain->startSampleIndex; sampleIndex < samplesToWrite; ++sampleIndex)
      {
        // r32 density = densityParamVals[sampleIndex];
        // r32 volume = MAX(1.f/density, 1.f); // TODO: how to adapt volume to prevent clipping?

        if(grain->samplesToPlay)
        {
          // r32 grainSampleL = buffer->samples[0][grain->readIndex];
          // r32 grainSampleR = buffer->samples[1][grain->readIndex];
          SamplePair grainSample = grainManager->grainBufferSamples[grain->readIndex];

          u32 samplesPlayed = grain->length - grain->samplesToPlay;
          r32 samplesPlayedFrac = (r32)samplesPlayed*grain->lengthInv;
          r32 windowVal = getWindowVal(grainManager, samplesPlayedFrac, grain->windowParam);

          r32 panL = 1.0f - MAX(0.0f, grain->stereoPosition);
          r32 panR = 1.0f + MIN(0.0f, grain->stereoPosition);

          grainSample.left *= windowVal * panL;
          grainSample.right *= windowVal *panR;
          // r32 outSampleL = volume * windowVal * panL * grainSampleL;
          // r32 outSampleR = volume * windowVal * panR * grainSampleR;

          destSamples[sampleIndex].left += grainSample.left;
          destSamples[sampleIndex].right += grainSample.right;
          // destBufferLInit[sampleIndex] += outSampleL;
          // destBufferRInit[sampleIndex] += outSampleR;

          ++grain->readIndex;
          //grain->readIndex %= buffer->capacity;
          grain->readIndex %= grainManager->grainBufferCount;
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
  }

  // NOTE: queue view entry
  grainStateView->viewWriteIndex = viewWriteIndex;
  u32 newEntriesQueued = (entriesQueued + 1) % ARRAY_COUNT(grainStateView->views);
  while(gsAtomicCompareAndSwap(&grainStateView->entriesQueued, entriesQueued, newEntriesQueued) != entriesQueued)
    {
      entriesQueued = gsAtomicLoad(&grainStateView->entriesQueued);
      newEntriesQueued = (entriesQueued + 1) % ARRAY_COUNT(grainStateView->views);
    }
}

static BUFFER_STREAM_REFILL_PROC(grainManagerRefill)
{
  ASSERT(stream->at == stream->end);

  GrainManager *grainManager = (GrainManager*)stream;
  //arenaEnd(grainManager->refillArena);

  // NOTE: refill our input buffers if needed
  BufferStream *sampleSource  = grainManager->sampleSource;
  if(sampleSource->at == sampleSource->end) sampleSource->refill(sampleSource);

  SamplePair *sampleSourceEnd = (SamplePair*)sampleSource->end;
  SamplePair *sampleSourceAt = (SamplePair*)sampleSource->at;
  u64 availableSamples = INT_FROM_PTR(sampleSourceEnd - sampleSourceAt);

  // NOTE: fill the grain buffer
  {
    SamplePair *grainBufferSamples = grainManager->grainBufferSamples;
    u32 samplesToBufferEnd = grainManager->grainBufferCount - grainManager->writeIndex;
    u32 samplesToCopy = MIN(samplesToBufferEnd, availableSamples);
    COPY_ARRAY(grainBufferSamples + grainManager->writeIndex, sampleSourceAt, samplesToCopy, SamplePair);
    if(samplesToCopy < availableSamples)
    {
      u32 samplesRemaining = availableSamples - samplesToCopy;
      COPY_ARRAY(grainBufferSamples, sampleSourceAt + samplesToCopy, samplesRemaining, SamplePair);
    }
    sampleSource->at = sampleSource->end;
  }

  SamplePair *samplesStart = arenaPushArray(grainManager->refillArena, availableSamples, SamplePair);
  SamplePair *samplesEnd = samplesStart + availableSamples;

  synthesize(samplesStart, grainManager, availableSamples);

  stream->start = (u8*)samplesStart;
  stream->end = (u8*)samplesEnd;
  stream->at = stream->start;
}


static GrainManager
initializeGrainManager(PluginState *pluginState)
{
  GrainManager result = {};
  //result.grainBuffer = &pluginState->grainBuffer;

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

  result.stream.refill = grainManagerRefill;
  //result.sampleSource = inputMixBuffer;

  result.parameters = pluginState->parameters;
  result.grainStateView = &pluginState->grainStateView;

#define GRAIN_BUFFER_SAMPLE_COUNT (1ULL << 16)
  STATIC_ASSERT(IS_POWER_OF_2(GRAIN_BUFFER_SAMPLE_COUNT), grainBufferSampleCountCheck);

  result.grainBufferCount = GRAIN_BUFFER_SAMPLE_COUNT;
  result.grainBufferSamples =
    arenaPushArray(pluginState->permanentArena, result.grainBufferCount, SamplePair);

  return(result);
}
