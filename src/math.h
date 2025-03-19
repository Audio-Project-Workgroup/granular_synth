#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#include <math.h>
#define M_TAU (2.0*M_PI)

//
// scalar
//

inline r32
lerp(r32 val1, r32 val2, r32 t)
{
  r32 result = (1 - t)*val1 + t*val2;

  return(result);
}

inline r32
binterp(r32 val0, r32 val1, r32 val2, r32 t0)
{
  r32 t1 = t0 - 1;
  r32 t2 = t0 - 2;
  
  r32 result = 0.5f*t1*t2*val0 - t0*t2*val1 + 0.5f*t0*t1*val2;

  return(result);
}

inline r32
cubicInterp(r32 val0, r32 val1, r32 val2, r32 val3, r32 t0)
{
  r32 t1 = t0 - 1;
  r32 t2 = t0 - 2;
  r32 t3 = t0 - 3;
  
  r32 result = (-1.f/6.f)*t1*t2*t3*val0 + 0.5f*t0*t2*t3*val1 - 0.5f*t0*t1*t3*val2 + 0.5f*t0*t1*t2*val3;

  return(result);
}

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

  for(s32 i = 4; i >= 0; --i)
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

#define ROUND_UP_TO_POWER_OF_2(num) ((num == (u32)(1 << log2(num))) ? num : (u32)(1 << (log2(num) + 1)))

