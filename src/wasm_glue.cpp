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

static void platformLogf(char *fmt, ...);

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

// BEHOLD: the most C++ code in the codebase
#define SCOPE_HAS_LOCK(lock) ScopedLock GLUE(GLUE(scopedLock__, __FUNCTION__), __LINE__)(&lock);
struct ScopedLock
{
  explicit ScopedLock(volatile u32 *lock)
  {
    _lock = lock;
    while(gsAtomicCompareAndSwap(_lock, 0, 1) != 0) {}
  }
  
  ~ScopedLock()
  {
    while(gsAtomicCompareAndSwap(_lock, 1, 0) != 1) {}
  }

  volatile u32 *_lock;
};

enum ThreadID
{
  ThreadID_render,
  ThreadID_audio,
  ThreadID_Count,
};

static const char *threadIDNames[ThreadID_Count] = {
  "render",
  "audio",
};

struct ThreadState
{
  b32 initialized;
  ThreadID id;
};
thread_var ThreadState threadState;

#define DEBUG_POINTER_XLIST\
  X(render__quads)\
  X(audio__grainMixBuffersL)	   \
  X(audio__grainMixBuffersR)	   \
  X(audio__genericOutputFramesL)   \
  X(audio__genericOutputFramesR)    

enum DebugPointer
{
#define X(name) GLUE(DebugPointer_, name),
  DEBUG_POINTER_XLIST
#undef X
  DebugPointer_Count,
};

struct DebugPointerData
{
  void *addr;
  void *value;
  char *name;
  b32 initialized;
  b32 printedIfShit;
};

#define DEBUG_POINTER(name) (debugPointers + GLUE(DebugPointer_, name))
static DebugPointerData debugPointers[DebugPointer_Count] = {};

#define DEBUG_POINTER_INITIALIZE(name_, p) do {	\
  DebugPointerData *data = DEBUG_POINTER(name_);\
  if(!data->initialized) {\
    data->addr = &p;\
    data->value = (void*)p;				\
    data->name = STRINGIFY(GLUE(DebugPointer_, name_));	\
    data->initialized = 1;\
  }\
} while(0)

#define DEBUG_POINTER_CHECK(name_) do {\
  DebugPointerData *data = DEBUG_POINTER(name_);\
  if(data->initialized) {\
    uintptr_t valueAtAddr = *(uintptr_t*)data->addr;\
    uintptr_t initializedValue = (uintptr_t)data->value;\
    if(valueAtAddr != initializedValue) {\
      logFormatString("%s(%p) fucked up at %s:%s:\n"\
		      "  expected: 0x%x, got: 0x%x",\
		      data->name, data->addr, STRINGIFY(__FILE__), STRINGIFY(__LINE__), \
		      initializedValue, valueAtAddr);\
      data->printedIfShit = 1;\
    }\
  }\
} while(0)

extern u8 __global_base;
extern u8 __heap_base;
extern u8 __heap_end;
static volatile u32 memoryLock;
static volatile usz __mem_used = 0;
static usz __mem_capacity = 0;
//static Arena *arenaFreelist = 0;
static Arena *firstFreeArena;
static Arena *lastFreeArena;

#define ARENA_MIN_ALLOCATION_SIZE KILOBYTES(64)

Arena*
gsArenaAcquire(usz size)
{
  usz allocSize = MAX(size, ARENA_MIN_ALLOCATION_SIZE);
  //ASSERT(__mem_used + allocSize <= __mem_capacity);

  if(threadState.initialized)
    {
      ASSERT(threadState.id < ThreadID_Count);
      platformLogf("[%s] acquiring arena with capacity of %u bytes\n",
		   threadIDNames[threadState.id], allocSize);
    }
  else
    {
      platformLogf("[UNKNOWN] acquiring arena with capacity of %u bytes\n",
		   allocSize);
    }

#if 0
  Arena *result = 0;
  {
    SCOPE_HAS_LOCK(memoryLock);
    if(firstFreeArena)
      {
	// NOTE: pop off of freelist
	// TODO: use some kind of tree/heap to accelerate search
	Arena *toRemove = 0;
	for(Arena *free = lastFreeArena; free; free = free->prev)
	  {
	    usz freeBase = INT_FROM_PTR(free);
	    usz freeSize = free->capacity - freeBase;
	    if(allocSize <= freeSize)
	      {
		result = free;
		allocSize = freeSize;
		toRemove = free;
		break;
	      }
	  }
	if(toRemove)
	  {
	    ASSERT(result != 0);
	    DLL_REMOVE__NP(firstFreeArena, lastFreeArena, toRemove, current, prev);
	  }
      }
    if(result == 0)
      {
	void *base = (void*)(&__heap_base + __mem_used);
	__mem_used += allocSize;
	result = (Arena*)base;
      }
  }
#else
  usz mem_pos = gsAtomicAdd((volatile u32*)&__mem_used, allocSize);
  void *base = (void*)(&__heap_base + mem_pos);
  // void *base = (void*)(&__heap_base + __mem_used);
  // __mem_used += allocSize;
  Arena *result = (Arena*)base;
#endif
  if(result)
    {
      result->current = result;
      result->prev = 0;
      result->base = 0;
      result->capacity = allocSize;
      result->pos = ARENA_HEADER_SIZE; 
    }
  return(result);
}

