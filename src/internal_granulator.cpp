static void
makeNewGrain(GrainManager* grainManager, u32 grainSize, WindowType windowParam)
{
    printf("NEW GRAIN\n");
    Grain* result;
    //TODO: Add some logic that could update based off of position so it doesn't just re-use the same grains over n over
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
    result->length = grainSize;

    result->rewrap_counter = 0;
    result->samplesTillRewrap = buffer->bufferSize - buffer->readIndex;
    result->next = grainManager->grainPlayList;
    grainManager->grainPlayList = result;

    result->window = windowParam;

    printf("%d, %d, %d!!!!!!!!!\n", result->rewrap_counter, result->samplesTillRewrap, buffer->bufferSize);
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

inline r32 applyWindow(r32 sample, u32 index, u32 length, WindowType window) {
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
static void
synthesize(r32* destBufferLInit, r32* destBufferRInit,
    r32 volumeInit, u32 samplesToWrite, GrainManager* grainManager)
{
    r32 volume = volumeInit;

    r32* destBufferL = destBufferLInit;
    r32* destBufferR = destBufferRInit;
    for (u32 sampleIndex = 0; sampleIndex < samplesToWrite; ++sampleIndex)
    {
        for (Grain* c_grain = grainManager->grainPlayList;
            c_grain != 0;
            c_grain = c_grain->next)
        {
            
            if (c_grain->samplesToPlay > 0){
                if (c_grain->samplesTillRewrap <= c_grain->rewrap_counter) {
                    c_grain->start[0] = grainManager->grainBuffer->samples[0];
                    c_grain->start[1] = grainManager->grainBuffer->samples[1];
                    c_grain->rewrap_counter = 0;
                }
                r32 sampleToWriteL = *c_grain->start[0]++;
                r32 sampleToWriteR = *c_grain->start[1]++;

                r32 envelopedL = applyWindow(sampleToWriteL, c_grain->length - c_grain->samplesToPlay, c_grain->length, HANN);
                r32 envelopedR = applyWindow(sampleToWriteR, c_grain->length - c_grain->samplesToPlay, c_grain->length, HANN); 

                --c_grain->samplesToPlay;
                ++c_grain->rewrap_counter;
                *destBufferL += volume * envelopedL;
                *destBufferR += volume * envelopedR;
            }

            else
            {
                destroyGrain(grainManager, c_grain);

            }
        }

        ++destBufferL;
        ++destBufferR;
    }
}