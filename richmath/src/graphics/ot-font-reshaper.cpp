#include <graphics/ot-font-reshaper.h>
#include <util/base.h>

#include <cassert>
#include <cstdio>

#define OT_ASSERT(a) \
  do{if(!(a)){ \
      assert_failed(); \
      assert(a); \
    }}while(0)


using namespace richmath;

//{ class GlyphCoverage ...

int GlyphCoverage::find_glyph(uint16_t glyph) const {
  uint16_t format = BigEndian::read(*(const uint16_t *)&_format);
  
  switch(format) {
    case 1: {
        const Format1 *table = (const Format1 *)&_format;
        uint16_t count = BigEndian::read(table->glyph_count);
        for(int i = 0; i < (int)count; ++i)
          if(BigEndian::read(table->glyphs[i]) == glyph)
            return i;
            
        return -1;
      }
      
    case 2: {
        const Format2 *table = (const Format2 *)&_format;
        uint16_t count = BigEndian::read(table->range_count);
        for(int i = 0; i < (int)count; ++i) {
          uint16_t start = BigEndian::read(table->ranges[i].start_glyph);
          uint16_t end   = BigEndian::read(table->ranges[i].end_glyph);
          
          if(start <= glyph && glyph <= end) {
            uint16_t index = BigEndian::read(table->ranges[i].start_coverage_index);
            
            return (int)(index + glyph - start);
          }
        }
        
        return -1;
      }
  }
  
  fprintf(stderr, "[invalid GlyphCoverage format %d]\n", (int)format);
  
  return -1;
}

//} ... class GlyphCoverage

//{ class ClassDef ...

uint16_t ClassDef::find_glyph_class(uint16_t glyph) const {
  uint16_t format = BigEndian::read(*(const uint16_t *)&_format);
  
  switch(format) {
    case 1: {
        const Format1 *table = (const Format1 *)&_format;
        
        uint16_t start_glyph = BigEndian::read(table->start_glyph);
        uint16_t count       = BigEndian::read(table->glyph_count);
        if(start_glyph <= glyph && glyph < start_glyph + count) {
          return BigEndian::read(table->class_values[glyph - start_glyph]);
        }
        
        return 0;
      }
      
    case 2: {
        const Format2 *table = (const Format2 *)&_format;
        
        uint16_t count = BigEndian::read(table->range_count);
        for(int i = 0; i < (int)count; ++i) {
          uint16_t start = BigEndian::read(table->ranges[i].start_glyph);
          uint16_t end   = BigEndian::read(table->ranges[i].end_glyph);
          
          if(start <= glyph && glyph <= end) {
            return BigEndian::read(table->ranges[i].glyph_class);
          }
        }
        
        return 0;
      };
  };
  
  fprintf(stderr, "[invalid ClassDef format %d]\n", (int)format);
  
  return 0;
}

//} ... class ClassDef

//{ class ScriptList ...

const Script *ScriptList::search_script(uint32_t tag) const {
  int cnt = count();
  
  for(int i = 0; i < cnt; ++i) {
    uint32_t script_tag = BigEndian::read(_records[i].script_tag);
    
    if(script_tag == tag) {
      uint16_t offset = BigEndian::read(_records[i].script_offset);
      
      return (const Script *)((const uint8_t *)this + offset);
    }
  }
  
  return NULL;
}

int ScriptList::count() const {
  return BigEndian::read(_count);
}

uint32_t ScriptList::script_tag(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < count());
  
  return BigEndian::read(_records[i].script_tag);
}

const Script *ScriptList::script(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < count());
  
  uint16_t offset = BigEndian::read(_records[i].script_offset);
  
  return (const Script *)((const uint8_t *)this + offset);
}

//} ... class ScriptList

//{ class Script ...

const LangSys *Script::search_default_lang_sys() const {
  uint16_t offset = BigEndian::read(_default_lang_sys_offset);
  
  if(offset == 0)
    return NULL;
    
  return (const LangSys *)((const uint8_t *)this + offset);
}

const LangSys *Script::search_lang_sys(uint32_t tag) const {
  uint16_t count = BigEndian::read(_count);
  
  for(int i = 0; i < (int)count; ++i) {
    uint32_t lang_sys_tag = BigEndian::read(_records[i].lang_sys_tag);
    
    if(lang_sys_tag == tag) {
      uint16_t offset = BigEndian::read(_records[i].lang_sys_offset);
      
      return (const LangSys *)((const uint8_t *)this + offset);
    }
  }
  
  return NULL;
}

int Script::lang_sys_count() const {
  return (int)BigEndian::read(_count);
}

