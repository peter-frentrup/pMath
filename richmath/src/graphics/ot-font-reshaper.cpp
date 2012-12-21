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

//{ class FontFeatureSet ...

FontFeatureSet::FontFeatureSet()
  : Base()
{
  _tag_to_value.default_value = 0;
}

uint32_t FontFeatureSet::tag_from_name(const String &s) {
  const uint16_t *buf = s.buffer();
  int len = s.length();
  
  if(len < 1 || len > 4)
    return 0;
    
  uint32_t tag = 0;
  
  for(int i = 0; i < len; ++i) {
    if(buf[i] > 0xFF)
      return 0;
      
    tag = (tag << 8) | (uint32_t)buf[i];
  }
  
  while(len++ < 4)
    tag = tag << 8;
    
  return tag;
}

uint32_t FontFeatureSet::tag_from_name(const char *s) {
  uint32_t tag = 0;
  
  int i;
  for(i = 0; i < 4; ++i) {
    if(s[i] == '\0')
      break;
      
    tag = (tag << 8) | (uint32_t)s[i];
  }
  
  if(i == 4 && s[i] != '\0')
    return 0;
    
  while(i++ < 4)
    tag = tag << 8;
    
  return tag;
}

void FontFeatureSet::add(const FontFeatureSet &other) {
  _tag_to_value.merge(other._tag_to_value);
}

void FontFeatureSet::add(Expr features) {
  // { "liga", "ssty"->1, ... }
  
  if(features[0] != PMATH_SYMBOL_LIST)
    return;
    
  for(size_t i = 1; i <= features.expr_length(); ++i) {
    Expr expr = features[i];
    
    if(expr.is_string()) {
      uint32_t tag = tag_from_name(String(expr));
      
      if(tag)
        set_feature(tag, 1);
        
      continue;
    }
    
    if(expr.is_rule()) {
      Expr name  = expr[1];
      
      if(name.is_string()) {
        uint32_t tag = tag_from_name(String(name));
        
        if(tag) {
          Expr value = expr[2];
          
          if(value.is_int32()) {
            set_feature(tag, PMATH_AS_INT32(value.get()));
            continue;
          }
          
          if(value == PMATH_SYMBOL_TRUE) {
            set_feature(tag, 1);
            continue;
          }
          
          if(value == PMATH_SYMBOL_FALSE) {
            set_feature(tag, 0);
            continue;
          }
          
          if(value == PMATH_SYMBOL_AUTOMATIC) {
            set_feature(tag, -1);
            continue;
          }
          
          if(value == PMATH_SYMBOL_INHERITED)
            continue;
        }
      }
    }
  }
}

//} ... class FontFeatureSet

//{ class OTFontReshaper ...

OTFontReshaper::OTFontReshaper()
  : current_lookup_list(0)
{
}

void OTFontReshaper::get_lookups(
  const GlyphSubstitutions *gsub_table,
  uint32_t                  script_tag,
  uint32_t                  language_tag,
  const FontFeatureSet     &features,
  Array<IndexAndValue>     *lookups
) {
  if(features.empty())
    return;
    
  const ScriptList  *script_list  = gsub_table->script_list();
  const FeatureList *feature_list = gsub_table->feature_list();
  
  const Script *script = script_list->search_script(script_tag);
  if(!script) {
    script = script_list->search_script(FONT_TAG_NAME('D', 'F', 'L', 'T'));
    
    if(!script)
      return;
  }
  
  const LangSys *lang_sys = script->search_lang_sys(language_tag);
  if(!lang_sys)
    lang_sys = script->search_default_lang_sys();
    
  if(!lang_sys)
    return;
    
  int feature_index = lang_sys->requied_feature_index();
  if(0 <= feature_index && feature_index < feature_list->count()) {
    const Feature *feature = feature_list->feature(feature_index);
    
    int len = lookups->length();
    lookups->length(len + feature->lookup_count());
    
    for(int li = 0; li < feature->lookup_count(); ++li) {
      int lookup_index = feature->lookup_index(li);
      
      lookups->set(len + li, IndexAndValue(lookup_index, 1));
    }
  }
  
  for(int fi = 0; fi < lang_sys->feature_count(); ++fi) {
    feature_index = lang_sys->feature_index(fi);
    
    if(0 <= feature_index && feature_index < feature_list->count()) {
      uint32_t tag = feature_list->feature_tag(feature_index);
      
      int feature_value = features.feature_value(tag);
      
      if(feature_value) {
        const Feature *feature = feature_list->feature(feature_index);
        
        int len = lookups->length();
        lookups->length(len + feature->lookup_count());
        
        for(int li = 0; li < feature->lookup_count(); ++li) {
          int lookup_index = feature->lookup_index(li);
          
          lookups->set(len + li, IndexAndValue(lookup_index, feature_value));
        }
      }
    }
  }
}


uint16_t OTFontReshaper::substitute_single_glyph(
  const GlyphSubstitutions   *gsub_table,
  uint16_t                    original_glyph,
  const Array<IndexAndValue> &lookups
) {
  if(lookups.length() == 0)
    return original_glyph;
    
  if(!gsub_table)
    return original_glyph;
    
  static OTFontReshaper reshaper;
  reshaper.glyphs.length(1);
  reshaper.glyph_info.length(1);
  
  reshaper.glyphs[0]     = original_glyph;
  reshaper.glyph_info[0] = 0;
  
  reshaper.apply_lookups(gsub_table, lookups);
  
  if(reshaper.glyphs.length() == 1)
    return reshaper.glyphs[0];
    
  return original_glyph;
}

