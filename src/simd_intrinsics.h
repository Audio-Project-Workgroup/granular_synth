#pragma once

union WideFloat;
union WideInt;

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

union WideFloat
{
  __m128 val;
  r32 floats[4];
};

union WideInt
{
  __m128i val;
  u32 ints[4];
};

static WideFloat
wideLoadFloats(r32 *src)
{
  WideFloat result = {};
  result.val = _mm_loadu_ps(src);

  return(result);
}

static WideInt
wideLoadInts(u32 *src)
{
  WideInt result = {};
  result.val = _mm_loadu_si128((__m128i *)src);

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
  // r32 *val = (r32 *)&result.val;
  // val[0] = a;
  // val[1] = b;
  // val[2] = c;
  // val[3] = d;
  result.floats[0] = a;
  result.floats[1] = b;
  result.floats[2] = c;
  result.floats[3] = d;

  return(result);
}

static void
wideSetLaneFloats(WideFloat *w, r32 val, u32 lane)
{
  // r32 *vals = (r32 *)&w->val;
  // vals[lane] = val;
  w->floats[lane] = val;
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
  // u32 *val = (u32 *)&result.val; // TODO: check this is legal
  // val[0] = a;
  // val[1] = b;
  // val[2] = c;
  // val[3] = d;
  result.ints[0] = a;
  result.ints[1] = b;
  result.ints[2] = c;
  result.ints[3] = d;

  return(result);
}

static void
wideSetLaneInts(WideInt *w, u32 val, u32 lane)
{
  // u32 *vals = (u32 *)&w->val;
  // vals[lane]= val;
  w->ints[lane] = val;
}

static void
wideStoreFloats(r32 *dest, WideFloat src)
{
  _mm_storeu_ps(dest, src.val);
}

