struct Vertex
{
  v2 vPos;
  v2 uv;
  v4 color;
};

struct RenderCommands
{
  u32 vertexCount;
  u32 vertexCapacity;
  Vertex *vertices;

  u32 indexCount;
  u32 indexCapacity;
  u32 *indices;
};

static inline void
renderPushIndex(RenderCommands *commands, u32 index)
{
  ASSERT(commands->indexCount < commands->indexCapacity);
  commands->indices[commands->indexCount++] = index;
}

static inline void
renderPushVertex(RenderCommands *commands, v2 vPos, v2 uv, v4 color)
{
  ASSERT(commands->vertexCount < commands->vertexCapacity);
  //ASSERT(commands->indexCount < commands->indexCapacity);
  
  //commands->indices[commands->indexCount++] = commands->vertexCount;
  renderPushIndex(commands, commands->vertexCount);
  Vertex *vertex = commands->vertices + commands->vertexCount++;
  vertex->vPos = vPos;
  vertex->uv = uv;
  vertex->color = color;
}

static inline void
renderPushQuad(RenderCommands *commands, Rect2 rect, v4 color)
{
  v2 bottomLeft = rect.min;
  v2 dim = getDim(rect);

  u32 startingVertexCount = commands->vertexCount;
  renderPushVertex(commands, bottomLeft, V2(0, 0), color);
  renderPushVertex(commands, bottomLeft + V2(dim.x, 0), V2(1, 0), color);
  renderPushVertex(commands, bottomLeft + V2(0, dim.y), V2(0, 1), color);

  renderPushIndex(commands, startingVertexCount + 1);
  renderPushIndex(commands, startingVertexCount + 2);
  renderPushVertex(commands, bottomLeft + dim, V2(1, 1), color);
}