void OTFontReshaper::apply_lookups(
  const GlyphSubstitutions   *gsub_table,
  const Array<IndexAndValue> &lookups
) {
  OT_ASSERT(glyph_info.length() == glyphs.length());
  
  if(lookups.length() == 0)
    return;
    
  const LookupList *old_lookup_list = current_lookup_list;
  current_lookup_list = gsub_table->lookup_list();
  
  for(int pos = 0; pos < glyphs.length(); ++pos) {
    for(int i = 0; i < lookups.length(); ++i) {
      const IndexAndValue iav = lookups[i];
      
      if(iav.value > 0 && 0 <= iav.index && iav.index < current_lookup_list->count()) {
        apply_lookup(
          current_lookup_list->lookup(iav.index),
          iav.value,
          pos);
      }
    }
  }
  
  current_lookup_list = old_lookup_list;
}

void OTFontReshaper::apply_lookup(
  const Lookup               *lookup,
  int                         value,
  int                         position
) {
  switch(lookup->type()) {
    case GlyphSubstitutions::GSUBLookupTypeSingle:
      for(int sub = 0; sub < lookup->sub_table_count(); ++sub) {
        apply_single_substitution(
          (const SingleSubstitution *)lookup->sub_table(sub),
          position);
      }
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeMultiple:
      for(int sub = 0; sub < lookup->sub_table_count(); ++sub) {
        apply_multiple_substitution(
          (const MultipleSubstitution *)lookup->sub_table(sub),
          position);
      }
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeAlternate:
      for(int sub = 0; sub < lookup->sub_table_count(); ++sub) {
        apply_alternate_substitution(
          (const AlternateSubstitution *)lookup->sub_table(sub),
          value,
          position);
      }
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeLigature:
      for(int sub = 0; sub < lookup->sub_table_count(); ++sub) {
        apply_ligature_substitution(
          (const LigatureSubstitution *)lookup->sub_table(sub),
          position);
      }
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeContext:
      for(int sub = 0; sub < lookup->sub_table_count(); ++sub) {
        apply_context_substitution(
          (const ContextSubstitution *)lookup->sub_table(sub),
          value,
          position);
      }
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeChainingContext:
      for(int sub = 0; sub < lookup->sub_table_count(); ++sub) {
        apply_chaining_context_substitution(
          (const ChainingContextSubstitution *)lookup->sub_table(sub),
          value,
          position);
      }
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeExtensionSubstitution:
      for(int sub = 0; sub < lookup->sub_table_count(); ++sub) {
        apply_extension_substitution(
          (const ExtensionSubstitution *)lookup->sub_table(sub),
          value,
          position);
      }
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeReverseChainingContextSingle:
//      for(int sub = 0; sub < lookup->sub_table_count(); ++sub) {
//        apply_reverse_chaining_context_substitution(
//          (const ReverseChainingContextSubstitution *)lookup->sub_table(sub),
//          value,
//          position);
//      }
      break;
      
    default:
      pmath_debug_print("[apply_lookup() unknown type %d]\n", lookup->type());
      break;
  }
}

void OTFontReshaper::apply_single_substitution(
  const SingleSubstitution *singsub,
  int                       position
) {
  glyphs[position] = singsub->apply(glyphs[position]);
}

void OTFontReshaper::apply_multiple_substitution(
  const MultipleSubstitution *multisub,
  int                         position
) {
  const GlyphSequence *seq = multisub->search(glyphs[position]);
  if(!seq)
    return;
    
  glyphs.insert(    position + 1, seq->count() - 1);
  glyph_info.insert(position + 1, seq->count() - 1);
  
  glyphs[position] = seq->glyph(0);
  
  for(int i = 1; i < seq->count() ; ++i) {
    glyphs[position]         = seq->glyph(i);
    glyph_info[position + i] = glyph_info[position];
  }
}

void OTFontReshaper::apply_alternate_substitution(
  const AlternateSubstitution *altsub,
  int                          alternative,
  int                          position
) {
  if(alternative <= 0)
    return;
    
  const AlternateGlyphSet *altset = altsub->search(glyphs[position]);
  if(!altset)
    return;
    
  if(alternative <= altset->count())
    glyphs[position] = altset->glyph(alternative - 1);
  else if(altset->count() > 0)
    glyphs[position] = altset->glyph(altset->count() - 1);
}

void OTFontReshaper::apply_ligature_substitution(
  const LigatureSubstitution *ligsub,
  int                         position
) {
  const LigatureSet *ligature_set = ligsub->search(glyphs[position]);
  if(!ligature_set)
    return;
    
  for(int i = 0; i < ligature_set->count(); ++i) {
    const Ligature *lig = ligature_set->ligature(i);
    int liglen = lig->components_count();
    
    if(glyphs.length() - position >= liglen) {
      bool found = true;
      
      for(int i = 1; i < liglen; ++i) {
        if(glyphs[position + i] != lig->rest_component_glyph(i)) {
          found = false;
          break;
        }
      }
      
      if(found) {
        glyphs[position] = lig->ligature_glyph();
        glyphs.remove(    position + 1, liglen - 1);
        glyph_info.remove(position + 1, liglen - 1);
        return;
      }
    }
  }
}

