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
  UIElement *result = arenaPushStruct(layout->frameArena, UIElement, arenaFlagsZeroNoAlign());      
  result->name = arenaPushString(layout->frameArena, name);
  result->textScale = V2(1, 1);
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

  // NOTE: if the new element existed previously, get the previously computed information
  UIElement *cachedElement = uiGetCachedElement(result, layout);
  if(cachedElement)
    {
      // TODO: it should be easier to add new cross-frame stateful data to UIElement
      result->region = cachedElement->region;
      result->commFlags = cachedElement->commFlags;
      result->lastFrameTouched = cachedElement->lastFrameTouched;
      result->mouseClickedP = cachedElement->mouseClickedP;
      result->fParamValueAtClick = cachedElement->fParamValueAtClick;
    }

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

  ++layout->elementCount;
  return(result);
}

inline void
uiSetElementDataBoolean(UIElement *element, PluginBooleanParameter *param)
{
  element->parameterType = UIParameter_boolean;
  element->bParam = param;
}

inline void
uiSetElementDataFloat(UIElement *element, PluginFloatParameter *param)
{
  element->parameterType = UIParameter_float;
  element->fParam = param;
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
  Rect2 parentRect = element->parent->region;
  v2 parentDim = getDim(parentRect);

  v2 textDim = getTextDim(font, element->name);
  v2 textScale = V2(1, 1);
  if(wScale)
    {
      textScale.x = wScale*parentDim.x/textDim.x;
      if(hScale)
	{
	  textScale.y = hScale*parentDim.y/textDim.y;
	}
      else
	{
	  textScale.y = textScale.x;
	}
    }
  else if(hScale)
    {
      textScale.y = hScale*parentDim.y/textDim.y;
      textScale.x = textScale.y;
    }
  textDim = hadamard(textScale, textDim);
      
  v2 textOffset = V2(0.5f*(parentDim.x - textDim.x),
		     parentDim.y - font->verticalAdvance);
  uiSetSemanticSizeAbsolute(element, textOffset, textDim);
  element->textScale = textScale;
}

inline void
uiBeginLayout(UILayout *layout, v2 screenDimPixels, MouseState *mouse, KeyboardState *keyboard,
	      v4 color = V4(1, 1, 1, 1))
{
  ASSERT(layout->elementCount == 0);;
  layout->root = uiMakeElement(layout, "root", 0, color);
  uiSetSemanticSizeAbsolute(layout->root, V2(0, 0), screenDimPixels);

  // NOTE: mouse input
  layout->lastMouseP = layout->mouseP;
  layout->mouseP.xy = mouse->position;
  layout->mouseP.z += (r32)mouse->scrollDelta;
  
  layout->leftButtonPressed = wasPressed(mouse->buttons[MouseButton_left]);  
  if(layout->leftButtonPressed) layout->mouseLClickP = layout->mouseP.xy;
  layout->leftButtonDown = isDown(mouse->buttons[MouseButton_left]);
  layout->leftButtonReleased = wasReleased(mouse->buttons[MouseButton_left]);
  
  layout->rightButtonPressed = wasPressed(mouse->buttons[MouseButton_right]);
  if(layout->rightButtonPressed) layout->mouseRClickP = layout->mouseP.xy;  
  layout->rightButtonDown = isDown(mouse->buttons[MouseButton_right]);
  layout->rightButtonReleased = wasReleased(mouse->buttons[MouseButton_right]);

  // NOTE: keyboard input
  layout->tabPressed = wasPressed(keyboard->keys[KeyboardButton_tab]);
  layout->backspacePressed = wasPressed(keyboard->keys[KeyboardButton_backspace]);
  layout->enterPressed = wasPressed(keyboard->keys[KeyboardButton_enter]);
  layout->minusPressed = wasPressed(keyboard->keys[KeyboardButton_minus]);
  layout->plusPressed = (wasPressed(keyboard->keys[KeyboardButton_equal]) &&
			 isDown(keyboard->modifiers[KeyboardModifier_shift]));  

  ++layout->frameIndex;
}

