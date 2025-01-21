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

struct BitmapHeader
{
  u16 signature;
  u32 fileSize;
  u32 reserved;
  u32 dataOffset;

  u32 headerSize;
  u32 width;
  u32 height;
  u16 planes;
  u16 bitsPerPixel;
  u32 compression;
  u32 imageSize;
  u32 xPixelsPerM;
  u32 yPixelsPerM;
  u32 colorsUsed;
  u32 colorsImportant;

  u32 redMask;
  u32 greenMask;
  u32 blueMask;
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

static inline LoadedSound
loadWav(char *filename, Arena *allocator)
{
  LoadedSound result = {};

  ReadFileResult readResult = globalPlatform.readEntireFile(filename, allocator);
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

static inline LoadedBitmap
loadBitmap(char *filename, Arena *allocator)
{
  LoadedBitmap result = {};

  ReadFileResult readResult = globalPlatform.readEntireFile(filename, allocator);
  if(readResult.contents)
    {
      BitmapHeader *header = (BitmapHeader *)readResult.contents;
      ASSERT(header->signature == (u16)RIFF_CODE('B', 'M', ' ', ' '));
      ASSERT(header->fileSize == readResult.contentsSize - 1);
      result.pixels = (u32 *)(readResult.contents + header->dataOffset);
      result.width = header->width;
      result.height = header->height;
      u16 bytesPerPixel = header->bitsPerPixel / 8;
      ASSERT(bytesPerPixel == 4);
      u32 compression = header->compression;
      ASSERT(header->imageSize == bytesPerPixel*result.width*result.height);

      u32 redMask = header->redMask;
      u32 greenMask = header->greenMask;
      u32 blueMask = header->blueMask;
      u32 alphaMask = ~(redMask | greenMask | blueMask);

      u32 alphaShiftDown = alphaMask ? lowestOrderBit(alphaMask) : 24;
      u32 redShiftDown = redMask ? lowestOrderBit(redMask) : 16;
      u32 greenShiftDown = greenMask ? lowestOrderBit(greenMask) : 8;
      u32 blueShiftDown = blueMask ? lowestOrderBit(blueMask) : 0;

      u32 *sourceDest = result.pixels;
      for(u32 y = 0; y < result.height; ++y)
	{
	  for(u32 x = 0; x < result.width; ++x)
	    {
	      u32 c = *sourceDest;
	      u32 red = (c & redMask) >> redShiftDown;
	      u32 green = (c & greenMask) >> greenShiftDown;
	      u32 blue = (c & blueMask) >> blueShiftDown;
	      u32 alpha = (c & alphaMask) >> alphaShiftDown;

	      // NOTE: texture format is argb
	      // TODO: premultiplied alpha, gamma-correctness
	      *sourceDest++ = ((alpha << 24) |
			       (red << 16) |
			       (green << 8) |
			       (blue << 0));
	    }
	}

      result.stride = result.width*bytesPerPixel;      
    }

  return(result);
}

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

static inline LoadedFont
loadFont(char *filename, Arena *loadAllocator, Arena *permanentAllocator,
	 RangeU32 characterRange, r32 scaleInPixels)
{
  LoadedFont result = {};

  ReadFileResult readResult = globalPlatform.readEntireFile(filename, loadAllocator);
  if(readResult.contents)
    {
      stbtt_fontinfo stbFontInfo;
      if(stbtt_InitFont(&stbFontInfo, readResult.contents, stbtt_GetFontOffsetForIndex(readResult.contents, 0)))
	{
	  int ascent, descent, lineGap;
	  stbtt_GetFontVMetrics(&stbFontInfo, &ascent, &descent, &lineGap);
	 
	  r32 fontScale = stbtt_ScaleForPixelHeight(&stbFontInfo, scaleInPixels);

	  result.characterRange = characterRange;
	  result.glyphCount = characterRange.max - characterRange.min;
	  result.verticalAdvance = fontScale*(r32)(ascent - descent + lineGap);
	  result.glyphs = arenaPushArray(permanentAllocator, result.glyphCount, LoadedBitmap);
	  result.horizontalAdvance = arenaPushArray(permanentAllocator, result.glyphCount*result.glyphCount, r32);
	  
	  LoadedBitmap *glyphAt = result.glyphs;
	  r32 *advanceAt = result.horizontalAdvance;
	  for(u32 character = characterRange.min; character < characterRange.max; ++character, ++glyphAt)
	    {
	      int width, height, xOffset, yOffset;
	      u8 *glyphSource =  stbtt_GetCodepointBitmap(&stbFontInfo, 0, fontScale, character,
							  &width, &height, &xOffset, &yOffset);
	      
	      int advanceWidth, leftSideBearing;	      
	      stbtt_GetCodepointHMetrics(&stbFontInfo, character, &advanceWidth, &leftSideBearing);
	      
	      for(u32 otherChar = characterRange.min; otherChar < characterRange.max; ++otherChar, ++advanceAt)
		{
		  if(otherChar == ' ')
		    {
		      *advanceAt = fontScale*(r32)advanceWidth - (r32)xOffset;
		    }
		  else
		    {
		      int kernAdvance = stbtt_GetCodepointKernAdvance(&stbFontInfo, character, otherChar);
		      *advanceAt = fontScale*(r32)advanceWidth + (r32)kernAdvance;
		    }
		}
	      
	      if(glyphSource)
		{
		  glyphAt->width = width;
		  glyphAt->height = height;
		  glyphAt->stride = width*sizeof(u32);
		  glyphAt->alignPercentage.x = 0;//width ? (r32)xOffset/(r32)width : 0;
		  glyphAt->alignPercentage.y = height ? (r32)(height + yOffset)/(r32)height : 0;
		  glyphAt->glHandle = 0;
		  glyphAt->pixels = (width && height) ? arenaPushArray(permanentAllocator, glyphAt->height*glyphAt->width, u32) : 0; 

		  // NOTE: stb bitmaps are top-down, but we want bottom-up
		  u8 *src = glyphSource;
		  u32 *destRow = glyphAt->pixels + glyphAt->width*(glyphAt->height - 1);
		  for(int y = 0; y < height; ++y)
		    {
		      u32 *dest = destRow;
		      for(int x = 0; x < width; ++x)
			{
			  u8 alpha = *src++;
			  *dest++ = ((alpha << 24) |
				     (alpha << 16) |
				     (alpha <<  8) |
				     (alpha <<  0));
			}

		      destRow -= glyphAt->width;
		    }

		  stbtt_FreeBitmap(glyphSource, 0);
		}
	    }
	}

      // TODO: need to think harder about how file memory is allocated and used
      globalPlatform.freeFileMemory(readResult, loadAllocator);
    }

  return(result);
}

static inline LoadedBitmap *
getGlyphFromChar(LoadedFont *font, char c)
{
  u32 glyphIndex = c - font->characterRange.min;
  ASSERT(glyphIndex < font->glyphCount);
  LoadedBitmap *glyph = font->glyphs + glyphIndex;

  return(glyph);
}

static inline r32
getHorizontalAdvance(LoadedFont *font, char current, char next)
{
  u32 currentGlyphIndex = current - font->characterRange.min;
  u32 nextGlyphIndex = next - font->characterRange.min;
  r32 result = font->horizontalAdvance[currentGlyphIndex*font->glyphCount + nextGlyphIndex];

  return(result);
}

inline Rect2
getTextRectangle(LoadedFont *font, char *text, u32 windowWidth, u32 windowHeight)
{
  Rect2 result = {};

  char *at = text;
  while(*at)
    {
      char c = *at;
      u32 glyphIndex = c - font->characterRange.min;
      ASSERT(glyphIndex < font->glyphCount);
      LoadedBitmap *glyph = font->glyphs + glyphIndex;
      r32 scaledGlyphHeight = 2.f*(r32)glyph->height/(r32)windowHeight;
      result.max.y = MAX(result.max.y, scaledGlyphHeight);

      ++at;
      if(*at)
	{	  
	  r32 scaledAdvance = 2.f*(r32)getHorizontalAdvance(font, c, *at)/(r32)windowWidth;
	  result.max.x += scaledAdvance;
	}      
    }

  return(result);
}

inline v2
getTextDim(LoadedFont *font, u8 *text)
{
  v2 result = {};

  u8 *at = text;
  while(*at)
    {
      u8 c = *at;
      u32 glyphIndex = c - font->characterRange.min;
      ASSERT(glyphIndex < font->glyphCount);
      LoadedBitmap *glyph = font->glyphs + glyphIndex;
      result.y = MAX(result.y, (r32)glyph->height);

      ++at;
      if(*at) result.x += (r32)getHorizontalAdvance(font, c, *at); 
    }

  return(result);
}
