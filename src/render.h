struct Vertex
{
  v2 vPos;
  v2 uv;
  v4 color;
};

static inline Vertex
makeVertex(v2 vPos, v2 uv, v4 color)
{
  Vertex result = {};
  result.vPos = vPos;
  result.uv = uv;
  result.color = color;

  return(result);
}
/*
struct Texture
{
  LoadedBitmap *bitmap;
  RangeU32 vertexRange;
};
*/
struct TexturedQuad
{
  Vertex vertices[4];
  LoadedBitmap *texture;
};

struct TexturedTriangle
{
  Vertex vertices[3];
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

struct RenderCommands
{
  u32 widthInPixels;
  u32 heightInPixels;
  
  Arena *allocator;

  u32 quadCount;
  u32 quadCapacity;
  TexturedQuad *quads;

  u32 triangleCount;
  u32 triangleCapacity;
  TexturedTriangle *triangles;

  bool generateNewTextures;
};

static inline void
renderBeginCommands(RenderCommands *commands, Arena *perFrameAllocator)
{  
  commands->allocator = perFrameAllocator;
  
  commands->quadCount = 0;
  commands->quadCapacity = 128;
  commands->quads = arenaPushArray(commands->allocator, commands->quadCapacity, TexturedQuad);

  commands->triangleCount = 0;
  commands->triangleCapacity = 32;
  commands->triangles = arenaPushArray(commands->allocator, commands->triangleCapacity, TexturedTriangle);  
}

static inline void
renderEndCommands(RenderCommands *commands)
{
  commands->allocator = 0;
  
  commands->quadCount = 0;
  commands->quadCapacity = 0;
  commands->quads = 0;

  commands->triangleCount = 0;
  commands->triangleCapacity = 0;
  commands->triangles = 0;

  commands->generateNewTextures = false;
}
