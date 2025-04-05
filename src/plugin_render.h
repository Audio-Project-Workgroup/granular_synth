static inline void 
renderPushQuad(RenderCommands *commands, Rect2 rect, LoadedBitmap *texture, r32 angle, RenderLevel level,
	       v4 color = V4(1, 1, 1, 1))
{
  if(texture)
    {
      ASSERT(commands->texturedQuadCount < commands->texturedQuadCapacity);
      TexturedQuad *quad = commands->texturedQuads + commands->texturedQuadCount++;

      v2 dim = getDim(rect);
      v2 bottomLeft = rect.min;
      bottomLeft -= V2(texture->alignPercentage.x*dim.x, texture->alignPercentage.y*dim.y);
  
      quad->vertices[0] = makeTexturedVertex(bottomLeft, V2(0, 0), color);
      quad->vertices[1] = makeTexturedVertex(bottomLeft + V2(dim.x, 0), V2(1, 0), color);
      quad->vertices[2] = makeTexturedVertex(bottomLeft + V2(0, dim.y), V2(0, 1), color);
      quad->vertices[3] = makeTexturedVertex(bottomLeft + dim, V2(1, 1), color);
  
      quad->texture = texture;
      quad->angle = angle;
      quad->level = level;
    }
  else
    {
      ASSERT(commands->quadCount < commands->quadCapacity);
      Quad *quad = commands->quads + commands->quadCount++;

      v2 dim = getDim(rect);
      v2 bottomLeft = rect.min;
  
      quad->vertices[0] = makeVertex(bottomLeft, color);
      quad->vertices[1] = makeVertex(bottomLeft + V2(dim.x, 0), color);
      quad->vertices[2] = makeVertex(bottomLeft + V2(0, dim.y), color);
      quad->vertices[3] = makeVertex(bottomLeft + dim, color);
  
      quad->angle = angle;
      quad->level = level;
    }
}

static inline void
renderPushRectOutline(RenderCommands *commands, Rect2 rect, r32 thickness, RenderLevel level,
		      v4 color = V4(1, 1, 1, 1))
{ 
  v2 rectDim = getDim(rect);
  v2 vDim = V2(thickness, rectDim.y);
  v2 hDim = V2(rectDim.x, thickness);

  v2 leftMiddle = rect.min + V2(0, 0.5f*rectDim.y);
  v2 rightMiddle = rect.max - V2(0, 0.5f*rectDim.y);
  v2 bottomMiddle = rect.min + V2(0.5f*rectDim.x, 0);
  v2 topMiddle = rect.max - V2(0.5f*rectDim.x, 0);
  
  renderPushQuad(commands, rectCenterDim(leftMiddle, vDim), 0, 0, level, color);
  renderPushQuad(commands, rectCenterDim(rightMiddle, vDim), 0, 0, level, color);
  renderPushQuad(commands, rectCenterDim(bottomMiddle, hDim), 0, 0, level, color);
  renderPushQuad(commands, rectCenterDim(topMiddle, hDim), 0, 0, level, color);
}

static inline void
renderPushTriangle(RenderCommands *commands, TexturedVertex v1, TexturedVertex v2, TexturedVertex v3,
		   LoadedBitmap *texture = 0)
{
  ASSERT(commands->triangleCount < commands->triangleCapacity);
  TexturedTriangle *triangle = commands->triangles + commands->triangleCount++;
  
  triangle->vertices[0] = v1;
  triangle->vertices[1] = v2;
  triangle->vertices[2] = v3;

  triangle->texture = texture;
}