void OTFontReshaper::apply_context_substitution(
  const ContextSubstitution *ctxsub,
  int                        value,
  int                        position
) {
  if(!current_lookup_list)
    return;
    
  switch(ctxsub->format()) {
    case 1:
      if(const SubRuleSet *rule_set = ctxsub->format1_search_rule_set(glyphs[position])) {
        for(int r = 0; r < rule_set->count(); ++r) {
          const SubRule *rule = rule_set->rule(r);
          
          int rule_len = rule->glyph_count();
          if(position + rule_len > glyphs.length())
            continue;
            
          bool found = true;
          for(int g = 1; g < rule_len; ++g) {
            if(rule->rest_glyph(g) != glyphs[position + g]) {
              found = false;
              break;
            }
          }
          
          if(found) {
            apply_lookups_at(
              rule->subst_records(),
              rule->subst_count(),
              value,
              position);
              
            return;
          }
        }
      }
      return;
      
    case 2:
      if(const SubClassSet *class_set = ctxsub->format2_search_class_set(glyphs[position])) {
        const ClassDef *classdef = ctxsub->format2_get_classdef();
        
        OT_ASSERT(classdef != NULL);
        
        static Array<int> rest_glyph_classes;
        rest_glyph_classes.length(0);
        for(int r = 0; r < class_set->count(); ++r) {
          const SubClassRule *rule = class_set->class_rule(r);
          
          int rule_len = rule->glyph_count();
          
          if(position + rule_len > glyphs.length())
            continue;
            
          for(int g = 1 + rest_glyph_classes.length(); g < rule_len; ++g)
            rest_glyph_classes.add(classdef->get_glyph_class(glyphs[position + g]));
        }
        
        for(int r = 0; r < class_set->count(); ++r) {
          const SubClassRule *rule = class_set->class_rule(r);
          
          int rule_len = rule->glyph_count();
          if(position + rule_len > glyphs.length())
            continue;
            
          bool found = true;
          for(int g = 1; g < rule_len; ++g) {
            if(rule->rest_glyph_class(g) != rest_glyph_classes[g - 1]) {
              found = false;
              break;
            }
          }
          
          if(found) {
            apply_lookups_at(
              rule->subst_records(),
              rule->subst_count(),
              value,
              position);
              
            return;
          }
        }
      }
      return;
      
    case 3:
      {
        int length = ctxsub->format3_glyph_count();
        
        if(position + length > glyphs.length())
          return;
          
        for(int pos = 0; pos < length; ++pos) {
          const GlyphCoverage *coverage = ctxsub->format3_glyph_coverage(pos);
          if(coverage->find_glyph(glyphs[position + pos]) < 0)
            return;
        }
        
        apply_lookups_at(
          ctxsub->format3_subst_records(),
          ctxsub->format3_subst_count(),
          value,
          position);
      }
      return;
  }
}

void OTFontReshaper::apply_chaining_context_substitution(
  const ChainingContextSubstitution *ctxsub,
  int                                value,
  int                                position
) {
  if(!current_lookup_list)
    return;
    
  switch(ctxsub->format()) {
    case 1:
      if(const ChainSubRuleSet *rule_set = ctxsub->format1_search_chain_rule_set(glyphs[position])) {
        for(int r = 0; r < rule_set->count(); ++r) {
          const ChainSubRule *rule = rule_set->rule(r);
          
          int blen = rule->backtrack_glyph_count();
          if(position < blen)
            continue;
            
          int ilen = rule->input_glyph_count();
          int llen = rule->lookahead_glyph_count();
          if(position + ilen + llen > glyphs.length())
            continue;
            
          bool found = true;
          for(int b = 0; b < blen; ++b) {
            if(rule->backtrack_glyph(b) != glyphs[position - b - 1]) {
              found = false;
              break;
            }
          }
          
          if(!found)
            continue;
            
          for(int i = 1; i < ilen; ++i) {
            if(rule->rest_input_glyph(i) != glyphs[position + i]) {
              found = false;
              break;
            }
          }
          
          if(!found)
            continue;
            
          for(int l = 1; l < llen; ++l) {
            if(rule->lookahead_glyph(l) != glyphs[position + ilen + l]) {
              found = false;
              break;
            }
          }
          
          if(found) {
            apply_lookups_at(
              rule->subst_records(),
              rule->subst_count(),
              value,
              position);
              
            return;
          }
        }
      }
      return;
      
    case 2:
      if(const ChainSubClassRuleSet *class_set = ctxsub->format2_search_class_set(glyphs[position])) {
        const ClassDef *b_classdef = ctxsub->format2_get_backtrack_classdef();
        const ClassDef *i_classdef = ctxsub->format2_get_input_classdef();
        const ClassDef *l_classdef = ctxsub->format2_get_lookahead_classdef();
        
        OT_ASSERT(b_classdef != NULL);
        OT_ASSERT(i_classdef != NULL);
        OT_ASSERT(l_classdef != NULL);
        
        static Array<int> backward_glyph_classes;
        backward_glyph_classes.length(0);
        
        static Array<int> rest_glyph_classes;
        rest_glyph_classes.length(0);
        
        for(int r = 0; r < class_set->count(); ++r) {
          const ChainSubClassRule *rule = class_set->rule(r);
          
          int blen = rule->backtrack_glyph_count();
          
          if(position < blen)
            continue;
            
          int ilen = rule->input_glyph_count();
          int llen = rule->lookahead_glyph_count();
          
          if(position + ilen + llen > glyphs.length())
            continue;
            
          for(int b = backward_glyph_classes.length(); b < blen; ++b)
            backward_glyph_classes.add(b_classdef->get_glyph_class(glyphs[position - b - 1]));
            
          for(int i = 1 + rest_glyph_classes.length(); i < ilen; ++i)
            rest_glyph_classes.add(i_classdef->get_glyph_class(glyphs[position + i]));
            
          for(int l = 1 + rest_glyph_classes.length(); l < ilen + llen; ++l)
            rest_glyph_classes.add(l_classdef->get_glyph_class(glyphs[position + l]));
        }
        
        for(int r = 0; r < class_set->count(); ++r) {
          const ChainSubClassRule *rule = class_set->rule(r);
          
          int blen = rule->backtrack_glyph_count();
          
          if(position < blen)
            continue;
            
          int ilen = rule->input_glyph_count();
          int llen = rule->lookahead_glyph_count();
          
          if(position + ilen + llen > glyphs.length())
            continue;
            
          bool found = true;
          for(int b = 0; b < blen; ++b) {
            if(rule->backtrack_glyph_class(b) != backward_glyph_classes[b]) {
              found = false;
              break;
            }
          }
          
          if(!found)
            continue;
            
          for(int i = 1; i < ilen; ++i) {
            if(rule->rest_input_glyph_class(i) != rest_glyph_classes[i - 1]) {
              found = false;
              break;
            }
          }
          
          if(!found)
            continue;
            
          for(int l = 1; l < llen; ++l) {
            if(rule->lookahead_glyph_class(l) != rest_glyph_classes[ilen - 1 + l]) {
              found = false;
              break;
            }
          }
          
          if(found) {
            apply_lookups_at(
              rule->subst_records(),
              rule->subst_count(),
              value,
              position);
              
            return;
          }
        }
      }
      return;
  }
}

