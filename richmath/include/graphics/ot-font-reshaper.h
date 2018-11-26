#ifndef RICHMATH__GRAPHICS__OT_FONT_RESHAPER_H__INCLUDED
#define RICHMATH__GRAPHICS__OT_FONT_RESHAPER_H__INCLUDED

#include <graphics/shapers.h>

#include <stdint.h>


namespace richmath {
  typedef struct  {
    uint16_t hi;
    uint16_t lo;
  } UnalignedBEUint32;
  
  class BigEndian {
    public:
//      static uint32_t read(uint32_t value) {
//        const uint8_t *p = (const uint8_t *)&value;
//
//        return ((uint32_t)p[0] << 24) + ((uint32_t)p[1] << 16) + ((uint32_t)p[2] << 8) + (uint32_t)p[3];
//      }

      static uint32_t read(UnalignedBEUint32 value) {
        return ((uint32_t)read(value.hi) << 16) + (uint32_t)read(value.lo);
      }
      
      static uint16_t read(uint16_t value) {
        const uint8_t *p = (const uint8_t *)&value;
        
        return (uint16_t)((p[0] << 8) + p[1]);
      }
      
      static int16_t read(int16_t value) {
        const uint8_t *p = (const uint8_t *)&value;
        
        return (int16_t)(((uint16_t)p[0] << 8) + (uint16_t)p[1]);
      }
  };
  
  class GlyphSubstitutions;
  class LookupList;
  class Lookup;
  class LookupSubTable;
  class SingleSubstitution;
  class MultipleSubstitution;
  class AlternateSubstitution;
  class LigatureSubstitution;
  class ContextSubstitution;
  class ChainingContextSubstitution;
  class ExtensionSubstitution;
  class ReverseChainingContextSubstitution {};
  class SubstLookupRecord;
  
#define FONT_TAG_NAME(a,b,c,d) \
  ( ((uint32_t)(a) << 24) \
    | ((uint32_t)(b) << 16) \
    | ((uint32_t)(c) << 8) \
    |  (uint32_t)(d))
  
  class FontFeatureSet: public Base {
    public:
      FontFeatureSet();
      
      static uint32_t tag_from_name(const String &s);
      static uint32_t tag_from_name(const char *s);
      
      bool empty() const { return _tag_to_value.size() == 0; }
      
      bool has_feature( uint32_t tag) const { return _tag_to_value[tag] != 0; }
      int feature_value(uint32_t tag) const { return _tag_to_value[tag]; }
      
      void set_feature(uint32_t tag, int value) {
        if(!value)
          _tag_to_value.remove(tag);
        else
          _tag_to_value.set(tag, value);
      }
      
      void clear() { _tag_to_value.clear(); }
      void add(const FontFeatureSet &other);
      void add(Expr features);
      
    public:
      static const uint32_t TAG_ssty = FONT_TAG_NAME('s', 's', 't', 'y');
      
    private:
      Hashtable<uint32_t, int> _tag_to_value;
  };
  
  class OTFontReshaper {
    public:
      class IndexAndValue {
        public:
          IndexAndValue() : index(0), value(0) {}
          IndexAndValue(int i, int v) : index(i), value(v) {}
          
          int index;
          int value;
      };
      
    public:
      Array<uint16_t> glyphs;
      Array<int>      glyph_info;
      
    public:
      const LookupList *current_lookup_list; // tempoarily set by apply_lookups()
      int               next_position; // tempoarily set by apply_lookups() 
      
    public:
      OTFontReshaper();
      
      // also adds langsys-required lookups
      static void get_lookups(
        const GlyphSubstitutions *gsub_table,
        uint32_t                  script_tag,
        uint32_t                  language_tag,
        const FontFeatureSet     &features,
        Array<IndexAndValue>     *lookups);
        
      static uint16_t substitute_single_glyph(
        const GlyphSubstitutions   *gsub_table,
        uint16_t                    original_glyph,
        const Array<IndexAndValue> &lookups);
        
