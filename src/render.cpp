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
 
in vec4 pattern;
in vec4 v_min_max;
in vec4 v_uv_min_max;
in vec4 v_color;
in float v_angle;
in float v_level;

uniform mat4 transform;

out vec4 f_color;
out vec2 f_uv;

void main() {
  vec2 v_pattern = pattern.xy;
  vec2 uv_pattern = pattern.zw;

  vec2 center = (v_min_max.zw + v_min_max.xy)*0.5;
  vec2 half_dim = (v_min_max.zw - v_min_max.xy)*0.5;

  vec2 uv_min = v_uv_min_max.xy;
  vec2 uv_dim = v_uv_min_max.zw - v_uv_min_max.xy;

  float cosa = cos(v_angle);
  float sina = sin(v_angle);
  vec2 rot_pattern = vec2(cosa*v_pattern.x - sina*v_pattern.y, sina*v_pattern.x + cosa*v_pattern.y);

  vec2 pos = center + half_dim*rot_pattern;
  gl_Position = transform * vec4(pos, v_level, 1.0);
  f_color = v_color;
  f_uv = uv_min + uv_dim*uv_pattern;
}
)VSRC";

static char fragmentShaderSource[] = R"FSRC(
#version 330

in vec4 f_color;
in vec2 f_uv;

uniform sampler2D atlas;

out vec4 out_color;

void main() {
  vec4 sampled = texture(atlas, f_uv);
  out_color = f_color * sampled;
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
      glGetShaderInfoLog(shader, ARRAY_COUNT(msg), &logLength, msg);
      fprintf(stderr, "shader compilation failed: %s\n", msg);
      ASSERT(!"failed");
    }
  
  return(shader);
}

static inline GLuint
makeGLProgram(GLuint vs, GLuint fs)
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
      glGetProgramInfoLog(program, ARRAY_COUNT(msg), &logLength, msg);
      fprintf(stderr, "program compilation failed: %s\n", msg);
      ASSERT(!"failed");
    }

  return(program);
}

static inline GLState*
makeGLState(Arena *arena, usz quadCapacity)
{
  GLuint vertexShader = makeGLShader(vertexShaderSource, GLShaderKind_vertex);
  GLuint fragmentShader = makeGLShader(fragmentShaderSource, GLShaderKind_fragment);
  GLuint program = makeGLProgram(vertexShader, fragmentShader);
  glUseProgram(program);
  
  GLuint patternPosition   = glGetAttribLocation(program, "pattern");
  GLuint vMinMaxPosition   = glGetAttribLocation(program, "v_min_max");
  GLuint vUVMinMaxPosition = glGetAttribLocation(program, "v_uv_min_max");
  GLuint vColorPosition    = glGetAttribLocation(program, "v_color");
  GLuint vAnglePosition    = glGetAttribLocation(program, "v_angle");
  GLuint vLevelPosition    = glGetAttribLocation(program, "v_level");

  GLuint transformPosition = glGetUniformLocation(program, "transform");
  GLuint samplerPosition   = glGetUniformLocation(program, "atlas");
  glUniform1i(samplerPosition, 0);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  GLsizei bufferDataSize = (GLsizei)quadCapacity * sizeof(R_Quad);
  glBufferData(GL_ARRAY_BUFFER, bufferDataSize, 0, GL_STREAM_DRAW);
  r32 patternData[] = {
    -1.f, -1.f,  0.f, 0.f,
     1.f, -1.f,  1.f, 0.f,
    -1.f,  1.f,  0.f, 1.f,
     1.f,  1.f,  1.f, 1.f,
  };
  u32 patternDataSize = sizeof(patternData);
  glBufferSubData(GL_ARRAY_BUFFER, 0, patternDataSize, patternData);

  GL_CATCH_ERROR();
	  
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  GL_CATCH_ERROR();

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  GL_CATCH_ERROR();

  glEnable(GL_SCISSOR_TEST);

  GL_CATCH_ERROR();

  GLState *result = arenaPushStruct(arena, GLState);
  result->patternPosition   = patternPosition;
  result->vMinMaxPosition   = vMinMaxPosition;
  result->vUVMinMaxPosition = vUVMinMaxPosition;
  result->vColorPosition    = vColorPosition;
  result->vAnglePosition    = vAnglePosition;
  result->vLevelPosition    = vLevelPosition;
  result->transformPosition = transformPosition;
  result->samplerPosition   = samplerPosition;
  result->vao = vao;
  result->vbo = vbo;
  result->vertexShader = vertexShader;
  result->fragmentShader = fragmentShader;
  result->program = program;
  result->quadDataOffset = patternDataSize;
  //result->quadCapacity = (bufferDataSize - patternDataSize) / sizeof(R_Quad);
  
  return(result);
}

