static inline void // NOTE: not textured
renderPushQuad(RenderCommands *commands, Rect2 rect, v4 color = V4(1, 1, 1, 1))
{
  ASSERT(commands->quadCount < commands->quadCapacity);
  TexturedQuad *quad = commands->quads + commands->quadCount++;

  v2 bottomLeft = rect.min;
  v2 dim = getDim(rect);
  quad->vertices[0] = {bottomLeft, V2(0, 0), color};
  quad->vertices[1] = {bottomLeft + V2(dim.x, 0), V2(0, 0), color};
  quad->vertices[2] = {bottomLeft + V2(0, dim.y), V2(0, 0), color};
  quad->vertices[3] = {bottomLeft + dim, V2(0, 0), color};
}

static inline void // NOTE: textured
renderPushQuad(RenderCommands *commands, Rect2 rect, LoadedBitmap *texture, v4 color = V4(1, 1, 1, 1))
{
  if(texture)
    {
      ASSERT(commands->quadCount < commands->quadCapacity);
      TexturedQuad *quad = commands->quads + commands->quadCount++;

      v2 dim = getDim(rect);
      v2 bottomLeft = rect.min - V2(texture->alignPercentage.x*dim.x, texture->alignPercentage.y*dim.y);      
      quad->vertices[0] = {bottomLeft, V2(0, 0), color};
      quad->vertices[1] = {bottomLeft + V2(dim.x, 0), V2(1, 0), color};
      quad->vertices[2] = {bottomLeft + V2(0, dim.y), V2(0, 1), color};
      quad->vertices[3] = {bottomLeft + dim, V2(1, 1), color};
  
      quad->texture = texture;
    }
}

static inline void
renderPushRectOutline(RenderCommands *commands, Rect2 rect, r32 thickness, v4 color = V4(1, 1, 1, 1))
{ 
  v2 rectDim = getDim(rect);
  v2 vDim = V2(thickness, rectDim.y);
  v2 hDim = V2(rectDim.x, thickness);

  v2 leftMiddle = rect.min + V2(0, 0.5f*rectDim.y);
  v2 rightMiddle = rect.max - V2(0, 0.5f*rectDim.y);
  v2 bottomMiddle = rect.min + V2(0.5f*rectDim.x, 0);
  v2 topMiddle = rect.max - V2(0.5f*rectDim.x, 0);
  
  renderPushQuad(commands, rectCenterDim(leftMiddle, vDim), color);
  renderPushQuad(commands, rectCenterDim(rightMiddle, vDim), color);
  renderPushQuad(commands, rectCenterDim(bottomMiddle, hDim), color);
  renderPushQuad(commands, rectCenterDim(topMiddle, hDim), color);
}

static inline void
renderPushTriangle(RenderCommands *commands, Vertex v1, Vertex v2, Vertex v3, LoadedBitmap *texture = 0)
{
  ASSERT(commands->triangleCount < commands->triangleCapacity);
  TexturedTriangle *triangle = commands->triangles + commands->triangleCount++;
  
  triangle->vertices[0] = v1;
  triangle->vertices[1] = v2;
  triangle->vertices[2] = v3;

  triangle->texture = texture;
}

static inline v2
renderPushText(RenderCommands *commands, LoadedFont *font, u8 *string,
	       v2 textMin, v2 textScale,
	       //Rect2 textRect,
	       v4 color = V4(1, 1, 1, 1))
{
  /* v2 textDimBase = getTextDim(font, string); */
  /* v2 textDim = getDim(textRect); */
  /* v2 textScale = V2(textDim.x/textDimBase.x, textDim.y/textDimBase.y); */

  //v2 atPos = textRect.min;
  v2 atPos = textMin;
  u8 *at = string;  
  while(*at)
    {
      u8 c = *at;
      LoadedBitmap *glyph = getGlyphFromChar(font, c);

      v2 glyphDim = V2(glyph->width, glyph->height);
      v2 scaledGlyphDim = hadamard(textScale, glyphDim);
      Rect2 glyphRect = rectMinDim(atPos, scaledGlyphDim);
      renderPushQuad(commands, glyphRect, glyph, color);

      ++at;
      //if(*at)
      atPos.x += textScale.x*getHorizontalAdvance(font, c, *at);
    }

  atPos.y -= textScale.y*font->verticalAdvance;
  return(atPos);
}

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
	elementMin.x = element->semanticOffset[UIAxis_x].value*getDim(element->parent->region).x;
      } break;
    case UISizeType_percentOfOwnDim:
      {
	elementMin.x = element->semanticOffset[UIAxis_x].value*elementDim.x;
      } break;
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
	elementMin.y = element->semanticOffset[UIAxis_y].value*getDim(element->parent->region).y;
      } break;
    case UISizeType_percentOfOwnDim:
      {
	elementMin.y = element->semanticOffset[UIAxis_y].value*elementDim.y;
      } break;
    default: ASSERT(!"unhandled/invalid case");	  
    }

  Rect2 elementRect = rectMinDim(elementMin, elementDim);
  return(elementRect);
}

