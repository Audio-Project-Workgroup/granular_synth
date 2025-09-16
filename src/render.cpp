#ifdef JUCE_GL
using namespace juce::gl;
#endif

enum GLShaderKind
{
  GLShaderKind_vertex,
  GLShaderKind_fragment,
  GLShaderKind_COUNT,
};

static GLenum shaderKinds[GLShaderKind_COUNT] =
{
  GL_VERTEX_SHADER,
  GL_FRAGMENT_SHADER,
};

static char vertexShaderSource[] = R"VSRC(
#version 330
 
in vec2 v_pattern;
in vec4 v_min_max;
in vec4 v_color;
in float v_angle;
in float v_level;

uniform mat4 transform;

out vec4 f_color;

void main() {
  vec2 center = (v_min_max.zw + v_min_max.xy)*0.5;
  vec2 half_dim = (v_min_max.zw - v_min_max.xy)*0.5;

  float cosa = cos(v_angle);
  float sina = sin(v_angle);
  vec2 rot_pattern = vec2(cosa*v_pattern.x - sina*v_pattern.y, sina*v_pattern.x + cosa*v_pattern.y);

  vec2 pos = center + half_dim*rot_pattern;
  gl_Position = transform * vec4(pos, v_level, 1.0);
  f_color = v_color;
}
)VSRC";

static char fragmentShaderSource[] = R"FSRC(
#version 330

in vec4 f_color;

void main() {
  gl_FragColor = f_color;
}
)FSRC";

static inline GLuint
makeGLShader(const char *source, GLShaderKind kind)
{
  GLuint shader = glCreateShader(shaderKinds[kind]);
  glShaderSource(shader, 1, &source, 0);
  glCompileShader(shader);

  GLint compiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if(compiled != GL_TRUE)
    {
      GLsizei logLength;
      GLchar msg[1024];
      glGetShaderInfoLog(shader, ARRAY_COUNT(msg), &logLength, message);
      fprintf(stderr, "shader compilation failed: %s\n", msg);
    }
  
  return(shader);
}

static inline GLuint
makeGLprogram(GLuint vs, GLuint fs)
{
  GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);

  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if(linked != GL_TRUE)
    {
      GLsizei logLength;
      GLchar msg[1024];
      glGetProgramInfoLog(program, ARRAY_COUNT(msg), &logLength, message);
      fprintf(stderr, "program compilation failed: %s\n", msg);
    }

  return(program);
}

static inline GLState
makeGLState(void)
{
  GLuint vertexShader = makeGLShader(vertexShaderSource, GLShaderKind_vertex);
  GLuint fragmentShader = makeGLShader(fragmentShaderSource, GLShaderKind_fragment);
  GLuint program = (vertexShader, fragmentShader);
  glUseProgram(program);
  
  GLuint vPatternPosition = glGetAttribLocation("v_pattern");
  GLuint vMinMaxPosition  = glGetAttribLocation("v_min_max");
  GLuint vColorPosition   = glGetAttribLocation("v_color");
  GLuint vAnglePosition   = glGetAttribLocation("v_angle");
  GLuint vLevelPosition   = glGetAttribLocation("v_level");

  GLuint transformPosition = glGetUniformLocation("transform");

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  GLsizei bufferDataSize = KILOBYTES(32);
  glBufferData(GL_ARRAY_BUFFER, bufferDataSize, 0, GL_STREAM_DRAW);
  r32 patternData[] = {
    -1.f, -1.f,
    1.f, -1.f,
    -1.f, 1.f,
    1.f, 1.f,
  };
  u32 patternDataSize = sizeof(patternData);
  glBufferSubData(GL_ARRAY_BUFFER, 0, patternDataSize, patternData);

  GLState result = {};
  result.vPatternPosition = vPatternPosition;
  result.vMinMaxPosition  = vMinMaxPosition;
  result.vColorPosition   = vColorPosition;
  result.vAnglePosition   = vAnglePosition;
  result.vLevelPosition   = vLevelPosition;
  result.transformPosition = transformPosition;
  result.vao = vao;
  result.vbo = vbo;
  result.vertexShader = vertexShader;
  result.fragmentShader = fragmentShader;
  result.program = program;
  result.quadDataOffset = patternDataSize;
  result.quadCapacity = bufferDataSize / sizeof(R_Quad);
  
  return(result);
}

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
#if 1

  glEnableVertexAttribArray(commands->glState->vPatternPosition);
  glVertexAttribDivisor(commands->glState->vPatternPosition, 0);
  glVertexAttribPointer(commands->glState->vPatternPosition, 2, GL_FLOAT, 0, 0, 0);

  glEnableVertexAttribArray(commands->glState->vMinMaxPosition);
  glVertexAttribDivisor(commands->glState->vMinMaxPosition, 1);
  glVertexAttribPointer(commands->glState->vMinMaxPosition, 4, GL_FLOAT, 0, sizeof(R_Quad), 32);

  glEnableVertexAttribArray(commands->glState->vColorPosition);
  glVertexAttribPointer(commands->glState->vColorPosition, 1);
  glVertexAttribPointer(commands->glState->vColorPosition, 4, GL_UNSIGNED_BYTE, 0, sizeof(R_Quad), 32 + 16);

  glEnableVertexAttribArray(commands->glState->vAnglePosition);
  glVertexAttribPointer(commands->glState->vAnglePosition, 1);
  glVertexAttribPointer(commands->glState->vAnglePosition, 1, GL_FLOAT, 0, sizeof(R_Quad), 32 + 16 + 4);

  glEnableVertexAttribArray(commands->glState->vLevelPosition);
  glVertexAttribPointer(commands->glState->vLevelPosition, 1);
  glVertexAttribPointer(commands->glState->vLevelPosition, 1, GL_FLOAT, 0, sizeof(R_Quad), 32 + 16 + 4 + 4);

  for(R_Batch *batch = commands->first; batch; batch = batch->next)
    {
      renderBindTexture(batch->texture, commands->generateNewTextures);
      glBufferSubData(GL_ARRAY_BUFFER, commands->glState->quadDataOffset, batch->quadCount*sizeof(R_Quad), batch->quads);
      glDrawArraysInstanced(GL_TRINAGLE_STRIP, 0, 4, batch->quadCount);
    }

#else
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
	  // v2 vertexCenter = 0.5f*(quad->vertices[3].vPos + quad->vertices[0].vPos);
	  // mat4 rotationMatrix = makeRotationMatrixXY(quad->angle);	  
	  // mat4 translationMatrix = makeTranslationMatrix(vertexCenter);
	  // mat4 invTranslationMatrix = makeTranslationMatrix(-vertexCenter);
	  
	  // mat4 mvMat = transpose(invTranslationMatrix*rotationMatrix*translationMatrix);
	  mat4 mvMat = quad->matrix;
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
	  
	  // v2 vertexCenter = 0.5f*(quad->vertices[3].vPos + quad->vertices[0].vPos);
	  // mat4 rotationMatrix = makeRotationMatrixXY(quad->angle);	  
	  // mat4 translationMatrix = makeTranslationMatrix(vertexCenter);
	  // mat4 invTranslationMatrix = makeTranslationMatrix(-vertexCenter);
	  
	  // mat4 mvMat = transpose(invTranslationMatrix*rotationMatrix*translationMatrix);
	  mat4 mvMat = quad->matrix;
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
#endif
}