      void apply_lookups(
        const GlyphSubstitutions   *gsub_table,
        const Array<IndexAndValue> &lookups);
        
        
      void apply_lookup_at(
        const Lookup *lookup,
        int           value,
        int           position);
        
      void apply_single_substitution(
        const SingleSubstitution *singsub,
        int                       position);
        
      void apply_multiple_substitution(
        const MultipleSubstitution *multisub,
        int                         position);
        
      void apply_alternate_substitution(
        const AlternateSubstitution *altsub,
        int                          alternative,
        int                          position);
        
      void apply_ligature_substitution(
        const LigatureSubstitution *ligsub,
        int                         position);
        
      void apply_context_substitution(
        const ContextSubstitution *ctxsub,
        int                        value,
        int                        position);
        
      void apply_chaining_context_substitution(
        const ChainingContextSubstitution *ctxsub,
        int                                value,
        int                                position);
        
      void apply_extension_substitution(
        const ExtensionSubstitution *extsub,
        int                          value,
        int                          position);
        
      void apply_multiple_lookups_at(
        const SubstLookupRecord *lookups,
        int                      count,
        int                      value,
        int                      position);
        
//      void apply_reverse_chaining_context_substitution(
//        const ReverseChainingContextSubstitution *rccsub,
//        int                                       value,
//        int                                       position);

    public:
      static const uint32_t SCRIPT_DFLT = FONT_TAG_NAME('D', 'F', 'L', 'T');
      static const uint32_t SCRIPT_latn = FONT_TAG_NAME('l', 'a', 't', 'n');
      static const uint32_t SCRIPT_math = FONT_TAG_NAME('m', 'a', 't', 'h');
      
      static const uint32_t LANG_dflt = FONT_TAG_NAME('d', 'f', 'l', 't');
  };
  
  class GlyphCoverage {
    private:
      uint16_t _format;
      
    public:
      // returns coverage index or -1
      int find_glyph(uint16_t glyph) const;
      
    private:
      class Format1 {
        public:
          uint16_t coverage_format; // = 1
          uint16_t glyph_count;
          uint16_t glyphs[1]; // [glyph_count]
      };
      
      class RangeRecord {
        public:
          uint16_t start_glyph;
          uint16_t end_glyph;
          uint16_t start_coverage_index;
      };
      
      class Format2 {
        public:
          uint16_t          coverage_format; // = 2
          uint16_t          range_count;
          class RangeRecord ranges[1]; // []range_count
      };
  };
  
  class ClassDef {
    private:
      uint16_t _format;
      
    public:
      int get_glyph_class(uint16_t glyph) const; // always fits info uint16_t
      
    private:
      class Format1 {
        public:
          uint16_t format; //= 1
          uint16_t start_glyph;
          uint16_t glyph_count;
          uint16_t class_values[1]; // [glyph_count]
      };
      
      class ClassRangeRecord {
        public:
          uint16_t start_glyph;
          uint16_t end_glyph;
          uint16_t glyph_class;
      };
      
      class Format2 {
        public:
          uint16_t         format; // = 2
          uint16_t         range_count;
          ClassRangeRecord ranges[1]; // [range_count]
      };
  };
  
  class ScriptList;
  class Script;
  class LangSys;
  class FeatureList;
  class Feature;
  
  class ScriptList {
    public:
      const Script *search_script(uint32_t tag) const;
      
      int           count() const;
      uint32_t      script_tag(int i) const;
      const Script *script(int i) const;
      
    private:
      uint16_t _count;
      struct {
        UnalignedBEUint32 script_tag;
        uint16_t          script_offset; // from beginning of ScriptList
      } _records[1]; // [count]
  };
  
  class Script {
    public:
      const LangSys *search_default_lang_sys() const;
      const LangSys *search_lang_sys(uint32_t tag) const;
      
