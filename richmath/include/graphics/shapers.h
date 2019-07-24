#ifndef RICHMATH__GRAPHICS__SHAPERS_H__INCLUDED
#define RICHMATH__GRAPHICS__SHAPERS_H__INCLUDED

#define CHAR_LINE_CONTINUATION  0xF3B1

#include <graphics/canvas.h>
#include <graphics/rectangle.h>

#include <util/array.h>
#include <util/hashtable.h>
#include <util/sharedptr.h>


namespace richmath {
  enum {
    GlyphStyleNone,
    GlyphStyleImplicit,
    GlyphStyleString,
    GlyphStyleComment,
    GlyphStyleParameter,
    GylphStyleLocal,
    GylphStyleScopeError,
    GlyphStyleNewSymbol,
    GlyphStyleShadowError,
    GlyphStyleSyntaxError,
    GlyphStyleSpecialUse,
    GlyphStyleExcessOrMissingArg,
    GlyphStyleInvalidOption,
    GlyphStyleSpecialStringPart,
    GlyphStyleKeyword,
    GlyphStyleFunctionCall
  };
  
  enum {
    FontSlantPlain = 1,
    FontSlantItalic = 2
  };
  
  enum {
    FontsPerGlyphCount = (1 << 5)
  };
  
  struct GlyphInfo {
    enum {
      MaxRelOverlap = ((1 << 4) - 1)
    };
    struct ExtendersAndOverlap {
      unsigned num_extenders: 12;
      unsigned rel_overlap: 4;
    };
    
    float right;
    float x_offset;
    union {
      uint16_t            index;
      ExtendersAndOverlap ext;
    };
    unsigned style:              4; // GlyphStyleXXX
    
    unsigned fontinfo:           5;
    
    unsigned slant:              2; // 0=default, otherwise FontSlantXXX
    
    unsigned composed:           1;
    unsigned horizontal_stretch: 1;
    unsigned is_normal_text:     1;
    unsigned missing_after:      1;
    unsigned vertical_centered:  1; // glyph ink center = math_axis above baseline; does not work when composed=1
  };
  
  const uint16_t IgnoreGlyph = 0x0000;
  const uint16_t UnknownGlyph = 0xFFFF;
  
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
      
      Rectangle to_rectangle() const {
        return to_rectangle(Point(0, 0));
      }
      
      Rectangle to_rectangle(const Point &origin) const {
        return Rectangle(origin.x, origin.y - ascent, width, height());
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
        Context        *context,
        int             len,
        const uint16_t *str,
        GlyphInfo      *result) = 0;
        
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
        
      virtual SharedPtr<TextShaper> set_style(FontStyle style) = 0;
      
      virtual FontStyle get_style() = 0;
      
      virtual float get_center_height(Context *context, uint8_t fontinfo);
      
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
        
      virtual SharedPtr<TextShaper> set_style(FontStyle style) override;
      
      virtual FontStyle get_style() override;
      
      virtual float get_center_height(Context *context, uint8_t fontinfo) override;
      
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
  
  typedef enum {
    ScriptUpperLeft,
    ScriptLowerLeft,
    ScriptUpperRight,
    ScriptLowerRight
  } ScriptPosition;
  
  class MathSequence;
  
  class MathShaper: public TextShaper {
    public:
      virtual bool horizontal_stretch_char(
        Context        *context,
        float           width,
        const uint16_t  ch,
        GlyphInfo      *result) = 0;
        
      virtual void vertical_stretch_char(
        Context        *context,
        float           ascent,
        float           descent,
        bool            full_stretch,
        const uint16_t  ch,
        GlyphInfo      *result) = 0;
        
      virtual void shape_fraction(
        Context        *context,
        const BoxSize  &num,
        const BoxSize  &den,
        float          *num_shift,
        float          *den_shift,
        float          *width) = 0;
        
