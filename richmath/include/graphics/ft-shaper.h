#ifndef __GRAPHICS__FT_SHAPER_H__
#define __GRAPHICS__FT_SHAPER_H__

#include <graphics/shapers.h>
#include <util/config.h>

#ifndef RICHMATH_USE_FT_FONT
  #error freetype fonts not supported in this build
#endif

#ifndef CAIRO_HAS_FC_FONT
  #error need freetype font backend
#endif

namespace richmath{
  class FreetypeFontShaper: public TextShaper{
    public:
      FreetypeFontShaper(
        const String          &name,
        FontStyle              style);
        
      virtual ~FreetypeFontShaper();
      
      virtual void decode_token(
        Context        *context,
        int             len,
        const uint16_t *str, 
        GlyphInfo      *result);
      
      virtual uint8_t num_fonts(){ return 1; }
      virtual FontFace font(uint8_t fontinfo){ return _font; }
      virtual String font_name(uint8_t fontinfo){ return _name; }
      virtual FontStyle get_style(){ return _style; }
      
      SharedPtr<TextShaper> set_style(FontStyle style);
      
    private:
      String    _name;
      FontStyle _style;
      FontFace  _font;
  };
}

#endif // __GRAPHICS__FT_SHAPER_H__
