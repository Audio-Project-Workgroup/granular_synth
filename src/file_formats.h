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
loadWav(char *filename, Arena *loadAllocator, Arena *permanentAllocator)
{
  LoadedSound result = {};

  ReadFileResult readResult = globalPlatform.readEntireFile(filename, loadAllocator);
  if(readResult.contents)
    {
      WavHeader *header = (WavHeader *)readResult.contents;
      ASSERT(header->RIFFID == WAVE_CHUNK_ID_RIFF);
      ASSERT(header->WAVEID == WAVE_CHUNK_ID_WAVE);

      u32 channelCount = 0;
      u32 sampleDataSize = 0;
      u32 bytesPerSample = 0;
      u32 sampleRate = 0;
      void *sampleData = 0;

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
		//ASSERT(fmt->nSamplesPerSec == 48000); // TODO: up/down-sample to the internal rate
		sampleRate = fmt->nSamplesPerSec;
		bytesPerSample = fmt->wBitsPerSample/8;
		ASSERT(fmt->nBlockAlign == (bytesPerSample*fmt->nChannels));
		
	        channelCount = fmt->nChannels;
	      } break;

	    case WAVE_CHUNK_ID_data:
	      {		
		sampleData = it.at + sizeof(WavChunk);
		sampleDataSize = chunkSize;
	      } break;
	    }
	}
      ASSERT(sampleData && channelCount);      

      r32 sampleRateConversionFactor = ((r32)INTERNAL_SAMPLE_RATE/(r32)sampleRate);
      
      u32 sourceSampleCount = sampleDataSize/(channelCount*bytesPerSample);      
      u32 destSampleCount = ROUND_UP_TO_MULTIPLE_OF_2((u32)(sampleRateConversionFactor*sourceSampleCount));
      u32 paddedDestSampleCount = ROUND_UP_TO_MULTIPLE(destSampleCount, GRAIN_LENGTH);
      u32 maxSampleCount = MAX(sourceSampleCount, paddedDestSampleCount);

      // NOTE: we allocate more space than we actually need for storing samples, since we use these buffers
      //       as both inputs to the fft and destinations of the ifft when doing sample-rate conversion
      result.sampleCount = destSampleCount + 0;
      result.channelCount = channelCount;
      result.samples[0] = arenaPushArray(permanentAllocator, 2*maxSampleCount, r32);
      result.samples[1] = result.samples[0] + maxSampleCount;//arenaPushArray(permanentAllocator, maxSampleCount, r32);
       
      // NOTE: convert sample format         
#define CONVERT_SAMPLE(srcData, srcSample, type, channels) do {	\
	type sample = *(type *)srcData;				\
	srcSample = (r32)sample/(r32)type##_MAX;		\
	srcData = (type *)srcData + channels;			\
      } while(0)
      
      r32 *destSamplesL = result.samples[0];
      r32 *destSamplesR = result.samples[1];
      void *srcDataL = sampleData;
      // TODO: it might be worthwhile doing some kind of dynamic dispatch here
      if(channelCount == 1)
	{
	  if(bytesPerSample == 1)
	    {
	      for(u32 sampleIndex = 0; sampleIndex < sourceSampleCount; ++sampleIndex)
		{
		  r32 srcSample = 0;
		  CONVERT_SAMPLE(srcDataL, srcSample, u8, channelCount);
	      
		  *destSamplesL++ = srcSample;
		  *destSamplesR++ = srcSample;
		}
	    }
	  else if(bytesPerSample == 2)
	    {
	      for(u32 sampleIndex = 0; sampleIndex < sourceSampleCount; ++sampleIndex)
		{
		  r32 srcSample = 0;
		  CONVERT_SAMPLE(srcDataL, srcSample, s16, channelCount);
	      
		  *destSamplesL++ = srcSample;
		  *destSamplesR++ = srcSample;
		}
	    }
	  else if(bytesPerSample == 4)
	    {
	      for(u32 sampleIndex = 0; sampleIndex < sourceSampleCount; ++sampleIndex)
		{
		  r32 srcSample = 0;
		  CONVERT_SAMPLE(srcDataL, srcSample, r32, channelCount);
	      
		  *destSamplesL++ = srcSample;
		  *destSamplesR++ = srcSample;
		}
	    }
	  else
	    {
	      ASSERT(!"ERROR: unsupported sample size");
	    }
	}
      else if(channelCount == 2)
	{
	  void *srcDataR = (u8 *)sampleData + bytesPerSample;
	  if(bytesPerSample == 1)
	    {
	      for(u32 sampleIndex = 0; sampleIndex < sourceSampleCount; ++sampleIndex)
		{
		  r32 srcSampleL = 0, srcSampleR = 0;
		  CONVERT_SAMPLE(srcDataL, srcSampleL, u8, channelCount);
		  CONVERT_SAMPLE(srcDataR, srcSampleR, u8, channelCount);
	      
		  *destSamplesL++ = srcSampleL;
		  *destSamplesR++ = srcSampleR;
		}
	    }
	  else if(bytesPerSample == 2)
	    {
	      for(u32 sampleIndex = 0; sampleIndex < sourceSampleCount; ++sampleIndex)
		{
		  r32 srcSampleL = 0, srcSampleR = 0;
		  CONVERT_SAMPLE(srcDataL, srcSampleL, s16, channelCount);
		  CONVERT_SAMPLE(srcDataR, srcSampleR, s16, channelCount);
	      
		  *destSamplesL++ = srcSampleL;
		  *destSamplesR++ = srcSampleR;
		}
	    }
	  else if(bytesPerSample == 4)
	    {
	      for(u32 sampleIndex = 0; sampleIndex < sourceSampleCount; ++sampleIndex)
		{
		  r32 srcSampleL = 0, srcSampleR = 0;
		  CONVERT_SAMPLE(srcDataL, srcSampleL, r32, channelCount);
		  CONVERT_SAMPLE(srcDataR, srcSampleR, r32, channelCount);
	      
		  *destSamplesL++ = srcSampleL;
		  *destSamplesR++ = srcSampleR;
		}
	    }
	  else
	    {
	      ASSERT(!"ERROR: unsupported sample size");
	    }
	}
      else
	{
	  ASSERT(!"ERROR: unsupported channel count");
	}