      int            lang_sys_count() const;    // exclusing default
      uint32_t       lang_sys_tag(int i) const; // exclusing default
      const LangSys *lang_sys(int i) const;     // exclusing default
      
    private:
      uint16_t _default_lang_sys_offset; // may be nullptr
      uint16_t _count;
      struct {
        UnalignedBEUint32 lang_sys_tag;
        uint16_t          lang_sys_offset;// from beginning of Script table
      } _records[1]; // [count]
  };
  
  class LangSys {
    public:
      int requied_feature_index() const; // -1 if there is none, otherwise uint16_t
      
      int feature_count() const;      // exclusing required
      int feature_index(int i) const; // exclusing required, result is uint16_t
      
    private:
      uint16_t _lookup_order_offset; // = nullptr
      uint16_t _req_feature_index; // 0xFFFF if there is no required feature for this language system
      uint16_t _feature_count;
      uint16_t _feature_indices[1]; // [feature_count]
  };
  
  class FeatureList {
    public:
      int            count() const;
      uint32_t       feature_tag(int i) const;
      const Feature *feature(int i) const;
      
    public:
      uint16_t _count;
      struct {
        UnalignedBEUint32 feature_tag;
        uint16_t          feature_offset;
      } _records[1]; // [count]
  };
  
  class Feature {
    public:
      int lookup_count() const;
      int lookup_index(int i) const;
      
    private:
      uint16_t _params_offset; // = nullptr (reserved for offset to FeatureParams)
      uint16_t _count;
      uint16_t _lookup_indices[1]; // [count]
  };
  
  class LookupList {
    public:
      int           count() const;
      const Lookup *lookup(int i) const;
      
    private:
      uint16_t _count;
      uint16_t _lookups_offsets[1]; // [count]
  };
  
  class Lookup {
    public:
      int                   type() const;
      int                   flags() const; // LookupFlagXXX bitset
      int                   sub_table_count() const;
      const LookupSubTable *sub_table(int i) const;
      
    public:
      enum {
        LookupFlagIgnoreBaseGlyphs    = 0x0002, //  If set, skips over base glyphs
        LookupFlagIgnoreLigatures     = 0x0004, //  If set, skips over ligatures
        LookupFlagIgnoreMarks         = 0x0008, //  If set, skips over all combining marks
        LookupFlagUseMarkFilteringSet = 0x0010, //  If set, indicates that the lookup table structure is followed by a MarkFilteringSet field. The layout engine skips over all mark glyphs not in the mark filtering set indicated.
        LookupFlagReserved            = 0x00E0, //  For future use (Set to zero)
        LookupFlagMarkAttachmentType  = 0xFF00  //  If not zero, skips over all marks of attachment type different from specified.
      };
      
    private:
      uint16_t _type;
      uint16_t _flags;
      uint16_t _sub_table_count;
      uint16_t _sub_table_offsets[1]; // [sub_table_count]
      //uint16_t  _mark_filtering_set; Index (base 0) into GDEF mark glyph sets structure. This field is only present if bit UseMarkFilteringSet of lookup flags is set.
  };
  
  class LookupSubTable {
      /* empty, see derived classes */
  };
  
  class GlyphSubstitutions {
    public:
      const ScriptList  *script_list() const;
      const FeatureList *feature_list() const;
      const LookupList  *lookup_list() const;
      
    public:
      enum {
        GSUBLookupTypeSingle                       = 1,
        GSUBLookupTypeMultiple                     = 2,
        GSUBLookupTypeAlternate                    = 3,
        GSUBLookupTypeLigature                     = 4,
        GSUBLookupTypeContext                      = 5,
        GSUBLookupTypeChainingContext              = 6,
        GSUBLookupTypeExtensionSubstitution        = 7,
        GSUBLookupTypeReverseChainingContextSingle = 8
      };
      
    private:
      UnalignedBEUint32 _version; // 16.16 fixed-point number
      uint16_t          _script_list_offset;
      uint16_t          _feature_list_offset;
      uint16_t          _lookup_list_offset;
  };
  
