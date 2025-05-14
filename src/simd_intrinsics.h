#pragma once

struct WideFloat;
struct WideInt;

static WideFloat wideLoadFloats(r32 *src);
static WideFloat wideSetConstantFloats(r32 src);
static WideFloat wideSetFloats(r32 a, r32 b, r32 c, r32 d);
static void      wideSetLaneFloats(WideFloat *w, r32 val, u32 lane);
static void	 wideStoreFloats(r32 *dest, WideFloat src);
static WideFloat wideAddFloats(WideFloat a, WideFloat b);
static WideFloat wideSubFloats(WideFloat a, WideFloat b);
static WideFloat wideMulFloats(WideFloat a, WideFloat b);
static WideFloat wideMaskFloats(WideFloat a, WideFloat b, WideInt mask);

static WideInt	 wideLoadInts(u32 *src);
static WideInt	 wideSetConstantInts(u32 src);
static WideInt   wideSetInts(u32 a, u32 b, u32 c, u32 d);
static void      wideSetLaneInts(WideInt *w, u32 val, u32 lane);
static void	 wideStoreInts(u32 *dest, WideInt src);
static WideInt	 wideAddInts(WideInt a, WideInt b);
static WideInt	 wideSubInts(WideInt a, WideInt b);
static WideInt	 wideMulInts(WideInt a, WideInt b);
static WideInt   wideAndInts(WideInt a, WideInt b);

#if ARCH_X86 || ARCH_X64

#include <immintrin.h>

struct WideFloat
{
  __m128 val;
};

struct WideInt
{
  __m128i val;
};

static WideFloat
wideLoadFloats(r32 *src)
{
  WideFloat result = {};
  result.val = _mm_load_ps(src);

  return(result);
}

static WideInt
wideLoadInts(u32 *src)
{
  WideInt result = {};
  result.val = _mm_load_si128((__m128i *)src);

  return(result);
}

static WideFloat
wideSetConstantFloats(r32 src)
{
  WideFloat result = {};
  result.val = _mm_set1_ps(src);

  return(result);
}

static WideFloat
wideSetFloats(r32 a, r32 b, r32 c, r32 d)
{
  WideFloat result = {};
  r32 *val = (r32 *)&result.val;
  val[0] = a;
  val[1] = b;
  val[2] = c;
  val[3] = d;

  return(result);
}

static void
wideSetLaneFloats(WideFloat *w, r32 val, u32 lane)
{
  r32 *vals = (r32 *)&w->val;
  vals[lane] = val;
}

static WideInt
wideSetConstantInts(u32 src)
{
  WideInt result = {};
  result.val = _mm_set1_epi32(src);

  return(result);
}

static WideInt
wideSetInts(u32 a, u32 b, u32 c, u32 d)
{
  WideInt result = {};
  u32 *val = (u32 *)&result.val; // TODO: check this is legal
  val[0] = a;
  val[1] = b;
  val[2] = c;
  val[3] = d;

  return(result);
}

static void
wideSetLaneInts(WideInt *w, u32 val, u32 lane)
{
  u32 *vals = (u32 *)&w->val;
  vals[lane]= val;  
}

static void
wideStoreFloats(r32 *dest, WideFloat src)
{
  _mm_store_ps(dest, src.val);
}

static void
wideStoreInts(u32 *dest, WideInt src)
{
  _mm_store_si128((__m128i *)dest, src.val);
}

static WideFloat
wideAddFloats(WideFloat a, WideFloat b)
{
  WideFloat result = {};
  result.val = _mm_add_ps(a.val, b.val);
  
  return(result);
}

static WideInt
wideAddInts(WideInt a, WideInt b)
{
  WideInt result = {};
  result.val = _mm_add_epi32(a.val, b.val);
  
  return(result);
}

static WideFloat
wideSubFloats(WideFloat a, WideFloat b)
{
  WideFloat result = {};
  result.val = _mm_sub_ps(a.val, b.val);
  
  return(result);
}

static WideInt
wideSubInts(WideInt a, WideInt b)
{
  WideInt result = {};
  result.val = _mm_sub_epi32(a.val, b.val);
  
  return(result);
}

static WideFloat
wideMulFloats(WideFloat a, WideFloat b)
{
  WideFloat result = {};
  result.val = _mm_mul_ps(a.val, b.val);
  
  return(result);
}

/* static WideFloat */
/* wideMaskFloats(WideFloat a, WideFloat mask) */
/* {   */
/*   //__m128i aInt = _mm_cvtps_epi32(a.val); */
/*   //__m128i resultInt = _mm_and_si128(aInt, mask.val); */

