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
  RenderLevel_boxBackground,
  
  RenderLevel_front,
};

struct Quad
{
  RenderLevel level;
  r32 angle;

  Vertex vertices[4];  
};

struct TexturedQuad
{
  RenderLevel level;
  r32 angle;
  
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
  u32 vertexCount;
  //u32 vertexCapacity;
  Vertex vertices[256];

  u32 indexCount;
  //u32 indexCapacity;
  u32 indices[512];

  //u32 textureCount;
  //Texture textures[64];  

  /*
  u32 vboID;
  u32 vaoID;
  u32 eboID;

  u32 samplerID;

  u32 programID;  
  u32 vertexShaderID;
  u32 fragmentShaderID;
  */
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

  u32 texturedQuadCount;
  u32 texturedQuadCapacity;
  TexturedQuad *texturedQuads;

  u32 quadCount;
  u32 quadCapacity;
  Quad *quads;

  u32 triangleCount;
  u32 triangleCapacity;
  TexturedTriangle *triangles;

  bool generateNewTextures;
  bool windowResized;
};

static inline void
renderBeginCommands(RenderCommands *commands, Arena *perFrameAllocator)
{  
  commands->allocator = perFrameAllocator;

  commands->cursorState = CursorState_default;

  commands->texturedQuadCount = 0;
  commands->texturedQuadCapacity = 512;
  commands->texturedQuads = arenaPushArray(commands->allocator, commands->texturedQuadCapacity, TexturedQuad);

  commands->quadCount = 0;
  commands->quadCapacity = THOUSAND(16);
  commands->quads = arenaPushArray(commands->allocator, commands->quadCapacity, Quad);

  commands->triangleCount = 0;
  commands->triangleCapacity = 32;
  commands->triangles = arenaPushArray(commands->allocator, commands->triangleCapacity, TexturedTriangle);  
}

static inline void
renderEndCommands(RenderCommands *commands)
{
  commands->allocator = 0;

  commands->cursorState = CursorState_default;
  
  commands->quadCount = 0;
  commands->quadCapacity = 0;
  commands->quads = 0;

  commands->texturedQuadCount = 0;
  commands->texturedQuadCapacity = 0;
  commands->texturedQuads = 0;

  commands->triangleCount = 0;
  commands->triangleCapacity = 0;
  commands->triangles = 0;

  commands->generateNewTextures = false;
}