  class GlyphSequence;
  class AlternateGlyphSet;
  class LigatureSet;
  class Ligature;
  class SubRuleSet;
  class SubRule;
  class SubClassSet;
  class SubClassRule;
  class ChainSubRuleSet;
  class ChainSubRule;
  class ChainSubClassRuleSet;
  class ChainSubClassRule;
  
  class SubstLookupRecord {
    public:
      int sequence_index() const;
      int lookup_list_index() const;
      
    private:
      uint16_t _sequence_index;
      uint16_t _lookup_list_index;
  };
  
  class SingleSubstitution: public LookupSubTable {
    public:
      uint16_t apply(uint16_t glyph) const;
      
    private:
      uint16_t _format;
      
    private:
      class Format1 {
        public:
          uint16_t format; // = 1
          uint16_t coverage_offset;
          int16_t  delta_glyph_id;
      };
      
      class Format2 {
        public:
          uint16_t format; // = 2
          uint16_t coverage_offset;
          uint16_t count;
          uint16_t substitute_glyphs[1]; // [count]
      };
  };
  
  class MultipleSubstitution: public LookupSubTable {
    public:
      const GlyphSequence *search(uint16_t glyph) const;
      
    private:
      uint16_t _format;
      
    private:
      class Format1 {
        public:
          uint16_t format; // = 1
          uint16_t coverage_offset;
          uint16_t count;
          uint16_t sequence_offsets[1]; // [count]
      };
  };
  
  class AlternateSubstitution: public LookupSubTable {
    public:
      const AlternateGlyphSet *search(uint16_t glyph) const;
      
      //uint16_t apply(uint16_t glyph, int index) const; // index is 1-based
      
    private:
      uint16_t _format;
      
    private:
      class Format1 {
        public:
          uint16_t format; // = 1
          uint16_t coverage_offset;
          uint16_t count;
          uint16_t alternate_set_offsets[1]; // [count]
      };
      
  };
  
  class LigatureSubstitution: public LookupSubTable {
    public:
      const LigatureSet *search(uint16_t glyph) const;
      
    private:
      uint16_t _format;
      
    private:
      class Format1 {
        public:
          uint16_t format;
          uint16_t coverage_offset;
          uint16_t count;
          uint16_t ligature_set_offsets[1]; // [count]
      };
      
  };
  
  class ContextSubstitution: public LookupSubTable {
    public:
      const uint16_t format() const { return BigEndian::read(_format); }
      
      // Format 1:
      const SubRuleSet *format1_search_rule_set(uint16_t first_glyph) const;
      
      // Format 2:
      const ClassDef    *format2_get_classdef() const;
      const SubClassSet *format2_search_class_set(uint16_t first_glyph) const;
      
      // Format 3:
      int                      format3_glyph_count() const;
      const GlyphCoverage     *format3_glyph_coverage(int i) const;
      
      int                      format3_subst_count() const;
      const SubstLookupRecord *format3_subst_records() const;
      
    private:
      uint16_t _format;
      
    private:
      class Format1 {
        public:
          uint16_t format;
          uint16_t coverage_offset;
          uint16_t count;
          uint16_t sub_rule_set_offsets[1]; // [count]
      };
      
      class Format2 {
        public:
          uint16_t format;
          uint16_t coverage_offset;
          uint16_t classdef_offset;
          uint16_t count;
          uint16_t sub_class_set_offsets[1]; // [count]
      };
      
      class Format3 {
        public:
          uint16_t format;
          uint16_t glyph_count;
          uint16_t subst_count;
          uint16_t coverage_offsets[1]; // [glyph_count]
          //SubstLookupRecord subst_lookup_records[1]; // [subst_count]
      };
  };
  
  class ChainingContextSubstitution: public LookupSubTable {
    public:
      const uint16_t format() const { return BigEndian::read(_format); }
      
