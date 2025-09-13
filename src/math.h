#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

//#include <math.h>
extern r32 gs_fabsf(r32 num);
extern r32 gs_sqrtf(r32 num);
extern r32 gs_sinf(r32 num);
extern r32 gs_cosf(r32 num);
extern r32 gs_powf(r32 base, r32 exp);

#define GS_PI 3.141592653589793
#define GS_TAU (2.0*GS_PI)

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
  //return(fabsf(val));
  return(gs_fabsf(val));
}

inline r32
Sqrt(r32 val)
{
  //return(sqrtf(val));
  return(gs_sqrtf(val));
}

inline r32
Sin(r32 angle)
{
  //return(sinf(angle));
  return(gs_sinf(angle));
}

inline r32
Cos(r32 angle)
{
  //return(cosf(angle));
  return(gs_cosf(angle));
}

inline r32
Pow(r32 base, r32 exp)
{
  return(gs_powf(base, exp));
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
makeRotationMatrixXY(v2 c, r32 a)
{
  mat4 result = {};
  r32 ca = Cos(a);
  r32 sa = Sin(a);  
  result.r1 = V4(ca, -sa, 0,  c.x*(1.f - ca) + c.y*sa);
  result.r2 = V4(sa,  ca, 0, -c.x*sa + c.y*(1.f - ca));
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

inline r32
mapToRange(r32 ratio, RangeR32 range)
{
  r32 result = ratio*getLength(range) + range.min;

  return(result);
}

inline r32
percentageFromRange(r32 val, RangeR32 range)
{
  r32 percentage = (val - range.min)/getLength(range);

  return(percentage);
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
