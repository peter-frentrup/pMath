#ifndef RICHMATH__GRAPHICS__SHAPERS_H__INCLUDED
#define RICHMATH__GRAPHICS__SHAPERS_H__INCLUDED

#define CHAR_LINE_CONTINUATION  0xF3B1

#include <graphics/canvas.h>
#include <graphics/glyphs.h>
#include <graphics/rectangle.h>

#include <util/array.h>
#include <util/autobool.h>
#include <util/hashtable.h>
#include <util/sharedptr.h>


namespace richmath {
  class Context;
  
  class BoxSize {
    public:
      BoxSize(): width(0), ascent(0), descent(0) {}
      BoxSize(float w, float a, float d): width(w), ascent(a), descent(d) {}
      
      float width;
      float ascent;
      float descent;
      float height() const { return ascent + descent; }
      float center() const { return (ascent - descent) / 2; }
      
      bool is_empty() const { return width == 0 && height() == 0; }
      
      bool operator==(const BoxSize &other) const {
        return other.width   == width &&
               other.ascent  == ascent &&
               other.descent == descent;
      }
      
      bool operator!=(const BoxSize &other) const {
        return !(*this == other);
      }
      
      const BoxSize &merge(const BoxSize &other) {
        if(other.width > width)
          width = other.width;
        if(other.ascent > ascent)
          ascent = other.ascent;
        if(other.descent > descent)
          descent = other.descent;
          
        return *this;
      }
      
      void bigger_y(float *a, float *d) const {
        if(*a < ascent)
          *a = ascent;
        if(*d < descent)
          *d = descent;
      };
      
      RectangleF to_rectangle() const {
        return to_rectangle(Point(0, 0));
      }
      
      RectangleF to_rectangle(const Point &origin) const {
        return RectangleF(origin.x, origin.y - ascent, width, height());
      }
  };
  
  class TextShaper: public Shareable {
    public:
      TextShaper();
      virtual ~TextShaper() {}
      
      virtual uint8_t num_fonts() = 0;
      virtual FontFace font(uint8_t fontinfo) = 0;
      virtual String font_name(uint8_t fontinfo) = 0;
      
      virtual void decode_token(
        Context        &context,
        int             len,
        const uint16_t *str,
        GlyphInfo      *result) = 0;
        
      virtual void vertical_glyph_size(
        Context         &context,
        const uint16_t   ch,
        const GlyphInfo &info,
        float           *ascent,
        float           *descent);
        
      virtual void show_glyph(
        Context         &context,
        Point            pos,
        const uint16_t   ch,
        const GlyphInfo &info);
        
      virtual SharedPtr<TextShaper> set_style(FontStyle style) = 0;
      
      virtual FontStyle get_style() = 0;
      
      virtual float get_center_height(Context &context, uint8_t fontinfo);
      
    public:
      static SharedPtr<TextShaper> find(const String &name, FontStyle style);
      
      static uint32_t get_accent_char(uint32_t input_char);
      
      static void clear_cache();
  };
  
  class FallbackTextShaper: public TextShaper {
    public:
      FallbackTextShaper(SharedPtr<TextShaper> default_shaper);
      virtual ~FallbackTextShaper();
      
      void add(SharedPtr<TextShaper> fallback);
      void add_default();
      
      static void add_or_create(SharedPtr<FallbackTextShaper> &all, SharedPtr<TextShaper> fallback);
      
      ArrayView<const SharedPtr<TextShaper>> all_shapers() const { return _shapers; }
      
      virtual uint8_t num_fonts() override;
      virtual FontFace font(uint8_t fontinfo) override;
      virtual String font_name(uint8_t fontinfo) override;
      
      virtual void decode_token(
        Context        &context,
        int             len,
        const uint16_t *str,
        GlyphInfo      *result) override;
        
      virtual void vertical_glyph_size(
        Context         &context,
        const uint16_t   ch,
        const GlyphInfo &info,
        float           *ascent,
        float           *descent) override;
        
      virtual void show_glyph(
        Context         &context,
        Point            pos,
        const uint16_t   ch,
        const GlyphInfo &info) override;
        
      virtual SharedPtr<TextShaper> set_style(FontStyle style) override;
      
