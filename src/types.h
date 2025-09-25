#include <stdint.h>
//#include <inttypes.h>
#include <float.h>
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
typedef s32      b32;

typedef float    r32;
typedef double   r64;

#define U8_MAX  (0xFF)
#define U16_MAX (0xFFFF)
#define U32_MAX (0xFFFFFFFF)
#define U64_MAX (0xFFFFFFFFFFFFFFFF)

#define u8_MAX  U8_MAX 
#define u16_MAX U16_MAX
#define u32_MAX U32_MAX
#define u64_MAX U64_MAX

#define S8_MAX  (0x7F)
#define S16_MAX (0x7FFF)
#define S32_MAX (0x7FFFFFFF)
#define S64_MAX (0x7FFFFFFFFFFFFFFF)

#define s8_MAX  S8_MAX 
#define s16_MAX S16_MAX
#define s32_MAX S32_MAX
#define s64_MAX S64_MAX

#define r32_MAX FLT_MAX;

#define S8_MIN  (0X80)
#define S16_MIN (0x8000)
#define S32_MIN (0x80000000)
#define S64_MIN (0x8000000000000000)

struct Buffer
{
  usz size;
  u8 *contents;
};

struct String8
{
  u8 *str;
  u64 size;
};

struct String16
{
  u16 *str;
  u64 size;
};

struct String32
{
  u32 *str;
  u64 size;
};

struct c64
{
  r32 re, im;
};

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

union v3u32
{
  struct { u32 x, y, z; };
  u32 E[3];
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

union mat4
{
  struct
  {
    v4 r1, r2, r3, r4;
  };

  v4 rows[4];

  r32 E[16];
};

union RangeU32
{
  struct
  {
    u32 min, max;
  };
  
  u32 E[2];
};

union RangeR32
{
  struct
  {
    r32 min, max;
  };

  r32 E[2];
};

struct Rect2
{
  v2 min;
  v2 max;
};

struct LoadedSound
{
  u32 sampleCount;
  u32 channelCount;
  
  r32 *samples[2];
};

struct LoadedBitmap
{
  u32 width;
  u32 height;
  u32 stride;
  v2 alignPercentage;

  Rect2 uv;

  u32 *pixels;
  u32 glHandle;
};

struct PluginAsset;
struct LoadedFont
{
  RangeU32 characterRange;

  r32 verticalAdvance; 

  //r32 horizontalAdvance;
  PluginAsset *glyphs;

  /* u32 glyphCount; */
  /* LoadedBitmap *glyphs; */
  /* r32 *horizontalAdvance;   */
};
