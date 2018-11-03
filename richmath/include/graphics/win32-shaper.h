#ifndef RICHMATH__GRAPHICS__WIN32_SHAPER_H__INCLUDED
#define RICHMATH__GRAPHICS__WIN32_SHAPER_H__INCLUDED

#include <graphics/shapers.h>

#ifndef RICHMATH_USE_WIN32_FONT
#error win32 fonts not supported in this build
#endif

#ifndef CAIRO_HAS_WIN32_FONT
#error need win32 font backend
#endif


namespace richmath {
  class WindowsFontShaper: public TextShaper {
    public:
      WindowsFontShaper(
        const String          &name,
        FontStyle              style);
        
      virtual ~WindowsFontShaper();
      
      virtual void decode_token(
        Context        *context,
        int             len,
        const uint16_t *str,
        GlyphInfo      *result) override;
        
      virtual uint8_t num_fonts() override { return 1; }
      virtual FontFace font(uint8_t fontinfo) override { return _font; }
      virtual String font_name(uint8_t fontinfo) override { return _name; }
      virtual FontStyle get_style() override { return _style; }
      
      virtual SharedPtr<TextShaper> set_style(FontStyle style) override;
      
    private:
      String    _name;
      FontStyle _style;
      FontFace  _font;
  };
}

#endif // RICHMATH__GRAPHICS__WIN32_SHAPER_H__INCLUDED