static inline v2
renderPushText(RenderCommands *commands, LoadedFont *font, String8 string,
	       v2 textMin, v2 textScale, r32 regionWidth,
	       v4 color = V4(1, 1, 1, 1))
{
  v2 atPos = textMin;
  u8 *at = string.str;
  u64 stringSize = string.size;
  
  struct GlyphPushData
  {
    u8 c;
    Rect2 rect;
    LoadedBitmap *glyph;
  } pastGlyphs[3] = {};

  bool overflow = false;
  for(u32 i = 0; i < stringSize; ++i)
    {      	
      u8 c = *at;
      LoadedBitmap *glyph = getGlyphFromChar(font, c);

      v2 glyphDim = V2(glyph->width, glyph->height);
      v2 scaledGlyphDim = hadamard(textScale, glyphDim);
      Rect2 glyphRect = rectMinDim(atPos, scaledGlyphDim);      

      if(i >= 2)
	{
	  GlyphPushData glyphPushData = pastGlyphs[2];
	  renderPushQuad(commands, glyphPushData.rect, glyphPushData.glyph, 0, RenderLevel_front, color);
	}

      //pastGlyphs[3] = pastGlyphs[2];
      pastGlyphs[2] = pastGlyphs[1];
      pastGlyphs[1] = pastGlyphs[0];
      pastGlyphs[0] = {c, glyphRect, glyph};
      
      ++at;
      atPos.x += textScale.x*getHorizontalAdvance(font, c, *at);
      if((atPos.x - textMin.x) >= regionWidth)
	{
	  if(i != (stringSize - 1))
	    {
	      overflow = true;
	      break;
	    }
	}           
    }

  if(!overflow)
    {
      GlyphPushData glyphPushData = pastGlyphs[2];
      renderPushQuad(commands, glyphPushData.rect, glyphPushData.glyph, 0, RenderLevel_front, color);
      glyphPushData = pastGlyphs[1];
      renderPushQuad(commands, glyphPushData.rect, glyphPushData.glyph, 0, RenderLevel_front, color);
      glyphPushData = pastGlyphs[0];
      renderPushQuad(commands, glyphPushData.rect, glyphPushData.glyph, 0, RenderLevel_front, color);
    }
  else
    {
      atPos = pastGlyphs[2].rect.min;
      r32 hAdvance = textScale.x*getHorizontalAdvance(font, '.', '.');	
      LoadedBitmap *glyph = getGlyphFromChar(font, '.');
      v2 scaledGlyphDim = hadamard(textScale, V2(glyph->width, glyph->height));      
      for(u32 i = 0; i < 3; ++i)
	{
	  Rect2 glyphRect = rectMinDim(atPos, scaledGlyphDim);
	  renderPushQuad(commands, glyphRect, glyph, 0, RenderLevel_front, color);
	  atPos.x += hAdvance;
	}
    }
  
  atPos.y -= textScale.y*font->verticalAdvance;
  return(atPos);
}

static inline void
renderChangeCursorState(RenderCommands *commands, RenderCursorState cursorState)
{
  commands->cursorState = cursorState;
}
#if 0
inline Rect2
renderComputeUIElementRegion(RenderCommands *commands, UIElement *element)
{
  v2 elementDim = V2(0, 0);      
  switch(element->semanticDim[UIAxis_x].type)
    {
    case UISizeType_none:
      {	
      } break;
    case UISizeType_pixels:
      {
	elementDim.x = element->semanticDim[UIAxis_x].value;
      } break;
    case UISizeType_percentOfParentDim:
      {
	elementDim.x = element->semanticDim[UIAxis_x].value*getDim(element->parent->region).x;
      } break;
    default: ASSERT(!"unhandled/invalid case");
    }
  switch(element->semanticDim[UIAxis_y].type)
    {
    case UISizeType_none:
      {
      } break;
    case UISizeType_pixels:
      {
	elementDim.y = element->semanticDim[UIAxis_y].value;
      } break;
    case UISizeType_percentOfParentDim:
      {
	elementDim.y = element->semanticDim[UIAxis_y].value*getDim(element->parent->region).y;
      } break;
    default: ASSERT(!"unhandled/invalid case");
    }
      
  v2 elementMin = V2(0, 0);
  switch(element->semanticOffset[UIAxis_x].type)
    {
    case UISizeType_none:
      {
      } break;
    case UISizeType_pixels:
      {
	elementMin.x = element->semanticOffset[UIAxis_x].value;
      } break;
    case UISizeType_percentOfParentDim:
      {
	elementMin.x = element->semanticOffset[UIAxis_x].value*getDim(element->parent->region).x + element->parent->region.min.x;
      } break;
    /* case UISizeType_percentOfOwnDim: */
    /*   { */
    /* 	elementMin.x = element->semanticOffset[UIAxis_x].value*elementDim.x; */
    /*   } break; */
    default: ASSERT(!"unhandled/invalid case");
    }
  switch(element->semanticOffset[UIAxis_y].type)
    {
    case UISizeType_none:
      {
      } break;
    case UISizeType_pixels:
      {
	elementMin.y = element->semanticOffset[UIAxis_y].value;
      } break;
    case UISizeType_percentOfParentDim:
      {
	elementMin.y = element->semanticOffset[UIAxis_y].value*getDim(element->parent->region).y + element->parent->region.min.y;
      } break;
    /* case UISizeType_percentOfOwnDim: */
    /*   { */
    /* 	elementMin.y = element->semanticOffset[UIAxis_y].value*elementDim.y; */
    /*   } break; */
    default: ASSERT(!"unhandled/invalid case");	  
    }

  Rect2 elementRect = rectMinDim(elementMin, elementDim);
  return(elementRect);
}
#endif

