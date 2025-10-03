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

void
gsCopyMemory(void *dest, void *src, usz size)
{
  __builtin_memcpy(dest, src, size);
}

void
gsSetMemory(void *dest, int value, usz size)
{
  __builtin_memset(dest, value, size);
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

struct WasmState
{
  Arena *arena;

  usz stringBufferCapacity;
  usz stringBufferCount;
  u8 *stringBuffer;

  RenderCommands renderCommands;

  PluginAudioBuffer audioBuffer;
  
  PluginMemory pluginMemory;

  PluginInput input[2];
  u32 currentInputIdx;
  volatile u32 inputLock;

#if BUILD_DEBUG
  PluginLogger logger;
#endif

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

#if BUILD_DEBUG
      {
	Arena *logArena = gsArenaAcquire(0);
	result->logger.logArena = logArena;
	result->logger.maxCapacity = logArena->capacity/2;
	result->pluginMemory.logger = &result->logger;
      }
#endif

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

      if(gsInitializePluginState(&result->pluginMemory)) {
	wasmState = result;      
      }
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

proc_export void*
getInputSamplesOffset(int channelIdx)
{
  // TODO: it would be nice to check if the javascript layer is trying to access
  //       more channels that we have, but the input channel count is set
  //       _after_ this setter is called for the first time
  //ASSERT(channelIdx < wasmState->audioBuffer.inputChannelCount);
  void *result = (void*)wasmState->audioBuffer.inputBuffer[channelIdx];
  return(result);
}

proc_export void*
getOutputSamplesOffset(int channelIdx)
{
  //ASSERT(channelIdx < wasmState->audioBuffer.outputChannelCount);
  void *result = wasmState->audioBuffer.outputBuffer[channelIdx];
  return(result);
}

proc_export u8*
getStringBufferOffset(void)
{
  u8 *result = wasmState->stringBuffer;
  return(result);
}

#define SCOPE_HAS_INPUT_LOCK() ScopedInputLock scopedInputLock__##__FUNCTION__;
struct ScopedInputLock
{
  ScopedInputLock()
  {
    while(gsAtomicCompareAndSwap(&wasmState->inputLock, 0, 1)) {}
  }
  
  ~ScopedInputLock()
  {
    while(gsAtomicCompareAndSwap(&wasmState->inputLock, 1, 0)) {}
  }
};

proc_export void
setMousePosition(r32 x, r32 y)
{
  SCOPE_HAS_INPUT_LOCK();
  PluginInput *input = wasmState->input + wasmState->currentInputIdx;
  input->mouseState.position = V2(x, y);
}

static void
processButtonPress(ButtonState *button, b32 pressed)
{
  button->endedDown = pressed;
  ++button->halfTransitionCount;
}

proc_export void
processMouseButtonPress(u32 buttonIdx, b32 pressed)
{
  SCOPE_HAS_INPUT_LOCK();
  PluginInput *input = wasmState->input + wasmState->currentInputIdx;
  ButtonState *button = input->mouseState.buttons + buttonIdx;
  processButtonPress(button, pressed);
}

proc_export void
processKeyboardButtonPress(u32 keyIdx, b32 pressed)
{
  SCOPE_HAS_INPUT_LOCK();
  PluginInput *input = wasmState->input + wasmState->currentInputIdx;
  ButtonState *button = input->keyboardState.keys + keyIdx;
  processButtonPress(button, pressed);
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
  PluginMemory *pluginMemory = &wasmState->pluginMemory;
  PluginInput *input = wasmState->input + wasmState->currentInputIdx;  
  RenderCommands *renderCommands = &wasmState->renderCommands;

  renderBeginCommands(renderCommands, width, height);
  {
    SCOPE_HAS_INPUT_LOCK();
    gsRenderNewFrame(pluginMemory, input, renderCommands);  

#if BUILD_DEBUG
    for(String8Node *node = wasmState->logger.log.first; node; node = node->next)
      {
	String8 string = node->string;
	if(string.str)
	  {
	    platformLog((const char*)string.str);
	    platformLog("\n");
	  }
	ZERO_STRUCT(&wasmState->logger.log);
	arenaEnd(wasmState->logger.logArena);
      }
#endif

    wasmState->currentInputIdx = !wasmState->currentInputIdx;
    PluginInput *newInput = wasmState->input + wasmState->currentInputIdx;
    newInput->mouseState.position = input->mouseState.position;
    for(u32 buttonIdx = 0; buttonIdx < MouseButton_COUNT; ++buttonIdx) {
      newInput->mouseState.buttons[buttonIdx].halfTransitionCount = 0;
      newInput->mouseState.buttons[buttonIdx].endedDown = input->mouseState.buttons[buttonIdx].endedDown;
    }
    for(u32 keyIdx = 0; keyIdx < KeyboardButton_COUNT; ++keyIdx) {
      newInput->keyboardState.keys[keyIdx].halfTransitionCount = 0;
      newInput->keyboardState.keys[keyIdx].endedDown = input->keyboardState.keys[keyIdx].endedDown;
    }
    for(u32 modIdx = 0; modIdx < KeyboardModifier_COUNT; ++modIdx) {
      newInput->keyboardState.modifiers[modIdx].halfTransitionCount = 0;
      newInput->keyboardState.modifiers[modIdx].endedDown = input->keyboardState.modifiers[modIdx].endedDown;
    }
  }
  
  u32 result = renderCommands->quadCount;

  renderEndCommands(renderCommands);   

  return(result);
}

proc_export void
audioProcess(int sampleRate, int inputChannelCount, int inputSampleCount, int outputChannelCount, int outputSampleCount)
{
  PluginMemory *pluginMemory = &wasmState->pluginMemory;
  PluginAudioBuffer *audioBuffer = &wasmState->audioBuffer;

  audioBuffer->outputFormat = AudioFormat_r32;
  audioBuffer->outputSampleRate = sampleRate;
  audioBuffer->outputChannels = outputChannelCount;
  audioBuffer->outputStride = sizeof(r32);

  audioBuffer->inputFormat = AudioFormat_r32;
  audioBuffer->inputSampleRate = sampleRate;
  audioBuffer->inputChannels = inputChannelCount;
  audioBuffer->inputStride = sizeof(r32);

  audioBuffer->framesToWrite = outputSampleCount;

  gsAudioProcess(pluginMemory, audioBuffer);
}