#undef CONVERT_SAMPLE
  
      // NOTE: convert sample rate
      if(sampleRate != INTERNAL_SAMPLE_RATE)
	{
#if 1
	  // NOTE: stupid linear interpolation until the fft method (or another better one) works
	  Arena tempAllocator = arenaSubArena(loadAllocator, 2*sourceSampleCount*sizeof(r32) + 1);
	  r32 *tempL = arenaPushArray(&tempAllocator, sourceSampleCount, r32);
	  r32 *tempR = arenaPushArray(&tempAllocator, sourceSampleCount, r32);
	  for(u32 index = 0; index < sourceSampleCount; ++index)
	    {
	      tempL[index] = result.samples[0][index];
	      tempR[index] = result.samples[1][index];
	    }

	  result.samples[1] = result.samples[0] + paddedDestSampleCount;
	  r32 sampleRateRatio = 1.f/sampleRateConversionFactor;
	  for(u32 destIndex = 0; destIndex < result.sampleCount; ++destIndex)
	    {

	      r32 sourceOffset = destIndex*sampleRateRatio;
	      u32 sourceIndex = (u32)sourceOffset;
	      r32 sourceFrac = sourceOffset - sourceIndex;

	      ASSERT(sourceIndex < sourceSampleCount);
	      r32 sourceSample0 =
		lerp(tempL[sourceIndex], tempL[sourceIndex + 1], sourceFrac);
	      r32 sourceSample1 =
		lerp(tempR[sourceIndex], tempR[sourceIndex + 1], sourceFrac);
	      
	      result.samples[0][destIndex] = sourceSample0;
	      result.samples[1][destIndex] = sourceSample1;
	    }
	  for(u32 padIndex = result.sampleCount; padIndex < paddedDestSampleCount; ++padIndex)
	    {
	      result.samples[0][padIndex] = 0.f;
	      result.samples[1][padIndex] = 0.f;
	    }

	  arenaEndArena(loadAllocator, tempAllocator);
#else
	  // TODO: make this work (problem is speed, numerical accuracy)
	  usz tempBufferSize = 4*maxSampleCount*sizeof(c64);
	  TemporaryMemory tempBufferAllocator = arenaBeginTemporaryMemory(loadAllocator, tempBufferSize);
	  c64 *tempFFTBufferL = arenaPushArray((Arena *)&tempBufferAllocator, maxSampleCount, c64,
					       arenaFlagsZeroNoAlign());
	  c64 *tempFFTBufferR = arenaPushArray((Arena *)&tempBufferAllocator, maxSampleCount, c64,
					       arenaFlagsZeroNoAlign());

	  usz cztScratchSize = 16*maxSampleCount*sizeof(c64);
	  TemporaryMemory cztScratch = arenaBeginTemporaryMemory(loadAllocator, cztScratchSize);
	  
	  czt(tempFFTBufferL, result.samples[0], sourceSampleCount, (Arena *)&cztScratch);
	  arenaEnd((Arena *)&cztScratch);
	  
	  czt(tempFFTBufferR, result.samples[1], sourceSampleCount, (Arena *)&cztScratch);
	  arenaEnd((Arena *)&cztScratch);
      
	  if(sampleRate < INTERNAL_SAMPLE_RATE)
	    {
	      // NOTE: upsample

	      // NOTE: zero out unused frequencies
	      for(u32 index = sourceSampleCount/2 + 1; index < destSampleCount; ++index)
		{
		  tempFFTBufferL[index] = C64(0, 0);
		  tempFFTBufferR[index] = C64(0, 0);
		}

	      // NOTE: copy frequencies, preserving hermitian symmetry
	      c64 *midpointL = tempFFTBufferL + sourceSampleCount/2;
	      c64 *midpointR = tempFFTBufferR + sourceSampleCount/2;
	      c64 *destL = tempFFTBufferL + destSampleCount - sourceSampleCount/2;
	      c64 *destR = tempFFTBufferR + destSampleCount - sourceSampleCount/2;
	      for(u32 index = 0; index < sourceSampleCount/2; ++index)
		{
		  c64 coeffL = *midpointL;
		  c64 coeffR = *midpointR;
		  if(index == 0)
		    {		      
		      coeffL *= 0.5f;
		      coeffR *= 0.5f;		      
		    }
		  //coeffL *= sampleRateConversionFactor;
		  //coeffR *= sampleRateConversionFactor;

		  *destL++ = conjugateC64(coeffL);
		  *destR++ = conjugateC64(coeffR);

		  *midpointL-- = coeffL;
		  *midpointR-- = coeffR;		  
		}	      
	    }
	  else if(sampleRate > INTERNAL_SAMPLE_RATE)
	    {
	      // NOTE: downsample
	      c64 *writeL = tempFFTBufferL + result.sampleCount/2;
	      c64 *writeR = tempFFTBufferR + result.sampleCount/2;
	      c64 *readL = tempFFTBufferL + sourceSampleCount - result.sampleCount/2;
	      c64 *readR = tempFFTBufferR + sourceSampleCount - result.sampleCount/2;
	      for(u32 index = 0; index < result.sampleCount/2; ++index)
		{
		  c64 coeffL = *readL++;
		  c64 coeffR = *readR++;
		  if(index == 0)
		    {
		      coeffL = {0, 0};
		      coeffR = {0, 0};
		    }

		  *writeL++ = coeffL;
		  *writeR++ = coeffR;
		}
	    }

	  r32 *IFFTtempImL = arenaPushArray((Arena *)&tempBufferAllocator, maxSampleCount, r32,
					    arenaFlagsZeroNoAlign());
	  r32 *IFFTtempImR = arenaPushArray((Arena *)&tempBufferAllocator, maxSampleCount, r32,
					    arenaFlagsZeroNoAlign());
	  iczt(result.samples[0], IFFTtempImL, tempFFTBufferL, destSampleCount, (Arena *)&cztScratch);
	  arenaEnd((Arena *)&cztScratch);
	  iczt(result.samples[1], IFFTtempImR, tempFFTBufferR, destSampleCount, (Arena *)&cztScratch);
	  arenaEnd((Arena *)&cztScratch);
	  
	  //arenaEndArena(loadAllocator, tempAllocator);
	  arenaEndTemporaryMemory(&cztScratch);
	  arenaEndTemporaryMemory(&tempBufferAllocator);
#endif
	} 
      
      globalPlatform.freeFileMemory(readResult, loadAllocator);
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
      (void)compression;
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
getTextDim(LoadedFont *font, String8 text)
{
  v2 result = {};

  u8 *at = text.str;
  u64 textSize = text.size;
  //while(*at)
  for(u64 i = 0; i < textSize; ++i)
    {
      u8 c = *at;
      u32 glyphIndex = c - font->characterRange.min;
      ASSERT(glyphIndex < font->glyphCount);
      LoadedBitmap *glyph = font->glyphs + glyphIndex;
      result.y = MAX(result.y, (r32)glyph->height);

      ++at;
      //if(*at)
	{
	  result.x += (r32)getHorizontalAdvance(font, c, *at); 
	}      
    }
  
  return(result);
}

