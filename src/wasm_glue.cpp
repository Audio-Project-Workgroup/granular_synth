#define HOST_LAYER
#include "common.h"

/*
  TODO:
  - implement platform fucntions
  - load the texture atlas
  - call the plugin functions
*/

#define proc_import(name, ret, args) __attribute__((import_module("env"), import_name(#name))) ret name args
#define proc_export C_LINKAGE

// imported functions
proc_import(platformLog, void, (const char *msg));
proc_import(platformRand, r32, (r32 min, r32 max));
proc_import(platformAbs,  r32, (r32 val));
proc_import(platformSqrt, r32, (r32 val));
proc_import(platformSin,  r32, (r32 val));
proc_import(platformCos,  r32, (r32 val));
proc_import(platformPow,  r32, (r32 base, r32 exp));

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

// NOTE: this should NEVER get called
String8
gsGetPathToModule(void *handleToModule, void *functionInModule, Arena *allocator)
{
  String8 result = {};
  UNUSED(handleToModule);
  UNUSED(functionInModule);
  UNUSED(allocator);
  return(result);
}

extern u8 __heap_base;
extern u8 __heap_end;
static usz __mem_used = 0;
static usz __mem_capacity = 0;
#define ARENA_MIN_ALLOCATION_SIZE KILOBYTES(64)

Arena*
gsArenaAcquire(usz size)
{
  usz allocSize = MAX(size, ARENA_MIN_ALLOCATION_SIZE);
  //ASSERT(__mem_used + allocSize <= __mem_capacity);

  void *base = (void*)(&__heap_base + __mem_used);
  __mem_used += allocSize;
  Arena *result = (Arena*)base;
  result->current = result;
  result->prev = 0;
  result->base = 0;
  result->capacity = allocSize;
  result->pos = ARENA_HEADER_SIZE;
  return(result);
}

// TODO: implement
void
gsArenaDiscard(Arena *arena)
{  
  platformLogf("Discarding arena with capacity of %u bytes\n", arena->capacity);
  return;
}

u32
gsAtomicLoad(volatile u32 *src)
{
  u32 result = __atomic_load_n(src, __ATOMIC_SEQ_CST);
  return(result);
}

void*
gsAtomicLoadPtr(volatile void **src)
{
  usz resultInt = __atomic_load_n((volatile usz*)src, __ATOMIC_SEQ_CST);
  void *result = PTR_FROM_INT(resultInt);
  return(result);
}

u32
gsAtomicStore(volatile u32 *dest, u32 value)
{
  u32 result = gsAtomicLoad(dest);
  __atomic_store_n(dest, value, __ATOMIC_SEQ_CST);
  return(result);
}

u32
gsAtomicAdd(volatile u32 *addend, u32 value)
{
  u32 result = __atomic_fetch_add(addend, value, __ATOMIC_SEQ_CST);
  return(result);
}

