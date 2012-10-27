#ifndef __GRAPHICS__OT_FONT_RESHAPER_H__
#define __GRAPHICS__OT_FONT_RESHAPER_H__

#include <stdint.h>


namespace richmath {
  class BigEndian {
    public:
      static uint32_t read(uint32_t value) {
        const uint8_t *p = (const uint8_t *)&value;
        
        return ((uint32_t)p[0] << 24) + ((uint32_t)p[1] << 16) + ((uint32_t)p[2] << 8) + (uint32_t)p[4];
      }
      
      static uint16_t read(uint16_t value) {
        const uint8_t *p = (const uint8_t *)&value;
        
        return (p[0] << 8) + p[1];
      }
      
      static int16_t read(int16_t value) {
        const uint8_t *p = (const uint8_t *)&value;
        
        return (int16_t)(((uint16_t)p[0] << 8) + (uint16_t)p[1]);
      }
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
      uint16_t find_glyph_class(uint16_t glyph) const;
      
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
  class LookupList;
  class Lookup;
  class LookupSubTable;
  
  class ScriptList {
    public:
      const Script *search_script(uint32_t tag) const;
      
      int           count() const;
      uint32_t      script_tag(int i) const;
      const Script *script(int i) const;
      
    private:
      uint16_t _count;
      struct {
        uint32_t script_tag;
        uint16_t script_offset; // from beginning of ScriptList
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
      uint16_t _default_lang_sys_offset; // may be NULL
      uint16_t _count;
      struct {
        uint32_t lang_sys_tag;
        uint16_t lang_sys_offset;// from beginning of Script table
      } _records[1]; // [count]
  };
  
  class LangSys {
    public:
      int requied_feature_index() const; // -1 if there is none, otherwise uint16_t
      
      int feature_count() const;      // exclusing required
      int feature_index(int i) const; // exclusing required, result is uint16_t
      
    private:
      uint16_t _req_feature_index; // 0xFFFF if ther is no required for this language system
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
        uint32_t feature_tag;
        uint16_t feature_offset;
      } _records[1]; // [count]
  };
  
  class Feature {
    public:
      int lookup_count() const;
      int lookup_index(int i) const;
      
    private:
      uint16_t _params_offset; // = NULL (reserved for offset to FeatureParams)
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
      uint32_t _version; // 16.16 fixed-point number
      uint16_t _script_list_offset;
      uint16_t _feature_list_offset;
      uint16_t _lookup_list_offset;
  };
  
  class GlyphSequence;
  class AlternateGlyphSet;
  class LigatureSet;
  class Ligature;
  
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
  
  class AlternateSubstitute: public LookupSubTable {
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
  
  class LigatureSubstitute: public LookupSubTable {
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
  
}

#endif // __GRAPHICS__OT_FONT_RESHAPER_H__
