#include <util/config.h>

#ifdef RICHMATH_USE_FT_FONT

#include <graphics/ft-shaper.h>
#include <graphics/context.h>

using namespace richmath;

//{ class FreetypeFontShaper ...

FreetypeFontShaper::FreetypeFontShaper(
  const String  &name,
  FontStyle      style)
: TextShaper(),
  _name(name),
  _style(style),
  _font(name, style)
{
}

FreetypeFontShaper::~FreetypeFontShaper(){
}

void FreetypeFontShaper::decode_token(
  Context        *context,
  int             len,
  const uint16_t *str, 
  GlyphInfo      *result
){
  memset(result, 0, len * sizeof(GlyphInfo));
  cairo_text_extents_t cte;
  cairo_glyph_t cg;
  
  FontInfo info(_font);
  context->canvas->set_font_face(_font);
  
  cg.x = 0;
  cg.y = 0;
  for(int i = 0;i < len;++i){
    if(is_utf16_high(str[i]) && i + 1 < len && is_utf16_low(str[i+1])){
      uint32_t ch = 0x10000 + (((str[i] & 0x03FF) << 10) | (str[i+1] & 0x03FF));
      
      result[i].index = cg.index = info.char_to_glyph(ch);
      if(cg.index == 0){
        result[i].index = UnknownGlyph;
      }
      else{
        context->canvas->glyph_extents(&cg, 1, &cte);
        result[i].right = cte.x_advance;
      }
      
      ++i;
    }
    else{
      result[i].index = cg.index = info.char_to_glyph(str[i]);
      if(cg.index == 0){
        result[i].index = UnknownGlyph;
      }
      else{
        context->canvas->glyph_extents(&cg, 1, &cte);
        result[i].right = cte.x_advance;
      }
    }
  }
  
}

SharedPtr<TextShaper> FreetypeFontShaper::set_style(FontStyle style){
  return find(_name, style);
}

//} ... class FreetypeFontShaper

#endif // RICHMATH_USE_FT_FONT