void OTFontReshaper::apply_extension_substitution(
  const ExtensionSubstitution *extsub,
  int                          value,
  int                          position
) {
  switch(extsub->extension_type()) {
    case GlyphSubstitutions::GSUBLookupTypeSingle:
      apply_single_substitution(
        (const SingleSubstitution *)extsub->extension_sub_table(),
        position);
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeMultiple:
      apply_multiple_substitution(
        (const MultipleSubstitution *)extsub->extension_sub_table(),
        position);
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeAlternate:
      apply_alternate_substitution(
        (const AlternateSubstitution *)extsub->extension_sub_table(),
        value,
        position);
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeLigature:
      apply_ligature_substitution(
        (const LigatureSubstitution *)extsub->extension_sub_table(),
        position);
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeContext:
      apply_context_substitution(
        (const ContextSubstitution *)extsub->extension_sub_table(),
        value,
        position);
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeChainingContext:
      apply_chaining_context_substitution(
        (const ChainingContextSubstitution *)extsub->extension_sub_table(),
        value,
        position);
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeExtensionSubstitution:
      /* not allowed */
      break;
      
    case GlyphSubstitutions::GSUBLookupTypeReverseChainingContextSingle:
//      apply_reverse_chaining_context_substitution(
//        (const ReverseChainingContextSubstitution *)extsub->extension_sub_table(),
//        value,
//        position);
      break;
      
    default:
      pmath_debug_print("[apply_extension_substitution() unknown extension type %d]\n", extsub->extension_type());
      break;
  }
}

void OTFontReshaper::apply_lookups_at(
  const SubstLookupRecord *lookups,
  int                      count,
  int                      value,
  int                      position
) {
  if(!current_lookup_list)
    return;
    
  for(int i = 0; i < count; ++i) {
    int rel_pos      = lookups[i].sequence_index();
    int lookup_index = lookups[i].lookup_list_index();
    
    OT_ASSERT(0 <= rel_pos);
    OT_ASSERT(position + rel_pos < glyphs.length());
    OT_ASSERT(0 <= lookup_index);
    OT_ASSERT(lookup_index < current_lookup_list->count());
    
    const Lookup *lookup = current_lookup_list->lookup(lookup_index);
    
    apply_lookup(lookup, value, position + rel_pos);
  }
}

//} ... class OTFontReshaper

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