inline bool
isPowerOf2(u32 num)
{
  return(num == ROUND_UP_TO_POWER_OF_2(num));
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

  return(result);
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

inline c64
lerp(c64 val1, c64 val2, r32 t)
{
  c64 result = (1 - t)*val1 + t*val2;

  return(result);
}

inline c64
binterp(c64 val0, c64 val1, c64 val2, r32 t0)
{
  r32 t1 = t0 - 1;
  r32 t2 = t0 - 2;
  
  c64 result = 0.5f*t1*t2*val0 - t0*t2*val1 + 0.5f*t0*t1*val2;

  return(result);
}

inline c64
cubicInterp(c64 val0, c64 val1, c64 val2, c64 val3, r32 t0)
{
  r32 t1 = t0 - 1;
  r32 t2 = t0 - 2;
  r32 t3 = t0 - 3;
  
  c64 result = (-1.f/6.f)*t1*t2*t3*val0 + 0.5f*t0*t2*t3*val1 - 0.5f*t0*t1*t3*val2 + 0.5f*t0*t1*t2*val3;

  return(result);
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

inline v4
operator+(v4 v, v4 w)
{
  v4 result = {v.x + w.x, v.y + w.y, v.z + w.z, v.w + w.w};

  return(result);
}

inline v4 & 
operator+=(v4 &v, v4 w)
{
  v = v + w;
  
  return(v);
}

inline v4
operator-(v4 v)
{
  v4 result = {-v.x, -v.y, -v.z, -v.w};

  return(result);
}

inline v4
operator-(v4 v, v4 w)
{
  v4 result = v + (-w);

  return(result);
}

inline v4 &
operator-=(v4 &v, v4 w)
{
  v = v - w;

  return(v);
}

inline v4
operator*(r32 a, v4 v)
{
  v4 result = {a*v.x, a*v.y, a*v.z, a*v.w};

  return(result);
}

inline v4
operator*(v4 v, r32 a)
{
  v4 result = a*v;

  return(result);  
}

inline v4 &
operator*=(v4 &v, r32 a)
{
  v = v*a;

  return(v);
}

inline v4
operator/(v4 v, r32 a)
{
  v4 result = v*(1.f/a);

  return(result);
}

inline v4 &
operator/=(v4 &v, r32 a)
{
  v = v/a;

  return(v);
}

inline v4
hadamard(v4 v, v4 w)
{
  v4 result = {v.x*w.x, v.y*w.y, v.z*w.z, v.w*w.w};

  return(result);
}

//
// mat4
//

inline mat4
operator*(mat4 lhs, mat4 rhs)
{
  mat4 result = {};
      
  r32 *dest = result.E;
  for(u32 i = 0; i < 4; ++i)
    {
      for(u32 j = 0; j < 4; ++j)
	{
	  r32 destVal = 0.f;
	  for(u32 k = 0; k < 4; ++k)
	    {
	      r32 lhsVal = lhs.rows[i].E[k];
	      r32 rhsVal = rhs.rows[k].E[j];
	      
	      destVal += lhsVal*rhsVal;
	    }

	  *dest++ = destVal;
	}
    }

  return(result);
}

inline mat4
makeTranslationMatrix(v2 center) // NOTE: center to origin
{
  mat4 result = {};
  result.r1 = V4(1, 0, 0, -center.x);
  result.r2 = V4(0, 1, 0, -center.y);
  result.r3 = V4(0, 0, 1, 0);
  result.r4 = V4(0, 0, 0, 1);

  return(result);
}

inline mat4
makeRotationMatrixXY(r32 angle)
{
  mat4 result = {};
  result.r1 = V4(Cos(angle), -Sin(angle), 0, 0);
  result.r2 = V4(Sin(angle), Cos(angle), 0, 0);
  result.r3 = V4(0, 0, 1, 0);
  result.r4 = V4(0, 0, 0, 1);

  return(result);
}

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
// RangeU32
//

inline RangeU32
makeRange(u32 min, u32 max)
{
  RangeU32 result = {};
  result.min = min;
  result.max = max;

  return(result);
}

inline u32
getLength(RangeU32 range)  
{
  ASSERT(range.max >= range.min);
  u32 result = range.max - range.min;

  return(result);
}

inline u32
clampToRange(u32 val, RangeU32 range)
{
  u32 result = val;
  if(val > range.max) result = range.max;
  if(val < range.min) result = range.min;

  return(result);
}

inline u32
clampToRange(u32 val, u32 min, u32 max)
{
  u32 result = val;
  if(val > max) result = max;
  if(val < min) result = min;

  return(result);
}

inline bool
isInRange(u32 val, RangeU32 range)
{
  bool result = (val <= range.max && val >= range.min);

  return(result);
}

inline bool
isInRange(u32 val, u32 min, u32 max)
{
  bool result = (val <= max && val >= min);

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

inline r32
clampToRange(r32 val, r32 min, r32 max)
{
  RangeR32 range = makeRange(min, max);
  r32 result = clampToRange(val, range);

  return(result);
}

inline bool
isInRange(r32 val, RangeR32 range)
{
  bool result = (val <= range.max && range.min <= val);

  return(result);
}

inline bool
isInRange(r32 val, r32 min, r32 max)
{
  bool result = (val <= max && min <= val);

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
rectMaxNegDim(v2 max, v2 negDim)
{
  Rect2 result = rectMinMax(max + negDim, max);

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

static inline v2
clipToRect(v2 v, Rect2 r)
{
  RangeR32 width = makeRange(r.min.x, r.max.x);
  RangeR32 height = makeRange(r.min.y, r.max.y);
  v2 result = V2(clampToRange(v.x, width), clampToRange(v.y, height));

  return(result);
}

static inline Rect2
rectAddRadius(Rect2 rect, v2 r)
{
  Rect2 result = rectMinMax(rect.min - r, rect.max + r);

  return(result);
}

//
// fft
// 

#define REAL_FFT_FUNCTION(name) void (name)(r32 *destRe, r32 *destIm, r32 *src, u32 length)
#define REAL_IFFT_FUNCTION(name) void (name)(r32 *dest, r32 *destImTemp, r32 *srcRe, r32 *srcIm, u32 length)

static REAL_FFT_FUNCTION(fft_real_noPermute);
static REAL_IFFT_FUNCTION(ifft_real_noPermute);

static REAL_FFT_FUNCTION(fft_real_permute);
static REAL_IFFT_FUNCTION(ifft_real_permute);

static REAL_FFT_FUNCTION(fft_real_permute)
{
  ASSERT(isPowerOf2(length));
  u32 logLength = log2(length);
  u32 simdWidth = 4;

  // TODO: simd-ize this
  u32 lengthSizeInBits = 8*sizeof(length);
  for(u32 index = 0; index < length; ++index)
    {
      u32 reversedIndex = reverseBits(index);
      reversedIndex >>= lengthSizeInBits - logLength;
      ASSERT(reversedIndex < length);
      
      destRe[index] = src[reversedIndex];
    }

  for(u32 m = 2; m <= length; m <<= 1)
    {
      r32 angle = -M_TAU/(r32)m;
      
      // TODO: make trig ops fast
      r32 wmRe = Cos(angle);
      r32 wmIm = Sin(angle);

      for(u32 k = 0; k < length; k += simdWidth*m)
	{
	  u32 mask = 0xFFFFFFFF;
	  WideInt storeMask = wideSetConstantInts(0);

	  r32 *dest0Re[4] = {};
	  r32 *dest0Im[4] = {};
	  r32 *dest1Re[4] = {};
	  r32 *dest1Im[4] = {};

	  for(u32 lane = 0; lane < simdWidth; ++lane)
	    {	      
	      if((k + lane*m) < length)
		{
		  r32 wRe = 1.f;
		  r32 wIm = 0.f;
	  	      
		  dest0Re[lane] = destRe + k + lane*m;
		  dest0Im[lane] = destIm + k + lane*m;
		  dest1Re[lane] = destRe + k + lane*m + m/2;
		  dest1Im[lane] = destIm + k + lane*m + m/2;
		  for(u32 j = 0; j < m/2; j += 1)
		    {	      
		      r32 in0Re = *dest0Re[lane];
		      r32 in0Im = *dest0Im[lane];
		      r32 in1Re = *dest1Re[lane];
		      r32 in1Im = *dest1Im[lane];

		      r32 out0Re = in0Re + (wRe*in1Re - wIm*in1Im);
		      r32 out0Im = in0Im + (wRe*in1Im + wIm*in1Re);
		      r32 out1Re = in0Re - (wRe*in1Re - wIm*in1Im);
		      r32 out1Im = in0Im - (wRe*in1Im + wIm*in1Re);

		      *dest0Re[lane] = out0Re;
		      *dest0Im[lane] = out0Im;
		      *dest1Re[lane] = out1Re;
		      *dest1Im[lane] = out1Im;

		      r32 wReOld = wRe;
		      r32 wImOld = wIm;
		      wRe = wReOld*wmRe - wImOld*wmIm;
		      wIm = wReOld*wmIm + wImOld*wmRe;

		      dest0Re[lane] += 1;
		      dest0Im[lane] += 1;
		      dest1Re[lane] += 1;
		      dest1Im[lane] += 1;
		    }
		}
	      else
		{
		  break;
		}
	    }
	}
    }
}

static REAL_IFFT_FUNCTION(ifft_real_permute)
{
  ASSERT(isPowerOf2(length));
  u32 logLength = log2(length);
  r32 invLength = 1.f/(r32)length;
  u32 simdWidth = 4;

  // TODO: simd-ize this
  u32 lengthSizeInBits = 8*sizeof(length);
  for(u32 index = 0; index < length; ++index)
    {
      u32 reversedIndex = reverseBits(index);
      reversedIndex >>= lengthSizeInBits - logLength;
      ASSERT(reversedIndex < length);
      
      dest[index] = invLength*srcRe[reversedIndex];
      destImTemp[index] = invLength*srcIm[reversedIndex];
    }

  for(u32 m = 2; m <= length; m <<= 1)
    {
      r32 angle = M_TAU/(r32)m;
      
      // TODO: make trig ops fast
      WideFloat wmRe = wideSetConstantFloats(Cos(angle));
      WideFloat wmIm = wideSetConstantFloats(Sin(angle));

      for(u32 k = 0; k < length; k += m)
	{
	  WideFloat wRe = wideSetConstantFloats(1.f);
	  WideFloat wIm = wideSetConstantFloats(0.f);

	  r32 *dest0Re = dest + k;
	  r32 *dest0Im = destImTemp + k;
	  r32 *dest1Re = dest + k + m/2;
	  r32 *dest1Im = destImTemp + k + m/2;
	  for(u32 j = 0; j < m/2; j += simdWidth)
	    {
	      u32 mask = 0xFFFFFFFF;
	      WideInt storeMask = wideSetConstantInts(mask);
	      for(u32 lane = 0; lane < simdWidth; ++lane)
		{
		  u32 val = (j + lane < m/2) ? mask : 0;
		  wideSetLaneInts(&storeMask, val, lane);
		}

	      WideFloat in0Re = wideLoadFloats(dest0Re);
	      WideFloat in0Im = wideLoadFloats(dest0Im);
	      WideFloat in1Re = wideLoadFloats(dest1Re);
	      WideFloat in1Im = wideLoadFloats(dest1Im);

	      WideFloat out0Re = wideAddFloats(in0Re,
					       wideSubFloats(wideMulFloats(wRe, in1Re),
							     wideMulFloats(wIm, in1Im)));
	      WideFloat out0Im = wideAddFloats(in0Re,
					       wideAddFloats(wideMulFloats(wRe, in1Im),
							     wideMulFloats(wIm, in1Re)));
	      WideFloat out1Re = wideSubFloats(in0Re,
					       wideSubFloats(wideMulFloats(wRe, in1Re),
							     wideMulFloats(wIm, in1Im)));
	      WideFloat out1Im = wideSubFloats(in0Re,
					       wideAddFloats(wideMulFloats(wRe, in1Im),
							     wideMulFloats(wIm, in1Re)));

	      wideStoreFloats(dest0Re, wideMaskFloats(out0Re, storeMask));
	      wideStoreFloats(dest0Im, wideMaskFloats(out0Im, storeMask));
	      wideStoreFloats(dest1Re, wideMaskFloats(out1Re, storeMask));
	      wideStoreFloats(dest1Im, wideMaskFloats(out1Im, storeMask));

	      wRe = wideSubFloats(wideMulFloats(wRe, wmRe),
				  wideMulFloats(wIm, wmIm));
	      wIm = wideAddFloats(wideMulFloats(wRe, wmIm),
				  wideMulFloats(wIm, wmRe));

	      dest0Re += simdWidth;
	      dest0Im += simdWidth;
	      dest1Re += simdWidth;
	      dest1Im += simdWidth;
	    }
	}
    }
}

static REAL_FFT_FUNCTION(fft_real_noPermute)
{
  // NOTE: radix-2 dit fft, with no input permutation
  ASSERT(isPowerOf2(length));
  u32 logLength = log2(length);
  u32 simdWidth = 4;

  // NOTE: copy input
  r32 *read = src;
  r32 *writeRe = destRe;
  r32 *writeIm = destIm;
  WideFloat readIm = wideSetConstantFloats(0);
  for(u32 i = 0; i < length; i += simdWidth)
    {
      WideFloat readRe = wideLoadFloats(src);      

      wideStoreFloats(writeRe, readRe);
      wideStoreFloats(writeIm, readIm);

      read += simdWidth;
      writeRe += simdWidth;
      writeIm += simdWidth;
    }

  // TODO: exploit the imaginaries being all 0 in the first stage in some way
  for(u32 level = 1; level <= logLength; ++level)
    {
      u32 m = 1 << level;
      r32 angle = -M_TAU/(r32)m;
      //c64 wm = C64Polar(1, angle);
      WideFloat wmRe = wideSetConstantFloats(Cos(angle));
      WideFloat wmIm = wideSetConstantFloats(Sin(angle));
      
      for(u32 k = 0; k < length; k += m)
	{
	  //c64 w = C64(1, 0);
	  WideFloat wRe = wideSetConstantFloats(1.f);
	  WideFloat wIm = wideSetConstantFloats(0.f);
	  	  
	  r32 *dest0Re = destRe + k;
	  r32 *dest0Im = destIm + k;
	  r32 *dest1Re = destRe + k + m/2;
	  r32 *dest1Im = destIm + k + m/2;
	  for(u32 j = 0; j < m/2; j += simdWidth)
	    {
	      u32 mask = 0xFFFFFFFF;
	      WideInt writeMask = wideSetConstantInts(mask);
	      for(u32 lane = 0; lane < simdWidth; ++lane)
		{
		  u32 val = j + lane < m/2 ? mask : 0;
		  wideSetLaneInts(&writeMask, val, lane);
		}

	      WideFloat in0Re = wideLoadFloats(dest0Re);
	      WideFloat in0Im = wideLoadFloats(dest0Im);
	      WideFloat in1Re = wideLoadFloats(dest1Re);
	      WideFloat in1Im = wideLoadFloats(dest1Im);
	      //c64 in0 = C64(*src0, 0);
	      //c64 in1 = C64(*src1, 0);

	      // dest0.re = in0.re + (w.re*in1.re - w.im*in1.im);
	      // dest0.im = in0.im + (w.re*in1.im + w.im*in1.im);
	      // dest1.re = in0.re - (w.re*in1.re - w.im*in1.im);
	      // dest1.im = in0.im - (w.re*in1.im + w.im*in1.im);
	      WideFloat out0Re = wideAddFloats(in0Re,
					       wideSubFloats(wideMulFloats(wRe, in1Re),
							     wideMulFloats(wIm, in1Im)));
	      WideFloat out0Im = wideAddFloats(in0Re,
					       wideAddFloats(wideMulFloats(wRe, in1Im),
							     wideMulFloats(wIm, in1Re)));
	      WideFloat out1Re = wideSubFloats(in0Re,
					       wideSubFloats(wideMulFloats(wRe, in1Re),
							     wideMulFloats(wIm, in1Im)));
	      WideFloat out1Im = wideSubFloats(in0Re,
					       wideAddFloats(wideMulFloats(wRe, in1Im),
							     wideMulFloats(wIm, in1Re)));

	      wideStoreFloats(dest0Re, wideMaskFloats(out0Re, writeMask));
	      wideStoreFloats(dest0Im, wideMaskFloats(out0Im, writeMask));
	      wideStoreFloats(dest1Re, wideMaskFloats(out1Re, writeMask));
	      wideStoreFloats(dest1Im, wideMaskFloats(out1Im, writeMask));
	      
	      //w *= wm;
	      wRe = wideSubFloats(wideMulFloats(wRe, wmRe),
				  wideMulFloats(wIm, wmIm));
	      wIm = wideAddFloats(wideMulFloats(wRe, wmIm),
				  wideMulFloats(wIm, wmRe));
	      	      
	      dest0Re += simdWidth;
	      dest0Im += simdWidth;
	      dest1Re += simdWidth;
	      dest1Im += simdWidth;
	    }
	}
    }
}

static REAL_IFFT_FUNCTION(ifft_real_noPermute)
{
  ASSERT(isPowerOf2(length));
  u32 simdWidth = 4;
  u32 logLength = log2(length);
  WideFloat invLength = wideSetConstantFloats(1.f/(r32)length);

  // NOTE: copy input
  r32 *readRe = srcRe;
  r32 *readIm = srcIm;
  r32 *writeRe = dest;
  r32 *writeIm = destImTemp;
  for(u32 i = 0; i < length; i += simdWidth)
    {
      WideFloat inRe = wideMulFloats(wideLoadFloats(srcRe), invLength);
      WideFloat inIm = wideMulFloats(wideLoadFloats(srcIm), invLength);

      wideStoreFloats(writeRe, inRe);
      wideStoreFloats(writeIm, inIm);

      readRe += simdWidth;
      readIm += simdWidth;
      writeRe += simdWidth;
      writeIm += simdWidth;
    }

  for(u32 level = 1; level <= logLength; ++level)
    {
      u32 m = 1 << level;
      r32 angle = M_TAU/(r32)m;
      WideFloat wmRe = wideSetConstantFloats(Cos(angle));
      WideFloat wmIm = wideSetConstantFloats(Sin(angle));
      
      for(u32 k = 0; k < length; k += m)
	{
	  WideFloat wRe = wideSetConstantFloats(1.f);
	  WideFloat wIm = wideSetConstantFloats(0.f);
	  	  
	  r32 *dest0Re = dest + k;
	  r32 *dest0Im = destImTemp + k;
	  r32 *dest1Re = dest + k + m/2;
	  r32 *dest1Im = destImTemp + k + m/2;
	  for(u32 j = 0; j < m/2; j += simdWidth)
	    {
	      u32 mask = 0xFFFFFFFF;
	      WideInt writeMask = wideSetConstantInts(mask);
	      for(u32 lane = 0; lane < simdWidth; ++lane)
		{
		  u32 val = j + lane < m/2 ? mask : 0;
		  wideSetLaneInts(&writeMask, val, lane);
		}

	      WideFloat in0Re = wideLoadFloats(dest0Re);
	      WideFloat in0Im = wideLoadFloats(dest0Im);
	      WideFloat in1Re = wideLoadFloats(dest1Re);
	      WideFloat in1Im = wideLoadFloats(dest1Im);
	      
	      WideFloat out0Re =  wideAddFloats(in0Re,
						wideSubFloats(wideMulFloats(wRe, in1Re),
							      wideMulFloats(wIm, in1Im)));
	      WideFloat out0Im = wideAddFloats(in0Im,
						wideAddFloats(wideMulFloats(wRe, in1Im),
							      wideMulFloats(wIm, in1Re)));	      
	      WideFloat out1Re = wideSubFloats(in0Re,
					       wideSubFloats(wideMulFloats(wRe, in1Re),
							     wideMulFloats(wIm, in1Im)));
	      WideFloat out1Im = wideSubFloats(in0Re,
					       wideAddFloats(wideMulFloats(wRe, in1Re),
							     wideMulFloats(wIm, in1Im)));

	      wideStoreFloats(dest0Re, wideMaskFloats(out0Re, writeMask));
	      wideStoreFloats(dest0Im, wideMaskFloats(out0Im, writeMask));	      
	      wideStoreFloats(dest1Re, wideMaskFloats(out1Re, writeMask));
	      wideStoreFloats(dest1Im, wideMaskFloats(out1Im, writeMask));
	      	     
	      wRe = wideSubFloats(wideMulFloats(wRe, wmRe),
				  wideMulFloats(wIm, wmIm));
	      wIm = wideAddFloats(wideMulFloats(wRe, wmIm),
				  wideMulFloats(wIm, wmRe));
	      	      
	      dest0Re += simdWidth;
	      dest0Im += simdWidth;
	      dest1Re += simdWidth;
	      dest1Im += simdWidth;
	    }
	}
    }
}

static void
convolve(r32 *dest, r32 *src0, r32 *src1, u32 length, Arena *tempAllocator)
{
  // NOTE: fast convolution with ffts  
  ASSERT(isPowerOf2(length));
  u32 simdWidth = 4;
  
  r32 *temp0Re = arenaPushArray(tempAllocator, length, r32, arenaFlagsZeroAlign(simdWidth));
  r32 *temp0Im = arenaPushArray(tempAllocator, length, r32, arenaFlagsZeroAlign(simdWidth));
  r32 *temp1Re = arenaPushArray(tempAllocator, length, r32, arenaFlagsZeroAlign(simdWidth));
  r32 *temp1Im = arenaPushArray(tempAllocator, length, r32, arenaFlagsZeroAlign(simdWidth));
  
  fft_real_noPermute(temp0Re, temp0Im, src0, length);
  fft_real_noPermute(temp1Re, temp1Im, src1, length);

  r32 *tempDestRe = temp0Re;
  r32 *tempDestIm = temp0Im;
  r32 *tempSrc0Re = temp0Re;
  r32 *tempSrc0Im = temp0Im;
  r32 *tempSrc1Re = temp1Re;
  r32 *tempSrc1Im = temp1Im;
  ASSERT((length & 0x3) == 0);
  for(u32 i = 0; i < length; i += simdWidth)
    {
      WideFloat in0Re = wideLoadFloats(tempSrc0Re);
      WideFloat in0Im = wideLoadFloats(tempSrc0Im);
      WideFloat in1Re = wideLoadFloats(tempSrc1Re);
      WideFloat in1Im = wideLoadFloats(tempSrc1Im);

      WideFloat outRe = wideSubFloats(wideMulFloats(in0Re, in1Re), wideMulFloats(in0Im, in1Re));
      WideFloat outIm = wideAddFloats(wideMulFloats(in0Re, in1Im), wideMulFloats(in0Im, in1Re));
      
      wideStoreFloats(tempDestRe, outRe);
      wideStoreFloats(tempDestIm, outIm);

      tempDestRe += simdWidth;
      tempDestIm += simdWidth;
      
      tempSrc0Re += simdWidth;
      tempSrc0Im += simdWidth;
      tempSrc1Re += simdWidth;
      tempSrc1Im += simdWidth;
    }

  ifft_real_noPermute(dest, temp1Im, temp0Re, temp0Im, length);
}

inline void
fft(c64 *destBuffer, r32 *sourceBuffer, u32 lengthInit, u32 stride = 1)
{
#if 0
  u32 length = lengthInit;
  if(length == 1)
    {
      (*destBuffer).re = *sourceBuffer;
    }
  else
    {
      fft(destBuffer, sourceBuffer, length/2, 2*stride);
      fft(destBuffer + length/2, sourceBuffer + stride, length/2, 2*stride);

      for(u32 k = 0; k < length/2; ++k)
	{
	  c64 w = C64Polar(1, -M_TAU*k/(r32)length);
	  c64 in0 = destBuffer[k];
	  c64 in1 = destBuffer[k + length/2];

	  destBuffer[k] = in0 + w*in1;
	  destBuffer[k + length/2] = in0 - w*in1;
	}
    }
#else 
  // NOTE: iterative radix-2 DIT fft
  // TODO: radix-4
  u32 length = ROUND_UP_TO_POWER_OF_2(lengthInit);
  u32 lengthSizeInBits = 8*sizeof(length);
  u32 logLength = log2(length);  

  // NOTE: bit-reverse copy
  // TODO: optimize
  for(u32 index = 0; index < length; ++index)
    {
      u32 reversedIndex = reverseBits(index);
      reversedIndex >>= lengthSizeInBits - logLength;
      ASSERT(reversedIndex < length);
      
      destBuffer[index].re = sourceBuffer[reversedIndex];
    }  

  // NOTE: twiddles
  for(u32 level = 1; level <= logLength; ++level)
    {
      u32 m = (1 << level);
      r32 angle = -M_TAU/(r32)m;
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
	      *dest0++ = in0 + w*in1;
	      *dest1++ = in0 - w*in1;

	      w *= wm;
	    }
	}
    }  
#endif
}

inline void
ifft(r32 *destBuffer, r32 *destImScratch, c64 *sourceBuffer, u32 lengthInit, u32 stride = 1)
{
#if 0
  if(length == 1)
    {
      *destBuffer = (*sourceBuffer).re;
    }
  else
    {
      ifft(destBuffer, sourceBuffer, length/2, 2*stride);
      ifft(destBuffer + length/2, sourceBuffer + stride, length/2, 2*stride);

      for(u32 k = 0; k < length/2; ++k)
	{
	  c64 w = C64Polar(1, M_TAU*k/(r32)length);
	  c64 in0 = destBuffer[k];
	  c64 in1 = destBuffer[k + length/2];

	  destBuffer[k] = in0 + w*in1;
	  destBuffer[k + length/2] = in0 - w*in1;
	}
    }
#else
  // TODO: optimize
  //ASSERT(!(length & 1));
  u32 length = ROUND_UP_TO_POWER_OF_2(lengthInit);
  u32 logLength = log2(length);
  // NOTE: assuming power-of-two size
  // TODO: support non-power-of-two sizes, don't require zero-padding input
  ASSERT(length == (u32)(1 << logLength));
  r32 invLength = 1.f/(r32)length;

  u32 lengthSizeInBits = 8*sizeof(length);
  for(u32 index = 0; index < length; ++index)
    {
      u32 reversedIndex = reverseBits(index);
      reversedIndex >>= lengthSizeInBits - logLength;

      c64 source = invLength*sourceBuffer[reversedIndex];
      destBuffer[index] = source.re;
      destImScratch[index] = source.im;
    }

  for(u32 level = 1; level <= logLength; ++level)
    {
      u32 m = 1 << level;
      r32 angle = M_TAU/(r32)m;
      c64 wm = C64Polar(1, angle);

      for(u32 k = 0; k < length; k += m)
	{
	  c64 w = C64(1, 0);
	  
	  r32 *dest0 = destBuffer + k;
	  r32 *dest0Im = destImScratch + k;
	  r32 *dest1 = destBuffer + k + m/2;
	  r32 *dest1Im = destImScratch + k + m/2;
	  for(u32 j = 0; j < m/2; ++j)
	    {
	      c64 in0 = C64(*dest0, *dest0Im);
	      c64 in1 = C64(*dest1, *dest1Im);

	      c64 result0 = in0 + w*in1;
	      c64 result1 = in0 - w*in1;
	      
	      *dest0++ = result0.re;
	      *dest1++ = result1.re;
	      *dest0Im++ = result0.im;
	      *dest1Im++ = result1.im;

	      w *= wm;
	    }
	}
    }
#endif
}

inline void
fft(c64 *output, c64 *input, u32 lengthInit)
{
  u32 length = ROUND_UP_TO_POWER_OF_2(lengthInit);
  u32 lengthSizeInBits = 8*sizeof(lengthInit);
  u32 logLength = log2(length);
  
  for(u32 index = 0; index < length; ++index)
    {
      u32 reverseIndex = reverseBits(index);
      reverseIndex >>= lengthSizeInBits - logLength;
      output[index] = input[reverseIndex];
    }

  for(u32 s = 1; s <= logLength; ++s)
    {
      u32 m = 1 << s;
      r32 angle = -M_TAU/(r32)m;
      c64 wm = C64Polar(1, angle);

      for(u32 k = 0; k < length; k += m)
	{
	  c64 w = C64(1, 0);
	  
	  c64 *dest0 = output + k;
	  c64 *dest1 = output + k + m/2;	  
	  for(u32 j = 0; j < m/2; ++j)
	    {
	      c64 in0 = *dest0;
	      c64 in1 = *dest1;     

	      // TODO: SIMD	 
	      *dest0++ = in0 + w*in1;
	      *dest1++ = in0 - w*in1;

	      w *= wm;
	    }
	}
    }
}

inline void
ifft(c64 *output, c64 *input, u32 lengthInit)
{
  u32 length = ROUND_UP_TO_POWER_OF_2(lengthInit);
  u32 lengthSizeInBits = 8*sizeof(lengthInit);
  u32 logLength = log2(length);
  r32 invLength = 1.f/(r32)length;
  
  for(u32 index = 0; index < length; ++index)
    {
      u32 reverseIndex = reverseBits(index);
      reverseIndex >>= lengthSizeInBits - logLength;
      output[index] = invLength*input[reverseIndex];
    }

  for(u32 s = 1; s <= logLength; ++s)
    {
      u32 m = 1 << s;
      r32 angle = M_TAU/(r32)m;
      c64 wm = C64Polar(1, angle);

      for(u32 k = 0; k < length; k += m)
	{
	  c64 w = C64(1, 0);
	  
	  c64 *dest0 = output + k;
	  c64 *dest1 = output + k + m/2;	  
	  for(u32 j = 0; j < m/2; ++j)
	    {
	      c64 in0 = *dest0;
	      c64 in1 = *dest1;     

	      // TODO: SIMD	 
	      *dest0++ = in0 + w*in1;
	      *dest1++ = in0 - w*in1;

	      w *= wm;
	    }
	}
    }
}

inline void
czt(c64 *output, r32 *input, u32 length, Arena *scratchAllocator)
{
  // NOTE: chirp z transform.
  //       for taking ffts of arbitrary length signals
  
  u32 chirpLength = 2*length - 1;
  u32 fftLength = ROUND_UP_TO_POWER_OF_2(chirpLength);

  r32 angle = -M_PI/(r32)length;
  c64 wBase = C64Polar(1, angle);  
  c64 *chirp = arenaPushArray(scratchAllocator, chirpLength, c64);//, arenaFlagsZeroNoAlign());
  c64 *reciprocalChirp = arenaPushArray(scratchAllocator, fftLength, c64);//, arenaFlagsZeroNoAlign());
  c64 *scaledInput = arenaPushArray(scratchAllocator, fftLength, c64);//, arenaFlagsZeroNoAlign());
  c64 *wTemp = arenaPushArray(scratchAllocator, fftLength, c64);//, arenaFlagsZeroNoAlign());
  chirp[length - 1] = C64(1, 0);
  reciprocalChirp[length - 1] = C64(1, 0);
  wTemp[0] = C64(1, 0);
  scaledInput[0] = input[0]*chirp[length];
  for(u32 k = 1; k < length; ++k)
    {
      // NOTE: generate chirp, multiply the input
      c64 chirpVal = chirp[length - 1 + k - 1] * wTemp[k - 1] * wBase;
      c64 conjChirpVal = conjugateC64(chirpVal);
      chirp[length - 1 + k] = chirpVal;
      chirp[length - 1 - k] = chirpVal;
      reciprocalChirp[length - 1 + k] = conjChirpVal;
      reciprocalChirp[length - 1 - k] = conjChirpVal;
      scaledInput[k] = input[k] * chirp[length + k - 1 ];

      // TODO: don't compute more of these values than necessary. may be a source of significant numerical error
      //if(k*k >= 2*length) 
      wTemp[k] = wTemp[k - 1] * wBase * wBase;
    } 

  // NOTE: convolve the scaled input with the reciprocal chirp
  // TODO: these fft calls are not optimized for doing convolutions:
  //       we do an (order 2) input permutation at the beginning of each fft and ifft so that our output coefficients
  //       are in order of increasing frequency. That is not necessary to do here, and a huge burden on performance
  c64 *fftScaledInput = wTemp;
  fft(fftScaledInput, scaledInput, fftLength);

  c64 *fftReciprocalChirp = scaledInput;
  fft(fftReciprocalChirp, reciprocalChirp, fftLength);
  
  for(u32 index = 0; index < fftLength; ++index)
    {
      fftScaledInput[index] *= fftReciprocalChirp[index];
    }

  c64 *ifftOutput = reciprocalChirp;
  ifft(ifftOutput, fftScaledInput, fftLength);

  // NOTE: write out, scaling by the chirp
  for(u32 index = 0; index < length; ++index)
    {
      output[index] = ifftOutput[length + index - 1] * chirp[length + index - 1];
    }
}

inline void
iczt(r32 *outputReal, r32 *outputImag, c64 *input, u32 length, Arena *scratchAllocator)
{
  r32 invLength = 1.f/(r32)length;
  u32 chirpLength = 2*length - 1;
  u32 fftLength = ROUND_UP_TO_POWER_OF_2(chirpLength);

  r32 angle = M_PI/(r32)length;
  c64 wBase = C64Polar(1, angle);  
  c64 *chirp = arenaPushArray(scratchAllocator, chirpLength, c64);//, arenaFlagsZeroNoAlign());
  c64 *reciprocalChirp = arenaPushArray(scratchAllocator, fftLength, c64);//, arenaFlagsZeroNoAlign());
  c64 *scaledInput = arenaPushArray(scratchAllocator, fftLength, c64);//, arenaFlagsZeroNoAlign());
  c64 *wTemp = arenaPushArray(scratchAllocator, fftLength, c64);//, arenaFlagsZeroNoAlign());
  chirp[length - 1] = C64(1, 0);
  reciprocalChirp[length - 1] = C64(1, 0);
  wTemp[0] = C64(1, 0);
  scaledInput[0] = input[0]*chirp[length];
  for(u32 k = 1; k < length; ++k)
    {      
      c64 chirpVal = chirp[length - 1 + k - 1] * wTemp[k - 1] * wBase;
      c64 conjChirpVal = conjugateC64(chirpVal);
      chirp[length - 1 + k] = chirpVal;
      chirp[length - 1 - k] = chirpVal;
      reciprocalChirp[length - 1 + k] = conjChirpVal;
      reciprocalChirp[length - 1 - k] = conjChirpVal;
      scaledInput[k] = input[k] * chirp[length + k - 1 ];
      
      wTemp[k] = wTemp[k - 1] * wBase * wBase;
    } 

  c64 *fftScaledInput = wTemp;
  fft(fftScaledInput, scaledInput, fftLength);

  c64 *fftReciprocalChirp = scaledInput;
  fft(fftReciprocalChirp, reciprocalChirp, fftLength);
  
  for(u32 index = 0; index < fftLength; ++index)
    {
      fftScaledInput[index] *= fftReciprocalChirp[index];
    }

  c64 *ifftOutput = reciprocalChirp;
  ifft(ifftOutput, fftScaledInput, fftLength);

  for(u32 index = 0; index < length; ++index)
    {
      c64 outVal = invLength * ifftOutput[length + index - 1] * chirp[length + index - 1];
      outputReal[index] = outVal.re;
      outputImag[index] = outVal.im;
    }
}
