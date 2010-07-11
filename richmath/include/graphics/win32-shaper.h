#ifndef __GRAPHICS__WIN32_SHAPER_H__
#define __GRAPHICS__WIN32_SHAPER_H__

#include <graphics/shapers.h>

#ifndef CAIRO_HAS_WIN32_FONT
  #error need win32 font backends
#endif

namespace richmath{
  class WindowsFontShaper: public TextShaper{
    public:
      WindowsFontShaper(
        const String  &name,
        FontStyle      style);
        
      virtual ~WindowsFontShaper();
      
      virtual void decode_token(
        Context        *context,
        int             len,
        const uint16_t *str, 
        GlyphInfo      *result);
      
      virtual FontFace font(uint8_t fontinfo){ return _font; }
      virtual FontStyle get_style(){ return _style; }
      
      SharedPtr<TextShaper> set_style(FontStyle style);
      
    private:
      String    _name;
      FontStyle _style;
      FontFace  _font;
  };
}

#endif // __GRAPHICS__WIN32_SHAPER_H__