inline void
uiEndElement(UILayout *layout, UIElement *element)
{
  if(element->next) element->next->prev = element->prev;
  if(element->prev) element->prev->next = element->next;  
  //ZERO_STRUCT(*element);
  --layout->elementCount;

  uiCacheElement(element, layout);  
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

	  if(!uiHashKeysAreEqual(hashKey, cachedElement->hashKey))
	    {
	      cachedElement = 0;
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
uiEndElementRecursive(UILayout *layout, UIElement *element)
{
  if(element->first)
    {
      if(element->first != (UIElement *)&element->first)
	{
	  uiEndElementRecursive(layout, element->first);
	}
    }

  if(element->next)
    {
      if(element->next != (UIElement *)&element->parent->first)
	{
	  uiEndElementRecursive(layout, element->next);
	}
    }

  uiEndElement(layout, element);
  //uiPrintLayout(layout);
}

inline void
uiEndLayout(UILayout *layout)
{  
  uiEndElementRecursive(layout, layout->root);
  layout->root = 0;
  layout->currentParent = 0;
}

inline UIComm
uiCommFromElement(UIElement *element, UILayout *layout)
{
  Rect2 elementRect = element->region;  
  v2 mouseP = layout->mouseP.xy;

  UIComm result = {};
  result.element = element;
  if(isInRectangle(elementRect, mouseP))
    {
      // TODO: maybe check clickable field here?
      if(layout->leftButtonPressed)
	{
	  result.flags |= UICommFlag_leftPressed;
	  
	  // TODO: too much cross-frame state is being managed here
	  result.element->mouseClickedP = layout->mouseP.xy;
	  if(result.element->parameterType == UIParameter_float)
	    {
	      result.element->fParamValueAtClick = pluginReadFloatParameter(result.element->fParam);
	    }
	}
      else if(layout->leftButtonReleased)
	{
	  result.flags |= UICommFlag_leftReleased;
	}
      else
	{
	  result.flags |= UICommFlag_hovering;
	}
      
      result.element->lastFrameTouched = layout->frameIndex;
    }

  if(uiHashKeysAreEqual(element->hashKey, layout->selectedElement))
    {
      result.element->lastFrameTouched = layout->frameIndex;
      if(layout->enterPressed)
	{
	  result.flags |= UICommFlag_enterPressed;
	}
      if(layout->minusPressed)
	{
	  result.flags |= UICommFlag_minusPressed;
	}
      if(layout->plusPressed)
	{
	  result.flags |= UICommFlag_plusPressed;
	}
    }

  // TODO: maybe check draggable field here?
  if((element->commFlags & UICommFlag_leftPressed) ||
     (element->commFlags & UICommFlag_leftDragging))
    {
      if(layout->leftButtonDown)
	{
	  result.flags |= UICommFlag_leftDragging;
	  result.element->lastFrameTouched = layout->frameIndex;
	}
      else if(layout->leftButtonReleased)
	{
	  result.flags |= UICommFlag_leftReleased;
	  result.element->lastFrameTouched = layout->frameIndex;
	}
    }
  
  result.element->commFlags = result.flags;
  return(result);
}

//
// common-case widget building functions
//

inline UIComm
uiMakeTextElement(UILayout *layout, char *message, r32 wScale, r32 hScale, v4 color = V4(1, 1, 1, 1))
{
  UIElement *text = uiMakeElement(layout, message, UIElementFlag_drawText | UIElementFlag_drawBorder, color);
  uiSetSemanticSizeTextCentered(text, layout->font, wScale, hScale);

  UIComm textComm = uiCommFromElement(text, layout);

  return(textComm);
}

inline UIComm
uiMakeButton(UILayout *layout, char *name, v2 offset, v2 dim, PluginBooleanParameter *param,
	     v4 color = V4(1, 1, 1, 1))
{
  u32 flags = UIElementFlag_clickable | UIElementFlag_drawBackground | UIElementFlag_drawBorder;
  UIElement *button = uiMakeElement(layout, name, flags, color);
  uiSetElementDataBoolean(button, param);
  uiSetSemanticSizeRelative(button, offset, dim);

  UIComm buttonComm = uiCommFromElement(button, layout);

  return(buttonComm);
}

inline UIComm
uiMakeSlider(UILayout *layout, char *name, v2 offset, v2 dim, PluginFloatParameter *param,
	     v4 color = V4(1, 1, 1, 1))
{
  u32 flags = (UIElementFlag_clickable | UIElementFlag_draggable | UIElementFlag_drawBorder |
	       UIElementFlag_drawLabelAbove | UIElementFlag_drawLabelBelow);
  UIElement *slider = uiMakeElement(layout, name, flags, color);
  uiSetElementDataFloat(slider, param);
  uiSetSemanticSizeRelative(slider, offset, dim);
  //uiSetHotRegionRelative(slider, 1.f, 0.1f);

  UIComm sliderComm = uiCommFromElement(slider, layout);

  return(sliderComm);
}

inline UIComm
uiMakeKnob(UILayout *layout, char *name, v2 offset, v2 dim, PluginFloatParameter *param,
	   v4 color = V4(1, 1, 1, 1))
{
  u32 flags = (UIElementFlag_clickable | UIElementFlag_turnable | UIElementFlag_drawBorder |
	       UIElementFlag_drawLabelBelow);
  UIElement *knob = uiMakeElement(layout, name, flags, color);
  uiSetElementDataFloat(knob, param);
  uiSetSemanticSizeRelative(knob, offset, dim);

  UIComm knobComm = uiCommFromElement(knob, layout);

  return(knobComm);
}
