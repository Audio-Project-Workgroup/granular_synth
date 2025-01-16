enum TextFlags : u32
{
  TextFlags_none = 0,
  
  TextFlags_centered = (1 << 0),
  TextFlags_scaleAspect = (1 << 1),
};

struct TextArgs
{
  u32 flags;
  r32 hScale;
  r32 vScale;
};

inline TextArgs
defaultTextArgs(void)
{
  TextArgs result = {};

  return(result);
}

inline Rect2
getTextRectangle(LoadedFont *font, char *text, u32 windowWidth, u32 windowHeight)
{
  Rect2 result = {};

  char *at = text;
  while(*at)
    {
      char c = *at;
      u32 glyphIndex = c - font->characterRange.min;
      ASSERT(glyphIndex < font->glyphCount);
      LoadedBitmap *glyph = font->glyphs + glyphIndex;
      r32 scaledGlyphHeight = 2.f*(r32)glyph->height/(r32)windowHeight;
      result.max.y = MAX(result.max.y, scaledGlyphHeight);

      ++at;
      if(*at)
	{	  
	  r32 scaledAdvance = 2.f*(r32)getHorizontalAdvance(font, c, *at)/(r32)windowWidth;
	  result.max.x += scaledAdvance;
	}      
    }

  return(result);
}

inline v2
getTextDim(LoadedFont *font, char *text)
{
  v2 result = {};

  char *at = text;
  while(*at)
    {
      char c = *at;
      u32 glyphIndex = c - font->characterRange.min;
      ASSERT(glyphIndex < font->glyphCount);
      LoadedBitmap *glyph = font->glyphs + glyphIndex;
      result.y = MAX(result.y, (r32)glyph->height);

      ++at;
      if(*at) result.x += (r32)getHorizontalAdvance(font, c, *at); 
    }

  return(result);
}
