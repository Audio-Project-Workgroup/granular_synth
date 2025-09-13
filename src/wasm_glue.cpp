//#include "types.h"
#include "common.h"

#define proc_export C_LINKAGE

proc_export int
fmadd(int a, int b, int c) {
  return(a * b + c);
}

extern u8 __heap_base;

static usz memoryAt = 0;
static u8 *memory = &__heap_base;

struct R_Quad {
  v2 min;
  v2 max;
};

static u32 quadCount = 0;
static u32 quadsToDraw = 3;
static R_Quad *quads = 0;
static s32 width = 0;
static s32 height = 0;

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
  
  quadCount = 0;
  memoryAt = 0;
  quads = 0;
  return(result);
}