inline void
renderPushUIElement(RenderCommands *commands, UIElement *element)
{
  UILayout *layout = element->layout;
  //UIContext *context = layout->context;
  
  //element->region = renderComputeUIElementRegion(commands, element);
  //element->hotRegion = element->region;

  v2 elementRegionCenter = getCenter(element->region);
  v2 elementRegionDim = getDim(element->region);
  // TODO: pull out common formatting computations
  if(element->flags & UIElementFlag_drawText)
    {      
      //v2 textDim = getTextDim(layout->font, element->name);
      //v2 textScale = V2(elementRegionDim.x/textDim.x, elementRegionDim.y/textDim.y);
      renderPushText(commands, layout->context->font,
		     element->name, element->region.min, element->textScale, getDim(element->region).x,
		     element->color);
    }
  if(element->flags & UIElementFlag_drawBorder)
    {
      v4 color = (uiHashKeysAreEqual(element->hashKey, layout->selectedElement)) ? V4(1, 0.5f, 0, 1) : V4(0, 0, 0, 1);
      
      Rect2 border = rectAddRadius(element->region, V2(1, 1));
      renderPushRectOutline(commands, element->region, 2.f, RenderLevel_front, color);
    }
  if(element->flags & UIElementFlag_drawBackground)
    {
      renderPushQuad(commands, element->region, 0, 0, RenderLevel_front, element->color);
    }  
  if(element->flags & UIElementFlag_draggable)
    {            
      v2 travelDim = V2(0.2f*elementRegionDim.x, elementRegionDim.y);
      Rect2 travelRect = rectCenterDim(elementRegionCenter, travelDim);
      
      renderPushQuad(commands, travelRect, 0, 0, RenderLevel_front, V4(0, 0, 0, 1));

      if(element->parameterType == UIParameter_float)
	{
	  r32 paramValue = pluginReadFloatParameter(element->fParam);
	  r32 paramPercentage = (paramValue - element->fParam->range.min)/(element->fParam->range.max - element->fParam->range.min);
	  
	  v2 faderCenter = V2(elementRegionCenter.x, element->region.min.y + paramPercentage*elementRegionDim.y);
	  Rect2 faderRect = rectCenterDim(faderCenter, V2(elementRegionDim.x, 0.1f*elementRegionDim.y));
	  
	  renderPushQuad(commands, faderRect, 0, 0, RenderLevel_front, element->color);
	}
    }
  if(element->flags & UIElementFlag_turnable)
    {
      if(element->parameterType == UIParameter_float)
	{
	  if(element->fParam)
	    {  
	      r32 paramValue = pluginReadFloatParameter(element->fParam);
	      r32 paramPercentage = (paramValue - element->fParam->range.min)/(element->fParam->range.max - element->fParam->range.min);
	      r32 paramAngle = -paramPercentage*M_PI;

	      renderPushQuad(commands, element->region, 0, paramAngle, RenderLevel_front, element->color);
	    }
	}
    }
  
  r32 labelVSpace = 5.f;
  r32 labelHeight = 15.f;
  v2 textDim = getTextDim(layout->context->font, element->name);
  r32 textScale = labelHeight/textDim.y;
  v2 labelDim = textScale*textDim;      
  if(element->flags & UIElementFlag_drawLabelAbove)
    {                                   
      v2 labelCenter = V2(elementRegionCenter.x,
			  elementRegionCenter.y + 0.5f*elementRegionDim.y + labelVSpace + 0.5f*labelHeight);
      Rect2 labelRegion = rectAddRadius(rectCenterDim(labelCenter, labelDim), V2(2, 1));
      
      renderPushRectOutline(commands, labelRegion, 2.f, RenderLevel_front, V4(0, 0, 0, 1));
      renderPushText(commands, layout->context->font, element->name,
		     labelRegion.min, V2(textScale, textScale), labelDim.x);
    }
  if(element->flags & UIElementFlag_drawLabelBelow)
    {           
      v2 labelCenter = V2(elementRegionCenter.x,
			  elementRegionCenter.y - 0.5f*elementRegionDim.y - labelVSpace - 0.5f*labelHeight);
      Rect2 labelRegion = rectAddRadius(rectCenterDim(labelCenter, labelDim), V2(2, 1));
      
      renderPushRectOutline(commands, labelRegion, 2.f, RenderLevel_front, V4(0, 0, 0, 1));
      renderPushText(commands, layout->context->font, element->name,
		     labelRegion.min, V2(textScale, textScale), labelDim.x);
    }

  if(element->next)
    {
      if(element->next != (UIElement *)&element->parent->first)
	{
	  renderPushUIElement(commands, element->next);
	}
    }
  if(element->first)
    {
      if(element->first != (UIElement *)&element->first)
	{
	  renderPushUIElement(commands, element->first);
	}
    }  
}

