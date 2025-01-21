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
  UIElementFlag_drawText = (1 << 2),
  UIElementFlag_drawBorder = (1 << 3),
  UIElementFlag_drawBackground = (1 << 4),
};

enum UIDataFlags : u32
{
  UIDataFlag_none,
  UIDataFlag_boolean,
  UIDataFlag_float,
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
  v4 color;

  UIDataFlags dataFlags;
  void *data;
  RangeR32 range;

  bool isActive;  
  UISize semanticDim[UIAxis_COUNT];
  UISize semanticOffset[UIAxis_COUNT];

  // NOTE: computed
  Rect2 region;  
};

#define ELEMENT_SENTINEL(element) (UIElement *)&element->first

struct UILayout
{
  Arena *frameArena;
  Arena *permanentArena;

  LoadedFont *font;

  u32 elementCount;
  UIElement *root;
  //UIElement *elementFreeList;
  
  UIElement *currentParent;

  UIElement *elementCache[512];

  //UIElement *hotElementStack[32];
  u32 selectedElementOrdinal;
  UIElement *selectedElement;

  u32 frameIndex;
};

inline UILayout
uiInitializeLayout(Arena *frameArena, Arena *permanentArena, LoadedFont *font)
{
  UILayout layout = {};
  layout.frameArena = frameArena;
  layout.permanentArena = permanentArena;
  layout.font = font;

  return(layout);
}

inline void
uiPushParentElement(UILayout *layout, UIElement *newParent)
{
  layout->currentParent = newParent;
  //newParent->first = newParent;
  //newParent->last = newParent;
  layout->currentParent->first = (UIElement *)&layout->currentParent->first;
  layout->currentParent->last = (UIElement *)&layout->currentParent->first;
}

inline void
uiPopParentElement(UILayout *layout)
{
  layout->currentParent = layout->currentParent->parent;  
}

inline UIElement *
uiAllocateElement(UILayout *layout, char *name, u32 flags, v4 color)
{
  UIElement *result = 0;
  //if(layout->elementFreeList)
  //{
  //  result = layout->elementFreeList;
  //  layout->elementFreeList = result->next;
  //  result->next = 0;
  //}
  //else
  //{
  result = arenaPushStruct(layout->frameArena, UIElement, arenaFlagsZeroNoAlign());      
      //}
  result->name = arenaPushString(layout->frameArena, name);
  result->flags = flags;
  result->color = color;

  return(result);
}

inline UIHashKey
uiHashKeyFromString(u8 *name)
{
  // TODO: better hash function
  u64 hashKey = 5381;
  u8 *at = name;
  while(*at)
    {
      hashKey = ((hashKey << 5) + hashKey + *at++);
    }

  UIHashKey result = {};
  result.key = hashKey;
  result.name = name;
  
  return(result);
}

inline bool
uiHashKeysAreEqual(UIHashKey key1, UIHashKey key2)
{
  bool result = (key1.key == key2.key);
  if(result)
    {
      u8 *at1 = key1.name;
      u8 *at2 = key2.name;
      while(*at1 && *at2)
	{
	  result = ((*at1++) == (*at2++));
	  if(!result) break;
	}
      if(result)
	{
	  if((*at1 != 0) || (*at2 != 0))
	    {
	      result = false;
	    }
	}
    }

  return(result);
}

inline UIElement *
uiMakeElement(UILayout *layout, char *name, u32 flags, v4 color)	      
{  
  UIElement *result = uiAllocateElement(layout, name, flags, color);
  result->hashKey = uiHashKeyFromString(result->name);

  // NOTE: add as a child of the current parent if there is one, else become the parent
  UIElement *parent = layout->currentParent;
  if(parent)
    {
      UIElement *parentSentinel = (UIElement *)&parent->first;
      DLINKED_LIST_APPEND(parentSentinel, result);
      result->parent = parent;
    }
  else
    {
      uiPushParentElement(layout, result);
    }
  
  // NOTE: set parameters of the new element
  //result->name = name;
  //result->flags = flags;
  //result->color = color;
  //result->data = parameter;   

  ++layout->elementCount;
  return(result);
}

inline void
uiSetElementDataBoolean(UIElement *element, bool *variable)
{
  element->dataFlags = UIDataFlag_boolean;
  element->data = variable;
}

inline void
uiSetElementDataFloat(UIElement *element, r32 *variable, RangeR32 range)
{
  element->dataFlags = UIDataFlag_float;
  element->data = variable;
  element->range = range;
}

inline void
uiSetSemanticSizeAbsolute(UIElement *element, v2 offset, v2 dim)
{
  element->semanticDim[UIAxis_x] = makeUISize(UISizeType_pixels, dim.x);
  element->semanticDim[UIAxis_y] = makeUISize(UISizeType_pixels, dim.y);
  element->semanticOffset[UIAxis_x] = makeUISize(UISizeType_pixels, offset.x);
  element->semanticOffset[UIAxis_y] = makeUISize(UISizeType_pixels, offset.y);
}

