//#include "types.h"
#include "common.h"

#define proc_import(name, ret, args) __attribute__((import_module("env"), import_name(#name))) ret name args
#define proc_export C_LINKAGE

// imported functions
proc_import(platformLog, void, (const char *msg));

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

// static usz memoryAt = 0;
// static u8 *memory = &__heap_base;

struct R_Quad
{
  v2 min;
  v2 max;
};

struct WasmState
{
  Arena *arena;
  
  R_Quad *quads;
  u32 quadCount;
  u32 quadCapacity;

  u32 quadsToDraw;
  b32 calledLog;
};

static WasmState *wasmState = 0;

proc_export WasmState*
wasmInit(usz memorySize)
{
  Arena *arena = (Arena*)&__heap_base;
  arena->base = &__heap_base;
  arena->capacity = memorySize;
  arena->used = sizeof(Arena);
  
  WasmState *result = 0;
  if(arena) {
    platformLogf("arena: %X\n  base=%u\n  capacity=%u\n  used=%u\n",
		 arena, arena->base, arena->capacity, arena->used);
    result = arenaPushStruct(arena, WasmState, arenaFlagsZeroAlign(sizeof(void*)));
    if(result) {
      result->arena = arena;
      result->quadCapacity = 1024;      
      result->quads = arenaPushArray(arena, result->quadCapacity, R_Quad);
      result->quadsToDraw = 3;
      
      wasmState = result;
      platformLogf("wasmState: %X\n  quads=%X\n  quadCount=%u\n  quadCapacity=%u\n",
		   wasmState, wasmState->quads, wasmState->quadCount, wasmState->quadCapacity);
    }
  }
  
  return(result);
}

// static u32 quadCount = 0;
// static u32 quadsToDraw = 3;
// static R_Quad *quads = 0;
// static s32 width = 0;
// static s32 height = 0;

// static b32 calledLog = 0;

static R_Quad*
pushQuad(v2 min, v2 max)
{
  R_Quad *result = 0;
  if(wasmState->quadCount < wasmState->quadCapacity) {
    result = wasmState->quads + wasmState->quadCount++;
    if(result) {
      result->min = min;
      result->max = max;
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
drawQuads(s32 width, s32 height)
{
  u32 quadsToDraw = wasmState->quadsToDraw;
  v2 dim = V2((r32)width/(r32)(quadsToDraw + 1), (r32)height/(r32)(quadsToDraw + 1));
  v2 advance = V2((r32)width/(r32)quadsToDraw, (r32)height/(r32)quadsToDraw);
  v2 min = V2(0, 0);
  v2 max = dim;
  for(u32 quadIdx = 0; quadIdx < wasmState->quadsToDraw; ++quadIdx) {
    pushQuad(min, max);
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

#if 0
proc_export R_Quad*
beginDrawQuads(s32 windowWidth, s32 windowHeight)
{
  R_Quad *result = (R_Quad*)(memory + memoryAt);
  if(result) {    
    memoryAt += quadsToDraw * sizeof(R_Quad);
    width = windowWidth;
    height = windowHeight;
  }
  quads = result;
  return(result);
}

proc_export u32
endDrawQuads(void)
{
  r32 widthFrac = (r32)width / (r32)quadsToDraw;
  r32 heightFrac = (r32)height / (r32)quadsToDraw;
  r32 startX = 0.1f * (r32)width;
  r32 startY = 0.1f * (r32)height;
  r32 dimX = (r32)width / (r32)(quadsToDraw + 1);
  r32 dimY = (r32)height / (r32)(quadsToDraw + 1);
  if(quads) {
    while(quadCount < quadsToDraw) {
      u32 quadIdx = quadCount;
      R_Quad *quad = quads + quadCount++;
      quad->min.x = startX + widthFrac*(r32)quadIdx;
      quad->min.y = startY + heightFrac*(r32)quadIdx;
      quad->max.x = startX + dimX + widthFrac*(r32)quadIdx;
      quad->max.y = startY + dimY + heightFrac*(r32)quadIdx;
      // quad->min.y = 100 + quadIdx*300;
      // quad->max.x = 300 + quadIdx*300;
      // quad->max.y = 300 + quadIdx*300;
    }
  }
  u32 result = quadCount;

  if(!calledLog) {
    platformLog("drew quads\n");
    calledLog = 1;
  }

  quadCount = 0;
  memoryAt = 0;
  quads = 0;
  return(result);
}
#endif
