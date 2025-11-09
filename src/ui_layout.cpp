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
uiAllocateElement(UILayout *layout, String8 name, u32 flags, v4 color, PluginAsset *texture)
{
  UIContext *context = layout->context;
  UIElement *result = arenaPushStruct(context->frameArena, UIElement, arenaFlagsZeroNoAlign());
  result->layout = layout;
  result->name = arenaPushString(context->frameArena, name);
  //result->name = arenaPushString(context->permanentArena, name);
  result->textScale = V2(1, 1);
  result->flags = flags;
  result->color = color;
  result->texture = texture;

  return(result);
}

inline UIHashKey
uiHashKeyFromString(String8 name)
{
  // TODO: better hash function
  u64 hashKey = 5381;
  u8 *at = name.str;
  u64 count = name.size;
  //while(*at)
  for(u64 i = 0; i < count; ++i)
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
      result = stringsAreEqual(key1.name, key2.name);   
    }

  return(result);
}

inline UIElement *
uiMakeElement(UILayout *layout, String8 name, u32 flags, v4 color, PluginAsset *texture)	      
{  
  UIElement *result = uiAllocateElement(layout, name, flags, color, texture);
  result->hashKey = uiHashKeyFromString(result->name);

  // NOTE: if the new element existed previously, get the previously computed information
  UIElement *cachedElement = uiGetCachedElement(result);
  if(cachedElement)
    {
      // TODO: it should be easier to add new cross-frame stateful data to UIElement
      result->region = cachedElement->region;
      result->commFlags = cachedElement->commFlags;
      result->lastFrameTouched = cachedElement->lastFrameTouched;
      result->mouseClickedP = cachedElement->mouseClickedP;
      result->fParamValueAtClick = cachedElement->fParamValueAtClick;
      result->dragData = cachedElement->dragData;
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

  result->inTooltip = layout->isTooltip;

  ++layout->elementCount;
  //++layout->context->elementCount;
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

#if 0
inline void
uiSetSemanticSizeAbsolute(UIElement *element, v2 offset, v2 dim)
{
  element->semanticDim[UIAxis_x] = makeUISize(UISizeType_pixels, dim.x);
  element->semanticDim[UIAxis_y] = makeUISize(UISizeType_pixels, dim.y);
  element->semanticOffset[UIAxis_x] = makeUISize(UISizeType_pixels, offset.x);
  element->semanticOffset[UIAxis_y] = makeUISize(UISizeType_pixels, offset.y);
}

inline void
uiSetSemanticSizeAbsolute(UIElement *element, Rect2 rect)
{
  uiSetSemanticSizeAbsolute(element, rect.min, getDim(rect));
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
      
  v2 textOffset = parentRect.min + V2(0.5f*(parentDim.x - textDim.x),
				      parentDim.y - textScale.y*font->verticalAdvance);
  uiSetSemanticSizeAbsolute(element, textOffset, textDim);
  element->textScale = textScale;
}

inline void
uiSetSemanticSizeText(UIElement *element, LoadedFont *font, v2 offset, v2 scale)
{
  Rect2 parentRect = element->parent->region;
  v2 parentDim = getDim(parentRect);

  r32 wScale = scale.x;
  r32 hScale = scale.y;
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

  uiSetSemanticSizeAbsolute(element, offset, textDim);
  element->textScale = textScale;
}
#endif

inline UIContext
uiInitializeContext(Arena *frameArena, Arena *permanentArena, LoadedFont *font)
{
  UIContext result = {};
  result.frameArena = frameArena;
  result.permanentArena = permanentArena;
  result.font = font;

  return(result);
}

static void
uiContextNewFrame(UIContext *context, MouseState *mouse, KeyboardState *keyboard, bool windowResized)
{  
  // NOTE: mouse input
  context->lastMouseP = context->mouseP;
  context->mouseP.xy = mouse->position;
  context->mouseP.z += (r32)mouse->scrollDelta;
  
  context->leftButtonPressed = wasPressed(mouse->buttons[MouseButton_left]);  
  if(context->leftButtonPressed) context->mouseLClickP = context->mouseP.xy;
  context->leftButtonDown = isDown(mouse->buttons[MouseButton_left]);
  context->leftButtonReleased = wasReleased(mouse->buttons[MouseButton_left]);
  
  context->rightButtonPressed = wasPressed(mouse->buttons[MouseButton_right]);
  if(context->rightButtonPressed) context->mouseRClickP = context->mouseP.xy;  
  context->rightButtonDown = isDown(mouse->buttons[MouseButton_right]);
  context->rightButtonReleased = wasReleased(mouse->buttons[MouseButton_right]);

  // NOTE: keyboard input
  context->escPressed = wasPressed(keyboard->keys[KeyboardButton_esc]);
  context->tabPressed = wasPressed(keyboard->keys[KeyboardButton_tab]);
  if(context->tabPressed) context->selectedElementOrdinal++;
  context->backspacePressed = wasPressed(keyboard->keys[KeyboardButton_backspace]);
  if(context->backspacePressed) if(context->selectedElementOrdinal) --context->selectedElementOrdinal;
  context->enterPressed = wasPressed(keyboard->keys[KeyboardButton_enter]);
  context->minusPressed = wasPressed(keyboard->keys[KeyboardButton_minus]);
  context->plusPressed = (wasPressed(keyboard->keys[KeyboardButton_equal]) &&
			 isDown(keyboard->modifiers[KeyboardModifier_shift]));
  
  context->windowResized = windowResized;

  ++context->frameIndex;
  context->layoutCount = 0;
  context->processedElementCount = 0;
}

static void
uiContextEndFrame(UIContext *context)
{
  context->selectedElementOrdinal %= context->processedElementCount + 1;
}

inline void
uiBeginLayout(UILayout *layout, UIContext *context, Rect2 parentRect,
	      v4 color = V4(1, 1, 1, 1), PluginAsset *texture = PLUGIN_ASSET(null), b32 isTooltip = 0)
{
  ASSERT(layout->elementCount == 0);
  layout->context = context;
  layout->index = context->layoutCount++;  
  //layout->isSelected = ((layout->index + 1) == context->selectedLayoutOrdinal);
  layout->isTooltip = isTooltip;

  if(layout->index == context->selectedElementLayoutIndex)
    {
      layout->selectedElement = context->selectedElement;
    }
  else
    {
      layout->selectedElement = {};
    }
  
  layout->root = uiMakeElement(layout, STR8_LIT("root"), 0, color, texture);
  layout->root->flags = // UIElementFlag_drawBorder | 
    UIElementFlag_drawBackground;
  layout->root->lastFrameTouched = context->frameIndex;
  //uiSetSemanticSizeAbsolute(layout->root, parentRect.min, getDim(parentRect));
  layout->root->region = parentRect;
  layout->regionRemaining = parentRect;
}

inline void
uiEndElement(UIElement *element)
{
  UILayout *layout = element->layout;
  
  if(element->next) element->next->prev = element->prev;
  if(element->prev) element->prev->next = element->next;  
  //ZERO_STRUCT(*element);
  --layout->elementCount;

  uiCacheElement(element);  
}

inline UIElement *
uiCacheElement(UIElement *element)
{
  UILayout *layout = element->layout;
  UIContext *context = layout->context;
  
  UIHashKey hashKey = element->hashKey;
  u64 cacheIndex = hashKey.key % ARRAY_COUNT(layout->elementCache);
  UIElement *cachedElement = layout->elementCache[cacheIndex];
  if(cachedElement)
    {
      if(uiHashKeysAreEqual(hashKey, cachedElement->hashKey))
	{
	  if(element->lastFrameTouched > cachedElement->lastFrameTouched || context->windowResized)
	    {
	      element->nextInHash = cachedElement->nextInHash;
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
		  if(element->lastFrameTouched > cachedElement->lastFrameTouched || context->windowResized)
		    {
		      element->nextInHash = cachedElement->nextInHash;
		      *cachedElement = *element;
		    }
		  break;
		}
	    }
      
	  if(!cachedElement->nextInHash)
	    {
	      cachedElement->nextInHash = arenaPushStruct(layout->context->permanentArena, UIElement);
	      *cachedElement->nextInHash = *element;
	      cachedElement = cachedElement->nextInHash;
	    }
	}
    }
  else
    {
      cachedElement = arenaPushStruct(layout->context->permanentArena, UIElement);
      layout->elementCache[cacheIndex] = cachedElement;
      *cachedElement = *element;
    }

  return(cachedElement);
}

inline UIElement *
uiGetCachedElement(UIElement *element)
{
  UILayout *layout = element->layout;
  
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
	  //printf("-");
	  logString("-");
	}
      if(element->name.str)
	{
	  //printf("%s\n", element->name.str);
	  logFormatString("%s\n", element->name.str);
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
uiEndElementRecursive(UIElement *element)
{
  if(element->first)
    {
      if(element->first != (UIElement *)&element->first)
	{
	  uiEndElementRecursive(element->first);
	}
    }

  if(element->next)
    {
      if(element->next != (UIElement *)&element->parent->first)
	{
	  uiEndElementRecursive(element->next);
	}
    }

  uiEndElement(element);
  //uiPrintLayout(layout);
}

inline void
uiEndLayout(UILayout *layout)
{
  layout->context->processedElementCount += layout->elementCount;
  
  uiEndElementRecursive(layout->root);  
  layout->root = 0;
  layout->currentParent = 0;  
}

inline UIComm
uiCommFromElement(UIElement *element)
{
  UILayout *layout = element->layout;
  UIContext *context = layout->context;
  
  Rect2 clickableRect = element->clickableRegion;  
  v2 mouseP = context->mouseP.xy;

  UIComm result = {};
  result.element = element;
  if(isInRectangle(clickableRect, mouseP))
    {
      // TODO: maybe check clickable field here?
      if(context->leftButtonPressed)
	{
	  result.flags |= UICommFlag_leftPressed;
	  
	  // TODO: too much cross-frame state is being managed here
	  result.element->mouseClickedP = context->mouseP.xy;	  
	  if(result.element->parameterType == UIParameter_float)
	    {
	      result.element->fParamValueAtClick = pluginReadFloatParameter(result.element->fParam);
	    }
	}
      else if(context->leftButtonReleased)
	{
	  result.flags |= UICommFlag_leftReleased;
	}
      else
	{
	  result.flags |= UICommFlag_hovering;
	}
      
      result.element->lastFrameTouched = context->frameIndex;
    }

  if(uiHashKeysAreEqual(element->hashKey, context->selectedElement))
    {
      result.element->lastFrameTouched = context->frameIndex;
      if(context->enterPressed)
	{
	  result.flags |= UICommFlag_enterPressed;
	}
      if(context->minusPressed)
	{
	  result.flags |= UICommFlag_minusPressed;
	}
      if(context->plusPressed)
	{
	  result.flags |= UICommFlag_plusPressed;
	}
    }

  // TODO: maybe check draggable field here?
  if((element->commFlags & UICommFlag_leftPressed) ||
     (element->commFlags & UICommFlag_leftDragging))
    {
      if(context->leftButtonDown)
	{
	  result.flags |= UICommFlag_leftDragging;
	  result.element->lastFrameTouched = context->frameIndex;
	}
      else if(context->leftButtonReleased)
	{
	  result.flags |= UICommFlag_leftReleased;
	  result.element->lastFrameTouched = context->frameIndex;
	}
    }
  
  result.element->commFlags = result.flags;
  return(result);
}

//
// common-case widget building functions
//

inline UIComm
uiMakeTextElement(UILayout *layout, String8 message, r32 desiredTextScale, v4 color = V4(1, 1, 1, 1))
{
  UIContext *context = layout->context;
  UIElement *text = uiMakeElement(layout, message, UIElementFlag_drawText, color, 0);

  v2 messageRectDim = desiredTextScale*getTextDim(context->font, message);
  v2 layoutRegionRemainingDim = getDim(layout->regionRemaining);

  r32 textScale = desiredTextScale;
  v2 textOffset = layout->regionRemaining.min;
  if(messageRectDim.x <= layoutRegionRemainingDim.x)
    {      
      textOffset.x += 0.5f*(layoutRegionRemainingDim.x - messageRectDim.x);
    }
  else
    {
      textScale = desiredTextScale*layoutRegionRemainingDim.x/messageRectDim.x;
    }
  textOffset.y += layoutRegionRemainingDim.y - textScale*context->font->verticalAdvance;

  text->textScale = V2(textScale, textScale);
  text->region = rectAddRadius(rectMinDim(textOffset, (textScale/desiredTextScale)*messageRectDim), V2(2, 1));
  
  layout->regionRemaining.max.y = text->region.min.y;

  UIComm textComm = uiCommFromElement(text);

  return(textComm);
}

inline UIComm
uiMakeSelectableTextElement(UILayout *layout, String8 message, r32 scale, v4 color = V4(1, 1, 1, 1))
{
  UIContext *context = layout->context;
  u32 flags = UIElementFlag_drawText | UIElementFlag_clickable;
  UIElement *text = uiMakeElement(layout, message, flags, color, 0);

  v2 layoutRegionRemainingDim = getDim(layout->regionRemaining);
  
  v2 textRectDim = scale*getTextDim(context->font, message);
  v2 textRectOffset = (layout->regionRemaining.min +
		       V2(12.f,
			  layoutRegionRemainingDim.y - scale*context->font->verticalAdvance));  

  // Rect2 textRect = rectAddRadius(rectMinDim(textRectOffset, textRectDim), V2(2, 1));
  // textRect.max.x = MIN(textRect.max.x, layout->regionRemaining.max.x);
  Rect2 textRect = rectMinDim(textRectOffset, textRectDim);
  textRect.max.x = MIN(textRect.max.x, layout->regionRemaining.max.x);
  
  text->textScale = V2(scale, scale);
  text->region = textRect;
  text->clickableRegion = text->region;

  layout->regionRemaining.max.y = text->region.min.y;

  UIComm textComm = uiCommFromElement(text);
  return(textComm);
}

inline UIComm
uiMakeBox(UILayout *layout, String8 name, Rect2 rect,
	  u32 flags = 0, v4 color = V4(1, 1, 1, 1), PluginAsset *texture = 0)
{
  UIElement *box = uiMakeElement(layout, name, flags, color, texture);
  box->region = rect;

  UIComm boxComm = uiCommFromElement(box);
  
  return(boxComm);
}

inline UIComm
uiMakeButton(UILayout *layout, String8 name, v2 offset, v2 dim, r32 aspectRatio,
	     PluginBooleanParameter *param,
	     PluginAsset *texture = PLUGIN_ASSET(null), v4 color = V4(1, 1, 1, 1))
{
  u32 flags = UIElementFlag_clickable | UIElementFlag_drawBackground | UIElementFlag_drawBorder;
  UIElement *button = uiMakeElement(layout, name, flags, color, texture);
  uiSetElementDataBoolean(button, param);

  button->region = uiComputeElementRegion(button, offset, dim, aspectRatio);
  button->clickableRegion = button->region;
  //layout->regionRemaining.max.y = button->region.min.y;

  UIComm buttonComm = uiCommFromElement(button);

  return(buttonComm);
}

inline UIComm
uiMakeSlider(UILayout *layout, String8 name,
	     v2 offset, v2 dim, r32 aspectRatio, PluginFloatParameter *param,
	     v2 clickableOffset, v2 clickableDim,
	     v2 textOffset, v2 textScale,
	     PluginAsset *backgroundTexture = 0, PluginAsset *clickableTexture = 0,
	     PluginAsset *labelTexture = 0, v4 color = V4(1, 1, 1, 1))
{
  u32 flags = (UIElementFlag_clickable | UIElementFlag_draggable | UIElementFlag_drawLabelBelow);
  UIElement *slider = uiMakeElement(layout, name, flags, color, backgroundTexture);
  uiSetElementDataFloat(slider, param);
  slider->secondaryTexture = clickableTexture;
  slider->labelTexture = labelTexture;   
  slider->region = uiComputeElementRegion(slider, offset, dim, aspectRatio);  
  slider->textOffset = textOffset;
  slider->textScale = textScale;

  v2 regionDim = getDim(slider->region);
  slider->clickableRegion = rectMinDim(slider->region.min + hadamard(clickableOffset, regionDim),
				       hadamard(clickableDim, regionDim));
  
  UIComm sliderComm = uiCommFromElement(slider);

  return(sliderComm);
}

inline UIComm
uiMakeKnob(UILayout *layout, String8 name, v2 offset, v2 dim, r32 aspectRatio,
	   PluginFloatParameter *param,	   
	   v2 labelOffset, v2 labelDim,
	   v2 clickableOffset, v2 clickableDim,
	   v2 textOffset, v2 textScale,
	   PluginAsset *texture = 0, PluginAsset *labelTexture = 0, v4 color = V4(1, 1, 1, 1))
{
  u32 flags = (UIElementFlag_clickable | UIElementFlag_turnable | UIElementFlag_drawLabelBelow);
  UIElement *knob = uiMakeElement(layout, name, flags, color, texture);
  uiSetElementDataFloat(knob, param);
  knob->labelOffset = labelOffset;
  knob->labelDim = labelDim;
  knob->labelTexture = labelTexture;
  knob->region = uiComputeElementRegion(knob, offset, dim, aspectRatio);    
  knob->textOffset = textOffset;
  knob->textScale = textScale;

  v2 regionDim = getDim(knob->region);
  knob->clickableRegion = rectMinDim(knob->region.min + hadamard(clickableOffset, regionDim),
				     hadamard(clickableDim, regionDim));
  //layout->regionRemaining.max.y = knob->region.min.y;

  UIComm knobComm = uiCommFromElement(knob);

  return(knobComm);
}

// NOTE: incomplete
#if 0
inline UIComm
uiMakeCollapsibleList(UILayout *layout, String8 text, v2 offset, v2 scale,
		      v4 color = V4(1, 1, 1, 1))
{
  //UIContext *context = layout->context;
  u32 flags = UIElementFlag_clickable | UIElementFlag_collapsible | UIElementFlag_drawText | UIElementFlag_drawBorder;
  UIElement *list = uiMakeElement(layout, text, flags, color, 0);
  //uiSetSemanticSizeText(list, context->font, offset, scale);

  UIComm listComm = uiCommFromElement(list);
  return(listComm);
}
#endif
