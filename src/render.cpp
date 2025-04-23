#ifdef JUCE_GL
using namespace juce::gl;
#endif

static inline void
renderTexturedVertex(TexturedVertex v)
{  
  //glColor4fv(v.color.E);
  glColor4ubv((const GLubyte *)&v.color);
  glTexCoord2fv(v.uv.E);
  glVertex2fv(v.vPos.E);
}

static inline void
renderVertex(Vertex v)
{
  //glColor4fv(v.color.E);
  glColor4ubv((const GLubyte *)&v.color);
  glVertex2fv(v.vPos.E);
}

static void
renderBindTexture(LoadedBitmap *texture, bool generateNewTextures)
{
  if(texture->glHandle && !generateNewTextures)
    {
      glBindTexture(GL_TEXTURE_2D, texture->glHandle);
    }
  else
    {
      glGenTextures(1, &texture->glHandle);
      glBindTexture(GL_TEXTURE_2D, texture->glHandle);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		   texture->width, texture->height, 0,
		   GL_BGRA_EXT, GL_UNSIGNED_BYTE, texture->pixels);
	      
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);	      
    }
}

static void
renderSortTexturedQuads(TexturedQuad *quads, u32 quadCount)
{
  // NOTE: bubble sort
  for(u32 outer = 0; outer < quadCount; ++outer)
    {
      bool quadsSorted = true;
      for(u32 inner = 0; inner < quadCount - 1; ++inner)
	{
	  TexturedQuad *q0 = quads + inner;
	  TexturedQuad *q1 = quads + inner + 1;
	  
	  if(q0->level > q1->level)
	    {
	      TexturedQuad temp = *q0;
	      *q0 = *q1;
	      *q1 = temp;

	      quadsSorted = false;
	    }
	}

      if(quadsSorted) break;
    }
}

static void
renderSortQuads(Quad *quads, u32 quadCount)
{
  // NOTE: bubble sort
  for(u32 outer = 0; outer < quadCount; ++outer)
    {
      bool quadsSorted = true;
      for(u32 inner = 0; inner < quadCount - 1; ++inner)
	{
	  Quad *q0 = quads + inner;
	  Quad *q1 = quads + inner + 1;

	  if(q0->level > q1->level)
	    {
	      Quad temp = *q0;
	      *q0 = *q1;
	      *q1 = temp;

	      quadsSorted = false;
	    }
	}

      if(quadsSorted) break;
    }
}

