#define HOST_LAYER
#include "common.h"

/*
  TODO:
  - implement platform fucntions
  - remove file_formats.h from the plugin, and instead do asset preprocessing
*/

#define proc_import(name, ret, args) __attribute__((import_module("env"), import_name(#name))) ret name args
#define proc_export C_LINKAGE

// imported functions
proc_import(platformLog, void, (const char *msg));
proc_import(platformSin, r32, (r32 val));
proc_import(platformCos, r32, (r32 val));

// TODO: implement
Arena*
gsArenaAcquire(usz capacity)
{
  UNUSED(capacity);
  return(0);
}

void
gsArenaDiscard(Arena *arena)
{
  UNUSED(arena);
  return;
}

u32
gsAtomicLoad(volatile u32 *src)
{
  UNUSED(src);
  return(0);
}

void*
gsAtomicLoadPtr(volatile void **src)
{
  UNUSED(src);
  return(0);
}

u32
gsAtomicStore(volatile u32 *dest, u32 value)
{
  UNUSED(dest);
  UNUSED(value);  
  return(0);
}

u32
gsAtomicAdd(volatile u32 *addend, u32 value)
{
  UNUSED(addend);
  UNUSED(value);  
  return(0);
}

u32
gsAtomicCompareAndSwap(volatile u32 *value, u32 oldVal, u32 newVal)
{
  UNUSED(value);
  UNUSED(oldVal);
  UNUSED(newVal);  
  return(0);
}

void*
gsAtomicCompareAndSwapPointers(volatile void **value, void *oldVal, void *newVal)
{
  UNUSED(value);
  UNUSED(oldVal);
  UNUSED(newVal);  
  return(0);
}

Buffer
gsReadEntireFile(char *filename, Arena *allocator)
{
  Buffer result = {};
  UNUSED(filename);
  UNUSED(allocator);
  return(result);
}

void
gsFreeFileMemory(Buffer file, Arena *allocator)
{
  UNUSED(file);
  UNUSED(allocator);
  return;
}

void
gsWriteEntireFile(char *filename, Buffer file)
{
  UNUSED(filename);
  UNUSED(file);
  return;
}

r32
gs_sinf(r32 num)
{
  return(platformSin(num));
}

r32
gs_cosf(r32 num)
{
  return(platformCos(num));
}

#include "plugin.cpp"

// internal and exported functions
static void
platformLogf(const char *fmt, ...)
{
  char buf[1024];

  va_list vaArgs;
  va_start(vaArgs, fmt);
  gs_vsnprintf(buf, ARRAY_COUNT(buf), fmt, vaArgs);
  va_end(vaArgs);

  platformLog(buf);
}

proc_export int
fmadd(int a, int b, int c) {
  return(a * b + c);
}

extern u8 __heap_base;

// struct R_Quad
// {
//   v2 min;
//   v2 max;

//   u32 color;
//   r32 angle;
//   r32 level;
// };

struct WasmState
{
  Arena *arena;
  
  R_Quad *quads;
  u32 quadCount;
  u32 quadCapacity;

  u32 quadsToDraw;
  b32 calledLog;

  r32 *outputSamples[2];
  u32 outputSampleCapacity;

  r32 phase;
  r32 freq;
  r32 volume;
};

static WasmState *wasmState = 0;

proc_export WasmState*
wasmInit(usz memorySize)
{
  // Arena *arena = (Arena*)&__heap_base;
  // arena->base = &__heap_base;
  // arena->capacity = memorySize;
  // arena->used = sizeof(Arena);
  UNUSED(memorySize);
  Arena *arena = gsArenaAcquire(0);
  
  WasmState *result = 0;
  if(arena) {
    platformLogf("arena: %u\n  base=%u\n  capacity=%u\n  used=%u\n",
		 arena, arena->base, arena->capacity, arena->pos);
    result = arenaPushStruct(arena, WasmState, arenaFlagsZeroAlign(sizeof(void*)));
    if(result) {
      result->arena = arena;
      result->quadCapacity = 1024;      
      result->quads = arenaPushArray(arena, result->quadCapacity, R_Quad);
      result->quadsToDraw = 3;

      result->outputSampleCapacity = 2048;
      result->outputSamples[0] = arenaPushArray(arena, result->outputSampleCapacity, r32);
      result->outputSamples[1] = arenaPushArray(arena, result->outputSampleCapacity, r32);
      result->phase = 0.f;
      result->freq = 440.f;
      result->volume = 0.1f;
      
      wasmState = result;
      platformLogf("wasmState: %X\n  quads=%X\n  quadCount=%u\n  quadCapacity=%u\n",
		   wasmState, wasmState->quads, wasmState->quadCount, wasmState->quadCapacity);
    }
  }
  
  return(result);
}

static R_Quad*
pushQuad(v2 min, v2 max, v4 color, r32 angle, r32 level)
{
  R_Quad *result = 0;
  if(wasmState->quadCount < wasmState->quadCapacity) {
    result = wasmState->quads + wasmState->quadCount++;
    if(result) {
      result->min = min;
      result->max = max;
      result->color = colorU32FromV4(color);
      result->angle = angle;
      result->level = level;
    }
  }
  
  return(result);
}

proc_export R_Quad*
getQuadsOffset(void)
{
  R_Quad *result = wasmState->quads;
  return(result);
}

proc_export u32
drawQuads(s32 width, s32 height, r32 timestamp)
{
  r32 timestampSec = timestamp * 0.001f;

  v4 colors[3] = {
    V4(1, 0, 0, 1),
    V4(0, 1, 0, 1),
    V4(0, 0, 1, 1),
  };

  u32 quadsToDraw = wasmState->quadsToDraw;
  v2 dim = V2((r32)width/(r32)(quadsToDraw + 1), (r32)height/(r32)(quadsToDraw + 1));
  v2 advance = V2((r32)width/(r32)quadsToDraw, (r32)height/(r32)quadsToDraw);
  v2 min = V2(0, 0);
  v2 max = dim;
  for(u32 quadIdx = 0; quadIdx < wasmState->quadsToDraw; ++quadIdx) {
    r32 angularVelocity = (r32)(quadIdx + 1);
    r32 angle = timestampSec * angularVelocity;
    pushQuad(min, max, colors[quadIdx], angle, 0.f);
    min += advance;
    max += advance;
  }

  if(!wasmState->calledLog) {
    platformLog("drew quads");
    wasmState->calledLog = 1;
  }

  u32 result = wasmState->quadCount;
  wasmState->quadCount = 0;
  return(result);
}

proc_export r32*
outputSamples(u32 channelIdx, u32 sampleCount, r32 samplePeriod)
{
  ASSERT(sampleCount <= wasmState->outputSampleCapacity);
  r32 phaseInc = GS_TAU * wasmState->freq * samplePeriod;
  for(u32 sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx) {
    r32 sineSample = wasmState->volume * platformSin(wasmState->phase);
    wasmState->outputSamples[channelIdx][sampleIdx] = sineSample;

    wasmState->phase += phaseInc;
    if(wasmState->phase >= GS_TAU) wasmState->phase -= GS_TAU;
  }

  r32 *result = wasmState->outputSamples[channelIdx];
  return(result);
}