inline void
renderPushUIElement(RenderCommands *commands, UILayout *layout, UIElement *element)
{
  element->region = renderComputeUIElementRegion(commands, element);
  //element->hotRegion = element->region;

  v2 elementRegionCenter = getCenter(element->region);
  v2 elementRegionDim = getDim(element->region);
  // TODO: pull out common formatting computations
  if(element->flags & UIElementFlag_drawText)
    {      
      //v2 textDim = getTextDim(layout->font, element->name);
      //v2 textScale = V2(elementRegionDim.x/textDim.x, elementRegionDim.y/textDim.y);
      renderPushText(commands, layout->font, element->name, element->region.min, element->textScale);
    }
  if(element->flags & UIElementFlag_drawBorder)
    {
      v4 color = (uiHashKeysAreEqual(element->hashKey, layout->selectedElement)) ? V4(1, 0.5f, 0, 1) : V4(0, 0, 0, 1);
      
      renderPushRectOutline(commands, element->region, 2.f, color);
    }
  if(element->flags & UIElementFlag_drawBackground)
    {
      renderPushQuad(commands, element->region, element->color);
    }  
  if(element->flags & UIElementFlag_draggable)
    {            
      v2 travelDim = V2(0.2f*elementRegionDim.x, elementRegionDim.y);
      Rect2 travelRect = rectCenterDim(elementRegionCenter, travelDim);
      
      renderPushQuad(commands, travelRect, V4(0, 0, 0, 1));

      if(element->parameterType == UIParameter_float)
	{
	  r32 paramValue = pluginReadFloatParameter(element->fParam);
	  r32 paramPercentage = (paramValue - element->fParam->range.min)/(element->fParam->range.max - element->fParam->range.min);
	  
	  v2 faderCenter = V2(elementRegionCenter.x, element->region.min.y + paramPercentage*elementRegionDim.y);
	  Rect2 faderRect = rectCenterDim(faderCenter, V2(elementRegionDim.x, 0.1f*elementRegionDim.y));
	  
	  renderPushQuad(commands, faderRect, element->color);
	}
    }
  r32 labelVSpace = 5.f;
  r32 labelHeight = 15.f;
  v2 textDim = getTextDim(layout->font, element->name);
  r32 textScale = labelHeight/textDim.y;
  v2 labelDim = textScale*textDim;      
  if(element->flags & UIElementFlag_drawLabelAbove)
    {                                   
      v2 labelCenter = V2(elementRegionCenter.x,
			  elementRegionCenter.y + 0.5f*elementRegionDim.y + labelVSpace + 0.5f*labelHeight);
      Rect2 labelRegion = rectCenterDim(labelCenter, labelDim);
      
      renderPushRectOutline(commands, labelRegion, 2.f, V4(0, 0, 0, 1));
      renderPushText(commands, layout->font, element->name, labelRegion.min, V2(textScale, textScale));
    }
  if(element->flags & UIElementFlag_drawLabelBelow)
    {           
      v2 labelCenter = V2(elementRegionCenter.x,
			  elementRegionCenter.y - 0.5f*elementRegionDim.y - labelVSpace - 0.5f*labelHeight);
      Rect2 labelRegion = rectCenterDim(labelCenter, labelDim);
      
      renderPushRectOutline(commands, labelRegion, 2.f, V4(0, 0, 0, 1));
      renderPushText(commands, layout->font, element->name, labelRegion.min, V2(textScale, textScale));
    }

  if(element->next)
    {
      if(element->next != (UIElement *)&element->parent->first)
	{
	  renderPushUIElement(commands, layout, element->next);
	}
    }
  if(element->first)
    {
      if(element->first != (UIElement *)&element->first)
	{
	  renderPushUIElement(commands, layout, element->first);
	}
    }  
}

inline void
renderPushUILayout(RenderCommands *commands, UILayout *layout)
{
  renderPushUIElement(commands, layout, layout->root);
}