u32
gsAtomicCompareAndSwap(volatile u32 *value, u32 oldVal, u32 newVal)
{
  u32 *expected = &oldVal;
  b32 success = __atomic_compare_exchange_n(value, expected, newVal,
					    0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
  u32 result = success ? oldVal : newVal;
  return(result);
}

void*
gsAtomicCompareAndSwapPointers(volatile void **value, void *oldVal, void *newVal)
{
  usz *expected = (usz*)&oldVal;
  b32 success = __atomic_compare_exchange_n((volatile usz*)value, expected, INT_FROM_PTR(newVal),
					    0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
  void *result = success ? oldVal : newVal;
  return(result);
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
gsRand(RangeR32 range)
{
  return(platformRand(range.min, range.max));
}

r32
gsAbs(r32 num)
{
  return(platformAbs(num));
}

r32
gsSqrt(r32 num)
{
  return(platformSqrt(num));
}

r32
gsSin(r32 num)
{
  return(platformSin(num));
}

r32
gsCos(r32 num)
{
  return(platformCos(num));
}

r32
gsPow(r32 base, r32 exp)
{
  return(platformPow(base, exp));
}

#include "plugin.cpp"

// internal state/functions

// proc_export int
// fmadd(int a, int b, int c) {
//   return(a * b + c);
// }

struct WasmState
{
  Arena *arena;
  
  // R_Quad *quads;
  // u32 quadCount;
  // u32 quadCapacity;
  RenderCommands renderCommands;

  // r32 *inputSamples[2];
  // u32 inputSampleCapacity;

  // r32 *outputSamples[2];
  // u32 outputSampleCapacity;
  PluginAudioBuffer audioBuffer;
  
  PluginMemory pluginMemory;

  PluginInput input[2];
  u32 newInputIndex;

  // DEBUG:
  u32 quadsToDraw;
  b32 calledLog;

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
  __mem_capacity = INT_FROM_PTR(&__heap_end) - INT_FROM_PTR(&__heap_base);
  platformLogf("WASM memory capacity: %u\n", __mem_capacity);
  UNUSED(memorySize);
  Arena *arena = gsArenaAcquire(0);
  
  WasmState *result = 0;
  if(arena) {
    platformLogf("arena: %u\n  base=%u\n  capacity=%u\n  used=%u\n",
		 arena, arena->base, arena->capacity, arena->pos);
    result = arenaPushStruct(arena, WasmState, arenaFlagsZeroAlign(sizeof(void*)));
    if(result) {
      result->arena = arena;

      result->pluginMemory.host = PluginHost_web;
      // result->pluginMemory.platformAPI.gsReadEntireFile		      = 0;
      // result->pluginMemory.platformAPI.gsFreeFileMemory		      = 0;
      // result->pluginMemory.platformAPI.gsWriteEntireFile	      = 0;
      // result->pluginMemory.platformAPI.gsGetPathToModule	      = 0;
      // result->pluginMemory.platformAPI.gsGetCurrentTimestamp	      = 0;
      // result->pluginMemory.platformAPI.gsRunModel		      = 0;
      // result->pluginMemory.platformAPI.gsRand			      = gsRand;
      // result->pluginMemory.platformAPI.gsAbs			      = gsAbs;
      // result->pluginMemory.platformAPI.gsSqrt			      = gsSqrt;
      // result->pluginMemory.platformAPI.gsSin			      = gsCos;
      // result->pluginMemory.platformAPI.gsCos			      = gsCos;
      // result->pluginMemory.platformAPI.gsPow			      = gsPow;
      // result->pluginMemory.platformAPI.gsAllocateMemory		      = 0;
      // result->pluginMemory.platformAPI.gsFreeMemory		      = 0;
      // result->pluginMemory.platformAPI.gsArenaAcquire		      = gsArenaAcquire;
      // result->pluginMemory.platformAPI.gsArenaDiscard		      = gsArenaDiscard;
      // result->pluginMemory.platformAPI.gsAtomicLoad		      = gsAtomicLoad;
      // result->pluginMemory.platformAPI.gsAtomicLoadPointer	      = gsAtomicLoadPointer;
      // result->pluginMemory.platformAPI.gsAtomicStore		      = gsAtomicStore;
      // result->pluginMemory.platformAPI.gsAtomicAdd		      = gsAtomicAdd;
      // result->pluginMemory.platformAPI.gsAtomicCompareAndSwap	      = gsAtomicCompareAndSwap;
      // result->pluginMemory.platformAPI.gsAtomicCompareAndSwapPointers = gsAtomicCompareAndSwapPointers;

      result->renderCommands.quadCapacity = 2048;      
      result->renderCommands.quads = arenaPushArray(arena, result->renderCommands.quadCapacity, R_Quad);
      platformLogf("arena used post quads push: %u\n", arenaGetPos(arena));

      result->audioBuffer.inputBufferCapacity = 2048;
      result->audioBuffer.inputBuffer[0] =
	arenaPushArray(arena, result->audioBuffer.inputBufferCapacity, r32);
      result->audioBuffer.inputBuffer[1] =
	arenaPushArray(arena, result->audioBuffer.inputBufferCapacity, r32);
      platformLogf("arena used post input samples push: %u\n", arenaGetPos(arena));

      result->audioBuffer.outputBufferCapacity = 2048;
      result->audioBuffer.outputBuffer[0] =
	arenaPushArray(arena, result->audioBuffer.outputBufferCapacity, r32);
      result->audioBuffer.outputBuffer[1] =
	arenaPushArray(arena, result->audioBuffer.outputBufferCapacity, r32);
      platformLogf("arena used post output samples push: %u\n", arenaGetPos(arena));      

      result->quadsToDraw = 3;

      result->phase = 0.f;
      result->freq = 440.f;
      result->volume = 0.1f;
      
      wasmState = result;      
    }
  }
  
  return(result);
}

static R_Quad*
pushQuad(v2 min, v2 max, v4 color, r32 angle, r32 level)
{
  R_Quad *result = 0;
  if(wasmState->renderCommands.quadCount < wasmState->renderCommands.quadCapacity) {
    result = wasmState->renderCommands.quads + wasmState->renderCommands.quadCount++;
    if(result) {
      result->min = min;
      result->max = max;
      result->uvMin = V2(0.f, 0.f);
      result->uvMin = V2(1.f, 1.f);
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
  R_Quad *result = wasmState->renderCommands.quads;
  return(result);
}

proc_export void**
getInputSamplesOffset(void)
{
  void **result = (void**)wasmState->audioBuffer.inputBuffer;
  return(result);
}

proc_export void**
getOutputSamplesOffset(void)
{
  void **result = (void**)wasmState->audioBuffer.outputBuffer;
  return(result);
}

static void
logQuad(R_Quad* quad)
{
  platformLogf("quad %u:\n"
	       "  min: (%.2f, %.2f)\n"
	       "  max: (%.2f, %.2f)\n"
	       "  uvMin: (%.2f, %.2f)\n"
	       "  uvMax: (%.2f, %.2f)\n"
	       "  color: 0x%X\n"
	       "  angle: %.2f\n"
	       "  level: %.2f\n",
	       quad, quad->min, quad->max, quad->uvMin, quad->uvMax,
	       quad->color, quad->angle, quad->level);
}

proc_export u32
drawQuads(s32 width, s32 height, r32 timestamp)
{
#if 1

  PluginMemory *pluginMemory = &wasmState->pluginMemory;
  PluginInput *newInput = wasmState->input + wasmState->newInputIndex;
  RenderCommands *renderCommands = &wasmState->renderCommands;

  renderBeginCommands(renderCommands, width, height);

  gsRenderNewFrame(pluginMemory, newInput, renderCommands);

  u32 result = renderCommands->quadCount;

  renderEndCommands(renderCommands);

  return(result);

#else
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
    R_Quad *quad = pushQuad(min, max, colors[quadIdx], angle, 0.f);
    UNUSED(quad);
    //logQuad(quad);
    min += advance;
    max += advance;
  }

  if(!wasmState->calledLog) {
    platformLog("drew quads");
    wasmState->calledLog = 1;
  }

  u32 result = wasmState->renderCommands.quadCount;
  wasmState->renderCommands.quadCount = 0;
  return(result);
#endif
}

proc_export r32*
outputSamples(u32 channelIdx, u32 sampleCount, r32 samplePeriod)
{
  r32 *outputSamples = (r32*)(wasmState->audioBuffer.outputBuffer[channelIdx]);

  ASSERT(sampleCount <= wasmState->audioBuffer.outputBufferCapacity);
  r32 phaseInc = GS_TAU * wasmState->freq * samplePeriod;
  for(u32 sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx) {
    r32 sineSample = wasmState->volume * platformSin(wasmState->phase);
    outputSamples[sampleIdx] = sineSample;

    wasmState->phase += phaseInc;
    if(wasmState->phase >= GS_TAU) wasmState->phase -= GS_TAU;
  }

  return(outputSamples);
}
