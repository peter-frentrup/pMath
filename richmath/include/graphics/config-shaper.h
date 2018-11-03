#ifndef RICHMATH__GRAPHICS__CONFIG_SHAPER_H__INCLUDED
#define RICHMATH__GRAPHICS__CONFIG_SHAPER_H__INCLUDED

#include <graphics/shapers.h>

#include <util/array.h>
#include <util/pmath-extra.h>


namespace richmath {
  class ConfigShaperTables;
  
  class ConfigShaper: public SimpleMathShaper {
      friend class ConfigShaperDB;
      friend class ConfigShaperTables;
    public:
      virtual ~ConfigShaper();
      
      String name();
      static SharedPtr<ConfigShaper> try_register(const Expr expr);
      static void dispose_all();
      
      virtual uint8_t num_fonts() override;
      virtual FontFace font(uint8_t fontinfo) override;
      virtual String font_name(uint8_t fontinfo) override;
      
      virtual void decode_token(
        Context        *context,
        int             len,
        const uint16_t *str,
        GlyphInfo      *result) override;
        
      virtual void vertical_glyph_size(
        Context         *context,
        const uint16_t   ch,
        const GlyphInfo &info,
        float           *ascent,
        float           *descent) override;
        
      virtual void show_glyph(
        Context         *context,
        float            x,
        float            y,
        const uint16_t   ch,
        const GlyphInfo &info) override;
        
      virtual float italic_correction(
        Context          *context,
        uint16_t          ch,
        const GlyphInfo  &info) override;
        
      virtual void script_corrections(
        Context           *context,
        uint16_t           base_char,
        const GlyphInfo   &base_info,
        MathSequence      *sub,
        MathSequence      *super,
        float              sub_y,
        float              super_y,
        float             *sub_x,
        float             *super_x) override;
        
      virtual void get_script_size_multis(Array<float> *arr) override;
      
      virtual SharedPtr<TextShaper> set_style(FontStyle _style) override;
      
      virtual FontStyle get_style() override { return style; }
      
    protected:
      ConfigShaper(SharedPtr<ConfigShaperTables> _db, FontStyle _style);
      
      virtual int h_stretch_glyphs(
        uint16_t         ch,
        const uint8_t  **fonts,
        const uint16_t **glyphs) override;
        
      virtual int h_stretch_big_glyphs(
        uint16_t  ch,
        uint16_t *left,
        uint16_t *middle,
        uint16_t *right,
        uint16_t *special_center) override;
        
      virtual int v_stretch_glyphs(
        uint16_t         ch,
        bool             full_stretch,
        const uint8_t  **fonts,
        const uint16_t **glyphs) override;
        
      virtual int v_stretch_pair_glyphs(
        uint16_t  ch,
        uint16_t *upper,
        uint16_t *lower) override;
        
      virtual int v_stretch_big_glyphs(
        uint16_t  ch,
        uint16_t *top,
        uint16_t *middle,
        uint16_t *bottom,
        uint16_t *special_center) override;
        
      virtual const SmallRadicalGlyph *small_radical_glyphs() override; // zero terminated
      
      virtual void big_radical_glyphs(
        uint16_t     *bottom,
        uint16_t     *vertical,
        uint16_t     *edge,
        uint16_t     *horizontal,
        float        *_rel_exp_x,
        float        *_rel_exp_y) override;
        
    protected:
      SharedPtr<ConfigShaperTables>  tables;
      SharedPtr<FallbackTextShaper>  text_shaper;
      Array<FontFace>                math_font_faces;
      FontStyle                      style;
  };
}

#endif /* RICHMATH__GRAPHICS__CONFIG_SHAPER_H__INCLUDED */
