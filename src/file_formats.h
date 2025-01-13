#include "sort.h"

#define RIFF_CODE(a, b, c, d) (((u32)(a) << 0) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))
enum
{
  WAVE_CHUNK_ID_fmt = RIFF_CODE('f', 'm', 't', ' '),
  WAVE_CHUNK_ID_data = RIFF_CODE('d', 'a', 't', 'a'),
  WAVE_CHUNK_ID_RIFF = RIFF_CODE('R', 'I', 'F', 'F'),
  WAVE_CHUNK_ID_WAVE = RIFF_CODE('W', 'A', 'V', 'E'),
};

#pragma pack(push, 1)

struct WavHeader
{
  u32 RIFFID;
  u32 size;
  u32 WAVEID;
};

struct WavChunk
{
  u32 ID;
  u32 size;
};

struct WavFormat
{
  u16 wFormatTag;
  u16 nChannels;
  u32 nSamplesPerSec;
  u32 nAverageBytesPerSec;
  u16 nBlockAlign;
  u16 wBitsPerSample;
  u16 cbSize;
  u16 wValidBitsPerSample;
  u32 dwChannelMask;
  u8 subFormat[16];
};

#pragma pack(pop)

struct RiffIterator
{
  u8 *at;
  u8 *end;
};

static inline bool
isValid(RiffIterator it)
{
  bool result = it.at < it.end;

  return(result);
}

static inline RiffIterator
nextChunk(RiffIterator it)
{
  WavChunk *chunk = (WavChunk *)it.at;
  u32 size = ROUND_UP_TO_MULTIPLE_OF_2(chunk->size);
  it.at += sizeof(WavChunk) + size;

  return(it);
}

struct LoadedSound
{
  u32 sampleCount;
  u32 channelCount;
  
  s16 *samples[2]; // TODO: support other formats
};

static inline LoadedSound
loadWav(char *filename, Arena *allocator)
{
  LoadedSound result = {};

  ReadFileResult readResult = platform.readEntireFile(filename, allocator);
  if(readResult.contents)
    {
      WavHeader *header = (WavHeader *)readResult.contents;
      ASSERT(header->RIFFID == WAVE_CHUNK_ID_RIFF);
      ASSERT(header->WAVEID == WAVE_CHUNK_ID_WAVE);

      u32 channelCount = 0;
      u32 sampleDataSize = 0;
      s16 *sampleData = 0;

      RiffIterator it = {};
      it.at = (u8 *)(header + 1);
      it.end = it.at + header->size - 4;
      for(; isValid(it); it = nextChunk(it))
	{
	  WavChunk *chunk = (WavChunk *)it.at;
	  u32 chunkID = chunk->ID;
	  u32 chunkSize = chunk->size;
	  
	  switch(chunkID)
	    {
	    case WAVE_CHUNK_ID_fmt:
	      {		
		WavFormat *fmt = (WavFormat *)(it.at + sizeof(WavChunk));
		ASSERT(fmt->wFormatTag == 1); // NOTE: only support pcm
		ASSERT(fmt->nSamplesPerSec == 48000); // TODO: up/down-sample to the internal rate
		ASSERT(fmt->wBitsPerSample == 16); // TODO: support other formats
		ASSERT(fmt->nBlockAlign = (sizeof(s16)*fmt->nChannels));
		
	        channelCount = fmt->nChannels;
	      } break;

	    case WAVE_CHUNK_ID_data:
	      {	
		sampleData = (s16 *)(it.at + sizeof(WavChunk));
		sampleDataSize = chunkSize;
	      } break;
	    }
	}
      ASSERT(sampleData && channelCount);

      result.channelCount = channelCount;
      u32 sampleCount = sampleDataSize/(channelCount*sizeof(s16));
      if(channelCount == 1)
	{
	  result.samples[0] = sampleData;
	  result.samples[1] = 0;
	}
      else if(channelCount == 2)
	{
	  result.samples[0] = sampleData;
	  result.samples[1] = sampleData + sampleCount;

	  // TODO: de-interleave stereo samples properly
#if 1
	  for(u32 sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
	    {
	      s16 source = sampleData[2*sampleIndex];
	      sampleData[2*sampleIndex] = sampleData[sampleIndex];	      
	      sampleData[sampleIndex] = source;	      
	    }

	  result.channelCount = 1; 
#else	  
	  Arena scratchMemory = arenaSubArena(allocator, 2*(sampleCount - 1)*sizeof(u32));	  
	  	  
	  u32 m = 2*sampleCount - 1;
	  u32 totalCycleCount = 0;
	  u32 cycleBegin = 1;	  
	  for(;;)
	    {
	      // find indices of cycle
	      u32 *cycleBase = (u32 *)(scratchMemory.base + scratchMemory.used);
	      u32 cycleCount = 0;
	      u32 cycleNext = cycleBegin;	      
	      do {
		++cycleCount;
		u32 *indexPtr = arenaPushStruct(&scratchMemory, u32);
		*indexPtr = cycleNext;
		
	        cycleNext = 2*cycleNext % (2*sampleCount - 1);		
	      } while(cycleNext != cycleBegin);

	      // do the permutation
	      for(u32 i = 1; i < cycleCount; ++i)
		{
		  u32 index = *(cycleBase + i);
		  
		  u32 temp = sampleData[cycleBegin];
		  sampleData[cycleBegin] = sampleData[index];
		  sampleData[index] = temp;
		}

	      // find next starting point
	      totalCycleCount += cycleCount;
	      if(totalCycleCount == (m - 1))
		{
		  break;
		}
	      else
		{
		  u32 *indices = (u32 *)scratchMemory.base;
		  bubbleSort(indices, totalCycleCount);//, allocator);
		  cycleBegin = smallestNotInSortedArray(indices, totalCycleCount);
		}
	    }
	  arenaEndArena(allocator, scratchMemory);
#endif	 	 
	}
      else
	{
	  ASSERT(!"unhandled channel count in wav file");
	}

      result.sampleCount = sampleCount;
    }

  return(result);
}
