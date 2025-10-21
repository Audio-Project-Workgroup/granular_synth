enum UISizeType
{
  UISizeType_pixels,
  UISizeType_percentOfParent,
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

enum UIAxis
{
  UIAxis_x,
  UIAxis_y,
  UIAxis_COUNT,
};

enum UIElementFlags : u32
{
  UIElementFlag_clickable      = (1 << 0),
  UIElementFlag_draggable      = (1 << 1),
  UIElementFlag_turnable       = (1 << 2),
  UIElementFlag_collapsible    = (1 << 3),
  UIElementFlag_drawText       = (1 << 4),
  UIElementFlag_drawBorder     = (1 << 5),
  UIElementFlag_drawBackground = (1 << 6),
  UIElementFlag_drawLabelAbove = (1 << 7),
  UIElementFlag_drawLabelBelow = (1 << 8),
  
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
  String8 name;
};

struct UIPressHistory
{
  UIHashKey key;
  u64 timestamp;
  v2 pos;
};

struct UIContext
{
  Arena *frameArena;
  Arena *permanentArena;

  LoadedFont *font;

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

  bool escPressed;
  bool tabPressed;
  bool backspacePressed;
  bool enterPressed;
  bool minusPressed;
  bool plusPressed;  

  UIPressHistory pressHistory[MouseButton_COUNT][2];

  bool windowResized;

  u32 frameIndex;

  u32 layoutCount;
  u32 processedElementCount;
  u32 selectedElementOrdinal;
  u32 selectedElementLayoutIndex;
  UIHashKey selectedElement;
  UIHashKey interactingElement;
};

struct UILayout;
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
  UILayout *layout;
  u32 flags;
  String8 name;
  v2 textScale;
  v4 color;
  PluginAsset *texture;
  PluginAsset *secondaryTexture;
  PluginAsset *labelTexture;

  v2 labelOffset;
  v2 labelDim;
  v2 textOffset;

  UIParameter parameterType;
  union // TODO: maybe there could be a use for an element that has both a boolean and a float parameter attached
  {
    PluginBooleanParameter *bParam;
    PluginFloatParameter *fParam;
  };  
  
  //UISize semanticDim[UIAxis_COUNT];
  //UISize semanticOffset[UIAxis_COUNT];
  //r32 aspectRatio;
  /* UIAxis sizingDim; */
  /* union */
  /* { */
  /*   r32 size; */
  /*   r32 heightPercentOfLayoutRegionRemaining; */
  /*   r32 widthPercentOfLayoutRegionRemaining;     */
  /* }; */
  /* v2 offset; */

  // NOTE: computed
  Rect2 region;
  Rect2 clickableRegion;
  u32 commFlags;
  v2 mouseClickedP;
  r32 fParamValueAtClick;
  v2 dragData;
  bool showChildren;
  b32 inTooltip;
};

#define ELEMENT_SENTINEL(element) (UIElement *)&element->first

inline void
uiStoreDragData(UIElement *element, v2 dragData)
{
  element->dragData = dragData;
}

inline v2
uiLoadDragData(UIElement *element)
{
  return(element->dragData);
}

struct UILayout
{  
  UIContext *context;
  
  u32 elementCount;
  UIElement *root;  
  
  UIElement *currentParent;
  
  UIElement *elementCache[512];
  UIElement *elementFreeList;
  
  Rect2 regionRemaining;
  UISizeType currentOffsetSizeType;
  UISizeType currentDimSizeType;

  u32 index;
  b32 isTooltip;
 
  //u32 selectedElementOrdinal;
  UIHashKey selectedElement;  
};

inline void
uiPushLayoutOffsetSizeType(UILayout *layout, UISizeType sizeType)
{
  layout->currentOffsetSizeType = sizeType;
}

inline void
uiPushLayoutDimSizeType(UILayout *layout, UISizeType sizeType)
{
  layout->currentDimSizeType = sizeType;
}

inline v2
uiGetDragDelta(UIElement *element)
{
  return(element->layout->context->mouseP.xy - element->mouseClickedP);
}