      virtual void show_fraction(
        Context        *context,
        float           width) = 0;
        
      virtual float italic_correction(
        Context          *context,
        uint16_t          ch,
        const GlyphInfo  &info) = 0;
        
      virtual void accent_positions(
        Context           *context,
        MathSequence      *base,
        MathSequence      *under,
        MathSequence      *over,
        float             *base_x,
        float             *under_x,
        float             *under_y,
        float             *over_x,
        float             *over_y) = 0;
        
      virtual void script_positions(
        Context           *context,
        float              base_ascent,
        float              base_descent,
        MathSequence      *sub,
        MathSequence      *super,
        float             *sub_y,
        float             *super_y) = 0;
        
      virtual void script_corrections(
        Context           *context,
        uint16_t           base_char,
        const GlyphInfo   &base_info,
        MathSequence      *sub,
        MathSequence      *super,
        float              sub_y,
        float              super_y,
        float             *sub_x,
        float             *super_x) = 0;
        
      virtual void shape_radical(
        Context          *context,    // in
        BoxSize          *box,        // in/out
        float            *radicand_x, // out
        float            *exponent_x, // out
        float            *exponent_y, // out
        RadicalShapeInfo *info) = 0;  // out
        
      virtual void show_radical(
        Context                *context,
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
        uint16_t     _index,
        uint16_t     _hbar_index,
        float        _rel_ascent,
        float        _rel_exp_x,
        float        _rel_exp_y)
        : index(_index),
        hbar_index(_hbar_index),
        rel_ascent(_rel_ascent),
        rel_exp_x(_rel_exp_x),
        rel_exp_y(_rel_exp_y)
      {}
      
    public:
      uint16_t index;
      uint16_t hbar_index;
      float    rel_ascent;
      float    rel_exp_x;
      float    rel_exp_y; // from top
  };
  
  class SimpleMathShaper: public MathShaper {
    public:
      SimpleMathShaper(int _radicalfont);
      ~SimpleMathShaper();
      
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
        
      virtual bool horizontal_stretch_char(
        Context        *context,
        float           width,
        const uint16_t  ch,
        GlyphInfo      *result) override;
        
      virtual void vertical_stretch_char(
        Context        *context,
        float           ascent,
        float           descent,
        bool            full_stretch,
        const uint16_t  ch,
        GlyphInfo      *result) override;
        
      virtual void accent_positions(
        Context           *context,
        MathSequence      *base,
        MathSequence      *under,
        MathSequence      *over,
        float             *base_x,
        float             *under_x,
        float             *under_y,
        float             *over_x,
        float             *over_y) override;
        
      virtual void script_positions(
        Context           *context,
        float              base_ascent,
        float              base_descent,
        MathSequence      *sub,
        MathSequence      *super,
        float             *sub_y,
        float             *super_y) override;
        
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
        
      virtual void shape_fraction(
        Context        *context,
        const BoxSize  &num,
        const BoxSize  &den,
        float          *num_y,
        float          *den_y,
        float          *width) override;
        
      virtual void show_fraction(
        Context  *context,
        float     width) override;
        
      virtual float italic_correction(
        Context          *context,
        uint16_t          ch,
        const GlyphInfo  &info) override;
        
      virtual void shape_radical(
        Context          *context,    // in
        BoxSize          *box,        // in/out
        float            *radicand_x, // out
        float            *exponent_x, // out
        float            *exponent_y, // out
        RadicalShapeInfo *info) override;      // out
        
      virtual void show_radical(
        Context                *context,
        const RadicalShapeInfo &info) override;
        
      virtual void get_script_size_multis(Array<float> *arr) override;
      
    public:
      static Hashtable<String, SharedPtr<MathShaper> > available_shapers;
      
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
        float        *_rel_exp_x,
        float        *_rel_exp_y) = 0;
  };
}

#endif // RICHMATH__GRAPHICS__SHAPERS_H__INCLUDED