inline void
uiSetSemanticSizeRelative(UIElement *element, v2 offset, v2 dim)
{
  element->semanticDim[UIAxis_x] = makeUISize(UISizeType_percentOfParentDim, dim.x);
  element->semanticDim[UIAxis_y] = makeUISize(UISizeType_percentOfParentDim, dim.y);
  element->semanticOffset[UIAxis_x] = makeUISize(UISizeType_percentOfParentDim, offset.x);
  element->semanticOffset[UIAxis_y] = makeUISize(UISizeType_percentOfParentDim, offset.y);
}

inline void
uiSetSemanticSizeTextCentered(UIElement *element, LoadedFont *font, r32 wScale, r32 hScale)
{
  v2 textDim = getTextDim(font, element->name);
  v2 textOffset = V2(0.5f*(element->parent->semanticDim[UIAxis_x].value - textDim.x),
		     element->parent->semanticDim[UIAxis_y].value - font->verticalAdvance);
  uiSetSemanticSizeAbsolute(element, textOffset, textDim);
}

inline void
uiBeginLayout(UILayout *layout, v2 screenDimPixels, v4 color = V4(1, 1, 1, 1))
{
  ASSERT(layout->elementCount == 0);;
  layout->root = uiMakeElement(layout, "root", 0, color);
  uiSetSemanticSizeAbsolute(layout->root, V2(0, 0), screenDimPixels);
  ++layout->frameIndex;
}

inline void
uiDestroyElement(UILayout *layout, UIElement *element)
{
  if(element->next) element->next->prev = element->prev;
  if(element->prev) element->prev->next = element->next;  
  //ZERO_STRUCT(*element);
  --layout->elementCount;

  /*
  // NOTE: update element cache, adding an element if necessary (we are not clearing the cache ever)
  UIHashKey hashKey = element->hashKey;
  u64 cacheIndex = hashKey.key % ARRAY_COUNT(layout->elementCache);
  UIElement *cachedElement = layout->elementCache[cacheIndex];
  if(cachedElement)
    {
      if(uiHashKeysAreEqual(hashKey, cachedElement->hashKey))
	{
	  element->nextInHash = cachedElement->nextInHash;
	  //COPY_SIZE(cachedElement, element, sizeof(*element));
	  *cachedElement = *element;
	}
      else
	{
	  while(cachedElement->nextInHash)
	    {
	      cachedElement = cachedElement->nextInHash;
	      if(uiHashKeysAreEqual(hashKey, cachedElement->hashKey))
		{
		  element->nextInHash = cachedElement->nextInHash;
		  //COPY_SIZE(cachedElement, element, sizeof(UIElement));
		  *cachedElement = *element;
		  break;
		}
	    }
      
	  if(!cachedElement->nextInHash)
	    {
	      cachedElement->nextInHash = arenaPushStruct(layout->permanentArena, UIElement);
	      //COPY_SIZE(layout->elementCache[cacheIndex], element, sizeof(UIElement));
	      *cachedElement->nextInHash = *element;
	    }
	}
    }
  else
    {
      layout->elementCache[cacheIndex] = arenaPushStruct(layout->permanentArena, UIElement);
      //COPY_SIZE(layout->elementCache[cacheIndex], element, sizeof(UIElement));
      *layout->elementCache[cacheIndex] = *element;
    }
  */
}

inline UIElement *
uiCacheElement(UIElement *element, UILayout *layout)
{
  UIHashKey hashKey = element->hashKey;
  u64 cacheIndex = hashKey.key % ARRAY_COUNT(layout->elementCache);
  UIElement *cachedElement = layout->elementCache[cacheIndex];
  if(cachedElement)
    {
      if(uiHashKeysAreEqual(hashKey, cachedElement->hashKey))
	{
	  if(element->lastFrameTouched > cachedElement->lastFrameTouched)
	    {
	      element->nextInHash = cachedElement->nextInHash;
	      //COPY_SIZE(cachedElement, element, sizeof(*element));
	      *cachedElement = *element;
	    }
	}
      else
	{
	  while(cachedElement->nextInHash)
	    {
	      cachedElement = cachedElement->nextInHash;
	      if(uiHashKeysAreEqual(hashKey, cachedElement->hashKey))
		{
		  if(element->lastFrameTouched > cachedElement->lastFrameTouched)
		    {
		      element->nextInHash = cachedElement->nextInHash;
		      //COPY_SIZE(cachedElement, element, sizeof(UIElement));
		      *cachedElement = *element;
		    }
		  break;
		}
	    }
      
	  if(!cachedElement->nextInHash)
	    {
	      cachedElement->nextInHash = arenaPushStruct(layout->permanentArena, UIElement);
	      //COPY_SIZE(layout->elementCache[cacheIndex], element, sizeof(UIElement));
	      *cachedElement->nextInHash = *element;
	      cachedElement = cachedElement->nextInHash;
	    }
	}
    }
  else
    {
      cachedElement = arenaPushStruct(layout->permanentArena, UIElement);
      layout->elementCache[cacheIndex] = cachedElement;
      //COPY_SIZE(layout->elementCache[cacheIndex], element, sizeof(UIElement));
      *cachedElement = *element;
    }

  return(cachedElement);
}