inline void
renderPushUILayout(RenderCommands *commands, UILayout *layout)
{
  UIContext *context = layout->context;
  layout->selectedElement = {};

  u32 selectedElementOrdinal = 0;
  if(context->selectedElementOrdinal + context->processedElementCount <= layout->elementCount)
    {
      selectedElementOrdinal = context->selectedElementOrdinal;
    }
  else if(context->selectedElementOrdinal >= context->processedElementCount)
    {
      if(context->selectedElementOrdinal - context->processedElementCount <= layout->elementCount)
      	{
	  selectedElementOrdinal = context->selectedElementOrdinal - context->processedElementCount;
      	}
    }
  
  UIElement *selectedElement = 0;
  for(u32 i = 0; i < selectedElementOrdinal; ++i)
    {	  
      if(selectedElement)
	{
	  bool advanced = false;
	  if(selectedElement->first)
	    {
	      if(selectedElement->first != ELEMENT_SENTINEL(selectedElement))
		{
		  selectedElement = selectedElement->first;
		  advanced = true;
		}
	    }
	      
	  if(!advanced && selectedElement->next)
	    {
	      if(selectedElement->next != ELEMENT_SENTINEL(selectedElement->parent))
		{
		  selectedElement = selectedElement->next;
		  advanced = true;
		}
	    }
	      
	  ASSERT(advanced);
	}
      else
	{
	  selectedElement = layout->root;
	}
    }

  if(selectedElement)
    {
      layout->selectedElement = selectedElement->hashKey;
      context->selectedElement = selectedElement->hashKey;
      context->selectedElementLayoutIndex = layout->index;
    }

  renderPushUIElement(commands, layout->root);
}


