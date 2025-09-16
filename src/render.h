struct TexturedVertex
{
  v2 vPos;
  v2 uv;
  //v4 color;
  u32 color;
};

struct Vertex
{
  v2 vPos;
  //v4 color;
  u32 color;
};

static inline TexturedVertex
makeTexturedVertex(v2 vPos, v2 uv, v4 color)
{
  TexturedVertex result = {};
  result.vPos = vPos;
  result.uv = uv;
  result.color = colorU32FromV4(color);

  return(result);
}

static inline Vertex
makeVertex(v2 vPos, v4 color)
{
  Vertex result = {};
  result.vPos = vPos;
  result.color = colorU32FromV4(color);

  return(result);
}

enum RenderLevel
{
  RenderLevel_background,
  //RenderLevel_boxBackground,
  RenderLevel_control,
  
  RenderLevel_front,
};

struct Quad
{
  RenderLevel level;
  r32 angle;
  mat4 matrix;

  Vertex vertices[4];  
};

struct TexturedQuad
{
  RenderLevel level;
  r32 angle;
  mat4 matrix;
  
  TexturedVertex vertices[4];
  LoadedBitmap *texture;
};

struct TexturedTriangle
{
  TexturedVertex vertices[3];
  LoadedBitmap *texture;
};

struct GLState
{
  u32 vPatternPosition;
  u32 vMinMaxPosition;
  u32 vColorPosition;
  u32 vAnglePosition;
  u32 vLevelPosition;

  u32 transformPosition;

  u32 vao;
  u32 vbo;

  u32 vertexShader;
  u32 fragmentShader;
  u32 program;

  u32 quadDataOffset;
  u32 quadCapacity;
};

struct R_Quad
{
  v2 min;
  v2 max;

  u32 color;
  r32 angle;
  r32 level;
};

struct R_Batch
{
  R_Batch *next;
  
  u32 quadCapacity;
  u32 quadCount;
  R_Quad *quads;

  LoadedBitmap *texture;
};

enum RenderCursorState
{
  CursorState_default,
  CursorState_hArrow,
  CursorState_vArrow,
  CursorState_hand,
  CursorState_text,
};

struct RenderCommands
{
  u32 widthInPixels;
  u32 heightInPixels;
  
  Arena *allocator;

  RenderCursorState cursorState;

  bool outputAudioDeviceChanged;
  u32 selectedOutputAudioDeviceIndex;
  bool inputAudioDeviceChanged;
  u32 selectedInputAudioDeviceIndex;

  GLState *glState;

  R_Batch *first;
  R_Batch *last;
  u32 batchCount;
  u32 totalQuadCount;

  LoadedBitmap *whiteTexture;

  /* u32 texturedQuadCount; */
  /* u32 texturedQuadCapacity; */
  /* TexturedQuad *texturedQuads; */

  /* u32 quadCount; */
  /* u32 quadCapacity; */
  /* Quad *quads; */

  /* u32 triangleCount; */
  /* u32 triangleCapacity; */
  /* TexturedTriangle *triangles; */  

  bool generateNewTextures;
  bool windowResized;
};

static inline void
renderBeginCommands(RenderCommands *commands, Arena *perFrameAllocator)
{  
  commands->allocator = perFrameAllocator;

  commands->cursorState = CursorState_default;
  
  commands->outputAudioDeviceChanged = false;
  commands->inputAudioDeviceChanged = false;  

  /* commands->texturedQuadCount = 0; */
  /* commands->texturedQuadCapacity = THOUSAND(4); */
  /* commands->texturedQuads = arenaPushArray(commands->allocator, commands->texturedQuadCapacity, TexturedQuad); */

  /* commands->quadCount = 0; */
  /* commands->quadCapacity = THOUSAND(16); */
  /* commands->quads = arenaPushArray(commands->allocator, commands->quadCapacity, Quad); */

  /* commands->triangleCount = 0; */
  /* commands->triangleCapacity = 32; */
  /* commands->triangles = arenaPushArray(commands->allocator, commands->triangleCapacity, TexturedTriangle);   */
}

static inline void
renderEndCommands(RenderCommands *commands)
{
  arenaEnd(commands->allocator);
  commands->allocator = 0;

  commands->cursorState = CursorState_default;
  
  /* commands->quadCount = 0; */
  /* commands->quadCapacity = 0; */
  /* commands->quads = 0; */

  /* commands->texturedQuadCount = 0; */
  /* commands->texturedQuadCapacity = 0; */
  /* commands->texturedQuads = 0; */

  /* commands->triangleCount = 0; */
  /* commands->triangleCapacity = 0; */
  /* commands->triangles = 0; */

  commands->first = 0;
  commands->last = 0;
  commands->batchCount = 0;
  commands->totalQuadCount = 0;

  commands->generateNewTextures = false;
}
