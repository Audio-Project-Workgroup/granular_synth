#if 0

static inline void
renderPushIndex(RenderCommands *commands, u32 index)
{
  ASSERT(commands->indexCount < ARRAY_COUNT(commands->indices));
  commands->indices[commands->indexCount++] = index;
}

static inline void
renderPushVertex(RenderCommands *commands, v2 vPos, v2 uv, v4 color)
{
  ASSERT(commands->vertexCount < ARRAY_COUNT(commands->vertices));
  
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
  renderPushVertex(commands, bottomLeft + V2(dim.x, 0), V2(0, 0), color);
  renderPushVertex(commands, bottomLeft + V2(0, dim.y), V2(0, 0), color);

  renderPushIndex(commands, startingVertexCount + 1);
  renderPushIndex(commands, startingVertexCount + 2);
  renderPushVertex(commands, bottomLeft + dim, V2(0, 0), color);
}

static inline void
renderPushBitmap(RenderCommands *commands, LoadedBitmap *bitmap, Rect2 rect, v4 color = V4(1, 1, 1, 1))
{
  v2 bottomLeft = rect.min;
  v2 dim = getDim(rect);

  Texture *texture = commands->textures + commands->textureCount++;
  texture->vertexRange.min = commands->vertexCount;
  texture->bitmap = bitmap;
  
  renderPushVertex(commands, bottomLeft, V2(0, 0), color);
  renderPushVertex(commands, bottomLeft + V2(dim.x, 0), V2(1, 0), color);
  renderPushVertex(commands, bottomLeft + V2(0, dim.y), V2(0, 1), color);
  
  renderPushIndex(commands, texture->vertexRange.min + 1);
  renderPushIndex(commands, texture->vertexRange.min + 2);
  renderPushVertex(commands, bottomLeft + dim, V2(1, 1), color);
  
  texture->vertexRange.max = commands->vertexCount;
}

#else

static inline void // NOTE: not textured
renderPushQuad(RenderCommands *commands, Rect2 rect, v4 color = V4(1, 1, 1, 1))
{
  ASSERT(commands->quadCount < commands->quadCapacity);
  TexturedQuad *quad = commands->quads + commands->quadCount++;

  v2 bottomLeft = rect.min;
  v2 dim = getDim(rect);
  quad->vertices[0] = {bottomLeft, V2(0, 0), color};
  quad->vertices[1] = {bottomLeft + V2(dim.x, 0), V2(0, 0), color};
  quad->vertices[2] = {bottomLeft + V2(0, dim.y), V2(0, 0), color};
  quad->vertices[3] = {bottomLeft + dim, V2(0, 0), color};
}

static inline void // NOTE: textured
renderPushQuad(RenderCommands *commands, Rect2 rect, LoadedBitmap *texture, v4 color = V4(1, 1, 1, 1))
{
  ASSERT(commands->quadCount < commands->quadCapacity);
  TexturedQuad *quad = commands->quads + commands->quadCount++;

  v2 bottomLeft = rect.min;
  v2 dim = getDim(rect);
  quad->vertices[0] = {bottomLeft, V2(0, 0), color};
  quad->vertices[1] = {bottomLeft + V2(dim.x, 0), V2(1, 0), color};
  quad->vertices[2] = {bottomLeft + V2(0, dim.y), V2(0, 1), color};
  quad->vertices[3] = {bottomLeft + dim, V2(1, 1), color};
  
  quad->texture = texture;
}

static inline void
renderPushTriangle(RenderCommands *commands, Vertex v1, Vertex v2, Vertex v3, LoadedBitmap *texture = 0)
{
  ASSERT(commands->triangleCount < commands->triangleCapacity);
  TexturedTriangle *triangle = commands->triangles + commands->triangleCount++;
  
  triangle->vertices[0] = v1;
  triangle->vertices[1] = v2;
  triangle->vertices[2] = v3;

  triangle->texture = texture;
}

#endif