int ClassDef::get_glyph_class(uint16_t glyph) const {
  uint16_t format = BigEndian::read(*(const uint16_t *)&_format);
  
  switch(format) {
    case 1: {
        const Format1 *table = (const Format1 *)&_format;
        
        uint16_t start_glyph = BigEndian::read(table->start_glyph);
        uint16_t count       = BigEndian::read(table->glyph_count);
        if(start_glyph <= glyph && glyph < start_glyph + count) {
          return (int)BigEndian::read(table->class_values[glyph - start_glyph]);
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
            return (int)BigEndian::read(table->ranges[i].glyph_class);
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

//{ class ExtensionSubstitution ...

int ExtensionSubstitution::extension_type() const {
  uint16_t format = BigEndian::read(_format);
  
  switch(format) {
    case 1: {
        const Format1 *table = (const Format1 *)this;
        
        return (int)BigEndian::read(table->extension_lookup_type);
      }
  }
  
  return -1;
}

const LookupSubTable *ExtensionSubstitution::extension_sub_table() const {
  uint16_t format = BigEndian::read(_format);
  
  switch(format) {
    case 1: {
        const Format1 *table = (const Format1 *)this;
        
        uint32_t offset = BigEndian::read(table->extension_offset);
        
        return (const LookupSubTable *)((const uint8_t *)table + offset);
      }
  }
  
  return NULL;
}

//} ... class ExtensionSubstitution

//{ class AlternateSubstitution ...

const AlternateGlyphSet *AlternateSubstitution::search(uint16_t glyph) const {
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

/*int16_t AlternateSubstitution::apply(uint16_t glyph, int index) const {
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

//} ... class AlternateSubstitution

//{ class LigatureSubstitution ...

const LigatureSet *LigatureSubstitution::search(uint16_t glyph) const {
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

//} ... class LigatureSubstitution

//{ class ContextSubstitution ...

const SubRuleSet *ContextSubstitution::format1_search_rule_set( uint16_t first_glyph) const {
  OT_ASSERT(format() == 1);
  
  const Format1 *table = (const Format1 *)this;
  
  uint16_t coverage_offset = BigEndian::read(table->coverage_offset);
  
  const GlyphCoverage *coverage = (const GlyphCoverage *)((const uint8_t *)table + coverage_offset);
  
  int i = coverage->find_glyph(first_glyph);
  if(i < 0)
    return NULL;
    
  OT_ASSERT(i < (int)BigEndian::read(table->count));
  
  uint16_t offset = BigEndian::read(table->sub_rule_set_offsets[i]);
  
  return (const SubRuleSet *)((const uint8_t *)table + offset);
}

const ClassDef *ContextSubstitution::format2_get_classdef() const {
  OT_ASSERT(format() == 2);
  
  const Format2 *table = (const Format2 *)this;
  
  uint16_t classdef_offset = BigEndian::read(table->classdef_offset);
  
  return (const ClassDef *)((const uint8_t *)table + classdef_offset);
}

const SubClassSet *ContextSubstitution::format2_search_class_set(uint16_t first_glyph) const {
  OT_ASSERT(format() == 2);
  
  const Format2 *table = (const Format2 *)this;
  
  uint16_t coverage_offset = BigEndian::read(table->coverage_offset);
  
  const GlyphCoverage *coverage = (const GlyphCoverage *)((const uint8_t *)table + coverage_offset);
  
  int i = coverage->find_glyph(first_glyph);
  if(i < 0)
    return NULL;
    
  uint16_t classdef_offset = BigEndian::read(table->classdef_offset);
  
  const ClassDef *classdef = (const ClassDef *)((const uint8_t *)table + classdef_offset);
  
  i = classdef->get_glyph_class(first_glyph);
  
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < (int)BigEndian::read(table->count));
  
  uint16_t offset = BigEndian::read(table->sub_class_set_offsets[i]);
  
  return (const SubClassSet *)((const uint8_t *)table + offset);
}

int ContextSubstitution::format3_glyph_count() const {
  OT_ASSERT(format() == 3);
  
  const Format3 *table = (const Format3 *)this;
  
  return (int)BigEndian::read(table->glyph_count);
}

const GlyphCoverage *ContextSubstitution::format3_glyph_coverage(int i) const {
  OT_ASSERT(format() == 3);
  
  const Format3 *table = (const Format3 *)this;
  
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < (int)BigEndian::read(table->glyph_count));
  
  uint16_t offset = BigEndian::read(table->coverage_offsets[i]);
  
  return (const GlyphCoverage *)((const uint8_t *)table + offset);
}

int ContextSubstitution::format3_subst_count() const {
  OT_ASSERT(format() == 3);
  
  const Format3 *table = (const Format3 *)this;
  
  return (int)BigEndian::read(table->subst_count);
}

const SubstLookupRecord *ContextSubstitution::format3_subst_records() const {
  OT_ASSERT(format() == 3);
  
  const Format3 *table = (const Format3 *)this;
  
  int glyph_count = (int)BigEndian::read(table->glyph_count);
  
  return (const SubstLookupRecord *)&table->coverage_offsets[glyph_count];
}

//} ... class ContextSubstitution

//{ class ChainContextSubstitution ...

const ChainSubRuleSet *ChainingContextSubstitution::format1_search_chain_rule_set(uint16_t first_glyph) const {
  OT_ASSERT(format() == 1);
  
  const Format1 *table = (const Format1 *)this;
  
  uint16_t coverage_offset = BigEndian::read(table->coverage_offset);
  
  const GlyphCoverage *coverage = (const GlyphCoverage *)((const uint8_t *)table + coverage_offset);
  
  int i = coverage->find_glyph(first_glyph);
  if(i < 0)
    return NULL;
    
  OT_ASSERT(i < (int)BigEndian::read(table->count));
  
  uint16_t offset = BigEndian::read(table->chain_sub_rule_set_offsets[i]);
  
  return (const ChainSubRuleSet *)((const uint8_t *)table + offset);
}

const ClassDef *ChainingContextSubstitution::format2_get_backtrack_classdef() const {
  OT_ASSERT(format() == 2);
  
  const Format2 *table = (const Format2 *)this;
  
  uint16_t backtrack_classdef_offset = BigEndian::read(table->backtrack_classdef_offset);
  
  return (const ClassDef *)((const uint8_t *)table + backtrack_classdef_offset);
}

const ClassDef *ChainingContextSubstitution::format2_get_input_classdef() const {
  OT_ASSERT(format() == 2);
  
  const Format2 *table = (const Format2 *)this;
  
  uint16_t input_classdef_offset = BigEndian::read(table->input_classdef_offset);
  
  return (const ClassDef *)((const uint8_t *)table + input_classdef_offset);
}

const ClassDef *ChainingContextSubstitution::format2_get_lookahead_classdef() const {
  OT_ASSERT(format() == 2);
  
  const Format2 *table = (const Format2 *)this;
  
  uint16_t lookahead_classdef_offset = BigEndian::read(table->lookahead_classdef_offset);
  
  return (const ClassDef *)((const uint8_t *)table + lookahead_classdef_offset);
}

const ChainSubClassRuleSet *ChainingContextSubstitution::format2_search_class_set(uint16_t first_glyph) const {
  OT_ASSERT(format() == 2);
  
  const Format2 *table = (const Format2 *)this;
  
  uint16_t coverage_offset = BigEndian::read(table->coverage_offset);
  
  const GlyphCoverage *coverage = (const GlyphCoverage *)((const uint8_t *)table + coverage_offset);
  
  int i = coverage->find_glyph(first_glyph);
  if(i < 0)
    return NULL;
    
  uint16_t input_classdef_offset = BigEndian::read(table->input_classdef_offset);
  
  const ClassDef *input_classdef = (const ClassDef *)((const uint8_t *)table + input_classdef_offset);
  
  i = input_classdef->get_glyph_class(first_glyph);
  
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < (int)BigEndian::read(table->count));
  
  uint16_t offset = BigEndian::read(table->chain_sub_class_set_offsets[i]);
  
  return (const ChainSubClassRuleSet *)((const uint8_t *)table + offset);
}

void ChainingContextSubstitution::format3_get_glyph_counts(
  int *backtrack,
  int *input,
  int *lookahead
) const {
  OT_ASSERT(format() == 3);
  
  const Format3 *table = (const Format3 *)this;
  
  *backtrack = (int)BigEndian::read(table->backtrack_glyph_count);
  
  const Format3::Part2 *part2 = (const Format3::Part2 *)&table->backtrack_coverage_offsets[*backtrack];
  
  *input = (int)BigEndian::read(part2->input_glyph_count);
  
  const Format3::Part3 *part3 = (const Format3::Part3 *)&part2->input_coverage_offsets[*input];
  
  *lookahead = (int)BigEndian::read(part3->lookahead_glyph_count);
}

const GlyphCoverage *ChainingContextSubstitution::format3_backtrack_glyph_coverage(int i) const {
  OT_ASSERT(format() == 3);
  
  const Format3 *table = (const Format3 *)this;
  
  int bgc = (int)BigEndian::read(table->backtrack_glyph_count);
  
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < bgc);
  
  uint16_t offset = BigEndian::read(table->backtrack_coverage_offsets[i]);
  
  return (const GlyphCoverage *)((const uint8_t *)table + offset);
}

const GlyphCoverage *ChainingContextSubstitution::format3_input_glyph_coverage(int i) const {
  OT_ASSERT(format() == 3);
  
  const Format3 *table = (const Format3 *)this;
  
  int bgc = (int)BigEndian::read(table->backtrack_glyph_count);
  
  const Format3::Part2 *part2 = (const Format3::Part2 *)&table->backtrack_coverage_offsets[bgc];
  
  int igc = (int)BigEndian::read(part2->input_glyph_count);
  
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < igc);
  
  uint16_t offset = BigEndian::read(part2->input_coverage_offsets[i]);
  
  return (const GlyphCoverage *)((const uint8_t *)table + offset);
}

const GlyphCoverage *ChainingContextSubstitution::format3_lookahead_glyph_coverage(int i) const {
  OT_ASSERT(format() == 3);
  
  const Format3 *table = (const Format3 *)this;
  
  int bgc = (int)BigEndian::read(table->backtrack_glyph_count);
  
  const Format3::Part2 *part2 = (const Format3::Part2 *)&table->backtrack_coverage_offsets[bgc];
  
  int igc = (int)BigEndian::read(part2->input_glyph_count);
  
  const Format3::Part3 *part3 = (const Format3::Part3 *)&part2->input_coverage_offsets[igc];
  
  int lgc = (int)BigEndian::read(part3->lookahead_glyph_count);
  
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < lgc);
  
  uint16_t offset = BigEndian::read(part3->lookahead_coverage_offsets[i]);
  
  return (const GlyphCoverage *)((const uint8_t *)table + offset);
}

const SubstLookupRecord *ChainingContextSubstitution::format3_subst_records(int *count) const {
  OT_ASSERT(format() == 3);
  
  const Format3 *table = (const Format3 *)this;
  
  int bgc = (int)BigEndian::read(table->backtrack_glyph_count);
  
  const Format3::Part2 *part2 = (const Format3::Part2 *)&table->backtrack_coverage_offsets[bgc];
  
  int igc = (int)BigEndian::read(part2->input_glyph_count);
  
  const Format3::Part3 *part3 = (const Format3::Part3 *)&part2->input_coverage_offsets[igc];
  
  int lgc = (int)BigEndian::read(part3->lookahead_glyph_count);
  
  const Format3::Part4 *part4 = (const Format3::Part4 *)&part3->lookahead_coverage_offsets[lgc];
  
  *count = (int)BigEndian::read(part4->subst_count);
  
  return part4->subst_lookup_records;
}

//} ... class ChainingContextSubstitution

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
  OT_ASSERT(1 <= i);
  OT_ASSERT(i < components_count());
  
  return BigEndian::read(_rest_components[i - 1]);
}

//} ... class Ligature

//{ class SubstLookupRecord ...

int SubstLookupRecord::sequence_index() const {
  return (int)BigEndian::read(_sequence_index);
}

int SubstLookupRecord::lookup_list_index() const {
  return (int)BigEndian::read(_lookup_list_index);
}

//} ... class SubstLookupRecord

//{ class SubRuleSet ...

int SubRuleSet::count() const {
  return BigEndian::read(_count);
}

const SubRule *SubRuleSet::rule(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < count());
  
  uint16_t offset = BigEndian::read(_rule_offsets[i]);
  
  return (const SubRule *)((const uint8_t *)this + offset);
}

//} ... class SubRuleSet

//{ class SubRule ...

int SubRule::glyph_count() const {
  return BigEndian::read(_glyph_count);
}

int SubRule::subst_count() const {
  return BigEndian::read(_subst_count);
}

uint16_t SubRule::rest_glyph(int i) const {// must be >= 1 and < glyph_count
  OT_ASSERT(1 <= i);
  OT_ASSERT(i < glyph_count());
  
  return BigEndian::read(_rest_glyphs[i - 1]);
}

const SubstLookupRecord *SubRule::subst_records() const {
  return (const SubstLookupRecord *)&this->_rest_glyphs[glyph_count() - 1];
}

//} ... class SubRule

//{ class SubClassSet ...

int SubClassSet::count() const {
  return BigEndian::read(_count);
}

const SubClassRule *SubClassSet::class_rule(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < count());
  
  uint16_t offset = BigEndian::read(_class_rule_offsets[i]);
  
  return (const SubClassRule *)((const uint8_t *)this + offset);
}

//} ... class SubClassSet

//{ class SubClassRule ...

int SubClassRule::glyph_count() const {
  return BigEndian::read(_glyph_count);
}

int SubClassRule::subst_count() const {
  return BigEndian::read(_subst_count);
}

int SubClassRule::rest_glyph_class(int i) const {// i must be >= 1 and < glyph_count
  OT_ASSERT(1 <= i);
  OT_ASSERT(i < glyph_count());
  
  return (int)BigEndian::read(_rest_glyph_classes[i - 1]);
}

const SubstLookupRecord *SubClassRule::subst_records() const {
  return (const SubstLookupRecord *)&this->_rest_glyph_classes[glyph_count() - 1];
}

//} ... class SubClassRule

//{ class ChainSubRuleSet ...

int ChainSubRuleSet::count() const {
  return BigEndian::read(_count);
}

const ChainSubRule *ChainSubRuleSet::rule(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < count());
  
  uint16_t offset = BigEndian::read(_rule_offsets[i]);
  
  return (const ChainSubRule *)((const uint8_t *)this + offset);
}

//} ... class ChainSubRuleSet

//{ class ChainSubRule ...

int ChainSubRule::backtrack_glyph_count() const {
  return BigEndian::read(_backtrack_glyph_count);
}

int ChainSubRule::input_glyph_count() const {
  int bgc = BigEndian::read(_backtrack_glyph_count);
  
  const ChainSubRulePart2 *part2 = (const ChainSubRulePart2 *)&_backtrack_glyphs[bgc];
  
  return BigEndian::read(part2->_input_glyph_count);
}

int ChainSubRule::lookahead_glyph_count() const {
  int bgc = BigEndian::read(_backtrack_glyph_count);
  
  const ChainSubRulePart2 *part2 = (const ChainSubRulePart2 *)&_backtrack_glyphs[bgc];
  
  int igc = BigEndian::read(part2->_input_glyph_count);
  
  const ChainSubRulePart3 *part3 = (const ChainSubRulePart3 *)&part2->_rest_input_glyphs[igc - 1];
  
  return BigEndian::read(part3->_lookahead_glyph_count);
}

int ChainSubRule::subst_count() const {
  int bgc = BigEndian::read(_backtrack_glyph_count);
  
  const ChainSubRulePart2 *part2 = (const ChainSubRulePart2 *)&_backtrack_glyphs[bgc];
  
  int igc = BigEndian::read(part2->_input_glyph_count);
  
  const ChainSubRulePart3 *part3 = (const ChainSubRulePart3 *)&part2->_rest_input_glyphs[igc - 1];
  
  int lgc = BigEndian::read(part3->_lookahead_glyph_count);
  
  const ChainSubRulePart4 *part4 = (const ChainSubRulePart4 *)&part3->_lookahead_glyphs[lgc];
  
  return BigEndian::read(part4->_subst_count);
}

uint16_t ChainSubRule::backtrack_glyph(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < backtrack_glyph_count());
  
  return BigEndian::read(_backtrack_glyphs[i]);
}

uint16_t ChainSubRule::rest_input_glyph(int i) const { // i must be >= 1 and < input_glyph_count
  int bgc = BigEndian::read(_backtrack_glyph_count);
  
  const ChainSubRulePart2 *part2 = (const ChainSubRulePart2 *)&_backtrack_glyphs[bgc];
  
  OT_ASSERT(1 <= i);
  OT_ASSERT(i < (int)BigEndian::read(part2->_input_glyph_count));
  
  return BigEndian::read(part2->_rest_input_glyphs[i - 1]);
}

uint16_t ChainSubRule::lookahead_glyph(int i) const {
  int bgc = BigEndian::read(_backtrack_glyph_count);
  
  const ChainSubRulePart2 *part2 = (const ChainSubRulePart2 *)&_backtrack_glyphs[bgc];
  
  int igc = BigEndian::read(part2->_input_glyph_count);
  
  const ChainSubRulePart3 *part3 = (const ChainSubRulePart3 *)&part2->_rest_input_glyphs[igc - 1];
  
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < (int)BigEndian::read(part3->_lookahead_glyph_count));
  
  return BigEndian::read(part3->_lookahead_glyphs[i]);
}

const SubstLookupRecord *ChainSubRule::subst_records() const {
  int bgc = BigEndian::read(_backtrack_glyph_count);
  
  const ChainSubRulePart2 *part2 = (const ChainSubRulePart2 *)&_backtrack_glyphs[bgc];
  
  int igc = BigEndian::read(part2->_input_glyph_count);
  
  const ChainSubRulePart3 *part3 = (const ChainSubRulePart3 *)&part2->_rest_input_glyphs[igc - 1];
  
  int lgc = BigEndian::read(part3->_lookahead_glyph_count);
  
  const ChainSubRulePart4 *part4 = (const ChainSubRulePart4 *)&part3->_lookahead_glyphs[lgc];
  
  return &part4->_subst_lookup_records[0];
}

//} ... class ChainSubRule

//{ class ChainSubClassRuleSet ...

int ChainSubClassRuleSet::count() const {
  return BigEndian::read(_count);
}

const ChainSubClassRule *ChainSubClassRuleSet::rule(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < count());
  
  uint16_t offset = BigEndian::read(_rule_offsets[i]);
  
  return (const ChainSubClassRule *)((const uint8_t *)this + offset);
}

//} ... class ChainSubClassRuleSet

//{ class ChainSubClassRule ...

int ChainSubClassRule::backtrack_glyph_count() const {
  return BigEndian::read(_backtrack_glyph_count);
}

int ChainSubClassRule::input_glyph_count() const {
  int bgc = BigEndian::read(_backtrack_glyph_count);
  
  const ChainSubClassRulePart2 *part2 = (const ChainSubClassRulePart2 *)&_backtrack_glyph_classes[bgc];
  
  return BigEndian::read(part2->_input_glyph_count);
}

int ChainSubClassRule::lookahead_glyph_count() const {
  int bgc = BigEndian::read(_backtrack_glyph_count);
  
  const ChainSubClassRulePart2 *part2 = (const ChainSubClassRulePart2 *)&_backtrack_glyph_classes[bgc];
  
  int igc = BigEndian::read(part2->_input_glyph_count);
  
  const ChainSubClassRulePart3 *part3 = (const ChainSubClassRulePart3 *)&part2->_rest_input_glyph_classes[igc - 1];
  
  return BigEndian::read(part3->_lookahead_glyph_count);
}

int ChainSubClassRule::subst_count() const {
  int bgc = BigEndian::read(_backtrack_glyph_count);
  
  const ChainSubClassRulePart2 *part2 = (const ChainSubClassRulePart2 *)&_backtrack_glyph_classes[bgc];
  
  int igc = BigEndian::read(part2->_input_glyph_count);
  
  const ChainSubClassRulePart3 *part3 = (const ChainSubClassRulePart3 *)&part2->_rest_input_glyph_classes[igc - 1];
  
  int lgc = BigEndian::read(part3->_lookahead_glyph_count);
  
  const ChainSubClassRulePart4 *part4 = (const ChainSubClassRulePart4 *)&part3->_lookahead_glyph_classes[lgc];
  
  return BigEndian::read(part4->_subst_count);
}

int ChainSubClassRule::backtrack_glyph_class(int i) const {
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < backtrack_glyph_count());
  
  return BigEndian::read(_backtrack_glyph_classes[i]);
}

int ChainSubClassRule::rest_input_glyph_class(int i) const { // i must be >= 1 and < input_glyph_count
  int bgc = BigEndian::read(_backtrack_glyph_count);
  
  const ChainSubClassRulePart2 *part2 = (const ChainSubClassRulePart2 *)&_backtrack_glyph_classes[bgc];
  
  OT_ASSERT(1 <= i);
  OT_ASSERT(i < (int)BigEndian::read(part2->_input_glyph_count));
  
  return BigEndian::read(part2->_rest_input_glyph_classes[i - 1]);
}

int ChainSubClassRule::lookahead_glyph_class(int i) const {
  int bgc = BigEndian::read(_backtrack_glyph_count);
  
  const ChainSubClassRulePart2 *part2 = (const ChainSubClassRulePart2 *)&_backtrack_glyph_classes[bgc];
  
  int igc = BigEndian::read(part2->_input_glyph_count);
  
  const ChainSubClassRulePart3 *part3 = (const ChainSubClassRulePart3 *)&part2->_rest_input_glyph_classes[igc - 1];
  
  OT_ASSERT(0 <= i);
  OT_ASSERT(i < (int)BigEndian::read(part3->_lookahead_glyph_count));
  
  return BigEndian::read(part3->_lookahead_glyph_classes[i]);
}

const SubstLookupRecord *ChainSubClassRule::subst_records() const {
  int bgc = BigEndian::read(_backtrack_glyph_count);
  
  const ChainSubClassRulePart2 *part2 = (const ChainSubClassRulePart2 *)&_backtrack_glyph_classes[bgc];
  
  int igc = BigEndian::read(part2->_input_glyph_count);
  
  const ChainSubClassRulePart3 *part3 = (const ChainSubClassRulePart3 *)&part2->_rest_input_glyph_classes[igc - 1];
  
  int lgc = BigEndian::read(part3->_lookahead_glyph_count);
  
  const ChainSubClassRulePart4 *part4 = (const ChainSubClassRulePart4 *)&part3->_lookahead_glyph_classes[lgc];
  
  return &part4->_subst_lookup_records[0];
}

//} ... class ChainSubClassRule
