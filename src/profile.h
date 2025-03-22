#pragma once

static u64 getCpuCounter(void);

#if ARCH_X86 || ARCH_X64

#if OS_WINDOWS
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

static u64
getCpuCounter(void)
{
  return(__rdtsc());
}

#elif ARCH_ARM || ARCH_ARM64

#endif
