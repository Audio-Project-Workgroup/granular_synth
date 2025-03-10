#define INTERNAL_SAMPLE_RATE (48000)

struct PluginBooleanParameter
{
  bool value;
};

inline bool
pluginReadBooleanParameter(PluginBooleanParameter *param)
{
  return(param->value);
}

inline void
pluginSetBooleanParameter(PluginBooleanParameter *param, bool value)
{
  param->value = value;
}

struct PluginFloatParameter
{
  RangeR32 range;

  //r32 oldValue;
  r32 currentValue;
  r32 targetValue;
  r32 dValue;
};

inline r32
pluginReadFloatParameter(PluginFloatParameter *param)
{
  return(param->currentValue);
}

inline void
pluginSetFloatParameter(PluginFloatParameter *param, r32 value, r32 changeTimeMS = 10)
{
  //param->oldValue = param->currentValue;
  r32 changeTimeSamples = 0.001f*changeTimeMS*(r32)INTERNAL_SAMPLE_RATE;
  param->targetValue = clampToRange(value, param->range);
  param->dValue = (param->targetValue - param->currentValue)/changeTimeSamples;
}

inline void
pluginUpdateFloatParameter(PluginFloatParameter *param)
{
  // NOTE: should be called once per sample, per parameter
  r32 err = 0.001f;
  if(Abs(param->targetValue - param->currentValue) > err)
    {
      param->currentValue += param->dValue;
    }
}

enum PluginParameterType
{
  PluginParameterType_float,
  PluginParamterType_bool,
};

struct PluginParameter
{
  PluginParameterType type;

  union
  {
    PluginFloatParameter floatParam;
    PluginBooleanParameter boolParam;
  };
};
//#include "internal_granulator.h"
#include "file_granulator.h"

#include "file_formats.h"
#include "ui_layout.h"
#include "plugin_render.h"

struct PlayingSound
{  
  LoadedSound sound;
  r32 samplesPlayed;  
};



u32 setReadPos(u32 write_pos, u32 bufferSize) {
    s32 scratch_rpos = write_pos - 3200;

    if (scratch_rpos < 0) {
        scratch_rpos += bufferSize;
    }
    else {
        scratch_rpos %= bufferSize;
    }
    
    return scratch_rpos;

}
//struct PluginState
//{
//  
//  Arena permanentArena;
//  Arena frameArena;
//  Arena loadArena; 
//  
//  r64 phasor;
//  r32 freq;
//  PluginFloatParameter volume;
//  PluginBooleanParameter soundIsPlaying;
//
//    PlayingSound loadedSound;
//    LoadedBitmap testBitmap;
//    LoadedFont testFont;
//
//    UILayout layout;
//
//    
//    bool initialized;
//};

struct GrainBuffer
{
    r32* samples[2];

    u32 writeIndex;
    u32 readIndex;

    u32 bufferSize;
};

struct Grain
{
    
    s32 samplesToPlay;
    u32 length;

    Grain* next;
    Grain* prev;
    bool onFreeList;
    r32* start[2];
};

struct GrainManager
{
    Arena* grainAllocator;
    Grain* grainPlayList;
    GrainBuffer* grainBuffer;
    u32 grainCount;

    r32 internal_clock;

    Grain* grainFreeList;
};


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

            if (c_grain->samplesToPlay > 1)
            {
                r32 sampleToWriteL = *c_grain->start[0]++;
                r32 sampleToWriteR = *c_grain->start[1]++;

                r32 evenlopedL = sampleToWriteL * (0.5 * (1 - cos(2 * M_PI * (c_grain->length - c_grain->samplesToPlay) / 2047)));
                r32 evenlopedR = sampleToWriteR * (0.5 * (1 - cos(2 * M_PI * (c_grain->length - c_grain->samplesToPlay) / 2047)));

                --c_grain->samplesToPlay;
                *destBufferL += volume * sampleToWriteL;
                *destBufferR += volume * sampleToWriteL;
                printf("%d\n", c_grain->samplesToPlay);
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
            printf("%d!\n", grainManager->grainCount);
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
struct PluginState
{
    u64 osTimerFreq;
    Arena grainArena;
    Arena permanentArena;
    Arena frameArena;
    Arena loadArena;

    LoadedGrainPackfile loadedGrainPackfile;
    FileGrainState silo;

    r32 start_pos;

    r64 phasor;
    r32 freq;
    PluginFloatParameter volume;
    PluginFloatParameter density;
    r32 t_density;
    PluginFloatParameter duration;
    PluginBooleanParameter soundIsPlaying;

    PlayingSound loadedSound;
    LoadedBitmap testBitmap;
    LoadedFont testFont;

    UILayout layout;

    GrainManager GrainManager;
    GrainBuffer* gbuff;

    bool initialized;
};

/*
    Frequency of time between new grains and old grains. Ok so what I could do
*/
#