static RenderCommands*
allocRenderCommands(void)
{
  Arena *renderArena = gsArenaAcquire(MEGABYTES(1));
  RenderCommands *result = arenaPushStruct(renderArena, RenderCommands);
  result->allocator = renderArena;  
  result->quadCapacity = 2048;
  result->quads = arenaPushArray(renderArena, result->quadCapacity, R_Quad);
  result->glState = makeGLState(renderArena, result->quadCapacity);
  return(result);
}

static void
renderBindTexture(LoadedBitmap *texture, bool generateNewTextures)
{
  if(texture->glHandle && !generateNewTextures)
    {
      glBindTexture(GL_TEXTURE_2D, texture->glHandle);
      GL_CATCH_ERROR();
    }
  else
    {
      glGenTextures(1, &texture->glHandle);
      glBindTexture(GL_TEXTURE_2D, texture->glHandle);      
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		   texture->width, texture->height, 0,
		   GL_RGBA, GL_UNSIGNED_BYTE, texture->pixels);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      GL_CATCH_ERROR();
    }
    
}

static void
renderCommands(RenderCommands *commands)
{  
  GL_CATCH_ERROR();

  // NOTE: setup
  if(!commands->atlasIsBound)
    {
      glActiveTexture(GL_TEXTURE0);
      renderBindTexture(commands->atlas, commands->generateNewTextures);
      commands->atlasIsBound = 1;
    }

  glEnableVertexAttribArray(commands->glState->patternPosition);
  glVertexAttribDivisor(commands->glState->patternPosition, 0);
  glVertexAttribPointer(commands->glState->patternPosition, 4, GL_FLOAT, 0, 0, 0);

  glEnableVertexAttribArray(commands->glState->vMinMaxPosition);
  glVertexAttribDivisor(commands->glState->vMinMaxPosition, 1);
  glVertexAttribPointer(commands->glState->vMinMaxPosition, 4, GL_FLOAT, 0, sizeof(R_Quad),
			PTR_FROM_INT(commands->glState->quadDataOffset + OFFSET_OF(R_Quad, min)));

  glEnableVertexAttribArray(commands->glState->vUVMinMaxPosition);
  glVertexAttribDivisor(commands->glState->vUVMinMaxPosition, 1);
  glVertexAttribPointer(commands->glState->vUVMinMaxPosition, 4, GL_FLOAT, 0, sizeof(R_Quad),
			PTR_FROM_INT(commands->glState->quadDataOffset + OFFSET_OF(R_Quad, uvMin)));

  glEnableVertexAttribArray(commands->glState->vColorPosition);
  glVertexAttribDivisor(commands->glState->vColorPosition, 1);
  glVertexAttribPointer(commands->glState->vColorPosition, 4, GL_UNSIGNED_BYTE, 1, sizeof(R_Quad),
			PTR_FROM_INT(commands->glState->quadDataOffset + OFFSET_OF(R_Quad, color)));

  glEnableVertexAttribArray(commands->glState->vAnglePosition);
  glVertexAttribDivisor(commands->glState->vAnglePosition, 1);
  glVertexAttribPointer(commands->glState->vAnglePosition, 1, GL_FLOAT, 0, sizeof(R_Quad),
			PTR_FROM_INT(commands->glState->quadDataOffset + OFFSET_OF(R_Quad, angle)));

  glEnableVertexAttribArray(commands->glState->vLevelPosition);
  glVertexAttribDivisor(commands->glState->vLevelPosition, 1);
  glVertexAttribPointer(commands->glState->vLevelPosition, 1, GL_FLOAT, 0, sizeof(R_Quad),
			PTR_FROM_INT(commands->glState->quadDataOffset + OFFSET_OF(R_Quad, level)));

  GL_CATCH_ERROR();

  mat4 transform = transpose(makeProjectionMatrix(commands->widthInPixels, commands->heightInPixels));
  glUniformMatrix4fv(commands->glState->transformPosition, 1, 0, (GLfloat*)transform.E);

  GL_CATCH_ERROR();

  // NOTE: draw
  glBufferSubData(GL_ARRAY_BUFFER,
		  commands->glState->quadDataOffset,
		  commands->quadCount*sizeof(R_Quad),
		  commands->quads);
  GL_CATCH_ERROR();
  
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)commands->quadCount);
  GL_CATCH_ERROR();
}
