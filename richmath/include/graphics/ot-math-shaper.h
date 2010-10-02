#ifndef __GRAPHICS__OT_MATH_SHAPER_H__
#define __GRAPHICS__OT_MATH_SHAPER_H__

#include <graphics/shapers.h>

#include <util/array.h>
#include <util/pmath-extra.h>

namespace richmath{
  class OTMathShaper;
  
  typedef struct{
    uint32_t version;
    uint16_t constants_offset;
    uint16_t glyphinfo_offset;
    uint16_t variants_offset;
  } MathTableHeader;
  
  typedef struct {
    int16_t value;
    uint16_t device_table_offset;
  } MathValueRecord;

  typedef struct {
    uint16_t script_percent_scale_down;
    uint16_t script_script_percent_scale_down;
    uint16_t delimited_sub_formula_min_height;
    uint16_t display_operator_min_height;
    MathValueRecord math_leading;
    MathValueRecord axis_height;
    MathValueRecord accent_base_height;
    MathValueRecord flattened_accent_base_height;
    MathValueRecord subscript_shift_down;
    MathValueRecord subscript_top_max;
    MathValueRecord subscript_baseline_drop_min;
    MathValueRecord superscript_shift_up;
    MathValueRecord superscript_shift_up_cramped;
    MathValueRecord superscript_bottom_min;
    MathValueRecord superscript_baseline_drop_max;
    MathValueRecord sub_superscript_gap_min;
    MathValueRecord superscript_bottom_max_with_subscript;
    MathValueRecord space_after_script;
    MathValueRecord upper_limit_gap_min;
    MathValueRecord upper_limit_baseline_rise_min;
    MathValueRecord lower_limit_gap_min;
    MathValueRecord lower_limit_baseline_drop_min;
    MathValueRecord stack_top_shift_up;
    MathValueRecord stack_top_display_style_shift_up;
    MathValueRecord stack_bottom_shift_down;
    MathValueRecord stack_bottom_display_style_shift_down;
    MathValueRecord stack_gap_min;
    MathValueRecord stack_display_style_gap_min;
    MathValueRecord stretch_stack_top_shift_up;
    MathValueRecord stretch_stack_bottom_shift_down;
    MathValueRecord stretch_stack_gap_above_min;
    MathValueRecord stretch_stack_gap_below_min;
    MathValueRecord fraction_numerator_shift_up;
    MathValueRecord fraction_numerator_display_style_shift_up;
    MathValueRecord fraction_denominator_shift_down;
    MathValueRecord fraction_denominator_display_style_shift_down;
    MathValueRecord fraction_numerator_gap_min;
    MathValueRecord fraction_num_display_style_gap_min;
    MathValueRecord fraction_rule_thickness;
    MathValueRecord fraction_denominator_gap_min;
    MathValueRecord fraction_denom_display_style_gap_min;
    MathValueRecord skewed_fraction_horizontal_gap;
    MathValueRecord skewed_fraction_vertical_gap;
    MathValueRecord overbar_vertical_gap;
    MathValueRecord overbar_rule_thickness;
    MathValueRecord overbar_extra_ascender;
    MathValueRecord underbar_vertical_gap;
    MathValueRecord underbar_rule_thickness;
    MathValueRecord underbar_extra_descender;
    MathValueRecord radical_vertical_gap;
    MathValueRecord radical_display_style_vertical_gap;
    MathValueRecord radical_rule_thickness;
    MathValueRecord radical_extra_ascender;
    MathValueRecord radical_kern_before_degree;
    MathValueRecord radical_kern_after_degree;
    uint16_t radical_degree_bottom_raise_percent;
  } MathConstants;
  
  typedef struct {
    uint16_t min_connector_overlap;
    uint16_t vert_glyph_coverage_offset;
    uint16_t horz_glyph_coverage_offset;
    uint16_t vert_glyph_count;
    uint16_t horz_glyph_count;
    uint16_t glyph_construction_offsets[1]; // vert_glyph_count + horz_glyph_count
//    uint16_t vert_glyph_construction_offsets[1]; // ANY
//    uint16_t horz_glyph_construction_offsets[1]; // ANY
  } MathVariants;
      
