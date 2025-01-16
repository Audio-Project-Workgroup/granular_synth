#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t   usz;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
//typedef ssize_t  ssz;

typedef float    r32;
typedef double   r64;

#define U8_MAX  (0xFF)
#define U16_MAX (0xFFFF)
#define U32_MAX (0xFFFFFFFF)
#define U64_MAX (0xFFFFFFFFFFFFFFFF)

#define S8_MAX  (0x7F)
#define S16_MAX (0x7FFF)
#define S32_MAX (0x7FFFFFFF)
#define S64_MAX (0x7FFFFFFFFFFFFFFF)

union v2
{
  struct
  {
    r32 x, y;
  };
  
  r32 E[2];
};

union v3
{
  struct
  {
    union
    {
      struct
      {
	r32 x, y;
      };
      
      v2 xy;
    };
    
    r32 z;
  };
  
  struct
  {
    r32 r, g, b;
  };
  
  r32 E[3];
};

union v4
{
  struct
  {
    union
    {
      struct
      {
	r32 x, y, z;
      };
      
      v3 xyz;
    };

    r32 w;
  };

  struct
  {
    union
    {
      struct
      {
	r32 r, g, b;
      };
      
      v3 rgb;
    };

    r32 a;
  };

  r32 E[4];
};

union RangeU32
{
  struct
  {
    u32 min, max;
  };
  
  u32 E[2];
};

struct Rect2
{
  v2 min, max;  
};

struct LoadedSound
{
  u32 sampleCount;
  u32 channelCount;
  
  s16 *samples[2]; // TODO: support other formats
};

struct LoadedBitmap
{
  u32 width;
  u32 height;
  u32 stride;
  v2 alignPercentage;

  u32 *pixels;
  u32 glHandle;
};

struct LoadedFont
{
  RangeU32 characterRange;

  r32 verticalAdvance; 

  u32 glyphCount;
  LoadedBitmap *glyphs;
  r32 *horizontalAdvance;
};
