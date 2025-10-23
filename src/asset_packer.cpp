#define _CRT_SECURE_NO_WARNINGS

#define HOST_LAYER
#include "context.h"
#include "types.h"
#include "utils.h"
#include "arena.h"
#include "strings.h"
#include "math.h"

// PLATFORM IMPLEMENATIONS
#include <math.h>
#include <string.h>

r32
gsSqrt(r32 num)
{
  return(sqrtf(num));
}

r32
gsCos(r32 num)
{
  return(cosf(num));
}

r32
gsSin(r32 num)
{
  return(sinf(num));
}

void
gsCopyMemory(void *dest, void *src, usz size)
{
  memcpy(dest, src, size);
}

void
gsSetMemory(void *dest, int value, usz size)
{
  memset(dest, value, size);
}

#define ARENA_MIN_ALLOCATION_SIZE KILOBYTES(64)

// FILE LOADERS
#if OS_WINDOWS
#include <windows.h>

static void*
allocateMemory(usz size)
{
  void *result = VirtualAlloc(0, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
  return(result);
}

static void
freeMemory(void *memory, usz size)
{
  VirtualFree(memory, size, MEM_RELEASE);
}

static Buffer
readEntireFile(Arena *arena, String8 path)
{
  usz fileSize = 0;
  u8 *fileContents = 0;

  HANDLE handle = CreateFileA((char*)path.str, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  if(handle != INVALID_HANDLE_VALUE)
    {
      LARGE_INTEGER size;
      if(GetFileSizeEx(handle, &size))
	{
	  fileSize = size.QuadPart;
	  fileContents = arenaPushArray(arena, fileSize + 1, u8);

	  u8 *dest = fileContents;
	  usz totalBytesToRead = fileSize;	  
	  while(totalBytesToRead)
	    {
	      u32 bytesToRead = (u32)totalBytesToRead;
	      DWORD bytesRead;
	      if(ReadFile(handle, dest, bytesToRead, &bytesRead, 0))
		{
		  dest += bytesRead;
		  totalBytesToRead -= bytesRead;
		}
	      else
		{
		  fileSize = 0;
		  fileContents = 0;
		  arenaPopSize(arena, fileSize + 1);
		  break;
		}
	    }
	  fileContents[fileSize] = 0; // NOTE: null termination
	}
      CloseHandle(handle);
    }
  
  Buffer result = {};
  result.size = fileSize;
  result.contents = fileContents;
  return(result);
}

static void
writeEntireFile(String8 path, Buffer file)
{
  HANDLE handle = CreateFileA((char*)path.str, GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, 0, 0);
  if(handle != INVALID_HANDLE_VALUE)
    {
      void *fileMemory = file.contents;
      usz bytesRemaining = file.size;
      while(bytesRemaining)
	{
	  u32 bytesToWrite = (u32)bytesRemaining;
	  if(WriteFile(handle, fileMemory, bytesToWrite, 0, 0))
	    {
	      bytesRemaining -= bytesToWrite;
	      fileMemory = (u8*)fileMemory + bytesToWrite;
	    }
	  else
	    {
	      ASSERT(0);
	      break;
	    }
	}
      CloseHandle(handle);
    }
}

#elif OS_LINUX || OS_MAC

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

static void*
allocateMemory(usz size)
{
  void *result = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  return(result);
}

static void
freeMemory(void *memory, usz size)
{
  munmap(memory, size);
}


typedef ssize_t ssz;
#define PLATFORM_MAX_READ_SIZE 0x7FFFF000

static Buffer
readEntireFile(Arena *arena, String8 path)
{
  Buffer result = {};

  int fileHandle = open((char*)path.str, O_RDONLY);
  if(fileHandle != -1)
    {
      struct stat fileStatus;
      if(fstat(fileHandle, &fileStatus) != -1)
	{
	  result.size = fileStatus.st_size;
	  result.contents = arenaPushSize(arena, result.size + 1);
	  
	  u8 *dest = result.contents;
	  usz totalBytesToRead = result.size;
	  usz bytesToRead = MIN(totalBytesToRead, PLATFORM_MAX_READ_SIZE);
	  while(totalBytesToRead)
	    {
	      ssz bytesRead = read(fileHandle, dest, bytesToRead);
	      if(bytesRead == bytesToRead)
		{
		  dest += bytesRead;
		  totalBytesToRead -= bytesRead;
		}
	      else
		{
		  result.contents = 0;
		  result.size = 0;
		  break;
		}

	      if(result.contents)
		{
		  result.contents[result.size] = 0; // NOTE: null termination
		}
	    }
	}

      close(fileHandle);
    }

  return(result);
}

static void
writeEntireFile(String8 path, Buffer file)
{
  int fileHandle = open((char*)path.str, O_CREAT | O_WRONLY | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if(fileHandle != -1)
    {
      void *fileMemory = file.contents;
      usz bytesRemaining = file.size;
      while(bytesRemaining)
	{
	  u32 bytesToWrite = safeTruncateU64(bytesRemaining);
	  ssize_t bytesWritten = write(fileHandle, fileMemory, bytesToWrite);
	  if(bytesWritten == -1)
	    {
	      break;
	    }
	  else
	    {
	      bytesRemaining -= bytesWritten;
	      fileMemory = (u8 *)fileMemory + bytesWritten;
	    }
	}

      close(fileHandle);
    }
}

#else
#  error unsupported OS
#endif

Arena*
gsArenaAcquire(usz size)
{
  usz allocSize = MAX(size, ARENA_MIN_ALLOCATION_SIZE);
  void *base = allocateMemory(allocSize);
  Arena *result = (Arena*)base;
  result->current = result;
  result->prev = 0;
  result->base = 0;
  result->capacity = allocSize;
  result->pos = ARENA_HEADER_SIZE;

  return(result);
}

void
gsArenaDiscard(Arena *arena)
{
  usz size = arena->capacity;
  freeMemory((void*)arena, size);
}

#define FOURCC(str) ((u32)((str[0] << 0*8) | (str[1] << 1*8) | (str[2] << 2*8) | (str[3] << 3*8)))

#pragma pack(push, 1)

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
};

struct BitmapHeaderWithMasks
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

struct BitmapHeaderV5
{
  u16 signature;
  u32 fileSize;
  u32 _reserved;
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
  u32 alphaMask;

  u32 csType;
  v3u32 cieXYZRed;
  v3u32 cieXYZGreen;
  v3u32 cieXYZBlue;

  u32 gammaRed;
  u32 gammaGreen;
  u32 gammaBlue;

  u32 intent;
  u32 profileData;
  u32 profileSize;
  u32 _reserved2;
};

#pragma pack(pop)

static LoadedBitmap*
loadBitmap(Arena *arena, String8 path, v2 alignment)
{
  LoadedBitmap *result = 0;
  TemporaryMemory scratch = arenaGetScratch(&arena, 1);

  Buffer file = readEntireFile(scratch.arena, path);
  if(file.contents)
    {
      BitmapHeaderWithMasks *header = (BitmapHeaderWithMasks*)file.contents;
      u32 redMask = header->redMask;
      u32 greenMask = header->greenMask;
      u32 blueMask = header->blueMask;
      u32 alphaMask = ~(redMask | greenMask | blueMask);

      u32 alphaShiftDown = alphaMask ? lowestOrderBit(alphaMask) : 24;
      u32 redShiftDown = redMask ? lowestOrderBit(redMask) : 16;
      u32 greenShiftDown = greenMask ? lowestOrderBit(greenMask) : 8;
      u32 blueShiftDown = blueMask ? lowestOrderBit(blueMask) : 0;
  
      u32 width = header->width;
      u32 height = header->height;
      u32 *srcPixels = (u32*)(file.contents + header->dataOffset);
      u32 *destPixels = arenaPushArray(arena, width * height, u32);
      {
	u32 *src = srcPixels;
	u32 *dest = destPixels;
	for(u32 j = 0; j < height; ++j)
	  {
	    for(u32 i = 0; i < width; ++i)
	      {
		u32	srcPixel = *src++;
		u32	red	 = (srcPixel & redMask) >> redShiftDown;
		u32	green	 = (srcPixel & greenMask) >> greenShiftDown;
		u32	blue	 = (srcPixel & blueMask) >> blueShiftDown;
		u32	alpha	 = (srcPixel & alphaMask) >> alphaShiftDown;
	    
		*dest++ = ((alpha << 24) |
			   (red   << 16) |
			   (green <<  8) |
			   (blue  <<  0));
	      }
	  }
      }
      
      result = arenaPushStruct(arena, LoadedBitmap);
      result->width = width;
      result->height = height;
      result->stride = result->width * sizeof(u32);
      result->alignPercentage = alignment;
      result->pixels = destPixels;
    }
  
  arenaReleaseScratch(scratch);
  return(result);
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
static LoadedBitmap*
loadPNG(Arena *arena, String8 path, v2 alignment)
{
  stbi_set_flip_vertically_on_load(1);

  int width, height, nChannels;
  u8 *data = stbi_load((char*)path.str, &width, &height, &nChannels, 4);
  ASSERT(nChannels == 4);

  LoadedBitmap *result = arenaPushStruct(arena, LoadedBitmap);
  result->width = width;
  result->height = height;
  result->stride = result->width * sizeof(u32);
  result->pixels = (u32*)data;
  result->alignPercentage = alignment;
  return(result);
}

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
static void
writePNG(LoadedBitmap *image, String8 destPath)
{
  stbi_flip_vertically_on_write(1);

  int width = image->width;
  int height = image->height;
  int stride = image->stride;
  const void *data = (const void*)image->pixels;
  int result = stbi_write_png((char*)destPath.str, width, height, 4, data, stride);
}

static LoadedBitmap*
makeWhiteBitmap(Arena *arena, u32 width, u32 height)
{
  LoadedBitmap *result = arenaPushStruct(arena, LoadedBitmap);
  result->width = width;
  result->height = height;
  result->stride = result->width * sizeof(u32);
  result->pixels = arenaPushArray(arena, result->width * result->height, u32);

  u8 *destRow = (u8*)result->pixels;
  for(u32 y = 0; y < height; ++y)
    {
      u32 *dest = (u32*)destRow;
      for(u32 x = 0; x < width; ++x)
	{
	  *dest++ = 0xFFFFFFFF;
	}
      destRow += result->stride;
    }

  return(result);
}

// ASSETS HELPERS
struct LooseBitmap
{
  LooseBitmap *next;

  // NOTE: specified at load
  LoadedBitmap *bitmap;
  String8 name;

  r32 advance;

  // NOTE: calculated
  s32 atlasOffsetX;
  s32 atlasOffsetY;
};

struct LooseFont
{
  LooseFont *next;

  String8 name;

  RangeU32 range;
  r32 verticalAdvance;
};

struct LooseAssets
{
  Arena *arena;

  LooseBitmap *firstBitmap;
  LooseBitmap *lastBitmap;
  u64 bitmapCount;

  LooseFont *firstFont;
  LooseFont *lastFont;
  u64 fontCount;
};

static LooseBitmap*
looseAssetPushBitmap(LooseAssets *looseAssets, LoadedBitmap *bitmap, String8 name)
{
  LooseBitmap *looseBitmap = arenaPushStruct(looseAssets->arena, LooseBitmap);
  looseBitmap->bitmap = bitmap;
  looseBitmap->name = arenaPushString(looseAssets->arena, name);

  QUEUE_PUSH(looseAssets->firstBitmap, looseAssets->lastBitmap, looseBitmap);
  ++looseAssets->bitmapCount;

  return(looseBitmap);
}

static LooseBitmap*
looseAssetPushBitmap(LooseAssets *looseAssets, LoadedBitmap *bitmap, char *fmt, ...)
{
  va_list vaArgs;
  va_start(vaArgs, fmt);

  TemporaryMemory scratch = arenaGetScratch(&looseAssets->arena, 1);

  String8 name = arenaPushStringFormatV(scratch.arena, fmt, vaArgs);
  LooseBitmap *result = looseAssetPushBitmap(looseAssets, bitmap, name);

  arenaReleaseScratch(scratch);

  va_end(vaArgs);

  return(result);
}

static LooseBitmap*
looseAssetPushBitmap(LooseAssets *looseAssets, String8 path, String8 name, v2 alignment = V2(0, 0))
{  
  //LoadedBitmap *bitmap = loadBitmap(looseAssets->arena, path, alignment);
  LoadedBitmap *bitmap = loadPNG(looseAssets->arena, path, alignment);
  LooseBitmap *result = looseAssetPushBitmap(looseAssets, bitmap, name);
  return(result);
}

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

static void
looseAssetPushFont(LooseAssets *looseAssets, String8 path, String8 name, RangeU32 cpRange, r32 ptSize)
{
  TemporaryMemory scratch = arenaGetScratch(&looseAssets->arena, 1);

  Buffer fontFile = readEntireFile(scratch.arena, path);
  if(fontFile.contents)
    {
      stbtt_fontinfo stbFontInfo;
      if(stbtt_InitFont(&stbFontInfo, fontFile.contents, stbtt_GetFontOffsetForIndex(fontFile.contents, 0)))
	{
	  int ascent, descent, lineGap;
	  stbtt_GetFontVMetrics(&stbFontInfo, &ascent, &descent, &lineGap);

	  r32 fontScale = stbtt_ScaleForPixelHeight(&stbFontInfo, ptSize);

	  for(u32 charIdx = cpRange.min; charIdx < cpRange.max; ++charIdx)
	    {	      
	      int width, height, xOffset, yOffset;
	      u8 *glyphSrc = stbtt_GetCodepointBitmap(&stbFontInfo, 0, fontScale, charIdx,
						      &width, &height, &xOffset, &yOffset);

	      int advanceWidth, leftSideBearing;
	      stbtt_GetCodepointHMetrics(&stbFontInfo, charIdx, &advanceWidth, &leftSideBearing);

	      LoadedBitmap *bitmap = arenaPushStruct(looseAssets->arena, LoadedBitmap);	      
	      bitmap->width = width;
	      bitmap->height = height;
	      bitmap->stride = bitmap->width * sizeof(u32);
	      bitmap->alignPercentage.x = 0.f;
	      bitmap->alignPercentage.y = height ? (r32)(height + yOffset)/(r32)height : 0.f;
	      bitmap->pixels = arenaPushArray(looseAssets->arena, bitmap->width * bitmap->height, u32);
	      {
		u8 *srcRow = glyphSrc + width * (height - 1);
		u8 *destRow = (u8*)bitmap->pixels;
		for(int y = 0; y < height; ++y)
		  {
		    u8 *src = srcRow;
		    u32 *dest = (u32*)destRow;
		    for(int x = 0; x < width; ++x)
		      {
			u8 alpha = *src++;
			*dest++ = ((alpha << 24) |
				   (alpha << 16) |
				   (alpha <<  8) |
				   (alpha <<  0));
		      }
		    
		    srcRow -= width;
		    destRow += width * sizeof(u32);
		  }
	      }

	      LooseBitmap *looseBitmap =
		looseAssetPushBitmap(looseAssets, bitmap, "%.*s_%u",
				     (int)name.size, name.str, charIdx);
	      looseBitmap->advance = fontScale * (r32)advanceWidth;

	      stbtt_FreeBitmap(glyphSrc, 0);
	    }

	  LooseFont *looseFont = arenaPushStruct(looseAssets->arena, LooseFont);
	  looseFont->verticalAdvance = fontScale * (r32)(ascent - descent + lineGap);
	  looseFont->name = arenaPushString(looseAssets->arena, name);
	  looseFont->range = cpRange;
	  QUEUE_PUSH(looseAssets->firstFont, looseAssets->lastFont, looseFont);
	  ++looseAssets->fontCount;
	}
    }

  arenaReleaseScratch(scratch);
}

struct AtlasRegionNode
{
  AtlasRegionNode *parent;
  AtlasRegionNode *children[Corner_Count];  
  v2s32 maxFreeDim[Corner_Count];
  // AtlasRegionNode *children[Axis2D_Count];
  // v2s32 maxFreeDim[Axis2D_Count];

  Axis2D splitAxis;
  b32 taken;
  u32 allocatedDescendantCount;
};

struct Atlas
{
  AtlasRegionNode *root;
  v2s32 rootDim;
};

#if 0
static Rect2S32
atlasRegionAlloc(Arena *arena, Atlas *atlas, v2s32 neededDim)
{
  v2s32 regionMin = V2S32(0, 0);
  v2s32 regionDim = V2S32(0, 0);
  
  // NOTE: find node to insert into
  AtlasRegionNode *node = 0;
  for(AtlasReginoNode *n = atlas->root, *next = 0; n; n = next, next = 0)
    {
      v2s32 availableDim = n->dim;
      for(Corner corner = (Corner)0; corner < Corner_Count; corner = (Corner)(corner + 1))
	{
	  if(!n->children[corner])
	    {
	      n->children[corner] = arenaPushStruct(arena, AtlasRegionNode);
	      n->children[corner]->dim = neededDim;
	      n->children[corner]->parent = n;
	    }
	}
    }
  
  Rect2S32 result = {};
  result.min = regionMin;
  result.max = regionMin + regionDim;
  return(result);
}
#else
static Rect2S32
atlasRegionAlloc(Arena *arena, Atlas *atlas, v2s32 neededDim)
{
  v2s32 regionMin = V2S32(0, 0);
  v2s32 regionDim = V2S32(0, 0);
  v2s32 supportedDim = atlas->rootDim;
  Corner nodeCorner = Corner_invalid;
  AtlasRegionNode *node = 0;
  for(AtlasRegionNode *n = atlas->root, *next = 0; n; n = next, next = 0)
    {
      if(n->taken) break;

      b32 canBeAllocated = (n->allocatedDescendantCount == 0);
      if(canBeAllocated) regionDim = supportedDim;

      v2s32 childDim = supportedDim / 2;
      AtlasRegionNode *bestChild = 0;
      if(childDim.x >= neededDim.x && childDim.y >= neededDim.y)
	{
	  for(Corner corner = (Corner)0; corner < Corner_Count; corner = (Corner)(corner + 1))
	    {
	      if(n->children[corner] == 0)
		{
		  n->children[corner] = arenaPushStruct(arena, AtlasRegionNode);
		  n->children[corner]->parent = n;
		  n->children[corner]->maxFreeDim[0] =
		    n->children[corner]->maxFreeDim[1] =
		    n->children[corner]->maxFreeDim[2] =
		    n->children[corner]->maxFreeDim[3] = childDim / 2;
		}
	      if(n->maxFreeDim[corner].x >= neededDim.x && n->maxFreeDim[corner].y >= neededDim.y)
		{
		  bestChild = n->children[corner];
		  nodeCorner = corner;
		  regionMin += hadamard(vertexFromCorner(corner), childDim);
		  break;
		}
	    }
	}

      if(canBeAllocated && !bestChild)
	{
	  node = n;
	}
      else
	{
	  next = bestChild;
	  supportedDim = childDim;
	}
    }

  if(node != 0 && nodeCorner != Corner_invalid)
    {
      node->taken = 1;
      if(node->parent)
	{
	  node->parent->maxFreeDim[nodeCorner] = V2S32(0, 0);
	}
      for(AtlasRegionNode *p = node->parent; p; p = p->parent)
	{
	  ++p->allocatedDescendantCount;
	  
	  AtlasRegionNode *parent = p->parent;
	  if(parent)
	    {
	      Corner pCorner = (p == parent->children[Corner_00] ? Corner_00 :
				p == parent->children[Corner_01] ? Corner_01 :
				p == parent->children[Corner_10] ? Corner_10 :
				p == parent->children[Corner_11] ? Corner_11 :
				Corner_invalid);
	      ASSERT(pCorner != Corner_invalid);

	      parent->maxFreeDim[pCorner].x = MAX(MAX(p->maxFreeDim[Corner_00].x,
						      p->maxFreeDim[Corner_01].x),
						  MAX(p->maxFreeDim[Corner_10].x,
						      p->maxFreeDim[Corner_11].x));
	      parent->maxFreeDim[pCorner].y = MAX(MAX(p->maxFreeDim[Corner_00].y,
						      p->maxFreeDim[Corner_01].y),
						  MAX(p->maxFreeDim[Corner_10].y,
						      p->maxFreeDim[Corner_11].y));
	    }
	}
    }

  Rect2S32 result = {};
  result.min = regionMin;
  result.max = regionMin + regionDim;
  return(result);
}
#endif

#define BITMAP_XLIST\
  X(editorSkin, "PNG/TREE.png", V2(0, 0))\
  X(grainViewBackground, "PNG/GREENFRAME_RECTANGLE.png", V2(0, 0))\
  X(grainViewOutline, "PNG/GREENFRAME.png", V2(0, 0))\
  X(levelBar, "PNG/LEVELBAR.png", V2(0, 0))\
  X(levelFader, "PNG/LEVELBAR_SLIDINGLEVER.png", V2(0, -0.416f))\
  X(pomegranateKnob, "PNG/POMEGRANATE_BUTTON.png", V2(0, -0.03f))\
  X(pomegranateKnobLabel, "PNG/POMEGRANATE_BUTTON_WHITEMARKERS.png", V2(0, 0))\
  X(halfPomegranateKnob, "PNG/HALFPOMEGRANATE_BUTTON.png", V2(-0.005f, -0.035f))\
  X(halfPomegranateKnobLabel, "PNG/HALFPOMEGRANATE_BUTTON_WHITEMARKERS.png", V2(0, 0))\
  X(densityKnob, "PNG/DENSITYPOMEGRANATE_BUTTON.png", V2(0.01f, -0.022f))\
  X(densityKnobShadow, "PNG/DENSITYPOMEGRANATE_BUTTON_SHADOW.png", V2(0, 0))\
  X(densityKnobLabel, "PNG/DENSITYPOMEGRANATE_BUTTON_WHITEMARKERS.png", V2(0, 0))
  
int
main(int argc, char **argv)
{
  TemporaryMemory scratch = arenaGetScratch(0, 0);

  Arena *arena = gsArenaAcquire(MEGABYTES(256));
  LooseAssets *looseAssets = arenaPushStruct(arena, LooseAssets);
  looseAssets->arena = arena;

#define DATA_PATH "../data/"
#define SRC_PATH "../src/"

  String8List pluginAssetXList = {0};
  stringListPush(looseAssets->arena, &pluginAssetXList,
		 STR8_LIT("#define PLUGIN_ASSET_XLIST\\\n"));

  LoadedBitmap *whiteBitmap = makeWhiteBitmap(looseAssets->arena, 16, 16);  
  stringListPush(looseAssets->arena, &pluginAssetXList, STR8_LIT("  X(null)\\\n"));

#define X(name, path, alignment)							\
  looseAssetPushBitmap(looseAssets, STR8_LIT(DATA_PATH path), STR8_LIT(#name), alignment); \
  stringListPushFormat(looseAssets->arena, &pluginAssetXList, "  X(%s)\\\n", #name);
  BITMAP_XLIST;
#undef X

  looseAssetPushBitmap(looseAssets, whiteBitmap, STR8_LIT("null"));

  RangeU32 cpRange = {32, 127};
  r32 ptSize = 32.f;
  looseAssetPushFont(looseAssets, STR8_LIT(DATA_PATH"FONT/AGENCYB.ttf"),
		     STR8_LIT("AgencyBold"), cpRange, ptSize);

  // NOTE: compute positions in the atlas
  // s32 atlasWidth = 8656;//7696;//8704;//7688;
  // s32 atlasHeight = 4328;
  s32 atlasWidth = 3842;
  s32 atlasHeight = 4362;
#if 0
  Atlas *atlas = arenaPushStruct(arena, Atlas);
  atlas->rootDim = V2S32(atlasWidth, atlasHeight);
  atlas->root = arenaPushStruct(arena, AtlasRegionNode);
  atlas->root->maxFreeDim[Corner_00] =
    atlas->root->maxFreeDim[Corner_01] =
    atlas->root->maxFreeDim[Corner_10] =
    atlas->root->maxFreeDim[Corner_11] = atlas->rootDim / 2;
  for(LooseBitmap *bitmap = looseAssets->firstBitmap; bitmap; bitmap = bitmap->next)
    {
      v2s32 bitmapDim = V2S32(bitmap->bitmap->width, bitmap->bitmap->height);
      Rect2S32 region = atlasRegionAlloc(arena, atlas, bitmapDim + V2S32(1, 1));
      bitmap->atlasOffsetX = region.min.x;
      bitmap->atlasOffsetY = region.min.y;
    }

#else
#  if 1

  struct RegionNode
  {
    RegionNode *next;
    Rect2S32 rect;
  };

  struct PackAtlas
  {
    Arena *arena;
    RegionNode *first;
    RegionNode *last;

    RegionNode *firstFree;
    RegionNode *lastFree;
    
    v2s32 dim;
  };

  Arena *packArena = gsArenaAcquire(MEGABYTES(1));
  PackAtlas *packAtlas = arenaPushStruct(packArena, PackAtlas);
  packAtlas->arena = packArena;
  packAtlas->dim = V2S32(atlasWidth, atlasHeight);
  
  RegionNode *atlasRegionNode = arenaPushStruct(packArena, RegionNode);
  atlasRegionNode->rect.min = V2S32(0, 0);
  atlasRegionNode->rect.max = packAtlas->dim;
  QUEUE_PUSH(packAtlas->firstFree, packAtlas->lastFree, atlasRegionNode);

  s32 minDimX = 16;
  s32 minDimY = 16;

  for(LooseBitmap *bitmap = looseAssets->firstBitmap; bitmap; bitmap = bitmap->next)
    {
      v2s32 insertDim = V2S32(bitmap->bitmap->width + 1, bitmap->bitmap->height + 1);

      v2s32 insertPos = V2S32(-1, -1);
      b32 inserted = 0;
      while(!inserted)
	{
	  RegionNode *freeRegion = packAtlas->firstFree;	  
	  ASSERT(freeRegion);
	  QUEUE_POP(packAtlas->firstFree, packAtlas->lastFree);
	  v2s32 freeRegionDim = getDim(freeRegion->rect);

	  if(freeRegionDim.x >= insertDim.x && freeRegionDim.y >= insertDim.y)
	    {
	      RegionNode *newRegion = freeRegion;
	      newRegion->rect.max = newRegion->rect.min + insertDim;
	      insertPos = newRegion->rect.min;
	      QUEUE_PUSH(packAtlas->first, packAtlas->last, newRegion);

	      v2s32 bottomRightDim = V2S32(freeRegionDim.x - insertDim.x, insertDim.y);
	      if(bottomRightDim.x >= minDimX && bottomRightDim.y >= minDimY)
		{
		  RegionNode *bottomRightFree = arenaPushStruct(packAtlas->arena, RegionNode);
		  bottomRightFree->rect.min = freeRegion->rect.min + V2S32(insertDim.x, 0);
		  bottomRightFree->rect.max = bottomRightFree->rect.min + bottomRightDim;
		  QUEUE_PUSH(packAtlas->firstFree, packAtlas->lastFree, bottomRightFree);
		}

	      v2s32 topLeftDim = V2S32(insertDim.x, freeRegionDim.y - insertDim.y);
	      if(topLeftDim.x >= insertDim.x && topLeftDim.y >= insertDim.y)
		{
		  RegionNode *topLeftFree = arenaPushStruct(packAtlas->arena, RegionNode);
		  topLeftFree->rect.min = freeRegion->rect.min + V2S32(0, insertDim.y);
		  topLeftFree->rect.max = topLeftFree->rect.min + topLeftDim;
		  QUEUE_PUSH(packAtlas->firstFree, packAtlas->lastFree, topLeftFree);
		}

	      v2s32 topRightDim = freeRegionDim - insertDim;
	      if(topRightDim.x >= minDimX && topRightDim.y >= minDimY)
		{
		  RegionNode *topRightFree = arenaPushStruct(packAtlas->arena, RegionNode);
		  topRightFree->rect.min = freeRegion->rect.min + insertDim;
		  topRightFree->rect.max = topRightFree->rect.min + topRightDim;	      	      
		  QUEUE_PUSH(packAtlas->firstFree, packAtlas->lastFree, topRightFree);
		}
	      
	      inserted = 1;
	    }
	  else
	    {
	      QUEUE_PUSH(packAtlas->firstFree, packAtlas->lastFree, freeRegion);
	    }
	}

      bitmap->atlasOffsetX = insertPos.x;
      bitmap->atlasOffsetY = insertPos.y;
    }
    
#  else
  s32 layoutX = 0;
  s32 layoutY = 0;
  s32 rowMaxHeight = 0;
  s32 spaceX = 0;
  u32 growCount = 0;
  u32 maxGrowCount = 10;
  for(LooseBitmap *bitmap = looseAssets->firstBitmap; bitmap; bitmap = bitmap->next)
    {
      b32 inserted = 0;
      while(!inserted)
	{
	  ASSERT(growCount <= maxGrowCount);

	  s32 maxX = layoutX + bitmap->bitmap->width + 1;
	  s32 maxY = layoutY + bitmap->bitmap->height + 1;
	  b32 fits = (maxX <= atlasWidth && maxY <= atlasHeight);
	  if(fits)
	    {
	      bitmap->atlasOffsetX = layoutX;
	      bitmap->atlasOffsetY = layoutY;

	      layoutX += bitmap->bitmap->width + 1;
	      rowMaxHeight = MAX(rowMaxHeight, (s32)(bitmap->bitmap->height + 1));

	      inserted = 1;
	    }
	  else
	    {
	      if(maxY > atlasHeight)
		{
		  // NOTE: overflowing y. grow atlas
		  ++growCount;
		  if(growCount & 1)
		    {
		      // NOTE: grow x
		      spaceX = atlasWidth;
		      layoutX = atlasWidth;
		      layoutY = 0;
		      rowMaxHeight = 0;
		      atlasWidth *= 2;
		    }
		  else
		    {
		      // NOTE: grow y
		      spaceX = 0;
		      layoutX = 0;
		      layoutY = atlasHeight;
		      rowMaxHeight = 0;
		      atlasHeight *= 2;
		    }
		}
	      else
		{
		  // NOTE: overflowing x. go to next row
		  layoutY += rowMaxHeight;
		  layoutX = spaceX;
		  rowMaxHeight = 0;
		}
	    }
	}
    }
  //atlasHeight = layoutY + rowMaxHeight + 1;
#  endif
#endif

  // NOTE: copy into the atlas
  u32 *atlasPixels = arenaPushArray(arena, atlasWidth * atlasHeight, u32);
  for(LooseBitmap *bitmap = looseAssets->firstBitmap; bitmap; bitmap = bitmap->next)
    {
      u8 *srcRow = (u8*)bitmap->bitmap->pixels;
      u8 *destRow = (u8*)(atlasPixels + bitmap->atlasOffsetY*atlasWidth + bitmap->atlasOffsetX);
      for(u32 j = 0; j < bitmap->bitmap->height; ++j)
	{
	  u32 *srcPixels = (u32*)srcRow;
	  u32 *destPixels = (u32*)destRow;
	  for(u32 i = 0; i < bitmap->bitmap->width; ++i)
	    {
	      *destPixels++ = *srcPixels++;
	    }

	  srcRow += bitmap->bitmap->stride;
	  destRow += atlasWidth * sizeof(u32);
	}
    }

  Arena *writeArena = gsArenaAcquire(MEGABYTES(512));

  // NOTE: generate code for asset names and associated uv rects
  String8List assetEnumList = {};
  String8List assetRectList = {};
  String8List assetFontList = {};

  stringListPush(writeArena, &assetEnumList, STR8_LIT("\nenum PluginAssets\n{\n"));
  stringListPush(writeArena, &assetRectList,
		 STR8_LIT("\nstatic PluginAsset globalPluginAssets[PluginAsset_Count] =\n{\n"));
  for(LooseBitmap *bitmap = looseAssets->firstBitmap; bitmap; bitmap = bitmap->next)
    {
      stringListPushFormat(writeArena, &assetEnumList, "  PluginAsset_%.*s,\n",
			   (int)bitmap->name.size, bitmap->name.str);

      r32 uvMinX = (r32)bitmap->atlasOffsetX/(r32)atlasWidth;
      r32 uvMinY = (r32)bitmap->atlasOffsetY/(r32)atlasHeight;
      r32 uvMaxX = (r32)(bitmap->atlasOffsetX + bitmap->bitmap->width)/(r32)atlasWidth;
      r32 uvMaxY = (r32)(bitmap->atlasOffsetY + bitmap->bitmap->height)/(r32)atlasHeight;
      stringListPushFormat(writeArena, &assetRectList,
			   "  { // %.*s\n"
			   "    {%ff, %ff},\n"
			   "    {%ff, %ff},\n"
			   "    {%ff, %ff, %ff, %ff},\n"
			   "    %ff,\n"
			   "  },\n",
			   (int)bitmap->name.size, bitmap->name.str,
			   bitmap->bitmap->alignPercentage.x, bitmap->bitmap->alignPercentage.y,
			   (r32)bitmap->bitmap->width, (r32)bitmap->bitmap->height,
			   uvMinX, uvMinY, uvMaxX, uvMaxY,
			   bitmap->advance);
    }
  stringListPush(writeArena, &assetEnumList, STR8_LIT("  PluginAsset_Count,\n};\n"));
  stringListPush(writeArena, &assetRectList, STR8_LIT("};\n"));
  
  for(LooseFont *font = looseAssets->firstFont; font; font = font->next)
    {
      stringListPushFormat(writeArena, &assetFontList, "\nstatic LoadedFont font%.*s = {\n",
			   (int)font->name.size, font->name.str);
      stringListPushFormat(writeArena, &assetFontList, "  {%u, %u},\n",
			   font->range.min, font->range.max);
      stringListPushFormat(writeArena, &assetFontList, "  %ff,\n",
			   font->verticalAdvance);
      stringListPushFormat(writeArena, &assetFontList, "  PLUGIN_ASSET(%.*s_%u),\n",
			   (int)font->name.size, font->name.str, font->range.min);
      stringListPush(writeArena, &assetFontList, STR8_LIT("};\n"));      
    }

  String8 pluginAssetXListString = stringListJoin(writeArena, &pluginAssetXList);
  String8 assetEnumString = stringListJoin(writeArena, &assetEnumList);
  String8 assetRectString = stringListJoin(writeArena, &assetRectList);
  String8 assetFontString = stringListJoin(writeArena, &assetFontList);
  String8 generatedCodeString =
    concatenateStrings(writeArena, pluginAssetXListString,
		       concatenateStrings(writeArena, assetEnumString,
					  concatenateStrings(writeArena, assetRectString, assetFontString)));
  
  Buffer generatedCode = {};
  generatedCode.contents = generatedCodeString.str;
  generatedCode.size = generatedCodeString.size;

  writeEntireFile(STR8_LIT(SRC_PATH"plugin_assets.generated.h"), generatedCode);

  arenaEnd(writeArena);

  // DEBUG: write atlas to file
#if 1
  LoadedBitmap *atlasImage = arenaPushStruct(writeArena, LoadedBitmap);
  atlasImage->width = atlasWidth;
  atlasImage->height = atlasHeight;
  atlasImage->stride = atlasImage->width * sizeof(u32);
  atlasImage->pixels = atlasPixels;

  writePNG(atlasImage, STR8_LIT(DATA_PATH"test_atlas.png"));
#else
  // TODO: this header masks business is janky as hell
  BitmapHeaderV5 *header = arenaPushStruct(writeArena, BitmapHeaderV5);
  header->signature = FOURCC("BM  ");
  header->fileSize = sizeof(BitmapHeaderV5) + atlasWidth * atlasHeight * sizeof(u32);
  header->dataOffset = sizeof(BitmapHeaderV5);
  header->headerSize = sizeof(BitmapHeaderV5) - 14;
  header->width = atlasWidth;
  header->height = atlasHeight;
  header->planes = 1;
  header->bitsPerPixel = 8 * sizeof(u32);
  header->compression = 0;
  header->imageSize = header->width * header->height * sizeof(u32);
  header->redMask = 0xFF0000;
  header->greenMask = 0xFF00;
  header->blueMask = 0xFF;
  header->alphaMask = 0xFF000000;
  header->csType = FOURCC(" NIW"); // NOTE: "WIN " byte-order swapped
  header->intent = 2; // NOTE: "graphics"  
  
  u32 *pixels = arenaPushArray(writeArena, atlasWidth * atlasHeight, u32);
  for(s32 i = 0; i < atlasWidth * atlasHeight; ++i)
    {
      pixels[i] = atlasPixels[i];
    }

  Buffer file = {};
  file.size = header->fileSize;
  file.contents = (u8*)header;
  writeEntireFile(STR8_LIT(DATA_PATH"test_atlas.bmp"), file);
#endif

  arenaReleaseScratch(scratch);
  return(0);
}