#if 1
inline Rect2
uiComputeElementRegion(UIElement *element, v2 offset, v2 dim, r32 aspectRatio)
{
  UISizeType offsetSizeType = element->layout->currentOffsetSizeType;
  UISizeType dimSizeType = element->layout->currentDimSizeType;

  Rect2 parentRegion = element->parent->region;
  v2 parentRegionDim = getDim(parentRegion);
  
  v2 elementOffset = {};
  v2 elementDim = {};
  switch(offsetSizeType)
    {
    case UISizeType_pixels:
      {
	elementOffset = offset;
      } break;
    case UISizeType_percentOfParent:
      {
	elementOffset = hadamard(offset, parentRegionDim);
      } break;
    default: {} break;
    }
  
  switch(dimSizeType)
    {
    case UISizeType_pixels:
      {
	elementDim = dim;
      } break;
    case UISizeType_percentOfParent:
      {
	elementDim = hadamard(dim, parentRegionDim);
      } break;
    default: {} break;
    }

  elementDim.x = elementDim.y*aspectRatio;

  Rect2 result = rectMinDim(elementOffset, elementDim);

  return(result);
}
#else
inline Rect2
uiComputeElementRegion(UIElement *element, v2 offset, UIAxis sizeDim, r32 sizePercentOfParent, r32 aspectRatio)
{
  ASSERT(isInRange(sizePercentOfParent, -1.f, 1.f));
  bool invertElementRect = isInRange(sizePercentOfParent, -1.f, 0.f);
  //if(invertElementRect) sizePercentOfParent = -sizePercentOfParent;

  Rect2 layoutRegion = element->layout->regionRemaining;
  Rect2 parentRegion = element->parent->region;
  v2 layoutRegionDim = getDim(layoutRegion);
  v2 parentRegionDim = getDim(parentRegion);

  // NOTE: adjust size for layout region
  v2 elementDim = V2(0, 0);
  if(sizeDim == UIAxis_x)
    {
      elementDim.x = sizePercentOfParent*parentRegionDim.x;            
      elementDim.y = elementDim.x/aspectRatio;
      if(Abs(elementDim.x) > layoutRegionDim.x)
	{
	  elementDim.x = invertElementRect ? -layoutRegionDim.x : layoutRegionDim.x;
	  elementDim.y = elementDim.x/aspectRatio;
	}      
      if(Abs(elementDim.y) > layoutRegionDim.y)
	{
	  elementDim.y = invertElementRect ? -layoutRegionDim.y : layoutRegionDim.y;
	  elementDim.x = elementDim.y*aspectRatio;
	}      
    }
  else if(sizeDim == UIAxis_y)
    {
      elementDim.y = sizePercentOfParent*parentRegionDim.y;
      elementDim.x = elementDim.y*aspectRatio;
      if(Abs(elementDim.y) > layoutRegionDim.y)
	{
	  elementDim.y = invertElementRect ? -layoutRegionDim.y : layoutRegionDim.y;
	  elementDim.x = elementDim.y*aspectRatio;
	}      
      if(Abs(elementDim.x) > layoutRegionDim.x)
	{
	  elementDim.x = invertElementRect ? -layoutRegionDim.x : layoutRegionDim.x;
	  elementDim.y = elementDim.x/aspectRatio;
	}      
    }
  else
    {
      ASSERT(!"ERROR: invalid sizeDim");
    }

  // NOTE: adjust offset for layout region
  v2 elementOffset = offset;
  if(elementOffset.x + Abs(elementDim.x) > layoutRegionDim.x)
    elementOffset.x = layoutRegionDim.x - Abs(elementDim.x);
  if(elementOffset.y + Abs(elementDim.y) > layoutRegionDim.y)
    elementOffset.y = layoutRegionDim.y - Abs(elementDim.y);

  Rect2 elementRegion;
  if(invertElementRect)
    {
      elementRegion = rectMaxNegDim(layoutRegion.max - elementOffset, elementDim);
    }
  else
    {
      elementRegion = rectMinDim(elementOffset + layoutRegion.min, elementDim);
    }

  return(elementRegion); 
}
#endif

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

static inline b32
uiHashKeyIsNull(UIHashKey key)
{
  b32 result = (key.key == 0 && key.name.str == 0 && key.name.size == 0);
  return(result);
}

UIElement *uiCacheElement(UIElement *element);
UIElement *uiGetCachedElement(UIElement *element);