      virtual FontStyle get_style() override;
      
      virtual float get_center_height(Context &context, uint8_t fontinfo) override;
      
    protected:
      int fallback_index(uint8_t *fontinfo);
      int first_missing_glyph(int len, const GlyphInfo *glyphs);
      
    private:
      Array<SharedPtr<TextShaper> > _shapers;
  };
  
  class CharBoxTextShaper: public TextShaper {
    public:
      CharBoxTextShaper();
      virtual ~CharBoxTextShaper();
      
      virtual uint8_t num_fonts() override {                return 1; }
      virtual FontFace font(uint8_t fontinfo) override {    return FontFace(); }
      virtual String font_name(uint8_t fontinfo) override { return ""; }
      
      virtual void decode_token(
        Context        &context,
        int             len,
        const uint16_t *str,
        GlyphInfo      *result) override;
        
      virtual void vertical_glyph_size(
        Context         &context,
        const uint16_t   ch,
        const GlyphInfo &info,
        float           *ascent,
        float           *descent) override;
        
      virtual void show_glyph(
        Context         &context,
        Point            pos,
        const uint16_t   ch,
        const GlyphInfo &info) override;
        
      virtual SharedPtr<TextShaper> set_style(FontStyle style) override;
      
      virtual FontStyle get_style() override { return NoStyle; }
  };
  
  class RadicalShapeInfo {
    public:
      int   size; // negative => small root, otherwise # of vertical pieces
      unsigned hbar      : 31;
      unsigned surd_form : 1;
      float y_offset;
  };
  
  class MathSequence;
  
  class MathShaper: public TextShaper {
    public:
      static const float SpanMaxRestrictedSizeFactor;

      virtual float flattened_accent_base_height(Context &context) = 0;
      
      virtual bool horizontal_stretch_char(
        Context        &context,
        float           width,
        const uint16_t  ch,
        GlyphInfo      *result) = 0;
        
      virtual void vertical_stretch_char(
        Context        &context,
        float           ascent,
        float           descent,
        bool            full_stretch,
        const uint16_t  ch,
        GlyphInfo      *result) = 0;
        
      virtual void shape_fraction(
        Context        &context,
        const BoxSize  &num,
        const BoxSize  &den,
        float          *num_shift,
        float          *den_shift,
        float          *width) = 0;
        
      virtual void show_fraction(
        Context        &context,
        float           width) = 0;
        
      virtual float italic_correction(
        Context          &context,
        uint16_t          ch,
        const GlyphInfo  &info) = 0;
        
      virtual void accent_positions(
        Context           &context,
        MathSequence      *base,
        MathSequence      *under,
        MathSequence      *over,
        float             *base_x,
        Vector2F          *underscript_offset,
        Vector2F          *overscript_offset,
        bool               under_is_stretched,
        bool               over_is_stretched,
        AutoBoolValues     limits_positioning) = 0;
        
      virtual void script_positions(
        Context           &context,
        float              base_ascent,
        float              base_descent,
        MathSequence      *sub,
        MathSequence      *super,
        float             *sub_y,
        float             *super_y) = 0;
        
      virtual void script_corrections(
        Context           &context,
        uint16_t           base_char,
        const GlyphInfo   &base_info,
        MathSequence      *sub,
        MathSequence      *super,
        float              sub_y,
        float              super_y,
        float             *sub_x,
        float             *super_x) = 0;
        
      virtual void shape_radical(
        Context          &context,
        BoxSize          *box,        // in/out
        float            *radicand_x, // out
        Vector2F         *exponent_offset, // out
        RadicalShapeInfo *info) = 0;  // out
        
      virtual void show_radical(
        Context                &context,
        const RadicalShapeInfo &info) = 0;
        
      virtual void get_script_size_multis(Array<float> *arr) = 0;
      
      SharedPtr<MathShaper> math_set_style(FontStyle style);
      
    public:
      static Hashtable<String, SharedPtr<MathShaper> > available_shapers;
  };
  
  class SmallRadicalGlyph {
    public:
      SmallRadicalGlyph() {
      }
      
      SmallRadicalGlyph(
        uint16_t     index,
        uint16_t     hbar_index,
        float        rel_ascent,
        Vector2F     rel_exp_offset)
        : index(index),
          hbar_index(hbar_index),
          rel_ascent(rel_ascent),
          rel_exp_offset(rel_exp_offset)
      {}
      
