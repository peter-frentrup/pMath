#ifndef RICHMATH__GRAPHICS__FT_SHAPER_H__INCLUDED
#define RICHMATH__GRAPHICS__FT_SHAPER_H__INCLUDED

#include <graphics/shapers.h>

#ifndef RICHMATH_USE_FT_FONT
#  error freetype fonts not supported in this build
#endif

#ifndef CAIRO_HAS_FT_FONT
#  error need freetype font backend
#endif


namespace richmath {
  class FreetypeFontShaper: public TextShaper {
    public:
      FreetypeFontShaper(
        const String          &name,
        FontStyle              style);

      virtual ~FreetypeFontShaper();

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

#endif // RICHMATH__GRAPHICS__FT_SHAPER_H__INCLUDED
