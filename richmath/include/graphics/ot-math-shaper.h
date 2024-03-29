#ifndef RICHMATH__GRAPHICS__OT_MATH_SHAPER_H__INCLUDED
#define RICHMATH__GRAPHICS__OT_MATH_SHAPER_H__INCLUDED

#include <graphics/shapers.h>
#include <graphics/ot-font-reshaper.h>

#include <util/array.h>
#include <util/pmath-extra.h>


namespace richmath {
  class OTMathShaperImpl;
  
  class OTMathShaper final : public MathShaper {
      friend class OTMathShaperDB;
    public:
      virtual ~OTMathShaper();
      
      String name();
      static SharedPtr<OTMathShaper> try_register(String name);
      static void dispose_all();
      
      virtual uint8_t num_fonts() override;
      virtual FontFace font(uint8_t fontinfo) override;
      virtual String font_name(uint8_t fontinfo) override;
      
      virtual float flattened_accent_base_height(Context &context) override;
      
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
        
      virtual bool horizontal_stretch_char(
        Context        &context,
        float           width,
        SmallerOrLarger rounding,
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
        Context        &context,
        float           width) override;
        
      virtual float italic_correction(
        Context          &context,
        uint16_t          ch,
        const GlyphInfo  &info) override;
        
      virtual void shape_radical(
        Context          &context,    // in
        BoxSize          *box,        // in/out
        float            *radicand_x, // out
        Vector2F         *exponent_offset, // out
        RadicalShapeInfo *info) override; // out
        
      virtual void show_radical(
        Context                &context,
        const RadicalShapeInfo &info) override;
        
      virtual void get_script_size_multis(Array<float> *arr) override;
      
      virtual SharedPtr<TextShaper> set_style(FontStyle _style) override;
      
      virtual FontStyle get_style() override { return style; }
      
      virtual float get_center_height(Context &context, uint8_t fontinfo) override;
      
    private:
      OTMathShaper(SharedPtr<OTMathShaperImpl> _impl, FontStyle _style);
      
    protected:
      SharedPtr<OTMathShaperImpl>  impl;
      FontStyle                    style;
  };
};

#endif /* RICHMATH__GRAPHICS__OT_MATH_SHAPER_H__INCLUDED */