uint32_t Script::lang_sys_tag(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < (int)BigEndian::read(_count));
  
  return BigEndian::read(_records[i].lang_sys_tag);
}

const LangSys *Script::lang_sys(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < (int)BigEndian::read(_count));
  
  uint16_t offset = BigEndian::read(_records[i].lang_sys_offset);
  
  return (const LangSys *)((const uint8_t *)this + offset);
}

//} ... class Script

//{ class LangSys ...

int LangSys::requied_feature_index() const {
  uint16_t index = BigEndian::read(_req_feature_index);
  if(index == 0xFFFF)
    return -1;
    
  return (int)index;
}

int LangSys::feature_count() const {
  return (int)BigEndian::read(_feature_count);
}

int LangSys::feature_index(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < (int)BigEndian::read(_feature_count));
  
  return (int)BigEndian::read(_feature_indices[i]);
}

//} ... class LangSys

//{ class FeatureList ...

int FeatureList::count() const {
  return (int)BigEndian::read(_count);
}

uint32_t FeatureList::feature_tag(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < (int)BigEndian::read(_count));
  
  return BigEndian::read(_records[i].feature_tag);
}

const Feature *FeatureList::feature(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < (int)BigEndian::read(_count));
  
  uint16_t offset = BigEndian::read(_records[i].feature_offset);
  
  return (const Feature *)((const uint8_t *)this + offset);
}

//} ... class FeatureList

//{ class Feature ...

int Feature::lookup_count() const {
  return (int)BigEndian::read(_count);
}

int Feature::lookup_index(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < (int)BigEndian::read(_count));
  
  return (int)BigEndian::read(_lookup_indices[i]);
}

//} ... class Feature

//{ class LookupList ...

int LookupList::count() const {
  return (int)BigEndian::read(_count);
}

const Lookup *LookupList::lookup(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < (int)BigEndian::read(_count));
  
  uint16_t offset = BigEndian::read(_lookups_offsets[i]);
  
  return (const Lookup *)((const uint8_t *)this + offset);
}

//{ ... class LookupList

//{ class Lookup ...

int Lookup::type() const {
  return (int)BigEndian::read(_type);
}

int Lookup::flags() const {
  return (int)BigEndian::read(_flags);
}

int Lookup::sub_table_count() const {
  return (int)BigEndian::read(_sub_table_count);
}

const LookupSubTable *Lookup::sub_table(int i) const  {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < (int)BigEndian::read(_sub_table_count));
  
  uint16_t offset = BigEndian::read(_sub_table_offsets[i]);
  
  return (const LookupSubTable *)((const uint8_t *)this + offset);
}

//} ... class Lookup


//{ class GlyphSubstitutions ...

const ScriptList *GlyphSubstitutions::script_list() const {
  uint16_t offset = BigEndian::read(_script_list_offset);
  
  return (const ScriptList *)((const uint8_t *)this + offset);
}

const FeatureList *GlyphSubstitutions::feature_list() const {
  uint16_t offset = BigEndian::read(_feature_list_offset);
  
  return (const FeatureList *)((const uint8_t *)this + offset);
}

const LookupList *GlyphSubstitutions::lookup_list() const {
  uint16_t offset = BigEndian::read(_lookup_list_offset);
  
  return (const LookupList *)((const uint8_t *)this + offset);
}

//} ... class GlyphSubstitutions

//{ class SingleSubstitution ...

uint16_t SingleSubstitution::apply(uint16_t glyph) const {
  uint16_t format = BigEndian::read(_format);
  
  switch(format) {
    case 1: {
        const Format1 *table = (const Format1 *)this;
        
        uint16_t coverage_offset = BigEndian::read(table->coverage_offset);
        
        const GlyphCoverage *coverage = (const GlyphCoverage *)((const uint8_t *)table + coverage_offset);
        
        int i = coverage->find_glyph(glyph);
        if(i < 0)
          return glyph;
          
        int16_t delta_glyph_id = BigEndian::read(table->delta_glyph_id);
        
        return (uint16_t)((int)glyph + delta_glyph_id);
      }
      
    case 2: {
        const Format2 *table = (const Format2 *)this;
        
        uint16_t coverage_offset = BigEndian::read(table->coverage_offset);
        
        const GlyphCoverage *coverage = (const GlyphCoverage *)((const uint8_t *)table + coverage_offset);
        
        int i = coverage->find_glyph(glyph);
        if(i < 0)
          return glyph;
          
        OT_ASSERT(i < (int)BigEndian::read(table->count));
        
        return BigEndian::read(table->substitute_glyphs[i]);
      }
  }
  
  return glyph;
}

