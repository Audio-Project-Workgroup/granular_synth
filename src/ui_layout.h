enum UISizeType : u32
{
  UISizeType_none,
  UISizeType_pixels,
  UISizeType_glCoords,
  UISizeType_percentOfParentDim,
  UISizeType_percentOfOwnDim,
  UISizeType_sumOfChildren,
};

struct UISize
{
  UISizeType type;
  r32 value;
};

inline UISize
makeUISize(UISizeType type, r32 value)
{
  UISize result = {};
  result.type = type;
  result.value = value;

  return(result);
}

inline UISize
defaultUISize(void)
{
  UISize result = {};

  return(result);
}

enum UIAxes
{
  UIAxis_x,
  UIAxis_y,
  UIAxis_COUNT,
};

enum UIElementFlags : u32
{
  UIElementFlag_clickable = (1 << 0),
  UIElementFlag_draggable = (1 << 1),
  UIElementFlag_turnable = (1 << 2),
  UIElementFlag_drawText = (1 << 3),
  UIElementFlag_drawBorder = (1 << 4),
  UIElementFlag_drawBackground = (1 << 5),
  UIElementFlag_drawLabelAbove = (1 << 6),
  UIElementFlag_drawLabelBelow = (1 << 7),
  
    //UIElementFlag_focusHot = (1 << 5),
    //UIElementFlag_focusHotOff = (1 << 6),
};

enum UIParameter
{
  UIParameter_none,
  UIParameter_boolean,
  UIParameter_float,
};

struct UIHashKey
{
  u64 key;
  u8 *name;
};

struct UIElement
{
  // NOTE: tree links
  // IMPORTANT: **do not** change the order of these pointers, for any reason! (it will fuck shit up)
  UIElement *next;
  UIElement *prev;
  UIElement *first;
  UIElement *last;  
  UIElement *parent;

  // NOTE: hashing utility
  UIHashKey hashKey;
  u32 lastFrameTouched;
  UIElement *nextInHash;

  // NOTE: specified at construction
  u32 flags;
  u8 *name;
  v2 textScale;
  v4 color;

  UIParameter parameterType;
  union // TODO: maybe there could be a use for an element that has both a boolean and a float parameter attached
  {
    PluginBooleanParameter *bParam;
    PluginFloatParameter *fParam;
  };  
  
  UISize semanticDim[UIAxis_COUNT];
  UISize semanticOffset[UIAxis_COUNT];

  // NOTE: computed
  Rect2 region;
  u32 commFlags;
  v2 mouseClickedP;
  r32 fParamValueAtClick;
};

#define ELEMENT_SENTINEL(element) (UIElement *)&element->first

struct UILayout
{
  Arena *frameArena;
  Arena *permanentArena;

  LoadedFont *font;

  u32 elementCount;
  UIElement *root;  
  
  UIElement *currentParent;
  
  UIElement *elementCache[512];
  UIElement *elementFreeList;

  //UIElement *hotElementStack[32];
  u32 selectedElementOrdinal;
  UIHashKey selectedElement;

  // NOTE: interaction state
  v3 mouseP;
  v3 lastMouseP;
  v2 mouseLClickP;
  v2 mouseRClickP;
  // TODO: put these bools into a bitfield (use UICommFlags?)
  bool leftButtonPressed;
  bool leftButtonReleased;
  bool leftButtonDown;
  bool rightButtonPressed;
  bool rightButtonReleased;
  bool rightButtonDown;

  bool tabPressed;
  bool backspacePressed;
  bool enterPressed;
  bool minusPressed;
  bool plusPressed;

  u32 frameIndex;
};

enum UICommFlags : u32
{
  // NOTE: mouse button pressed while hovering
  UICommFlag_leftPressed = (1 << 0),
  UICommFlag_rightPressed = (1 << 1),

  // NOTE: mouse button previously pressed, button still down
  UICommFlag_leftDragging = (1 << 2),
  UICommFlag_rightDragging = (1 << 3),

  // NOTE: mouse button previously pressed, button released in bounds
  UICommFlag_leftClicked = (1 << 4),
  UICommFlag_rightClicked = (1 << 5),

  // NOTE: mouse button previously pressed, button released in or out of bounds
  UICommFlag_leftReleased = (1 << 6),
  UICommFlag_rightReleased = (1 << 7),

  // NOTE: mouse button previously clicked, button pressed again
  UICommFlag_leftDoubleClicked = (1 << 8),
  UICommFlag_rightDoubleClicked = (1 << 9),

  // NOTE: key pressed while element focused
  UICommFlag_enterPressed = (1 << 10),
  UICommFlag_minusPressed = (1 << 11),
  UICommFlag_plusPressed = (1 << 12),

  // NOTE: mouse over element
  UICommFlag_hovering = (1 << 13),

  // NOTE: combos
  UICommFlag_pressed = UICommFlag_leftPressed | UICommFlag_enterPressed,
  UICommFlag_released = UICommFlag_leftReleased,
  UICommFlag_clicked = UICommFlag_leftClicked | UICommFlag_enterPressed,
  UICommFlag_doubleClicked = UICommFlag_leftDoubleClicked,
  UICommFlag_dragging = UICommFlag_leftDragging | UICommFlag_minusPressed | UICommFlag_plusPressed,
};

struct UIComm
{  
  UIElement *element;
  u32 flags;
};

UIHashKey uiHashKeyFromString(u8 *name);
bool uiHashKeysAreEqual(UIHashKey key1, UIHashKey key2);

UIElement *uiCacheElement(UIElement *element, UILayout *layout);
UIElement *uiGetCachedElement(UIElement *element, UILayout *layout);
