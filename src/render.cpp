#ifdef JUCE_GL
using namespace juce::gl;
#endif

static void
renderCommands(RenderCommands *commands)
{
  glBegin(GL_TRIANGLES);

  u32 *indices = commands->indices++;
  for(u32 i = 0; i < commands->indexCount; ++i)
    {
      u32 index = *indices++;
      Vertex vertex = commands->vertices[index];

      glColor4fv(vertex.color.E);
      glVertex2fv(vertex.vPos.E);
    }
  
  glEnd();

  commands->vertexCount = 0;
  commands->indexCount = 0;
}
