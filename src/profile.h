#pragma once

static u64 getCpuCounter(void);
static u64 getCpuCounterFreq(void);

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

static u64
getCpuCounter(void)
{
# error TODO: implement
}

#endif

struct ProfileInfo
{
  String8 label;
  
  u64 tscElapsed;
  u64 tscElapsed_children;
  u64 tscElapsed_root;
  
  u64 hitCount;  
};

// TODO: it would be sick to put all the profile infos in their own section, and
//       append into that section every time a scoped profiler is declared. So
//       we would have a "dynamic array at compile time"
struct Profiler
{
  ProfileInfo profileInfos[4096];
  u64 startCycles;
  u64 endCycles;
};
static Profiler globalProfiler;
static u32 globalProfilerCurrentParentIdx;

#define PROFILE_BLOCK(name) ScopedProfiler GLUE(profiler__, __LINE__)(STR8_LIT(name), __COUNTER__ + 1)
#define PROFILE_FUNCTION() PROFILE_BLOCK(__func__)
struct ScopedProfiler
{
  explicit ScopedProfiler(String8 _label, u32 _idx)
  {
    label = _label;
    profileInfoIdx = _idx;
    parentInfoIdx = globalProfilerCurrentParentIdx;
    start = getCpuCounter();
    
    ProfileInfo *info = globalProfiler.profileInfos + profileInfoIdx;
    oldTscElapsedAtRoot = info->tscElapsed_root;

    globalProfilerCurrentParentIdx = profileInfoIdx;
  }

  ~ScopedProfiler()
  {
    u64 end = getCpuCounter();
    u64 tscElapsed = end - start;

    globalProfilerCurrentParentIdx = parentInfoIdx;

    ProfileInfo *parentInfo = globalProfiler.profileInfos + parentInfoIdx;
    parentInfo->tscElapsed_children += tscElapsed;

    ProfileInfo *info = globalProfiler.profileInfos + profileInfoIdx;    
    info->label = label;
    info->tscElapsed += tscElapsed;
    info->tscElapsed_root = oldTscElapsedAtRoot + tscElapsed;
    info->hitCount += 1;
  }

  String8 label;
  u64 start;
  u64 oldTscElapsedAtRoot;
  u32 profileInfoIdx;
  u32 parentInfoIdx;
};

static inline void
profileBegin(void)
{
  globalProfiler.startCycles = getCpuCounter();
}

static inline String8List
profileEnd(Arena *arena)
{
  globalProfiler.endCycles = getCpuCounter();
  u64 totalTscElapsed = globalProfiler.endCycles - globalProfiler.startCycles;

  String8List result = {};
  for(u32 profileInfoIdx = 0;
      profileInfoIdx < ARRAY_COUNT(globalProfiler.profileInfos);
      ++profileInfoIdx)
    {
      ProfileInfo *info = globalProfiler.profileInfos + profileInfoIdx;
      if(info->tscElapsed)
	{
	  u64 tscElapsed_self = info->tscElapsed - info->tscElapsed_children;
	  r64 percent = 100.0 * ((r64)tscElapsed_self/(r64)totalTscElapsed);
	  if(info->tscElapsed_root != tscElapsed_self)
	    {
	      r64 percentWithChildren = 100.0 * ((r64)info->tscElapsed_root/(r64)totalTscElapsed);
	      stringListPushFormat(arena, &result,
				   "%.*s[%llu]: %llu(%llu) (%.2f%%, %.2f%% w/ children)",
				   (int)info->label.size, info->label.str,
				   info->hitCount,
				   tscElapsed_self,
				   info->tscElapsed_root,
				   percent,
				   percentWithChildren);
	    }
	  else
	    {
	      stringListPushFormat(arena, &result,
				   "%.*s[%llu]: %llu(%llu) (%.2f%%)",
				   (int)info->label.size, info->label.str,
				   info->hitCount,
				   tscElapsed_self,
				   info->tscElapsed_root,
				   percent);
	    }
	}
    }

  return(result);
}