inline UIElement *
uiGetCachedElement(UIElement *element, UILayout *layout)
{
  UIHashKey hashKey = element->hashKey;
  u64 cacheIndex = hashKey.key % ARRAY_COUNT(layout->elementCache);
  UIElement *cachedElement = layout->elementCache[cacheIndex];
  if(cachedElement)
    {
      if(!uiHashKeysAreEqual(hashKey, cachedElement->hashKey))
	{
	  while(cachedElement->nextInHash)
	    {
	      cachedElement = cachedElement->nextInHash;
	      if(uiHashKeysAreEqual(hashKey, cachedElement->hashKey))
		{
		  break;
		}
	    }
	}
    }

  return(cachedElement);
}

inline void
uiPrintElement(UIElement *element, u32 depth)
{
  if(element)
    {
      for(u32 i = 0; i < depth; ++i)
	{
	  printf("-");
	}
      if(element->name)
	{
	  printf("%s\n", element->name);
	}

      if(element->first)
	{
	  if(element->first != (UIElement *)&element->first)
	    {
	      uiPrintElement(element->first, depth + 1);
	    }
 	}

      if(element->next)
	{
	  if(element->next != (UIElement *)&element->parent->first)
	    {
	      uiPrintElement(element->next, depth);
	    }
	}
    }
}

inline void
uiPrintLayout(UILayout *layout)
{
  if(layout->elementCount)
    {
      uiPrintElement(layout->root, 0);
    }
}

inline void
uiDestroyElementRecursive(UILayout *layout, UIElement *element)
{
  if(element->first)
    {
      if(element->first != (UIElement *)&element->first)
	{
	  uiDestroyElementRecursive(layout, element->first);
	}
    }

  if(element->next)
    {
      if(element->next != (UIElement *)&element->parent->first)
	{
	  uiDestroyElementRecursive(layout, element->next);
	}
    }

  uiDestroyElement(layout, element);
  //uiPrintLayout(layout);
}

inline void
uiEndLayout(UILayout *layout)
{  
  uiDestroyElementRecursive(layout, layout->root);
  layout->root = 0;
  layout->currentParent = 0;
}

inline UIElement *
uiMakeTextElement(UILayout *layout, char *message, r32 wScale, r32 hScale, v4 color = V4(1, 1, 1, 1))
{
  UIElement *text = uiMakeElement(layout, message, UIElementFlag_drawText, color);
  uiSetSemanticSizeTextCentered(text, layout->font, wScale, hScale);

  return(text);
}

inline UIElement *
uiMakeButton(UILayout *layout, char *name, v2 offset, v2 dim, bool *variable, v4 color = V4(1, 1, 1, 1))
{
  UIElement *button = uiMakeElement(layout, name, UIElementFlag_clickable | UIElementFlag_drawBackground, color);
  uiSetElementDataBoolean(button, variable);
  uiSetSemanticSizeRelative(button, offset, dim);

  return(button);
}

inline UIElement *
uiMakeSlider(UILayout *layout, char *name, v2 offset, v2 dim, r32 *variable, RangeR32 range,
	     v4 color = V4(1, 1, 1, 1))
{
  UIElement *slider = uiMakeElement(layout, name, UIElementFlag_clickable | UIElementFlag_draggable, color);
  uiSetElementDataFloat(slider, variable, range);
  uiSetSemanticSizeRelative(slider, offset, dim);

  return(slider);
}

enum TextFlags : u32
{
  TextFlag_none = 0,
  
  TextFlag_centered = (1 << 0),
  TextFlag_scaleAspect = (1 << 1),
};

struct TextArgs
{
  u32 flags;
  v2 alignment;
};

inline TextArgs
defaultTextArgs(void)
{
  TextArgs result = {};

  return(result);
}

inline TextArgs
centeredTextArgsScaleH(r32 hScale)
{
  TextArgs result = defaultTextArgs();
  result.flags |= TextFlag_centered;
  result.flags |= TextFlag_scaleAspect;
  result.alignment.x = hScale;

  return(result);
}

inline v2
getTextPos(UIElement *element, UIElement *parent)
{
  v2 result = {};

  return(result);
}
