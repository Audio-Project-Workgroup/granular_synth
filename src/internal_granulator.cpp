static void
makeNewGrain(GrainManager* grainManager, u32 grainSize)
{
    printf("NEW GRAIN\n");
    Grain* result;
    if (grainManager->grainFreeList)
    {
        result = grainManager->grainFreeList;
        grainManager->grainFreeList = result->next;
        result->next = 0;
    }
    else
    {
        result = arenaPushStruct(grainManager->grainAllocator, Grain);
    }

    GrainBuffer* buffer = grainManager->grainBuffer;
    result->start[0] = buffer->samples[0] + buffer->readIndex;
    result->start[1] = buffer->samples[1] + buffer->readIndex;
    result->samplesToPlay = grainSize;

    result->next = grainManager->grainPlayList;
    grainManager->grainPlayList = result;

    ++grainManager->grainCount;
}
static void
destroyGrain(GrainManager* grainManager, Grain* grain)
{
    --grainManager->grainCount;


    if (grain->prev == 0) {
        grainManager->grainPlayList = grain->next;
        grainManager->grainFreeList = grain;
    }
    else {
        printf("NUH UH");
        grain->prev->next = grain->next;

        grain->next = grainManager->grainFreeList;

        grainManager->grainFreeList = grain;

    }



}

static void
synthesize(r32* destBufferLInit, r32* destBufferRInit,
    r32 volumeInit, u32 samplesToWrite, GrainManager* grainManager)
{
    r32 volume = volumeInit;

    r32* destBufferL = destBufferLInit;
    r32* destBufferR = destBufferRInit;
    for (u32 sampleIndex = 0; sampleIndex < samplesToWrite; ++sampleIndex)
    {
        //if (nextGrainInQueue)
        //{
        //	//r64 currentTimestamp = sampleIndex*internalSamplePeriod + callerTimestamp;
        //	//if(nextGrainInQueue->startTimestamp <= currentTimestamp)
        //	if (grainState->samplesElapsedSinceLastQueue >= nextGrainInQueue->startSampleIndex)
        //	{
        //		// NOTE: dequeue the next grain and put it on the playlist	      
        //		PlayingGrain* playlistSentinel = grainState->playlistSentinel;
        //		nextGrainInQueue->nextPlaying = playlistSentinel;
        //		nextGrainInQueue->prevPlaying = playlistSentinel->prevPlaying;
        //		nextGrainInQueue->nextPlaying->prevPlaying = nextGrainInQueue;
        //		nextGrainInQueue->prevPlaying->nextPlaying = nextGrainInQueue;

        //		nextGrainInQueue = nextGrainInQueue->nextQueued;
        //		grainState->grainQueue = nextGrainInQueue;

        //		--grainState->queuedGrainCount;
        //		++grainState->playingGrainCount;
        //	}
        //}

        // NOTE: loop over playing grains and mix their samples
        //for(PlayingGrain *grain = grainState->playlistSentinel;
        //	  grain && grain->nextPlaying != grainState->playlistSentinel;
        //	  grain = grain->nextPlaying)
        for (Grain* c_grain = grainManager->grainPlayList;
            c_grain != 0;
            c_grain = c_grain->next)
        {
            
            if (c_grain->samplesToPlay > 0)
            {
                u32 samplesTillRewrap = grainManager->grainBuffer->bufferSize - c_grain->samplesToPlay -1;
                if (samplesTillRewrap <= c_grain->rewrap_counter) {
                    c_grain->start[0] = grainManager->grainBuffer->samples[0];
                    c_grain->start[1] = grainManager->grainBuffer->samples[1];
                    c_grain->rewrap_counter = 0;
                }
                r32 sampleToWriteL = *c_grain->start[0]++;
                r32 sampleToWriteR = *c_grain->start[1]++;

                //r32 evenlopedL = sampleToWriteL;
                //r32 evenlopedR = sampleToWriteR;
                r32 envelopedL = sampleToWriteL * (0.5 * (1 - cos(2 * M_PI * (c_grain->length - c_grain->samplesToPlay) / 2047)));
                r32 envelopedR = sampleToWriteR * (0.5 * (1 - cos(2 * M_PI * (c_grain->length - c_grain->samplesToPlay) / 2047)));

                --c_grain->samplesToPlay;
                ++c_grain->rewrap_counter;
                *destBufferL += volume * envelopedL;
                *destBufferR += volume * envelopedR;
                if (sampleToWriteL == 0 && sampleToWriteR == 0) {
                    //printf("HELP\n");
                }
            }

            else
            {
                //// NOTE: remove from playing chain
                //c_grain->prev->next = c_grain->next;
                //c_grain->next->prev = c_grain->prev;
                ////grain->nextPlaying = 0;
                ////grain->prevPlaying = 0;

                //// NOTE: append to free list
                //c_grain->onFreeList = true;
                //c_grain->next = grainManager->grainFreeList;
                destroyGrain(grainManager, c_grain);

            }
            //printf("%d!\n", grainManager->grainCount);
        }

        //if (grainState->queuedGrainCount && !grainState->playingGrainCount)
        //{
        //	printf("FUCK! need grains!: %u\n", grainState->queuedGrainCount);
        //}
        //else if (grainState->playingGrainCount > 1)
        //{
        //	printf("FUCK! too many grains!: %u\n", grainState->playingGrainCount);
        //}

        ++destBufferL;
        ++destBufferR;
    }
}