    public:
      uint16_t index;
      uint16_t hbar_index;
      float    rel_ascent;
      Vector2F rel_exp_offset; // from top-left
  };
  
  class SimpleMathShaper: public MathShaper {
    public:
      SimpleMathShaper(int _radicalfont);
      ~SimpleMathShaper();
      
      virtual float flattened_accent_base_height(Context &context) override;
      
      virtual void vertical_glyph_size(
        Context         &context,
        const uint16_t   ch,
        const GlyphInfo &info,
        float           *ascent,
        float           *descent) override;
        
      virtual void show_glyph(
        Context         &context,
        Point            pos,
        const uint16_t   ch,
        const GlyphInfo &info) override;
        
      virtual bool horizontal_stretch_char(
        Context        &context,
        float           width,
        const uint16_t  ch,
        GlyphInfo      *result) override;
        
      virtual void vertical_stretch_char(
        Context        &context,
        float           ascent,
        float           descent,
        bool            full_stretch,
        const uint16_t  ch,
        GlyphInfo      *result) override;
        
      virtual void accent_positions(
        Context           &context,
        MathSequence      *base,
        MathSequence      *under,
        MathSequence      *over,
        float             *base_x,
        Vector2F          *underscript_offset,
        Vector2F          *overscript_offset,
        bool               under_is_stretched,
        bool               over_is_stretched,
        AutoBoolValues     limits_positioning) override;
        
      virtual void script_positions(
        Context           &context,
        float              base_ascent,
        float              base_descent,
        MathSequence      *sub,
        MathSequence      *super,
        float             *sub_y,
        float             *super_y) override;
        
      virtual void script_corrections(
        Context           &context,
        uint16_t           base_char,
        const GlyphInfo   &base_info,
        MathSequence      *sub,
        MathSequence      *super,
        float              sub_y,
        float              super_y,
        float             *sub_x,
        float             *super_x) override;
        
      virtual void shape_fraction(
        Context        &context,
        const BoxSize  &num,
        const BoxSize  &den,
        float          *num_y,
        float          *den_y,
        float          *width) override;
        
      virtual void show_fraction(
        Context  &context,
        float     width) override;
        
      virtual float italic_correction(
        Context          &context,
        uint16_t          ch,
        const GlyphInfo  &info) override;
        
      virtual void shape_radical(
        Context          &context,    // in
        BoxSize          *box,        // in/out
        float            *radicand_x, // out
        Vector2F         *exponent_offset, // out
        RadicalShapeInfo *info) override;      // out
        
      virtual void show_radical(
        Context                &context,
        const RadicalShapeInfo &info) override;
        
      virtual void get_script_size_multis(Array<float> *arr) override;
      
    protected:
      int _radical_font;
      
    protected:
      virtual int h_stretch_glyphs(
        uint16_t         ch,
        const uint8_t  **fonts,
        const uint16_t **glyphs) = 0;
        
      virtual int h_stretch_big_glyphs(
        uint16_t  ch,
        uint16_t *left,
        uint16_t *middle,
        uint16_t *right,
        uint16_t *special_center) = 0;
        
      virtual int v_stretch_glyphs(
        uint16_t         ch,
        bool             full_stretch,
        const uint8_t  **fonts,
        const uint16_t **glyphs) = 0;
        
      virtual int v_stretch_pair_glyphs(
        uint16_t  ch,
        uint16_t *upper,
        uint16_t *lower);
        
      virtual int v_stretch_big_glyphs(
        uint16_t  ch,
        uint16_t *top,
        uint16_t *middle,
        uint16_t *bottom,
        uint16_t *special_center) = 0;
        
      virtual const SmallRadicalGlyph *small_radical_glyphs() = 0; // zero terminated
      
      virtual void big_radical_glyphs(
        uint16_t     *bottom,
        uint16_t     *vertical,
        uint16_t     *edge,
        uint16_t     *horizontal,
        Vector2F     *rel_exponent_offset) = 0;
  };
}

#endif // RICHMATH__GRAPHICS__SHAPERS_H__INCLUDED