/*   WideFloat result = {}; */
/*   result.val = _mm_and_ps(a.val, mask.val); */
/*   //result.val = _mm_cvtepi32_ps(resultInt); */
/*   return(result); */
/* } */

static WideFloat
wideMaskFloats(WideFloat a, WideFloat b, WideInt mask)
{
  WideFloat result = {};
  __m128 maskF = _mm_castsi128_ps(mask.val);
  result.val = _mm_or_ps(_mm_and_ps(a.val, maskF),
			 _mm_andnot_ps(b.val, maskF));

  return(result);
}

static WideInt
wideMulInts(WideInt a, WideInt b)
{
  WideInt result = {};
  result.val = _mm_mul_epi32(a.val, b.val);
  
  return(result);
}

static WideInt
wideAndInts(WideInt a, WideInt b)
{
  WideInt result = {};
  result.val = _mm_and_si128(a.val, b.val);

  return(result);
}

#elif ARCH_ARM || ARCH_ARM64

#include <arm_neon.h>

struct WideFloat
{
  float32x4_t val;
};

struct WideInt
{
  uint32x4_t val;
};

static WideFloat
wideLoadFloats(r32 *src)
{
  WideFloat result = {};
  result.val = vld1q_f32(src);

  return(result);
}

static WideInt
wideLoadInts(u32 *src)
{
  WideInt result = {};
  result.val = vld1q_u32(src);

  return(result);
}

static WideFloat
wideSetConstantFloats(r32 src)
{
  WideFloat result = {};
  result.val = vdupq_n_f32(src);

  return(result);
}

static WideInt
wideSetConstantInts(u32 src)
{
  WideInt result = {};
  result.val = vdupq_n_u32(src);

  return(result);
}

static WideFloat
wideSetFloats(r32 a, r32 b, r32 c, r32 d)
{
  WideFloat result = {};
  result.val = {a, b, c, d};

  return(result);
}

static WideInt
wideSetInts(u32 a, u32 b, u32 c, u32 d)
{
  WideInt result = {};
  result.val = {a, b, c, d};

  return(result);
}

static void
wideSetLaneFloats(WideFloat *w, r32 val, u32 lane)
{
  float32x4_t temp = vdupq_n_f32(val);
  
  u32 mask[4] = {};
  mask[lane] = U32_MAX;
  uint32x4_t maskVec = vld1q_u32(mask);
    
  w->val = vbslq_f32(maskVec, temp, w->val);
}

static void
wideSetLaneInts(WideInt *w, u32 val, u32 lane)
{
  uint32x4_t temp = vdupq_n_u32(val);
  
  u32 mask[4] = {};
  mask[lane] = U32_MAX;
  uint32x4_t maskVec = vld1q_u32(mask);
    
  w->val = vbslq_u32(maskVec, temp, w->val);
}

static void
wideStoreFloats(r32 *dest, WideFloat src)
{
  vst1q_f32(dest, src.val);
}

static void
wideStoreInts(u32 *dest, WideInt src)
{
  vst1q_u32(dest, src.val);
}

static WideFloat
wideAddFloats(WideFloat a, WideFloat b)
{
  WideFloat result = {};
  result.val = vaddq_f32(a.val, b.val);

  return(result);
}

static WideInt
wideAddInts(WideInt a, WideInt b)
{
  WideInt result = {};
  result.val = vaddq_u32(a.val, b.val);

  return(result);
}

static WideFloat
wideSubFloats(WideFloat a, WideFloat b)
{
  WideFloat result = {};
  result.val = vsubq_f32(a.val, b.val);

  return(result);
}

static WideInt
wideSubInts(WideInt a, WideInt b)
{
  WideInt result = {};
  result.val = vsubq_u32(a.val, b.val);

  return(result);
}

static WideFloat
wideMulFloats(WideFloat a, WideFloat b)
{
  WideFloat result = {};
  result.val = vmulq_f32(a.val, b.val);

  return(result);
}

static WideInt
wideMulInts(WideInt a, WideInt b)
{
  WideInt result = {};
  result.val = vmulq_u32(a.val, b.val);

  return(result);
}

static WideFloat
wideMaskFloats(WideFloat a, WideFloat b, WideInt mask)
{
  WideFloat result = {};
  result.val = vbslq_f32(mask.val, a.val, b.val);

  return(result);
}

#else
#error ERROR: unsupported architecture
#endif