static void
wideStoreInts(u32 *dest, WideInt src)
{
  _mm_storeu_si128((__m128i *)dest, src.val);
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

static WideFloat
wideMaskFloats(WideFloat a, WideFloat b, WideInt mask)
{
  WideFloat result = {};
  __m128 maskF = _mm_castsi128_ps(mask.val);
  result.val = _mm_or_ps(_mm_and_ps(maskF, a.val),
			 _mm_andnot_ps(maskF, b.val));

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

union WideFloat
{
  float32x4_t val;
  r32 floats[4];
};

union WideInt
{
  uint32x4_t val;
  u32 ints[4];
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

#elif ARCH_WASM32 || ARCH_WASM64

#include <wasm_simd128.h>

union WideFloat
{
  v128_t val;
  r32 floats[4];
};

union WideInt
{
  v128_t val;
  u32 ints[4];
};

static WideFloat
wideLoadFloats(r32 *src)
{
  WideFloat result = {};
  result.val = wasm_v128_load(src);
  return(result);
}

static WideFloat
wideSetConstantFloats(r32 src)
{
  WideFloat result = {};
  result.val = wasm_f32x4_make(src, src, src, src);
  return(result);
}

static WideFloat
wideSetFloats(r32 a, r32 b, r32 c, r32 d)
{
  WideFloat result = {};
  result.val = wasm_f32x4_make(a, b, c, d);
  return(result);
}

static void
wideSetLaneFloats(WideFloat *w, r32 val, u32 lane)
{
  // NOTE: this sucks
  // switch(lane)
  //   {
  //   case 0: { w->val = wasm_f32x4_replace_lane(w->val, 0, val); } break;
  //   case 1: { w->val = wasm_f32x4_replace_lane(w->val, 1, val); } break;
  //   case 2: { w->val = wasm_f32x4_replace_lane(w->val, 2, val); } break;
  //   case 3: { w->val = wasm_f32x4_replace_lane(w->val, 3, val); } break;
  //   default: { ASSERT(!"lane index out of bounds"); } break;
  //   }
  w->floats[lane] = val;
}

static void
wideStoreFloats(r32 *dest, WideFloat src)
{
  wasm_v128_store(dest, src.val);
}

static WideFloat
wideAddFloats(WideFloat a, WideFloat b)
{
  WideFloat result = {};
  result.val = wasm_f32x4_add(a.val, b.val);
  return(result);
}

static WideFloat
wideSubFloats(WideFloat a, WideFloat b)
{
  WideFloat result = {};
  result.val = wasm_f32x4_sub(a.val, b.val);
  return(result);
}

static WideFloat
wideMulFloats(WideFloat a, WideFloat b)
{
  WideFloat result = {};
  result.val = wasm_f32x4_mul(a.val, b.val);
  return(result);
}

static WideFloat
wideMaskFloats(WideFloat a, WideFloat b, WideInt mask)
{
  WideFloat result = {};
  result.val = wasm_v128_or(wasm_v128_and(a.val, mask.val),
			    wasm_v128_andnot(b.val, mask.val));
  return(result);
}

static WideInt
wideLoadInts(u32 *src)
{
  WideInt result = {};
  result.val = wasm_v128_load(src);
  return(result);
}

static WideInt
wideSetConstantInts(u32 src)
{
  WideInt result = {};
  result.val = wasm_u32x4_make(src, src, src, src);
  return(result);
}

static WideInt
wideSetInts(u32 a, u32 b, u32 c, u32 d)
{
  WideInt result = {};
  result.val = wasm_u32x4_make(a, b, c, d);
  return(result);
}

static void
wideSetLaneInts(WideInt *w, u32 val, u32 lane)
{
  // NOTE: this still sucks
  // switch(lane)
  //   {
  //   case 0: { w->val = wasm_u32x4_replace_lane(w->val, 0, val); } break;
  //   case 1: { w->val = wasm_u32x4_replace_lane(w->val, 1, val); } break;
  //   case 2: { w->val = wasm_u32x4_replace_lane(w->val, 2, val); } break;
  //   case 3: { w->val = wasm_u32x4_replace_lane(w->val, 3, val); } break;
  //   default: { ASSERT(!"lane index out of bounds"); } break;
  //   }
  w->ints[lane] = val;
}

static void
wideStoreInts(u32 *dest, WideInt src)
{
  wasm_v128_store(dest, src.val);
}

static WideInt
wideAddInts(WideInt a, WideInt b)
{
  WideInt result = {};
  result.val = wasm_i32x4_add(a.val, b.val);
  return(result);
}

static WideInt
wideSubInts(WideInt a, WideInt b)
{
  WideInt result = {};
  result.val = wasm_i32x4_sub(a.val, b.val);
  return(result);
}

static WideInt
wideMulInts(WideInt a, WideInt b)
{
  WideInt result = {};
  result.val = wasm_i32x4_mul(a.val, b.val);
  return(result);
}

static WideInt
wideAndInts(WideInt a, WideInt b)
{
  WideInt result = {};
  result.val = wasm_v128_and(a.val, b.val);
  return(result);
}

#else
// NOTE: default to scalar

struct WideFloat
{
  r32 val;
};

struct WideInt
{
  u32 val;
};

static WideFloat
wideLoadFloats(r32 *src)
{
  WideFloat result = { *src };
  return(result);
}

static WideFloat
wideSetConstantFloats(r32 src)
{
  WideFloat result = { src };
  return(result);
}

// TODO: make sure this is ok
static WideFloat
wideSetFloats(r32 a, r32 b, r32 c, r32 d)
{
  WideFloat result = { a };
  UNUSED(b);
  UNUSED(c);
  UNUSED(d);
  return(result);
}

static void
wideSetLaneFloats(WideFloat *w, r32 val, u32 lane)
{
  w->val = val;
  UNUSED(lane);
}

static void
wideStoreFloats(r32 *dest, WideFloat src)
{
  *dest = src.val;
}

static WideFloat
wideAddFloats(WideFloat a, WideFloat b)
{
  WideFloat result = { a.val + b.val };
  return(result);
}

static WideFloat
wideSubFloats(WideFloat a, WideFloat b)
{
  WideFloat result = { a.val - b.val };
  return(result);
}

static WideFloat
wideMulFloats(WideFloat a, WideFloat b)
{
  WideFloat result = { a.val * b.val };
  return(result);
}

static WideFloat
wideMaskFloats(WideFloat a, WideFloat b, WideInt mask)
{
  //  TODO: implement
  WideFloat result = {};
  return(result);
}

static WideInt
wideLoadInts(u32 *src)
{
  WideInt result = { *src };
  return(result);
}

static WideInt
wideSetConstantInts(u32 src)
{
  WideInt result = { src };
  return(result);
}

static WideInt
wideSetInts(u32 a, u32 b, u32 c, u32 d)
{
  // TODO: make sure this is ok
  WideInt result = { a };
  UNUSED(b);
  UNUSED(c);
  UNUSED(d);
  return(result);
}

static void
wideSetLaneInts(WideInt *w, u32 val, u32 lane)
{
  w->val = val;
  UNUSED(lane);
}

static void
wideStoreInts(u32 *dest, WideInt src)
{
  *dest = src.val;
}

static WideInt
wideAddInts(WideInt a, WideInt b)
{
  WideInt result = { a.val + b.val };
  return(result);
}

static WideInt
wideSubInts(WideInt a, WideInt b)
{
  WideInt result = { a.val - b.val };
  return(result);
}

static WideInt
wideMulInts(WideInt a, WideInt b)
{
  WideInt result = { a.val * b.val };
  return(result);
}

static WideInt
wideAndInts(WideInt a, WideInt b)
{
  WideInt result = { a.val & b.val };
  return(result);
}

#endif

#if LANG_CPP
static inline WideFloat operator+(WideFloat a, WideFloat b) { return(wideAddFloats(a, b)); }
static inline WideFloat operator-(WideFloat a, WideFloat b) { return(wideSubFloats(a, b)); }
static inline WideFloat operator*(WideFloat a, WideFloat b) { return(wideMulFloats(a, b)); }
static inline WideFloat& operator+=(WideFloat& a, WideFloat b) { a = a + b; return(a); }
static inline WideFloat& operator-=(WideFloat& a, WideFloat b) { a = a - b; return(a); }
static inline WideFloat& operator*=(WideFloat& a, WideFloat b) { a = a + b; return(a); }

static inline WideInt operator+(WideInt a, WideInt b) { return(wideAddInts(a, b)); }
static inline WideInt operator-(WideInt a, WideInt b) { return(wideSubInts(a, b)); }
static inline WideInt operator*(WideInt a, WideInt b) { return(wideMulInts(a, b)); }
static inline WideInt operator&(WideInt a, WideInt b) { return(wideAndInts(a, b)); }
static inline WideInt& operator+=(WideInt& a, WideInt b) { a = a + b; return(a); }
static inline WideInt& operator-=(WideInt& a, WideInt b) { a = a - b; return(a); }
static inline WideInt& operator*=(WideInt& a, WideInt b) { a = a * b; return(a); }
static inline WideInt& operator&=(WideInt& a, WideInt b) { a = a & b; return(a); }
#endif
