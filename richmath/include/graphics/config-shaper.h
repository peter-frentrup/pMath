#ifndef __GRAPHICS__CONFIG_SHAPER_H__
#define __GRAPHICS__CONFIG_SHAPER_H__

#include <graphics/shapers.h>

#include <util/array.h>
#include <util/pmath-extra.h>

namespace richmath{
  class GlyphFontOffset{
    public:
      GlyphFontOffset(
        uint16_t g = 0,
        uint8_t  f = 0,
        int8_t   o = 0)
      : glyph(g), font(f), offset(o)
      {
      }
      
      explicit GlyphFontOffset(Expr expr);
      
    public:
      uint16_t glyph;
      uint8_t  font;
      int8_t   offset;
      
      static const float EmPerOffset;
  };
  
  class ScriptIndent{
    public:
      int8_t super;
      int8_t sub;
      int8_t center;
      
      ScriptIndent(int8_t _super = 0, int8_t _sub = 0, int8_t _center = 0)
      : super(_super), sub(_sub), center(_center)
      {
      }
      
      explicit ScriptIndent(Expr expr);
  };
  
  class StretchGlyphArray{
    public:
      Array<uint16_t>  glyphs;
      Array<uint8_t>   fonts;
      bool             vertical;
  };
  
  class ComposedGlyph{
    public:
      ComposedGlyph()
      : top(0), 
        bottom(0), 
        middle(0), 
        special_center(0), 
        upper(0), 
        lower(0), 
        tbms_font(0), 
        ul_font(0)
      {
      }
      
    public:
      uint16_t top;
      uint16_t bottom;
      uint16_t middle;
      uint16_t special_center;
      
      uint16_t upper;
      uint16_t lower;
      
      uint8_t tbms_font;
      uint8_t ul_font;
      
      bool vertical;
  };
  
  class BigRadicalGlyph{
    public:
      uint16_t  bottom;
      uint16_t  vertical;
      uint16_t  edge;
      uint16_t  horizontal;
      float     rel_exp_x;
      float     rel_exp_y;
  };
  
  class RadicalGlyphs{
    public:
      Array<SmallRadicalGlyph>  small_glyphs;
      BigRadicalGlyph           big_glyph;
      uint8_t                   font;
  };
  
  class ConfigShaper;
  
  class ConfigShaperDB: public Shareable {
    public:
      virtual ~ConfigShaperDB();
      void clear_cache();
      
      bool verify();
      static SharedPtr<ConfigShaperDB> load_from_object(const Expr expr);
      
      SharedPtr<ConfigShaper> find(FontStyle style);
      
      static Hashtable<String, SharedPtr<ConfigShaperDB> > registered;
      static void clear_all();
      
    private:
      ConfigShaperDB();
      
    private:
      SharedPtr<ConfigShaper> shapers[FontStyle::Permutations];
    
    public:
      Hashtable<
        uint32_t, 
        GlyphFontOffset, 
        cast_hash> char_to_glyph_map;
        
      Hashtable<
        String, 
        Array<GlyphFontOffset> > ligatures;
      
      Hashtable<
        uint32_t, 
        Array<GlyphFontOffset>,
        cast_hash> complex_glyphs;
      
      Hashtable<
        uint32_t,
        StretchGlyphArray,
        cast_hash> stretched_glyphs;
        
      Hashtable<
        uint32_t,
        ComposedGlyph,
        cast_hash> composed_glyphs;
      
      Hashtable<
        uint32_t,
        ScriptIndent,
        cast_hash> script_indents;
      
      Array<float> script_size_multipliers;
        
      ScriptIndent italic_script_indent;
      
      RadicalGlyphs radical;
      
      Array<String> math_fontnames;
      Array<String> text_fontnames;
      String shaper_name;
  };
  
  class ConfigShaper: public SimpleMathShaper{
    friend class ConfigShaperDB;
    public:
      virtual ~ConfigShaper();
      
      virtual uint8_t num_fonts();
      virtual FontFace font(uint8_t fontinfo);
      virtual String font_name(uint8_t fontinfo);
      
      virtual void decode_token(
        Context        *context,
        int             len,
        const uint16_t *str, 
        GlyphInfo      *result);
      
      virtual void vertical_glyph_size(
        Context         *context,
        const uint16_t   ch,
        const GlyphInfo &info,
        float           *ascent,
        float           *descent);
      
      virtual void show_glyph(
        Context         *context, 
        float            x,
        float            y,
        const uint16_t   ch,
        const GlyphInfo &info);
      
      virtual float italic_correction(
        Context          *context,
        uint16_t          ch,
        const GlyphInfo  &info);
      
      virtual void script_corrections(
        Context           *context,
        uint16_t           base_char, 
        const GlyphInfo   &base_info,
        MathSequence      *sub,
        MathSequence      *super,
        float              sub_y,
        float              super_y,
        float             *sub_x,
        float             *super_x);
      
      virtual void get_script_size_multis(Array<float> *arr);
      
      virtual SharedPtr<TextShaper> set_style(FontStyle _style);
      
      virtual FontStyle get_style(){ return style; }
      
    protected:
      ConfigShaper(SharedPtr<ConfigShaperDB> _db, FontStyle _style);
      
      virtual int h_stretch_glyphs(
        uint16_t         ch,
        const uint8_t  **fonts, 
        const uint16_t **glyphs);
      
      virtual int h_stretch_big_glyphs(
        uint16_t  ch,
        uint16_t *left,
        uint16_t *middle,
        uint16_t *right,
        uint16_t *special_center);
        
      virtual int v_stretch_glyphs(
        uint16_t         ch,
        bool             full_stretch,
        const uint8_t  **fonts, 
        const uint16_t **glyphs);
        
      virtual int v_stretch_pair_glyphs(
        uint16_t  ch,
        uint16_t *upper,
        uint16_t *lower);
        
      virtual int v_stretch_big_glyphs(
        uint16_t  ch,
        uint16_t *top,
        uint16_t *middle,
        uint16_t *bottom,
        uint16_t *special_center);
      
      virtual const SmallRadicalGlyph *small_radical_glyphs(); // zero terminated
      
      virtual void big_radical_glyphs(
        uint16_t     *bottom,
        uint16_t     *vertical,
        uint16_t     *edge,
        uint16_t     *horizontal,
        float        *_rel_exp_x,
        float        *_rel_exp_y);
        
    protected:
      SharedPtr<ConfigShaperDB>      db;
      SharedPtr<FallbackTextShaper>  text_shaper;
      Array<FontFace>                math_font_faces;
      FontStyle                      style;
  };
}

#endif /* __GRAPHICS__CONFIG_SHAPER_H__ */