      // Format 1:
      const ChainSubRuleSet *format1_search_chain_rule_set(uint16_t first_glyph) const;
      
      // Format 2:
      const ClassDef             *format2_get_backtrack_classdef() const;
      const ClassDef             *format2_get_input_classdef() const;
      const ClassDef             *format2_get_lookahead_classdef() const;
      const ChainSubClassRuleSet *format2_search_class_set(uint16_t first_glyph) const;
      
      // Format 3:
      void                 format3_get_glyph_counts(int *backtrack, int *input, int *lookahead) const;
      const GlyphCoverage *format3_backtrack_glyph_coverage(int i) const;
      const GlyphCoverage *format3_input_glyph_coverage(    int i) const;
      const GlyphCoverage *format3_lookahead_glyph_coverage(int i) const;
      
      const SubstLookupRecord *format3_subst_records(int *count) const;
      
    private:
      uint16_t _format;
      
    private:
      class Format1 {
        public:
          uint16_t format;
          uint16_t coverage_offset;
          uint16_t count;
          uint16_t chain_sub_rule_set_offsets[1]; // [count]
      };
      
      class Format2 {
        public:
          uint16_t format;
          uint16_t coverage_offset;
          uint16_t backtrack_classdef_offset;
          uint16_t input_classdef_offset;
          uint16_t lookahead_classdef_offset;
          uint16_t count;
          uint16_t chain_sub_class_set_offsets[1]; // [count]
      };
      
      class Format3 {
        public:
          uint16_t format;
          uint16_t backtrack_glyph_count;
          uint16_t backtrack_coverage_offsets[1]; // [backtrack_glyph_count]
          
        public:
          class Part2 {
            public:
              uint16_t input_glyph_count;
              uint16_t input_coverage_offsets[1]; // [input_glyph_count]
          };
          
          class Part3 {
            public:
              uint16_t lookahead_glyph_count;
              uint16_t lookahead_coverage_offsets[1]; // [lookahead_glyph_count]
          };
          
          class Part4 {
            public:
              uint16_t          subst_count;
              SubstLookupRecord subst_lookup_records[1]; // [subst_count]
          };
      };
  };
  
  class ExtensionSubstitution: public LookupSubTable {
    public:
      int                   extension_type() const;
      const LookupSubTable *extension_sub_table() const;
      
    private:
      uint16_t _format;
      
    private:
      class Format1 {
        public:
          uint16_t          format;
          uint16_t          extension_lookup_type;
          UnalignedBEUint32 extension_offset;
      };
      
  };
  
  class GlyphSequence {
    public:
      int count() const;
      int glyph(int i) const;
      
    private:
      uint16_t _count;
      uint16_t _glyphs[1]; // [count]
  };
  
  class AlternateGlyphSet {
    public:
      int count() const;
      int glyph(int i) const;
      
    private:
      uint16_t _count;
      uint16_t _glyphs[1]; // [count]
  };
  
  class LigatureSet {
    public:
      int             count() const;
      const Ligature *ligature(int i) const;
      
    private:
      uint16_t _count;
      uint16_t _ligature_offsets[1]; // [count]
  };
  
  class Ligature {
    public:
      uint16_t ligature_glyph() const;
      int      components_count() const; // always >= 1
      uint16_t rest_component_glyph(int i) const; // must be >= 1 and < components_count
      
    private:
      uint16_t _ligature_glyph;
      uint16_t _component_count;
      uint16_t _rest_components[1]; // [_count - 1]
  };
  
  class SubRuleSet {
    public:
      int            count() const;
      const SubRule *rule(int i) const;
      
    private:
      uint16_t _count;
      uint16_t _rule_offsets[1]; // [_count]
  };
  
  class SubRule {
    public:
      int glyph_count() const;
      int subst_count() const;
      
      uint16_t rest_glyph(int i) const; // must be >= 1 and < glyph_count
      
      const SubstLookupRecord *subst_records() const;
      