  typedef struct {
    uint16_t glyph;
    uint16_t advance;
  } MathGlyphVariantRecord;

  typedef struct {
    uint16_t assembly_offset;
    uint16_t count;
    MathGlyphVariantRecord variants[1]; // count
  } MathGlyphConstruction;
  
  typedef struct {
    uint16_t glyph;
    uint16_t start_connector_length;
    uint16_t end_connector_length;
    uint16_t full_advance;
    uint16_t flags;
  } MathGlyphPartRecord;
  
  typedef enum {
    MGPRF_Extender = 0x0001
  } MathGlyphPartRecordFlag;
  
  typedef struct {
    MathValueRecord     italics_correction;
    uint16_t            count;
    MathGlyphPartRecord parts[1]; // count
  } MathGlyphAssembly;

  typedef struct {
    uint16_t	italics_correction_info_offset;
    uint16_t	top_accent_attachment_offset;
    uint16_t	extended_shape_coverage_offset;
    uint16_t	kern_info_offset;
  } MathGlyphInfo;

  typedef struct {
    uint16_t        coverage_offset;
    uint16_t        count;
    MathValueRecord	italics_corrections[1]; // count
  } MathItalicsCorrectionInfo;

  typedef struct {
    uint16_t        coverage_offset;
    uint16_t        count;
    MathValueRecord	top_accent_attachment[1]; // count
  } MathTopAccentAttachment;
  
  typedef struct {
    uint16_t        heigth_count;  // there is one more kern width that height
    
    MathValueRecord values[1];     // heigth_count  +  (heigth_count + 1)
//    MathValueRecord heights[1];    // heigth_count
//    MathValueRecord kern_width[1]; // heigth_count + 1
  } MathKernVertex;
  
  typedef enum {
    MKE_TOP_RIGHT,
    MKE_TOP_LEFTT,
    MKE_BOTTOM_RIGHT,
    MKE_BOTTOM_LEFT
  } MathKernEdge;
  
  typedef struct {
    uint16_t  coverage_offset;
    uint16_t  count;
    uint16_t  offsets[1][4]; // [count,4]
  } MathKernInfo;
  
  typedef struct {
    uint16_t start_glyph;
    uint16_t end_glyph;
    uint16_t start_index;
  } GlyphRangeRecord;
  
  
  
  class DeviceAdjustment {
    public:
      DeviceAdjustment(): start_size(0){}
      DeviceAdjustment(const uint16_t *data);
      
      int adjustment(int fontsize);
      
    public:
      Array<int8_t> values;
      int start_size;
  };
  
  class KernVertexObject {
    public:
      KernVertexObject(){}
      KernVertexObject(const MathKernVertex *v);
      
      int16_t height_to_kern(int16_t height, bool above);
      
    public:
      Array<int16_t> values; // 2*h+1 entries!!!
  };
  
  class OTMathShaperDB: public Shareable {
    friend class OTMathShaper;
    public:
      virtual ~OTMathShaperDB();
      void clear_cache();
      
      static void clear_all();
      
      static SharedPtr<OTMathShaper> find(String name, FontStyle style);
      SharedPtr<OTMathShaper> find(FontStyle style);
      
      bool set_private_char(uint32_t ch, uint32_t fallback);
      bool set_alt_char(uint32_t ch, uint32_t fallback);
      
    private:
      OTMathShaperDB();
      
    protected:
      MathConstants consts;
      uint16_t      units_per_em;
      uint16_t      min_connector_overlap;
      
      Hashtable<uint16_t, Array<MathGlyphVariantRecord>, cast_hash> vert_variants;
      Hashtable<uint16_t, Array<MathGlyphVariantRecord>, cast_hash> horz_variants;
      
      Hashtable<uint16_t, Array<MathGlyphPartRecord>, cast_hash> vert_assembly;
      Hashtable<uint16_t, Array<MathGlyphPartRecord>, cast_hash> horz_assembly;
      