static void
renderCommands(RenderCommands *commands)
{      
  bool textureEnabled = true;

  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();

  mat4 projectionMatrix = transpose(makeProjectionMatrix(commands->widthInPixels, commands->heightInPixels));
  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf(projectionMatrix.E);

  renderSortTexturedQuads(commands->texturedQuads, commands->texturedQuadCount);
  glEnable(GL_TEXTURE_2D);
  for(u32 quadIndex = 0; quadIndex < commands->texturedQuadCount; ++quadIndex)
    {
      TexturedQuad *quad = commands->texturedQuads + quadIndex;
      renderBindTexture(quad->texture, commands->generateNewTextures);

      if(quad->angle != 0.f)
	{
	  v2 vertexCenter = 0.5f*(quad->vertices[3].vPos + quad->vertices[0].vPos);
	  mat4 rotationMatrix = makeRotationMatrixXY(quad->angle);	  
	  mat4 translationMatrix = makeTranslationMatrix(vertexCenter);
	  mat4 invTranslationMatrix = makeTranslationMatrix(-vertexCenter);
	  
	  mat4 mvMat = transpose(invTranslationMatrix*rotationMatrix*translationMatrix);
	  glMatrixMode(GL_MODELVIEW);
	  glLoadMatrixf(mvMat.E);
	}
      else
	{
	  glMatrixMode(GL_MODELVIEW);
	  glLoadIdentity();
	}      
      
      glBegin(GL_TRIANGLES);
      {
	renderTexturedVertex(quad->vertices[0]);
	renderTexturedVertex(quad->vertices[1]);
	renderTexturedVertex(quad->vertices[3]);
	
	renderTexturedVertex(quad->vertices[0]);
	renderTexturedVertex(quad->vertices[3]);
	renderTexturedVertex(quad->vertices[2]);
      }
      glEnd();     
    }  
  
  renderSortQuads(commands->quads, commands->quadCount);

  bool drawing = true;  
  glDisable(GL_TEXTURE_2D);
  glBegin(GL_TRIANGLES);
  for(u32 quadIndex = 0; quadIndex < commands->quadCount; ++quadIndex)
    {
      Quad *quad = commands->quads + quadIndex;
      
      if(quad->angle != 0.f)
	{
	  drawing = false;
	  glEnd();
	  
	  v2 vertexCenter = 0.5f*(quad->vertices[3].vPos + quad->vertices[0].vPos);
	  mat4 rotationMatrix = makeRotationMatrixXY(quad->angle);	  
	  mat4 translationMatrix = makeTranslationMatrix(vertexCenter);
	  mat4 invTranslationMatrix = makeTranslationMatrix(-vertexCenter);
	  
	  mat4 mvMat = transpose(invTranslationMatrix*rotationMatrix*translationMatrix);
	  glMatrixMode(GL_MODELVIEW);
	  glLoadMatrixf(mvMat.E);
	}
      else
	{
	  drawing = false;
	  glEnd();
	  
	  glMatrixMode(GL_MODELVIEW);
	  glLoadIdentity();
	}

      if(!drawing)
	{
	  drawing = true;
	  glBegin(GL_TRIANGLES);	  
	}

      renderVertex(quad->vertices[0]);
      renderVertex(quad->vertices[1]);
      renderVertex(quad->vertices[3]);
      
      renderVertex(quad->vertices[0]);
      renderVertex(quad->vertices[3]);
      renderVertex(quad->vertices[2]);
    }
  glEnd();

  for(u32 triangleIndex = 0; triangleIndex < commands->triangleCount; ++triangleIndex)
    {
      TexturedTriangle *triangle = commands->triangles + triangleIndex;
      if(triangle->texture)
	{
	  renderBindTexture(triangle->texture, commands->generateNewTextures);
	}
      else
	{
	  glDisable(GL_TEXTURE_2D);
	  textureEnabled = false;
	}
      
      glBegin(GL_TRIANGLES);
      {
	renderTexturedVertex(triangle->vertices[0]);
	renderTexturedVertex(triangle->vertices[1]);
	renderTexturedVertex(triangle->vertices[2]);
      }
      glEnd();

      if(!textureEnabled)
	{
	  glEnable(GL_TEXTURE_2D);
	  textureEnabled = true;
	}
    }
  
  renderEndCommands(commands);  
  
#if 0
  GLenum error = glGetError();
  if(error)
    {
      fprintf(stderr, "GL ERROR: %u at renderCommands start\n", error);
    }
  
  for(u32 textureIndex = 0; textureIndex < commands->textureCount; ++textureIndex)
    {
      Texture *texture = commands->textures + textureIndex;
      LoadedBitmap *texBitmap = texture->bitmap;
      if(!texBitmap->glHandle)
	{
	  glGenTextures(1, &texBitmap->glHandle);
	  glBindTexture(GL_TEXTURE_2D, texBitmap->glHandle);
	  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		       texBitmap->width, texBitmap->height, 0,
		       GL_BGRA_EXT, GL_UNSIGNED_BYTE, texBitmap->pixels);
	      
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	  
	  error = glGetError();
	  if(error)
	    {
	      fprintf(stderr, "GL ERROR: %u in renderCommands after generating texture %u\n", error, textureIndex);
	    }
	}
    }

  glBindBuffer(GL_ARRAY_BUFFER, glState->vboID);
  glBufferData(GL_ARRAY_BUFFER, commands->vertexCount*sizeof(Vertex), commands->vertices, GL_STATIC_DRAW);

  glUseProgram(glState->programID);

  glActiveTextre(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, commands->textures[0].bitmap->glHandle);
  glUniform1i(glState->samplerID, 0);

  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, glState->vboID);
  glVertexAttribPointer(0, ARRAY_COUNT(commands->vertices[0].vPos.E),
			GL_FLOAT, GL_FALSE, sizeof(Vertex), &OFFSET_OF(Vertex, vpos));

  glEnableVertexAttribArray(1);
  //glBindBuffer(GL_ARRAY_BUFFER, glState->vboID);
  glVertexAttribPointer(1, ARRAY_COUNT(commands->vertices[0].uv.E),
			GL_FLOAT, GL_FALSE, sizeof(Vertex), &OFFSET_OF(Vertex, uv));

  glEnableVertexAttribArray(2);
  //glBindBuffer(GL_ARRAY_BUFFER, glState->vboID);
  glVertexAttribPointer(2, ARRAY_COUNT(commands->vertices[0].color.E),
			GL_FLOAT, GL_FALSE, sizeof(Vertex), &OFFSET_OF(Vertex, color));

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glState->eboID);
  glDrawElements(GL_TRIANGLES, commands->indexCount, GL_UNSIGNED_INT, 0);

  /*
  bool textureBound = false;
  u32 textureAt = 0;
  Texture *nextTexture = commands->textures + textureAt++;

  //glDisable(GL_TEXTURE_2D);
  glBegin(GL_TRIANGLES);

  u32 *indices = commands->indices++;
  for(u32 i = 0; i < commands->indexCount; ++i)
    {
      u32 index = *indices++;
      Vertex vertex = commands->vertices[index];
      
      if(index == nextTexture->vertexRange.min)
	{
	  ASSERT(!textureBound);
	  //glEnable(GL_TEXTURE_2D);
	  LoadedBitmap *textureBitmap = nextTexture->bitmap;
	  glBindTexture(GL_TEXTURE_2D, textureBitmap->glHandle);

	  error = glGetError();
	  if(error)
	    {
	      fprintf(stderr, "GL ERROR: %u binding texture %u\n", error, textureBitmap->glHandle);
	    }
	  textureBound = true;	  
	}
      else if(index == nextTexture->vertexRange.max)
	{
	  ASSERT(textureBound);	  
	  textureBound = false;
	  //glDisable(GL_TEXTURE_2D);
	  nextTexture = commands->textures + textureAt++;
	}

      glColor4fv(vertex.color.E);
      glTexCoord2fv(vertex.uv.E);
      glVertex2fv(vertex.vPos.E);
    }
  
  glEnd();  

  commands->vertexCount = 0;
  commands->indexCount = 0;
  */
#endif
}
