struct PluginAsset
{
  Rect2 rect;
  Rect2 uv;
  r32 advance;  
};

#define PLUGIN_ASSET(name) (globalPluginAssets + PluginAsset_##name)
#define ASSET_UV(name) PLUGIN_ASSET(name)->uv

static inline PluginAsset*
getGlyphFromChar(LoadedFont *font, u8 c)
{
  u32 glyphIdx = c - font->characterRange.min;
  PluginAsset *result = font->glyphs + glyphIdx;
  return(result);
}

static inline v2
getGlyphDim(PluginAsset *glyph)
{
  v2 result = getDim(glyph->rect);
  return(result);
}

static inline r32
getHorizontalAdvance(LoadedFont *font, u8 c, u8 other)
{
  UNUSED(other);
  PluginAsset *glyph = getGlyphFromChar(font, c);
  r32 result = glyph->advance;
  return(result);
}

static inline v2
getTextDim(LoadedFont *font, String8 text)
{
  r32 width = 0;
  r32 height = 0;

  u8 *opl = text.str + text.size;
  for(u8 *at = text.str; at < opl; ++at)
    {
      u8 c = *at;
      PluginAsset *glyph = getGlyphFromChar(font, c);
      height = MAX(height, getGlyphDim(glyph).y);
      width += getHorizontalAdvance(font, c, 0);
    }

  v2 result = {};
  result.x = width;
  result.y = height;

  return(result);
}

#include "plugin_assets.generated.h"
