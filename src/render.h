#if BUILD_DEBUG
#  define GL_CATCH_ERROR() do { GLenum err = glGetError(); if(err != GL_NO_ERROR) ASSERT(0); } while(0)
#else
#  define GL_CATCH_ERROR()
#endif

#define RENDER_LEVEL(l) ((r32)(RenderLevel_##l)/(r32)RenderLevel_COUNT)
enum RenderLevel
{
  RenderLevel_front,

  RenderLevel_text,
  RenderLevel_label,
  RenderLevel_control,
  RenderLevel_controlBackground,

  RenderLevel_grainViewBorder,
  RenderLevel_grainViewMiddleBar,
  RenderLevel_grainViewBackground,

  //RenderLevel_boxBackground,
  RenderLevel_background,
  RenderLevel_COUNT,
};

struct GLState
{
  u32 patternPosition;
  u32 vMinMaxPosition;
  u32 vAlignmentPosition;
  u32 vUVMinMaxPosition;
  u32 vColorPosition;
  u32 vAnglePosition;
  u32 vLevelPosition;

  u32 transformPosition;
  u32 samplerPosition;

  u32 vao;
  u32 vbo;

  u32 vertexShader;
  u32 fragmentShader;
  u32 program;

  u32 quadDataOffset;
};

struct R_Quad
{
  v2 min;
  v2 max;
  v2 alignment;

  v2 uvMin;
  v2 uvMax;

  u32 color;
  r32 angle;
  r32 level;
};

// struct R_Batch
// {
//   R_Batch *next;
  
//   u32 quadCapacity;
//   u32 quadCount;
//   R_Quad *quads;


// };

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
  Arena *allocator;

  u32 widthInPixels;
  u32 heightInPixels; 

  RenderCursorState cursorState;

  bool outputAudioDeviceChanged;
  u32 selectedOutputAudioDeviceIndex;
  bool inputAudioDeviceChanged;
  u32 selectedInputAudioDeviceIndex;

  GLState *glState;

  LoadedBitmap *atlas;
  b32 atlasIsBound;

  usz quadCapacity;
  usz quadCount;
  R_Quad *quads;

  bool generateNewTextures;
  bool windowResized;
};

static inline void
renderBeginCommands(RenderCommands *commands, u32 widthInPixels, u32 heightInPixels)
{
  commands->windowResized = (commands->widthInPixels != widthInPixels ||
			     commands->heightInPixels != heightInPixels);
  commands->widthInPixels = widthInPixels;
  commands->heightInPixels = heightInPixels;

  commands->cursorState = CursorState_default;
  
  commands->outputAudioDeviceChanged = false;
  commands->inputAudioDeviceChanged = false;  
}

static inline void
renderEndCommands(RenderCommands *commands)
{
  commands->quadCount = 0;

  commands->generateNewTextures = false;
}