      Hashtable<uint32_t, Array<MathGlyphPartRecord>, cast_hash> private_ligatures;
      
      Hashtable<uint32_t, uint16_t, cast_hash> private_characters;
      Hashtable<uint32_t, uint16_t, cast_hash> alt_glyphs;
      
      Hashtable<uint16_t, int16_t, cast_hash> italics_correction;
      Hashtable<uint16_t, int16_t, cast_hash> top_accents;
      
      Hashtable<uint16_t, KernVertexObject, cast_hash> math_kern[4]; // index: MathKernEdge
      
      Array<MathGlyphVariantRecord> *get_vert_variants(uint32_t ch, uint16_t glyph);
      Array<MathGlyphVariantRecord> *get_horz_variants(uint32_t ch, uint16_t glyph);
      Array<MathGlyphPartRecord> *get_vert_assembly(uint32_t ch, uint16_t glyph);
      Array<MathGlyphPartRecord> *get_horz_assembly(uint32_t ch, uint16_t glyph);
      
    private:
      static Hashtable<String, SharedPtr<OTMathShaperDB> > registered;
      
      SharedPtr<OTMathShaper> shapers[FontStyle::Permutations];
      String name;
      
      FontInfo *fi;
  };
  
  class OTMathShaper: public MathShaper{
    friend class OTMathShaperDB;
    public:
      virtual ~OTMathShaper();
      
      virtual uint8_t num_fonts(){ return text_shaper->num_fonts(); }
      virtual FontFace font(   uint8_t fontinfo){ return text_shaper->font(fontinfo); }
      virtual String font_name(uint8_t fontinfo){ return text_shaper->font_name(fontinfo); }
      
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
      
      virtual bool horizontal_stretch_char(
        Context        *context,
        float           width,
        const uint16_t  ch,
        GlyphInfo      *result);
        
      virtual void vertical_stretch_char(
        Context        *context,
        float           ascent,
        float           descent,
        bool            full_stretch,
        const uint16_t  ch,
        GlyphInfo      *result);
      
      virtual void accent_positions(
        Context           *context,
        MathSequence          *base,
        MathSequence          *under,
        MathSequence          *over,
        float             *base_x,
        float             *under_x,
        float             *under_y,
        float             *over_x,
        float             *over_y);
      
      virtual void script_positions(
        Context           *context,
        float              base_ascent,
        float              base_descent,
        MathSequence      *sub,
        MathSequence      *super,
        float             *sub_y,
        float             *super_y);
      
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
      
      virtual void shape_fraction(
        Context        *context,
        const BoxSize  &num,
        const BoxSize  &den,
        float          *num_y,
        float          *den_y,
        float          *width);
      
      virtual void show_fraction(
        Context        *context,
        float           width);
      
      virtual float italic_correction(
        Context          *context,
        uint16_t          ch,
        const GlyphInfo  &info);
      
      virtual void shape_radical(
        Context          *context,    // in
        BoxSize          *box,        // in/out
        float            *radicand_x, // out
        float            *exponent_x, // out
        float            *exponent_y, // out
        RadicalShapeInfo *info);      // out
      
      virtual void show_radical(
        Context                *context,
        const RadicalShapeInfo &info);
      
      virtual void get_script_size_multis(Array<float> *arr);
      
      virtual SharedPtr<TextShaper> set_style(FontStyle _style);
      
      virtual FontStyle get_style(){ return style; }
      
    protected:
      void stretch_glyph_assembly(
        Context                    *context,
        float                       width,
        Array<MathGlyphPartRecord> *parts,
        GlyphInfo                  *result);
      
      OTMathShaper(SharedPtr<OTMathShaperDB> _db, FontStyle _style);
      
    protected:
      SharedPtr<OTMathShaperDB>      db;
      SharedPtr<FallbackTextShaper>  text_shaper;
      FontStyle                      style;
      FontInfo                       fi;
  };
};

#endif /* __GRAPHICS__OT_MATH_SHAPER_H__ */