//} ... class SingleSubstitution

//{ class MultipleSubstitution ...

const GlyphSequence *MultipleSubstitution::search(uint16_t glyph) const {
  uint16_t format = BigEndian::read(_format);
  
  switch(format) {
    case 1: {
        const Format1 *table = (const Format1 *)this;
        
        uint16_t coverage_offset = BigEndian::read(table->coverage_offset);
        
        const GlyphCoverage *coverage = (const GlyphCoverage *)((const uint8_t *)table + coverage_offset);
        
        int i = coverage->find_glyph(glyph);
        if(i < 0)
          return NULL;
          
        OT_ASSERT(i < (int)BigEndian::read(table->count));
        
        uint16_t offset = BigEndian::read(table->sequence_offsets[i]);
        
        return (const GlyphSequence *)((const uint8_t *)table + offset);
      }
  }
  
  return NULL;
}

//} ... class MultipleSubstitution

//{ class AlternateSubstitute ...

const AlternateGlyphSet *AlternateSubstitute::search(uint16_t glyph) const {
  uint16_t format = BigEndian::read(_format);
  
  switch(format) {
    case 1: {
        const Format1 *table = (const Format1 *)this;
        
        uint16_t coverage_offset = BigEndian::read(table->coverage_offset);
        
        const GlyphCoverage *coverage = (const GlyphCoverage *)((const uint8_t *)table + coverage_offset);
        
        int i = coverage->find_glyph(glyph);
        if(i < 0)
          return NULL;
          
        OT_ASSERT(i < (int)BigEndian::read(table->count));
        
        uint16_t offset = BigEndian::read(table->alternate_set_offsets[i]);
        
        return (const AlternateGlyphSet *)((const uint8_t *)table + offset);
      }
  }
  
  return NULL;
}

/*int16_t AlternateSubstitute::apply(uint16_t glyph, int index) const {
  if(index <= 0)
    return glyph;

  const AlternateSet *alternatives = search(glyph);
  if(!alternatives)
    return glyph;

  uint16_t count = BigEndian::read(alternatives->count);
  if(index <= (int)count)
    return BigEndian::read(alternatives->glyphs[index - 1]);

  return glyph;
}*/

//} ... class AlternateSubstitute

//{ class LigatureSubstitute ...

const LigatureSet *LigatureSubstitute::search(uint16_t glyph) const {
  uint16_t format = BigEndian::read(_format);
  
  switch(format) {
    case 1: {
        const Format1 *table = (const Format1 *)this;
        
        uint16_t coverage_offset = BigEndian::read(table->coverage_offset);
        
        const GlyphCoverage *coverage = (const GlyphCoverage *)((const uint8_t *)table + coverage_offset);
        
        int i = coverage->find_glyph(glyph);
        if(i < 0)
          return NULL;
          
        OT_ASSERT(i < (int)BigEndian::read(table->count));
        
        uint16_t offset = BigEndian::read(table->ligature_set_offsets[i]);
        
        return (const LigatureSet *)((const uint8_t *)table + offset);
      }
  }
  
  return NULL;
}

//} ... class LigatureSubstitute


//{ class GlyphSequence ...

int GlyphSequence::count() const {
  return BigEndian::read(_count);
}

int GlyphSequence::glyph(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < count());
  
  return BigEndian::read(_glyphs[i]);
}

//} ... class GlyphSequence

//{ class AlternateGlyphSet ...

int AlternateGlyphSet::count() const {
  return BigEndian::read(_count);
}

int AlternateGlyphSet::glyph(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < count());
  
  return BigEndian::read(_glyphs[i]);
}

//} ... class AlternateGlyphSet

//{ class LigatureSet ...

int LigatureSet::count() const {
  return BigEndian::read(_count);
}

const Ligature *LigatureSet::ligature(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < count());
  
  uint16_t offset = BigEndian::read(_ligature_offsets[i]);
  
  return (const Ligature *)((const uint8_t *)this + offset);
}

//} ... class LigatureSet

//{ class Ligature ...

uint16_t Ligature::ligature_glyph() const {
  return BigEndian::read(_ligature_glyph);
}

int Ligature::components_count() const { // always >= 1
  return BigEndian::read(_component_count);
}

uint16_t Ligature::rest_component_glyph(int i) const { // must be >= 1 and < components_count
  assert(1 <= i);
  assert(i < components_count());
  
  return BigEndian::read(_rest_components[i - 1]);
}

//} ... class Ligature
