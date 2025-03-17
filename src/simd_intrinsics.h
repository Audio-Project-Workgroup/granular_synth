#pragma once

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

static WIDE_LOAD_FLOATS(wideLoadFloats)
{
  WideFloat result = {};
  result.val = _mm_load_ps(src);

  return(result);
}

static WIDE_LOAD_INTS(wideLoadInts)
{
  WideInt result = {};
  result.val = _mm_load_si128((__m128i *)src);

  return(result);
}

static WIDE_SET_CONSTANT_FLOATS(wideSetConstantFloats)
{
  WideFloat result = {};
  result.val = _mm_set1_ps(src);

  return(result);
}

static WIDE_SET_CONSTANT_INTS(wideSetConstantInts)
{
  WideInt result = {};
  result.val = _mm_set1_epi32(src);

  return(result);
}

static WIDE_STORE_FLOATS(wideStoreFloats)
{
  _mm_store_ps(dest, src.val);
}

static WIDE_STORE_INTS(wideStoreInts)
{
  _mm_store_si128((__m128i *)dest, src.val);
}

static WIDE_ADD_FLOATS(wideAddFloats)
{
  WideFloat result = {};
  result.val = _mm_add_ps(a.val, b.val);
  
  return(result);
}

static WIDE_ADD_INTS(wideAddInts)
{
  WideInt result = {};
  result.val = _mm_add_epi32(a.val, b.val);
  
  return(result);
}

static WIDE_SUB_FLOATS(wideSubFloats)
{
  WideFloat result = {};
  result.val = _mm_sub_ps(a.val, b.val);
  
  return(result);
}

static WIDE_SUB_INTS(wideSubInts)
{
  WideInt result = {};
  result.val = _mm_sub_epi32(a.val, b.val);
  
  return(result);
}

static WIDE_MUL_FLOATS(wideMulFloats)
{
  WideFloat result = {};
  result.val = _mm_mul_ps(a.val, b.val);
  
  return(result);
}

static WIDE_MUL_INTS(wideMulInts)
{
  WideInt result = {};
  result.val = _mm_mul_epi32(a.val, b.val);
  
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

static WIDE_LOAD_FLOATS(wideLoadFloats)
{
  WideFloat result = {};
  result.val = vld1q_f32(src);

  return(result);
}

static WIDE_LOAD_INTS(wideLoadInts)
{
  WideInt result = {};
  result.val = vld1q_u32(src);

  return(result);
}

static WIDE_SET_CONSTANT_FLOATS(wideSetConstantFloats)
{
  WideFloat result = {};
  result.val = vdupq_n_f32(src);

  return(result);
}

static WIDE_SET_CONSTANT_INTS(wideSetConstantInts)
{
  WideInt result = {};
  result.val = vdupq_n_u32(src);

  return(result);
}

static WIDE_STORE_FLOATS(wideStoreFloats)
{
  vst1q_f32(dest, src.val);
}

static WIDE_STORE_INTS(wideStoreInts)
{
  vst1q_u32(dest, src.val);
}

static WIDE_ADD_FLOATS(wideAddFloats)
{
  WideFloat result = {};
  result.val = vaddq_f32(a.val, b.val);

  return(result);
}

static WIDE_ADD_INTS(wideAddInts)
{
  WideInt result = {};
  result.val = vaddq_u32(a.val, b.val);

  return(result);
}

static WIDE_SUB_FLOATS(wideSubFloats)
{
  WideFloat result = {};
  result.val = vsubq_f32(a.val, b.val);

  return(result);
}

static WIDE_SUB_INTS(wideSubInts)
{
  WideInt result = {};
  result.val = vsubq_u32(a.val, b.val);

  return(result);
}

static WIDE_MUL_FLOATS(wideMulFloats)
{
  WideFloat result = {};
  result.val = vmulq_f32(a.val, b.val);

  return(result);
}

static WIDE_MUL_INTS(wideMulInts)
{
  WideInt result = {};
  result.val = vmulq_u32(a.val, b.val);

  return(result);
}

#else
#error ERROR: unsupported architecture
#endif