    private:
      uint16_t _glyph_count;
      uint16_t _subst_count;
      uint16_t _rest_glyphs[1]; // [_glyph_count - 1]
      //SubstLookupRecord _subst_lookup_records[_subst_count]
  };
  
  class SubClassSet {
    public:
      int                 count() const;
      const SubClassRule *class_rule(int i) const;
      
    private:
      uint16_t _count;
      uint16_t _class_rule_offsets[1]; // [_count]
  };
  
  class SubClassRule {
    public:
      int glyph_count() const;
      int subst_count() const;
      
      int rest_glyph_class(int i) const; // i must be >= 1 and < glyph_count
      
      const SubstLookupRecord *subst_records() const;
      
    private:
      uint16_t _glyph_count;
      uint16_t _subst_count;
      uint16_t _rest_glyph_classes[1]; // [_glyph_count - 1]
      //SubstLookupRecord _subst_lookup_records[_subst_count]
  };
  
  class ChainSubRule;
  
  class ChainSubRuleSet {
    public:
      int                 count() const;
      const ChainSubRule *rule(int i) const;
      
    private:
      uint16_t _count;
      uint16_t _rule_offsets[1]; // [_count]
  };
  
  class ChainSubRule {
    public:
      int backtrack_glyph_count() const;
      int input_glyph_count() const;
      int lookahead_glyph_count() const;
      int subst_count() const;
      
      uint16_t backtrack_glyph(int i) const;
      uint16_t rest_input_glyph(int i) const; // i must be >= 1 and < input_glyph_count
      uint16_t lookahead_glyph(int i) const;
      
      const SubstLookupRecord *subst_records() const;
      
    private:
      uint16_t _backtrack_glyph_count;
      uint16_t _backtrack_glyphs[1]; // [_backtrack_glyph_count]
      
    private:
      class ChainSubRulePart2 {
        public:
          uint16_t _input_glyph_count;
          uint16_t _rest_input_glyphs[1]; // [_input_glyph_count - 1]
      };
      
      class ChainSubRulePart3 {
        public:
          uint16_t _lookahead_glyph_count;
          uint16_t _lookahead_glyphs[1]; // [_lookahead_glyph_count]
      };
      
      class ChainSubRulePart4 {
        public:
          uint16_t _subst_count;
          SubstLookupRecord _subst_lookup_records[1]; // [_subst_count]
      };
  };
  
  class ChainSubClassRule;
  
  class ChainSubClassRuleSet {
    public:
      int                      count() const;
      const ChainSubClassRule *rule(int i) const;
      
    private:
      uint16_t _count;
      uint16_t _rule_offsets[1]; // [_count]
  };
  
  class ChainSubClassRule {
    public:
      int backtrack_glyph_count() const;
      int input_glyph_count() const;
      int lookahead_glyph_count() const;
      int subst_count() const;
      
      int backtrack_glyph_class(int i) const;
      int rest_input_glyph_class(int i) const; // i must be >= 1 and < input_glyph_count
      int lookahead_glyph_class(int i) const;
      
      const SubstLookupRecord *subst_records() const;
      
    private:
      uint16_t _backtrack_glyph_count;
      uint16_t _backtrack_glyph_classes[1]; // [_backtrack_glyph_count]
      
    private:
      class ChainSubClassRulePart2 {
        public:
          uint16_t _input_glyph_count;
          uint16_t _rest_input_glyph_classes[1]; // [_input_glyph_count - 1]
      };
      
      class ChainSubClassRulePart3 {
        public:
          uint16_t _lookahead_glyph_count;
          uint16_t _lookahead_glyph_classes[1]; // [_lookahead_glyph_count]
      };
      
      class ChainSubClassRulePart4 {
        public:
          uint16_t _subst_count;
          SubstLookupRecord _subst_lookup_records[1]; // [_subst_count]
      };
  };
}

#endif // RICHMATH__GRAPHICS__OT_FONT_RESHAPER_H__INCLUDED
