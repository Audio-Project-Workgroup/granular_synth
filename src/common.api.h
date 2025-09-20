#define PLATFORM_API_XLIST\
  X(ReadEntireFile, Buffer, (char *filename, Arena *allocator))	\
  X(FreeFileMemory, void, (Buffer file, Arena *allocator))\
  X(WriteEntireFile, void, (char *filename, Buffer file))\
  X(GetPathToModule, String8, (void *handleToModule, void *functionInModule, Arena *allocator))\
  X(GetCurrentTimestamp, u64, (void))\
  X(RunModel, void*, (r32 *inputData, s64 inputLength))\
  X(Abs, r32, (r32 num))\
  X(Sqrt, r32, (r32 num))\
  X(Sin, r32, (r32 num))\
  X(Cos, r32, (r32 num))\
  X(Pow, r32, (r32 base, r32 exp))\
  X(AllocateMemory, void*, (usz size))\
  X(FreeMemory, void, (void *memory, usz size))\
  X(ArenaAcquire, Arena*, (usz size))\
  X(ArenaDiscard, void, (Arena *arena))\
  X(AtomicLoad, u32, (volatile u32 *src))\
  X(AtomicLoadPointer, void*, (volatile void **src))\
  X(AtomicStore, u32, (volatile u32 *dest, u32 value))\
  X(AtomicAdd, u32, (volatile u32 *addend, u32 value))\
  X(AtomicCompareAndSwap, u32, (volatile u32 *value, u32 oldValue, u32 newVal))\
  X(AtomicCompareAndSwapPointers, void*, (volatile void **value, void *oldVal, void *newVal))

struct Arena;
#define X(name, ret, args) typedef ret GS_##name args;
PLATFORM_API_XLIST
#undef X

struct PlatformAPI
{
#define X(name, ret, args) GS_##name *gs##name;
  PLATFORM_API_XLIST
#undef X
};

// TODO: get rid of this, and set function pointers in plugin layer when it is
//       not being included in the host translation unit
//extern PlatformAPI globalPlatform;

#if !defined(HOST_LAYER)
#define X(name, ret, args) static GS_##name *gs##name = 0;
PLATFORM_API_XLIST
#undef X
#endif