// TODO: implement
void
gsArenaDiscard(Arena *arena)
{
  if(threadState.initialized)
    {
      ASSERT(threadState.id < ThreadID_Count);
      platformLogf("[%s] discarding arena with capacity of %u bytes\n",
		   threadIDNames[threadState.id], arena->capacity);
    }
  else
    {
      platformLogf("[UNKNOWN] discarding arena with capacity of %u bytes\n",
		   arena->capacity);
    }
  // NOTE: push onto freelist
  // TODO: merge contiguous blocks
  //DLL_PUSH_BACK__NP(firstFreeArena, lastFreeArena, arena, current, prev);
  return;
}

#include "plugin.cpp"

// internal state/functions

static volatile u32 inputLock;
struct WasmState
{
  Arena *arena;

  RenderCommands renderCommands;

  PluginAudioBuffer audioBuffer;
  
  PluginMemory pluginMemory;

  PluginInput input[2];
  u32 currentInputIdx;  

#if BUILD_LOGGING
  PluginLogger logger;  
#endif
};

static WasmState *wasmState = 0;

proc_export WasmState*
wasmInit(usz memorySize)
{
  __mem_capacity = INT_FROM_PTR(&__heap_end) - INT_FROM_PTR(&__heap_base);
   platformLogf("WASM memory capacity: %u\n", __mem_capacity);
  UNUSED(memorySize);
  Arena *arena = gsArenaAcquire(KILOBYTES(144));
  
  WasmState *result = 0;
  if(arena != 0) {
    platformLogf("arena: %u\n  base=%u\n  capacity=%u\n  used=%u\n",
		 arena, arena->base, arena->capacity, arena->pos);
    result = arenaPushStruct(arena, WasmState, arenaFlagsZeroAlign(sizeof(void*)));
    if(result) {
      result->arena = arena;
      
      result->pluginMemory.host = PluginHost_web;
      
#if BUILD_LOGGING
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
      DEBUG_POINTER_INITIALIZE(render__quads, result->renderCommands.quads);

      result->audioBuffer.inputBufferCapacity = 512;
      result->audioBuffer.inputBuffer[0] =
	arenaPushArray(arena, result->audioBuffer.inputBufferCapacity, r32);
      result->audioBuffer.inputBuffer[1] =
	arenaPushArray(arena, result->audioBuffer.inputBufferCapacity, r32);
      platformLogf("arena used post input samples push: %u\n", arenaGetPos(arena));

      result->audioBuffer.outputBufferCapacity = 512;
      result->audioBuffer.outputBuffer[0] =
	arenaPushArray(arena, result->audioBuffer.outputBufferCapacity, r32);
      result->audioBuffer.outputBuffer[1] =
	arenaPushArray(arena, result->audioBuffer.outputBufferCapacity, r32);
      platformLogf("arena used post output samples push: %u\n", arenaGetPos(arena));
      DEBUG_POINTER_INITIALIZE(audio__genericOutputFramesL, result->audioBuffer.outputBuffer[0]);
      DEBUG_POINTER_INITIALIZE(audio__genericOutputFramesR, result->audioBuffer.outputBuffer[1]);

      if(gsInitializePluginState(&result->pluginMemory)) {
	wasmState = result;      
      }
    }
  }
  
  return(result);
}

static void
platformLogf(char *fmt, ...)
{
  char buf[1024];
  usz bufCount = ARRAY_COUNT(buf);
  // TemporaryMemory scratch = arenaGetScratch(0, 0);
  // usz bufCount = 1024;
  // char *buf = arenaPushArray(scratch.arena, bufCount, char, arenaFlagsZeroNoAlign());

  va_list vaArgs;
  va_start(vaArgs, fmt);
  usz actualSize = gs_vsnprintf(buf, bufCount, fmt, vaArgs);
  ASSERT(actualSize < bufCount);
  va_end(vaArgs);

  platformLog(buf);
  
  //arenaReleaseScratch(scratch);
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
  void *result = (void*)wasmState->audioBuffer.outputBuffer[channelIdx];
  return(result);
}

proc_export void
setMousePosition(r32 x, r32 y)
{
  SCOPE_HAS_LOCK(inputLock);
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
  SCOPE_HAS_LOCK(inputLock);
  PluginInput *input = wasmState->input + wasmState->currentInputIdx;
  ButtonState *button = input->mouseState.buttons + buttonIdx;
  processButtonPress(button, pressed);
}

proc_export void
processKeyboardButtonPress(u32 keyIdx, b32 pressed)
{
  SCOPE_HAS_LOCK(inputLock);
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
  if(!threadState.initialized)
    {
      threadState.id = ThreadID_render;
      threadState.initialized = 1;
    }
  platformLogf("memory used = %u\n", __mem_used);

  PluginMemory *pluginMemory = &wasmState->pluginMemory;
  PluginInput *input = wasmState->input + wasmState->currentInputIdx;  
  RenderCommands *renderCommands = &wasmState->renderCommands;

  renderBeginCommands(renderCommands, width, height);
  {
    SCOPE_HAS_LOCK(inputLock);
    gsRenderNewFrame(pluginMemory, input, renderCommands);  

#if BUILD_LOGGING
    {
      SCOPE_HAS_LOCK(wasmState->logger.mutex);
      for(String8Node *node = wasmState->logger.log.first; node; node = node->next)
	{
	  String8 string = node->string;
	  if(string.str)
	    {
	      platformLog((const char*)string.str);
	    }
	  ZERO_STRUCT(&wasmState->logger.log);
	  arenaEnd(wasmState->logger.logArena);
	}
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
  if(!threadState.initialized)
    {
      threadState.id = ThreadID_audio;
      threadState.initialized = 1;
      //platformLog("audioProcess() test\n");
    }

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
