#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#include <math.h>
#define M_TAU (2.0*M_PI)

//
// scalar
//

// TODO: stop using the C runtime library
inline r32
Abs(r32 val)
{
  return(fabsf(val));
}

inline r32
Sqrt(r32 val)
{
  return(sqrtf(val));
}

inline r32
Sin(r32 angle)
{
  return(sinf(angle));
}

inline r32
Cos(r32 angle)
{
  return(cosf(angle));
}

inline u32
log2(u32 num)
{
  u32 v = num;
  u32 masks[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
  u32 shifts[] = {1, 2, 4, 8, 16};
  u32 result = 0;

  for(u32 i = 4; i >= 0; --i)
    {
      if(v & masks[i])
	{
	  v >>= shifts[i];
	  result |= shifts[i];
	}
    }

  return(result);
}

inline u64
log2(u64 num)
{
  u64 v = num;
  u64 masks[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000, 0xFFFFFFFF00000000};
  u64 shifts[] = {1, 2, 4, 8, 16, 32};
  u64 result = 0;

  for(u32 i = 5; i >= 0; --i)
    {
      if(v & masks[i])
	{
	  v >>= shifts[i];
	  result |= shifts[i];
	}
    }

  return(result);  
}

//
// complex
//

inline c64
C64(r32 re, r32 im)
{
  c64 result = {re, im};

  return(result);
}

inline c64
C64Polar(r32 mag, r32 arg)
{
  c64 result = C64(mag*Cos(arg), mag*Sin(arg));

  return(result);
}

inline c64
conjugateC64(c64 z)
{
  c64 result = C64(z.re, -z.im);

  return(result);
}

inline r32
lengthC64(c64 z)
{
  r32 result = Sqrt(z.re*z.re + z.im*z.im);

  return(result);
}

inline c64
operator-(c64 z)
{
  c64 result = C64(-z.re, -z.im);
}

inline c64
operator+(c64 z, c64 w)
{
  c64 result = C64(z.re + w.re, z.im + w.im);

  return(result);
}

inline c64 &
operator+=(c64 &z, c64 w)
{
  z = z + w;

  return(z);
}

inline c64
operator-(c64 z, c64 w)
{
  c64 result = C64(z.re - w.re, z.im - w.im);

  return(result);
}

inline c64 &
operator-=(c64 &z, c64 w)
{
  z = z - w;

  return(z);
}

inline c64
operator*(c64 z, c64 w)
{
  c64 result = C64(z.re*w.re - w.im*z.im, z.re*w.im + z.im*w.re);

  return(result);
}

inline c64 &
operator*=(c64 &z, c64 w)
{
  z = z*w;

  return(z);
}

inline c64
operator*(r32 x, c64 z)
{
  c64 result = C64(x*z.re, x*z.im);

  return(result);
}

inline c64
operator*(c64 z, r32 x)
{
  return(x*z);
}

inline c64 &
operator*=(c64 &z, r32 x)
{
  z = z * x;

  return(z);
}

inline c64
operator/(c64 z, c64 w)
{
  r32 coeff = 1.f/(w.re*w.re + w.im*w.im);
  c64 result = C64(coeff*(z.re*w.re + z.im*w.im), coeff*(z.im*w.re - z.re*w.im));

  return(result);
}

inline c64 &
operator/=(c64 &z, c64 w)
{
  z = z / w;
  
  return(z);
}

//
// v2
//

static inline v2
V2(r32 x, r32 y)
{
  v2 result = {x, y};

  return(result);
}

static inline v2
operator-(v2 v)
{
  v2 result = V2(-v.x, -v.y);

  return(result);
}

static inline v2
operator+(v2 v, v2 w)
{
  v2 result = V2(v.x + w.x, v.y + w.y);

  return(result);
}

static inline v2 &
operator+=(v2 &v, v2 w)
{
  v = v + w;

  return(v);
}

static inline v2
operator-(v2 v, v2 w)
{
  v2 result = v + (-w);

  return(result);
}

static inline v2 &
operator-=(v2 &v, v2 w)
{
  v = v - w;

  return(v);
}

static inline v2
operator*(r32 a, v2 v)
{
  v2 result = V2(a*v.x, a*v.y);

  return(result);
}

static inline v2
operator*(v2 v, r32 a)
{
  v2 result = a*v;

  return(result);
}

static inline v2 &
operator*=(v2 &v, r32 a)
{
  v = v * a;

  return(v);
}

static inline v2
operator/(v2 v, r32 a)
{
  v2 result = v * (1.f/a);

  return(result);
}

static inline v2 &
operator/=(v2 &v, r32 a)
{
  v = v/a;

  return(v);
}

static inline v2
hadamard(v2 v, v2 w)
{
  v2 result = V2(v.x*w.x, v.y*w.y);

  return(result);
}

//
// v3
//

static inline v3
V3(r32 x, r32 y, r32 z)
{
  v3 result = {x, y, z};

  return(result);
}

//
// v4
// 

static inline v4
V4(r32 x, r32 y, r32 z, r32 w)
{
  v4 result = {x, y, z, w};

  return(result);
}

//
// mat4
//

inline mat4
makeProjectionMatrix(u32 widthInPixels, u32 heightInPixels)
{
  mat4 result = {};
  result.r1 = V4(2.f/(r32)widthInPixels, 0, 0, -1.f);
  result.r2 = V4(0, 2.f/(r32)heightInPixels, 0, -1.f);
  result.r3 = V4(0, 0, 1, 0);
  result.r4 = V4(0, 0, 0, 1);

  return(result);
}

inline mat4
transpose(mat4 m)
{
  mat4 result = {};
  for(u32 i = 0; i < 4; ++i)
    {
      for(u32 j = 0; j < 4; ++j)
	{
	  result.rows[j].E[i] = m.rows[i].E[j];
	}
    }

  return(result);
}

//
// RangeR32
//

inline RangeR32
makeRange(r32 min, r32 max)
{
  RangeR32 result = {};
  result.min = min;
  result.max = max;

  return(result);
}

inline r32
getLength(RangeR32 range)
{
  r32 result = range.max - range.min;

  return(result);
}

inline r32
clampToRange(r32 val, RangeR32 range)
{
  r32 result = val;
  if(result > range.max) result = range.max;
  if(result < range.min) result = range.min;

  return(result);
}

//
// Rect2
//

static inline Rect2
rectMinMax(v2 min, v2 max)  
{
  Rect2 result = {};
  result.min = min;
  result.max = max;

  return(result);
}

static inline Rect2
rectMinDim(v2 min, v2 dim)
{
  Rect2 result = rectMinMax(min, min + dim);
    
  return(result);
}

static inline Rect2
rectCenterHalfDim(v2 center, v2 halfDim)
{
  Rect2 result = {};
  result.min = center - halfDim;
  result.max = center + halfDim;

  return(result);
}

static inline Rect2
rectCenterDim(v2 center, v2 dim)
{
  Rect2 result = rectCenterHalfDim(center, 0.5f*dim);

  return(result);
}

static inline v2
getDim(Rect2 rect)
{
  v2 result = rect.max - rect.min;

  return(result);
}

static inline v2
getCenter(Rect2 rect)
{
  v2 result = rect.min + 0.5f*getDim(rect);

  return(result);
}

static inline bool
isInRectangle(Rect2 r, v2 v)
{
  bool result = (r.min.x <= v.x &&
		 r.min.y <= v.y &&
		 v.x <= r.max.x &&
		 v.y <= r.max.y);
  return(result);
}

inline void
fft(c64 *destBuffer, r32 *sourceBuffer, u32 length)
{
  // NOTE: radix-2 DIT fft
  u32 logLength = log2(length);  

  // NOTE: bit-reverse copy
  for(u32 index = 0; index < length; ++index)
    {
      u32 reversedIndex = (reverseBits(index) >> logLength);
      destBuffer[index].re = sourceBuffer[reversedIndex];
    }  

  // NOTE: swizzles
  for(u32 level = 1; level <= logLength; ++level)
    {
      u32 m = (1 << level);
      r32 angle = -2.f*M_PI/(r32)m;
      c64 wm = C64Polar(1, angle);
      
      for(u32 k = 0; k < length; k += m)
	{
	  c64 w = C64(1, 0);
	  
	  c64 *dest0 = destBuffer + k;
	  c64 *dest1 = destBuffer + k + m/2;	  
	  for(u32 j = 0; j < m/2; ++j)
	    {
	      c64 in0 = *dest0;
	      c64 in1 = *dest1;     

	      // TODO: optimize this math:
	      //       use SIMD intrinsics, and take advantage of the fact that the input data is real
	      *dest0 = in0 + w*in1;
	      *dest1 = in0 - w*in1;

	      w *= wm;
	    }
	}
    }
}
