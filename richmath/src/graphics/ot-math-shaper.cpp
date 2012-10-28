#define _WIN32_WINNT 0x501

#include <graphics/ot-math-shaper.h>

#include <cmath>

#include <boxes/mathsequence.h>

#include <graphics/context.h>

#include <util/style.h>
#include <util/syntax-state.h>

using namespace richmath;

static uint16_t SqrtChar = 0x221A;

class StaticCanvas: public Base {
  public:
    StaticCanvas() {
      surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 1, 1);
      cr = cairo_create(surface);

      canvas = new Canvas(cr);
    }

    ~StaticCanvas() {
      delete canvas;

      cairo_destroy(cr);
      cairo_surface_destroy(surface);
    }

  public:
    cairo_surface_t *surface;
    cairo_t         *cr;
    Canvas          *canvas;
};

static StaticCanvas static_canvas;

//{ class DeviceAdjustment ...

DeviceAdjustment::DeviceAdjustment(const uint16_t *data)
  : start_size(0)
{
  start_size = BigEndian::read(data[0]);
  values.length(BigEndian::read(data[1]) - start_size + 1);

  switch(BigEndian::read(data[2])) {
    case 1: {
        union {
          uint16_t big;
          struct {
            signed v1: 2;
            signed v2: 2;
            signed v3: 2;
            signed v4: 2;
            signed v5: 2;
            signed v6: 2;
            signed v7: 2;
            signed v8: 2;
          } v;
        } pack;
        data += 3;

        for(int i = 0; i < values.length() / 8; ++i) {
          pack.big = data[i];

          values[8 * i]     = pack.v.v1;
          values[8 * i + 1] = pack.v.v2;
          values[8 * i + 2] = pack.v.v3;
          values[8 * i + 3] = pack.v.v4;
          values[8 * i + 4] = pack.v.v5;
          values[8 * i + 5] = pack.v.v6;
          values[8 * i + 6] = pack.v.v7;
          values[8 * i + 7] = pack.v.v8;
        }

        if(values.length() % 8) {
          int i = values.length() / 8;
          pack.big = data[i];

          switch(values.length() % 8) {
            case 7: values[8 * i + 6] = pack.v.v7;
            case 6: values[8 * i + 5] = pack.v.v6;
            case 5: values[8 * i + 4] = pack.v.v5;
            case 4: values[8 * i + 3] = pack.v.v4;
            case 3: values[8 * i + 2] = pack.v.v3;
            case 2: values[8 * i + 1] = pack.v.v2;
            case 1: values[8 * i]     = pack.v.v1;
          }
        }
      } break;

    case 2: {
        union {
          uint16_t big;
          struct {
            signed v1: 4;
            signed v2: 4;
            signed v3: 4;
            signed v4: 4;
          } v;
        } pack;
        data += 3;

        for(int i = 0; i < values.length() / 4; ++i) {
          pack.big = data[i];

          values[4 * i]     = pack.v.v1;
          values[4 * i + 1] = pack.v.v2;
          values[4 * i + 2] = pack.v.v3;
          values[4 * i + 3] = pack.v.v4;
        }

        if(values.length() % 4) {
          int i = values.length() / 4;
          pack.big = data[i];

          switch(values.length() % 8) {
            case 3: values[4 * i + 2] = pack.v.v3;
            case 2: values[4 * i + 1] = pack.v.v2;
            case 1: values[4 * i]     = pack.v.v1;
          }
        }
      } break;

    case 3: {
        union {
          uint16_t big;
          struct {
            int8_t v1;
            int8_t v2;
          } v;
        } pack;
        data += 3;

        for(int i = 0; i < values.length() / 2; ++i) {
          pack.big = data[i];

          values[2 * i]     = pack.v.v1;
          values[2 * i + 1] = pack.v.v2;
        }

        if(values.length() % 2) {
          int i = values.length() / 2;
          pack.big = data[i];

          values[2 * i] = pack.v.v1;
        }
      } break;
  }
}

int DeviceAdjustment::adjustment(int fontsize) {
  if(fontsize >= start_size
      && fontsize < start_size + values.length()) {
    return values[fontsize - start_size];
  }

  return 0;
}

//} ... class DeviceAdjustment

//{ class KernVertexObject ...

KernVertexObject::KernVertexObject(const MathKernVertex *v) {
  values.length(1 + 2 * BigEndian::read(v->heigth_count));

  for(uint16_t i = 0; i < values.length(); ++i)
    values[i] = BigEndian::read(v->values[i].value);
}

int16_t KernVertexObject::height_to_kern(int16_t height, bool above) {
  int h = values.length() / 2;

  if(values.length() == 2 * h + 1) {
    for(int i = 0; i < h; ++i) {
      if( ( above && values[i] <= height) ||
          (!above && values[i] >= height))
      {
        return values[h + i];
      }
    }

    return values[2 * h];
  }

  return 0;
}

//} ... class KernVertexObject

//{ class OTMathShaperDB ...

Hashtable<String, SharedPtr<OTMathShaperDB> > OTMathShaperDB::registered;

OTMathShaperDB::OTMathShaperDB()
  : Shareable(),
    fi(0)
{
  private_characters.default_value = 0;
  alt_glyphs.default_value = 0;
  italics_correction.default_value = 0;
  top_accents.default_value = 0;
}

OTMathShaperDB::~OTMathShaperDB() {
}

void OTMathShaperDB::clear_cache() {
  fi = 0;
  for(int i = 0; i < FontStyle::Permutations; ++i)
    shapers[i] = 0;
}

void OTMathShaperDB::clear_all() {
  int c = registered.size();
  for(int i = 0; c > 0; ++i) {
    Entry<String, SharedPtr<OTMathShaperDB> > *e = registered.entry(i);

    if(e) {
      --c;

      e->value->clear_cache();
    }
  }

  registered.clear();
}

bool OTMathShaperDB::set_private_char(uint32_t ch, uint32_t fallback) {
  if(fi) {
    uint16_t glyph;
    glyph = fi->char_to_glyph(fallback);
    if(glyph) {
      private_characters.set(ch, glyph);
      alt_glyphs.set(ch, glyph);
      return true;
    }
  }

  return false;
}

bool OTMathShaperDB::set_alt_char(uint32_t ch, uint32_t fallback) {
  if(fi) {
    uint16_t glyph;
    glyph = fi->char_to_glyph(fallback);
    if(glyph) {
      alt_glyphs.set(ch, glyph);
      return true;
    }
  }

  return false;
}

SharedPtr<OTMathShaper> OTMathShaperDB::find(String name, FontStyle style) {
  SharedPtr<OTMathShaperDB> db = registered[name];
  if(db)
    return db->find(style);

  db = new OTMathShaperDB();
  db->name = name;
  db->shapers[(int)style] = new OTMathShaper(db, style);
  db->fi = &db->shapers[(int)style]->fi;

  size_t size = db->fi->get_truetype_table(FONT_TABLE_NAME('M', 'A', 'T', 'H'), 0, 0, 0);

  if(size) {
//    db->private_characters.set(0x2061, 0); // function application
    db->set_private_char(0x2061, ' ');

    db->set_private_char(PMATH_CHAR_LEFTINVISIBLEBRACKET,     0x200B);
    db->set_private_char(PMATH_CHAR_RIGHTINVISIBLEBRACKET,    0x200B);
    db->set_private_char(PMATH_CHAR_LEFTBRACKETINGBAR,        0x2223);
    db->set_private_char(PMATH_CHAR_RIGHTBRACKETINGBAR,       0x2223);
    db->set_private_char(PMATH_CHAR_LEFTDOUBLEBRACKETINGBAR,  0x2225);
    db->set_private_char(PMATH_CHAR_RIGHTDOUBLEBRACKETINGBAR, 0x2225);
    db->set_private_char(PMATH_CHAR_PIECEWISE, '{');
    db->set_private_char(PMATH_CHAR_ALIASDELIMITER, 0x21E9);
    db->set_private_char(PMATH_CHAR_ALIASINDICATOR, 0x21E9);
    db->set_private_char(CHAR_REPLACEMENT,       0x220E);
    db->set_private_char(CHAR_LINE_CONTINUATION, 0x22F1);
    if(!db->set_private_char(PMATH_CHAR_PLACEHOLDER, 0x29E0))
      if(!db->set_private_char(PMATH_CHAR_PLACEHOLDER, 0x25A1))
        db->set_private_char(PMATH_CHAR_PLACEHOLDER, 0x2B1A);
    db->set_private_char('-', 0x2212);
    db->set_private_char(0x2145, 0x1D403); // DD
    db->set_private_char(0x2146, 0x1D41D); // dd
    db->set_private_char(0x2147, 0x1D41E); // ee
    db->set_private_char(0x2148, 0x1D422); // ii
    db->set_private_char(0x2149, 0x1D423); // jj

    db->set_alt_char('^',    0x0302);
    db->set_alt_char('~',    0x0303);
    db->set_alt_char('_',    0x0305);
    db->set_alt_char(0x02C7, 0x030C); // caron
    db->set_alt_char(0x21C0, 0x20D1); // vector

    db->set_alt_char(0x2227, 0x22C0); // and
    db->set_alt_char(0x2228, 0x22C1); // or
    db->set_alt_char(0x2229, 0x22C2); // intersect
    db->set_alt_char(0x222A, 0x22C3); // union
    db->set_alt_char(0x2299, 0x2A00); // c.
    db->set_alt_char(0x2295, 0x2A01); // c+
    db->set_alt_char(0x2297, 0x2A02); // c*
    db->set_alt_char(0x2293, 0x2A05); // cap
    db->set_alt_char(0x2294, 0x2A06); // cup
    db->set_alt_char(0x00D7, 0x2A09); // times

    /* Ugly workaround to access cross product character. Glyph 941 is a small
       cross, but it has no character mapping. Fontforge doesn't even list this
       glyph! So we have to hardwire it here, because the cross product is
       rather important and should be distinguishable from the times character.

       A better workaround would be to specify some fallback font.
     */
    if(name.equals("Cambria Math")) {
      db->private_characters.set(0x2A2F, 941);
    }
    else if(db->fi->char_to_glyph(0x2A2F) == 0) { // cross
      db->set_private_char(0x2A2F, 0x00D7);
    }

    /* Ugly workaround to access degree character in Asana Math. The font
       assigns the glyph #1 to the degree character, which is clearly a bug,
       because glyph #1 is the space.
       We use the MASCULINE ORDINAL INDICATOR U+00BA which looks like a degree
       sign in that font.
     */
    if(name.equals("Asana Math")) {
//      pmath_debug_print("Asana Math degree glyph: %d\n", db->fi->char_to_glyph(0x00B0));
      db->set_private_char(0x00B0, 0x00BA);
    }

    db->units_per_em = 1024;
    db->fi->get_truetype_table(FONT_TABLE_NAME('h', 'e', 'a', 'd'), 18, &db->units_per_em, 2);
    db->units_per_em = BigEndian::read(db->units_per_em);

    Array<uint8_t> data(size);
    db->fi->get_truetype_table(FONT_TABLE_NAME('M', 'A', 'T', 'H'), 0, data.items(), size);
    const uint8_t *table = data.items();

    const MathTableHeader *math_header = (const MathTableHeader *)table;

    {
      uint16_t       *db_consts = (uint16_t *)&db->consts;
      const uint16_t *consts    = (const uint16_t *)(table + BigEndian::read(math_header->constants_offset));
      for(size_t i = 0; i < sizeof(MathConstants) / 2; ++i)
        db_consts[i] = BigEndian::read(consts[i]);
    }

    if(math_header->glyphinfo_offset) {
      const MathGlyphInfo *glyphinfo = (const MathGlyphInfo *)
                                       (table + BigEndian::read(math_header->glyphinfo_offset));

      if(glyphinfo->kern_info_offset) {
        const MathKernInfo *kern_info = (const MathKernInfo *)
                                        (((const char *)glyphinfo) + BigEndian::read(glyphinfo->kern_info_offset));

        const uint16_t *coverage = (const uint16_t *)
                                   (((const char *)kern_info) + BigEndian::read(kern_info->coverage_offset));

        switch(BigEndian::read(coverage[0])) { // coverage format
          case 1: {
              const uint16_t *glyphs = (const uint16_t *)
                                       (((const char *)coverage) + 4); // skip format, count

              for(uint16_t i = 0; i < BigEndian::read(kern_info->count); ++i) {
                const MathKernVertex *vertex;

                for(int edge = 0; edge < 4; ++edge) {
                  if(kern_info->offsets[i][edge]) {
                    vertex = (const MathKernVertex *)
                             (((const char *)kern_info) + BigEndian::read(kern_info->offsets[i][edge]));

                    db->math_kern[edge].set(
                      BigEndian::read(glyphs[i]),
                      KernVertexObject(vertex));
                  }
                }
              }
            } break;

          case 2: {
              const GlyphRangeRecord *ranges = (const GlyphRangeRecord *)
                                               (((const char *)coverage) + 4);

              for(uint16_t r = 0; r < BigEndian::read(coverage[1]); ++r) {
                uint16_t start_glyph = BigEndian::read(ranges[r].start_glyph);
                uint16_t end_glyph   = BigEndian::read(ranges[r].end_glyph);
                uint16_t start_index = BigEndian::read(ranges[r].start_index);

                for(uint16_t g = start_glyph; g <= end_glyph; ++g) {
                  const MathKernVertex *vertex;

                  uint16_t i = start_index + g - start_glyph;

                  for(int edge = 0; edge < 4; ++edge) {
                    if(kern_info->offsets[i][edge]) {
                      vertex = (const MathKernVertex *)
                               (((const char *)kern_info) + BigEndian::read(kern_info->offsets[i][edge]));

                      db->math_kern[edge].set(g, KernVertexObject(vertex));
                    }
                  }
                }
              }
            } break;
        }
      }

      if(glyphinfo->italics_correction_info_offset) {
        const MathItalicsCorrectionInfo *ital_corr_info = (const MathItalicsCorrectionInfo *)
            (((const char *)glyphinfo) + BigEndian::read(glyphinfo->italics_correction_info_offset));

        const uint16_t *coverage = (const uint16_t *)
                                   (((const char *)ital_corr_info) + BigEndian::read(ital_corr_info->coverage_offset));

        switch(BigEndian::read(coverage[0])) { // coverage format
          case 1: {
              const uint16_t *glyphs = (const uint16_t *)
                                       (((const char *)coverage) + 4); // skip format, count

              for(uint16_t i = 0; i < BigEndian::read(ital_corr_info->count); ++i) {
                db->italics_correction.set(
                  BigEndian::read(glyphs[i]),
                  BigEndian::read(ital_corr_info->italics_corrections[i].value));
              }
            } break;

          case 2: {
              const GlyphRangeRecord *ranges = (const GlyphRangeRecord *)
                                               (((const char *)coverage) + 4);

              for(uint16_t r = 0; r < BigEndian::read(coverage[1]); ++r) {
                uint16_t start_glyph = BigEndian::read(ranges[r].start_glyph);
                uint16_t end_glyph   = BigEndian::read(ranges[r].end_glyph);
                uint16_t start_index = BigEndian::read(ranges[r].start_index);

                for(uint16_t g = start_glyph; g <= end_glyph; ++g) {
                  int16_t value = BigEndian::read(
                                    ital_corr_info->italics_corrections[start_index + g - start_glyph].value);

                  db->italics_correction.set(g, value);
                }
              }
            } break;
        }
      }

      if(glyphinfo->top_accent_attachment_offset) {
        const MathTopAccentAttachment *top_acc = (const MathTopAccentAttachment *)
            (((const char *)glyphinfo) + BigEndian::read(glyphinfo->top_accent_attachment_offset));

        const uint16_t *coverage = (const uint16_t *)
                                   (((const char *)top_acc) + BigEndian::read(top_acc->coverage_offset));

        switch(BigEndian::read(coverage[0])) { // coverage format
          case 1: {
              const uint16_t *glyphs = (const uint16_t *)
                                       (((const char *)coverage) + 4); // skip format, count

              for(uint16_t i = 0; i < BigEndian::read(top_acc->count); ++i) {
                db->top_accents.set(
                  BigEndian::read(glyphs[i]),
                  BigEndian::read(top_acc->top_accent_attachment[i].value));
              }
            } break;

          case 2: {
              const GlyphRangeRecord *ranges = (const GlyphRangeRecord *)
                                               (((const char *)coverage) + 4);

              for(uint16_t r = 0; r < BigEndian::read(coverage[1]); ++r) {
                uint16_t start_glyph = BigEndian::read(ranges[r].start_glyph);
                uint16_t end_glyph   = BigEndian::read(ranges[r].end_glyph);
                uint16_t start_index = BigEndian::read(ranges[r].start_index);

                for(uint16_t g = start_glyph; g <= end_glyph; ++g) {
                  int16_t value = BigEndian::read(
                                    top_acc->top_accent_attachment[start_index + g - start_glyph].value);

                  db->top_accents.set(g, value);
                }
              }
            } break;
        }
      }
    }

    if(math_header->variants_offset) {
      static Array<MathGlyphVariantRecord> variant_array;
      static Array<MathGlyphPartRecord>    assambly_array;

      const MathVariants *variants = (const MathVariants *)
                                     (table + BigEndian::read(math_header->variants_offset));

      db->min_connector_overlap = BigEndian::read(variants->min_connector_overlap);

      uint16_t vert_count = BigEndian::read(variants->vert_glyph_count);
      for(uint16_t i = 0; i < vert_count; ++i) {
        const MathGlyphConstruction *construction = (const MathGlyphConstruction *)
            (((const char *)variants) + BigEndian::read(variants->glyph_construction_offsets[i]));

        if(construction->count) {
          if(construction->assembly_offset) {
            const MathGlyphAssembly *assambly = (const MathGlyphAssembly *)
                                                (((const char *)construction) + BigEndian::read(construction->assembly_offset));

            if(assambly->count) {
              assambly_array.length(BigEndian::read(assambly->count));

              for(int j = 0; j < assambly_array.length(); ++j) {
                const uint16_t *src = (const uint16_t *)&assambly->parts[j];
                uint16_t       *dst = (uint16_t *)&assambly_array[j];

                for(size_t k = 0; k < sizeof(MathGlyphPartRecord) / 2; ++k)
                  dst[k] = BigEndian::read(src[k]);
              }

              db->vert_assembly.set(
                BigEndian::read(construction->variants[0].glyph),
                assambly_array);
            }
          }

          variant_array.length(BigEndian::read(construction->count));
          if(variant_array.length() > 1) {
            for(int j = 0; j < variant_array.length(); ++j) {
              variant_array[j].glyph   = BigEndian::read(construction->variants[j].glyph);
              variant_array[j].advance = BigEndian::read(construction->variants[j].advance);
            }

            db->vert_variants.set(
              BigEndian::read(construction->variants[0].glyph),
              variant_array);
          }
        }
      }

      uint16_t horz_count = BigEndian::read(variants->horz_glyph_count);
      for(uint16_t i = 0; i < horz_count; ++i) {
        const MathGlyphConstruction *construction = (const MathGlyphConstruction *)
            (((const char *)variants) + BigEndian::read(variants->glyph_construction_offsets[vert_count + i]));

        if(construction->count) {
          if(construction->assembly_offset) {
            const MathGlyphAssembly *assambly = (const MathGlyphAssembly *)
                                                (((const char *)construction) + BigEndian::read(construction->assembly_offset));

            if(assambly->count) {
              assambly_array.length(BigEndian::read(assambly->count));

              for(int j = 0; j < assambly_array.length(); ++j) {
                const uint16_t *src = (const uint16_t *)&assambly->parts[j];
                uint16_t       *dst = (uint16_t *)&assambly_array[j];

                for(size_t k = 0; k < sizeof(MathGlyphPartRecord) / 2; ++k)
                  dst[k] = BigEndian::read(src[k]);
              }

              db->horz_assembly.set(
                BigEndian::read(construction->variants[0].glyph),
                assambly_array);
            }
          }

          variant_array.length(BigEndian::read(construction->count));
          if(variant_array.length() > 1) {
            for(int j = 0; j < variant_array.length(); ++j) {
              variant_array[j].glyph   = BigEndian::read(construction->variants[j].glyph);
              variant_array[j].advance = BigEndian::read(construction->variants[j].advance);
            }

            db->horz_variants.set(
              BigEndian::read(construction->variants[0].glyph),
              variant_array);
          }
        }
      }

    }
    
    if(db->fi->char_to_glyph(PMATH_CHAR_ASSIGNDELAYED) == 0) { // ::= (it is needed, but not in Cambria Math)
      Array<MathGlyphPartRecord> lig;

      lig.length(3);
      lig.zeromem();
      lig[0].glyph = lig[1].glyph = db->fi->char_to_glyph(0x2236); //colon
      lig[2].glyph = db->fi->char_to_glyph('=');

      static_canvas.canvas->set_font_face(db->shapers[(int)style]->font(0));
      static_canvas.canvas->set_font_size(db->units_per_em);
      cairo_text_extents_t cte;
      cairo_glyph_t cg;
      cg.x = 0;
      cg.y = 0;
      cg.index = lig[0].glyph;
      static_canvas.canvas->glyph_extents(&cg, 1, &cte);
      lig[0].full_advance = lig[1].full_advance =
                              (uint16_t)cte.x_advance + db->min_connector_overlap;

      cg.index = lig[lig.length() - 1].glyph;
      static_canvas.canvas->glyph_extents(&cg, 1, &cte);
      lig[lig.length() - 1].full_advance = (uint16_t)cte.x_advance;

      db->private_ligatures.set(PMATH_CHAR_ASSIGNDELAYED, lig);
    }
    
    if(db->fi->char_to_glyph(PMATH_CHAR_RULEDELAYED) == 0) { // :-> (it is needed, but not in Cambria Math)
      Array<MathGlyphPartRecord> lig;

      lig.length(2);
      lig.zeromem();
      lig[0].glyph = db->fi->char_to_glyph(0x2236); //colon
      lig[1].glyph = db->fi->char_to_glyph(PMATH_CHAR_RULE);

      static_canvas.canvas->set_font_face(db->shapers[(int)style]->font(0));
      static_canvas.canvas->set_font_size(db->units_per_em);
      cairo_text_extents_t cte;
      cairo_glyph_t cg;
      cg.x = 0;
      cg.y = 0;

      cg.index = lig[0].glyph;
      static_canvas.canvas->glyph_extents(&cg, 1, &cte);
      lig[0].full_advance = //lig[1].full_advance =
        (uint16_t)cte.x_advance + db->min_connector_overlap;

      cg.index = lig[1].glyph;
      static_canvas.canvas->glyph_extents(&cg, 1, &cte);
      lig[1].full_advance = (uint16_t)cte.x_advance;

      db->private_ligatures.set(PMATH_CHAR_RULEDELAYED, lig);
    }
    
    registered.set(name, db);
    return db->shapers[(int)style];
  }

  db->clear_cache();
  return 0;
}

SharedPtr<OTMathShaper> OTMathShaperDB::find(FontStyle style) {
  if(shapers[(int)style].is_valid())
    return shapers[(int)style];

  ref();
  shapers[(int)style] = new OTMathShaper(this, style);

  if(!fi)
    fi = &shapers[(int)style]->fi;

  return shapers[(int)style];
}

Array<MathGlyphVariantRecord> *OTMathShaperDB::get_vert_variants(uint32_t ch, uint16_t glyph) {
  Array<MathGlyphVariantRecord> *res = vert_variants.search(glyph);
  if(res)
    return res;

  uint16_t alt = alt_glyphs[ch];
  if(alt)
    return vert_variants.search(alt);

  return 0;
}

Array<MathGlyphVariantRecord> *OTMathShaperDB::get_horz_variants(uint32_t ch, uint16_t glyph) {
  Array<MathGlyphVariantRecord> *res = horz_variants.search(glyph);
  if(res)
    return res;

  uint16_t alt = alt_glyphs[ch];
  if(alt)
    return horz_variants.search(alt);

  return 0;
}

Array<MathGlyphPartRecord> *OTMathShaperDB::get_vert_assembly(uint32_t ch, uint16_t glyph) {
  Array<MathGlyphPartRecord> *res = vert_assembly.search(glyph);
  if(res)
    return res;

  uint16_t alt = alt_glyphs[ch];
  if(alt)
    return vert_assembly.search(alt);

  return 0;
}

Array<MathGlyphPartRecord> *OTMathShaperDB::get_horz_assembly(uint32_t ch, uint16_t glyph) {
  Array<MathGlyphPartRecord> *res = horz_assembly.search(glyph);
  if(res)
    return res;

  uint16_t alt = alt_glyphs[ch];
  if(alt) {
    res = horz_assembly.search(alt);
    if(res)
      return res;
  }

  return private_ligatures.search(ch);
}

//} ... class OTMathShaperDB

//{ class OTMathShaper ...

OTMathShaper::OTMathShaper(SharedPtr<OTMathShaperDB> _db, FontStyle _style)
  : MathShaper(),
    db(_db),
    text_shaper(new FallbackTextShaper(TextShaper::find(_db->name, _style))),
    style(_style),
    fi(text_shaper->font(0))
{
  text_shaper->add_default();
}

OTMathShaper::~OTMathShaper() {
}

void OTMathShaper::decode_token(
  Context        *context,
  int             len,
  const uint16_t *str,
  GlyphInfo      *result
) {
  if(len == 1
      && context->math_spacing
      && context->single_letter_italics) {
    uint32_t ch = *str;
    if(ch >= 'a' && ch <= 'z')
      ch = 0x1D44E + ch - 'a';
    else if(ch >= 'A' && ch <= 'Z')
      ch = 0x1D434 + ch - 'A';
    else if(ch >= 0x03B1 && ch <= 0x03C9) // alpha - omega
      ch = 0x1D6FC + ch - 0x03B1;
    else if(ch >= 0x0391 && ch <= 0x03A9) // Alpha - Omega
      ch = 0x1D6E2 + ch - 0x0391;
    else if(ch == 0x03D1) // theta symbol
      ch = 0x1D717;
    else if(ch == 0x03D5) // phi symbol
      ch = 0x1D719;
    else if(ch == 0x03D6) // pi symbol
      ch = 0x1D71B;
    else if(ch == 0x03F0) // kappa symbol
      ch = 0x1D718;
    else if(ch == 0x03F1) // rho symbol
      ch = 0x1D71A;
    else
      ch = 0;

    if(ch) {
      uint16_t glyph = db->private_characters[ch];
      if(glyph) {
        if(style.italic) {
          math_set_style(style - Italic)->decode_token(context, len, str, result);
          result->slant = FontSlantPlain;
          return;
        }

        result->index    = glyph;
        result->fontinfo = 0;

        context->canvas->set_font_face(font(0));
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = 0;
        cg.y = 0;
        cg.index = result->index;
        context->canvas->glyph_extents(&cg, 1, &cte);

        result->x_offset = 0;
        result->right = cte.x_advance;
        return;
      }

      uint16_t utf16[2];
      GlyphInfo r[2];
      memset(r, 0, sizeof(r));

      if(ch <= 0xFFFF) {
        utf16[0] = ch;
      }
      else {
        ch -= 0x10000;

        utf16[0] = 0xD800 | (ch >> 10);
        utf16[1] = 0xDC00 | (ch & 0x03FF);

        ch += 0x10000;
      }

      bool old_boxchar_fallback_enabled = context->boxchar_fallback_enabled;
      context->boxchar_fallback_enabled = false;
      text_shaper->decode_token(
        context,
        ch <= 0xFFFF ? 1 : 2,
        utf16,
        r);
      context->boxchar_fallback_enabled = old_boxchar_fallback_enabled;

      if(r->index != 0
          && r->index != UnknownGlyph) {
        if(style.italic) {
          math_set_style(style - Italic)->decode_token(context, len, str, result);
          result->slant = FontSlantPlain;
          return;
        }

        r[0].missing_after = result->missing_after;
        r[0].style         = result->style;
        memcpy(result, r, sizeof(GlyphInfo));
        return;
      }

      if(!style.italic) {
        math_set_style(style + Italic)->decode_token(context, len, str, result);
        result->slant = FontSlantItalic;
        return;
      }
    }
  }

  while(len > 0) {
    int sub_len = 0;
    int char_len = 1;
    uint16_t *glyph_ptr = 0;
    Array<MathGlyphPartRecord> *lig = 0;

    while(sub_len < len) {
      uint16_t ch;
      char_len = 1;

      if( sub_len + 1 < len &&
          is_utf16_high(str[sub_len]) &&
          is_utf16_low(str[sub_len + 1]))
      {
        uint32_t hi = str[sub_len];
        uint32_t lo = str[sub_len + 1];
        ch = 0x10000 + (((hi & 0x03FF) << 10) | (lo & 0x03FF));

        char_len = 2;
      }
      else
        ch = str[sub_len];

      glyph_ptr = db->private_characters.search(ch);

      if(glyph_ptr)
        break;

      lig = db->private_ligatures.search(ch);
      if(lig)
        break;

      sub_len += char_len;
    }

    if(sub_len > 0) {
      text_shaper->decode_token(
        context,
        sub_len,
        str,
        result);

      str   += sub_len;
      result += sub_len;
      len   -= sub_len;
    }

    if(glyph_ptr) {
      result->index    = *glyph_ptr;
      result->fontinfo = 0;

      if(char_len == 2) {
        result[1].index    = 0;
        result[1].fontinfo = 0;
      }

      if(result->index) {
        context->canvas->set_font_face(font(0));
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = 0;
        cg.y = 0;
        cg.index = result->index;
        context->canvas->glyph_extents(&cg, 1, &cte);

        result->x_offset = 0;
        result->right = cte.x_advance;
      }

      str   += char_len;
      result += char_len;
      len   -= char_len;
    }
    else if(lig) {
      if(char_len == 2) {
        result[1].index    = 0;
        result[1].fontinfo = 0;
      }

      result->index = 0;
      stretch_glyph_assembly(context, 0, lig, result);
//      result->index    = 1;
//      result->fontinfo = 0;
//      result->horizontal_stretch = 1;
//      result->composed           = 1;

      str   += char_len;
      result += char_len;
      len   -= char_len;
    }
    else {
      assert(len == 0);
    }
  }
}

void OTMathShaper::vertical_glyph_size(
  Context         *context,
  const uint16_t   ch,
  const GlyphInfo &info,
  float           *ascent,
  float           *descent
) {
  if(info.fontinfo > 0) {
    text_shaper->vertical_glyph_size(context, ch, info, ascent, descent);
    return;
  }

  if(info.composed) {
    if(style.italic) {
      math_set_style(style - Italic)->vertical_glyph_size(
        context,
        ch,
        info,
        ascent,
        descent);
      return;
    }

    context->canvas->set_font_face(font(0));

    uint16_t glyph = fi.char_to_glyph(ch);

    cairo_text_extents_t cte;
    cairo_glyph_t        cg;
    cg.x = 0;
    cg.y = 0;

    if(info.horizontal_stretch) {
      Array<MathGlyphPartRecord> *ass = db->get_horz_assembly(ch, glyph);

      if(ass) {
        for(int i = 0; i < ass->length(); ++i) {
          cg.index = ass->get(i).glyph;
          context->canvas->glyph_extents(&cg, 1, &cte);

          if(*ascent < -cte.y_bearing)
            *ascent = -cte.y_bearing;
          if(*descent < cte.height + cte.y_bearing)
            *descent = cte.height + cte.y_bearing;
        }

        return;
      }
    }
    else {
      Array<MathGlyphPartRecord> *ass = db->get_vert_assembly(ch, glyph);
      if(ass) {
        float em      = context->canvas->get_font_size();
        float axis    = db->consts.axis_height.value * em / db->units_per_em;
        float overlap = db->min_connector_overlap    * em / db->units_per_em;
        float height = 0;

        for(int i = 0; i < ass->length(); ++i) {
          if(ass->get(i).flags & MGPRF_Extender) {
            height += info.index * (ass->get(i).full_advance * em / db->units_per_em);
            height -= info.index * overlap;
          }
          else {
            height += ass->get(i).full_advance * em / db->units_per_em;
            height -= overlap;
          }
        }

        height += overlap;
        if(*ascent < height / 2 + axis)
          *ascent = height / 2 + axis;
        if(*descent < height / 2 - axis)
          *descent = height / 2 - axis;

        return;
      }
    }
  }

  MathShaper::vertical_glyph_size(context, ch, info, ascent, descent);
}

void OTMathShaper::show_glyph(
  Context         *context,
  float            x,
  float            y,
  const uint16_t   ch,
  const GlyphInfo &info
) {
  if(info.fontinfo > 0) {
    text_shaper->show_glyph(context, x, y, ch, info);
    return;
  }

  if(info.composed) {
    if(style.italic) {
      math_set_style(style - Italic)->show_glyph(
        context,
        x,
        y,
        ch,
        info);
      return;
    }

    context->canvas->set_font_face(font(0));

    uint16_t glyph = fi.char_to_glyph(ch);
    float    em    = context->canvas->get_font_size();
    float overlap  = db->min_connector_overlap * em / db->units_per_em;

    cairo_glyph_t cg;
    cg.x = x + info.x_offset;
    cg.y = y;

    if(info.horizontal_stretch) {
      Array<MathGlyphPartRecord> *ass = db->get_horz_assembly(ch, glyph);
      if(ass) {
        for(int i = 0; i < ass->length(); ++i) {
          cg.index = ass->get(i).glyph;

          if(ass->get(i).flags & MGPRF_Extender) {
            for(uint16_t repeat = info.index; repeat > 0; --repeat) {
              context->canvas->show_glyphs(&cg, 1);
              cg.x += ass->get(i).full_advance * em / db->units_per_em;
              cg.x -= overlap;
            }
          }
          else {
            context->canvas->show_glyphs(&cg, 1);
            cg.x += ass->get(i).full_advance * em / db->units_per_em;
            cg.x -= overlap;
          }
        }

        return;
      }
    }
    else {
      Array<MathGlyphPartRecord> *ass = db->get_vert_assembly(ch, glyph);
      if(ass) {
        float a = 0;
        float d = 0;
        vertical_glyph_size(context, ch, info, &a, &d);

        cg.y += d;
        for(int i = 0; i < ass->length(); ++i) {
          cg.index = ass->get(i).glyph;

          if(ass->get(i).flags & MGPRF_Extender) {
            for(uint16_t repeat = info.index; repeat > 0; --repeat) {
              context->canvas->show_glyphs(&cg, 1);
              cg.y -= ass->get(i).full_advance * em / db->units_per_em;
              cg.y += overlap;
            }
          }
          else {
            context->canvas->show_glyphs(&cg, 1);
            cg.y -= ass->get(i).full_advance * em / db->units_per_em;
            cg.y += overlap;
          }
        }

        return;
      }
    }
  }

  MathShaper::show_glyph(context, x, y, ch, info);
}

bool OTMathShaper::horizontal_stretch_char(
  Context        *context,
  float           width,
  const uint16_t  ch,
  GlyphInfo      *result
) {
  if(style.italic) {
    return math_set_style(style - Italic)->horizontal_stretch_char(
             context,
             width,
             ch,
             result);
  }

  uint16_t glyph = result->index;
  Array<MathGlyphVariantRecord> *var = db->get_horz_variants(ch, glyph);

  if(var) {
    cairo_text_extents_t cte;
    cairo_glyph_t        cg;

    context->canvas->set_font_face(font(0));
    cg.x = 0;
    cg.y = 0;
    result->fontinfo       = 0;
    result->x_offset       = 0;
    result->composed       = 0;
    result->is_normal_text = 0;
    for(int i = 0; i < var->length(); ++i) {
      cg.index = var->get(i).glyph;
      context->canvas->glyph_extents(&cg, 1, &cte);

      result->index = cg.index;
      result->right = cte.x_advance;//var->get(i).advance * em / db->units_per_em;
      if(width <= cte.x_advance)
        return true;
    }
  }

  Array<MathGlyphPartRecord> *ass = db->get_horz_assembly(ch, glyph);
  if(ass) {
    stretch_glyph_assembly(context, width, ass, result);
    return true;
  }
  else if(var)
    return true;

  return false;
}

void OTMathShaper::stretch_glyph_assembly(
  Context                    *context,
  float                       width,
  Array<MathGlyphPartRecord> *parts,
  GlyphInfo                  *result
) {
  float pt = context->canvas->get_font_size() / db->units_per_em;
  int extenders = 0;
  float ext_w = 0;
  float non_ext_w = 0;
  float overlap  = db->min_connector_overlap * pt;

  context->canvas->set_font_face(font(0));
  result->fontinfo           = 0;
  result->x_offset           = 0;
  result->composed           = 1;
  result->horizontal_stretch = 1;
  result->is_normal_text     = 0;

  for(int i = 0; i < parts->length(); ++i) {
    if(parts->get(i).flags & MGPRF_Extender) {
      ++extenders;
      ext_w += parts->get(i).full_advance * pt;
      ext_w -= overlap;
    }
    else {
      non_ext_w += parts->get(i).full_advance * pt;
      non_ext_w -= overlap;
    }
  }

  result->index = 0;
  if(extenders) {
    int count = floorf((width - non_ext_w) / ext_w + 0.5f);

    if(count >= 0) {
      result->index = count;
    }
    else {
      if(result->index) {
        result->composed = 0;
        return;
      }
      result->index = 0;
    }
  }

  result->right = 0;
  for(int i = 0; i < parts->length(); ++i) {
    if(parts->get(i).flags & MGPRF_Extender) {
      result->right += result->index * (parts->get(i).full_advance * pt);
      result->right -= result->index * overlap;
    }
    else {
      result->right += parts->get(i).full_advance * pt;
      result->right -= overlap;
    }
  }

  result->right += overlap;
}

void OTMathShaper::vertical_stretch_char(
  Context        *context,
  float           ascent,
  float           descent,
  bool            full_stretch,
  const uint16_t  ch,
  GlyphInfo      *result
) {
  if(style.italic) {
    math_set_style(style - Italic)->vertical_stretch_char(
      context,
      ascent,
      descent,
      full_stretch,
      ch,
      result);
    return;
  }

  uint16_t glyph = result->index;
  if(!glyph) {
    glyph = fi.char_to_glyph(ch);
  }

  Array<MathGlyphVariantRecord> *var = db->get_vert_variants(ch, glyph);

  float em = context->canvas->get_font_size();
  float axis = (em * db->consts.axis_height.value) / db->units_per_em;

  if(pmath_char_maybe_bigop(ch)) {
    ascent -=  0.1 * em;
    descent -= 0.1 * em;
  }

  float half = ascent - axis;
  if(half < descent + axis)
    half = descent + axis;

  half = floorf(half);
  float max = Infinity;
  if(!full_stretch)
    max = 2 * em;

//  float min = 0;
//  if(context->script_indent == 0
//  && (pmath_char_is_integral(ch) || pmath_char_maybe_bigop(ch))){
//    min = (em * db->consts.display_operator_min_height) / db->units_per_em;
//
//    min = 2 * floorf(min/2);
//  }

  if(var) {
    cairo_text_extents_t cte;
    cairo_glyph_t        cg;

    context->canvas->set_font_face(font(0));
    cg.x = 0;
    cg.y = 0;
    result->fontinfo          = 0;
    result->x_offset          = 0;
    result->composed          = 0;
    result->is_normal_text    = 0;
    result->vertical_centered = 1;

    int i = 0;
    if(context->script_indent == 0) {
      if(pmath_char_is_integral(ch) || pmath_char_maybe_bigop(ch))
        i = 1;
    }

    for(; i < var->length(); ++i) {
      cg.index = var->get(i).glyph;
      context->canvas->glyph_extents(&cg, 1, &cte);

      result->index = cg.index;
      result->right = cte.x_advance;

      if( 2 * half <= cte.height * 1.1 ||
          max < cte.height /*&& min <= cte.height*/)
      {
        return;
      }
    }
  }

  if(!full_stretch)
    return;

  Array<MathGlyphPartRecord> *ass = db->get_vert_assembly(ch, glyph);
  if(ass) {
    int extenders = 0;
    float ext_h = 0;
    float non_ext_h = 0;
    float overlap  = db->min_connector_overlap * em / db->units_per_em;

    context->canvas->set_font_face(font(0));
    result->fontinfo           = 0;
    result->x_offset           = 0;
    result->composed           = 1;
    result->horizontal_stretch = 0;
    result->is_normal_text     = 0;
    result->vertical_centered  = 0;

    for(int i = 0; i < ass->length(); ++i) {
      if(ass->get(i).flags & MGPRF_Extender) {
        ++extenders;
        ext_h += ass->get(i).full_advance * em / db->units_per_em;
        ext_h -= overlap;
      }
      else {
        non_ext_h += ass->get(i).full_advance * em / db->units_per_em;
        non_ext_h -= overlap;
      }
    }

    if(extenders) {
      int count = floorf((2 * half - non_ext_h) / ext_h + 0.5f);

      if(count * ext_h + non_ext_h <= 2 * half - 0.5 * em)
        count += 1;

      if(count >= 0) {
        result->index = count;
      }
      else {
        if(var) {
          result->composed = 0;
          return;
        }
        result->index = 0;
      }
    }

    result->right = 0;
    context->canvas->set_font_face(font(0));
    cairo_text_extents_t cte;
    cairo_glyph_t        cg;
    cg.x = 0;
    cg.y = 0;
    for(int i = 0; i < ass->length(); ++i) {
      cg.index = ass->get(i).glyph;
      context->canvas->glyph_extents(&cg, 1, &cte);

      if(result->right < cte.x_advance)
        result->right = cte.x_advance;
    }
  }
}

void OTMathShaper::accent_positions(
  Context           *context,
  MathSequence      *base,
  MathSequence      *under,
  MathSequence      *over,
  float             *base_x,
  float             *under_x,
  float             *under_y,
  float             *over_x,
  float             *over_y
) {
  float pt = context->canvas->get_font_size() / db->units_per_em;
  uint16_t base_char = 0;
  if(base->length() == 1)
    base_char = base->text()[0];

  bool is_integral = pmath_char_is_integral(base_char);

  // actually do subscript/superscript
  if(context->script_indent > 0 && is_integral) {
    script_positions(
      context, base->extents().ascent, base->extents().descent,
      under, over,
      under_y, over_y);

    script_corrections(
      context, base_char, base->glyph_array()[0],
      under, over, *under_y, *over_y,
      under_x, over_x);

    *base_x = 0;
    *under_x += base->extents().width;
    *over_x += base->extents().width;
    return;
  }

  *under_y = base->extents().descent;
  if(under) {
    float gap = db->consts.lower_limit_gap_min.value * pt;

    if(*under_y < db->consts.lower_limit_baseline_drop_min.value * pt)
      *under_y  = db->consts.lower_limit_baseline_drop_min.value * pt;

    if(*under_y < base->extents().descent + gap + under->extents().ascent)
      *under_y = base->extents().descent + gap + under->extents().ascent;
  }

  *over_y = -base->extents().ascent;
  if(over) {
    float gap = db->consts.upper_limit_gap_min.value * pt;

    if(over->extents().descent < 0)
      *over_y -= over->extents().descent;

    if(*over_y > -db->consts.upper_limit_baseline_rise_min.value * pt)
      *over_y  = -db->consts.upper_limit_baseline_rise_min.value * pt;

    if(*over_y > -base->extents().ascent - gap - over->extents().descent)
      *over_y  = -base->extents().ascent - gap - over->extents().descent;
  }

  float w = base->extents().width;
  if(under && w < under->extents().width)
    w = under->extents().width;
  if(over && w < over->extents().width)
    w = over->extents().width;

  *base_x = (w - base->extents().width) / 2;

  if(is_integral) {
    float dummy_uy, dummy_oy;
    script_positions(
      context, base->extents().ascent, base->extents().descent,
      under, over,
      &dummy_uy, &dummy_oy);

    script_corrections(
      context, base_char, base->glyph_array()[0],
      under, over, dummy_uy, dummy_oy,
      under_x, over_x);

    float diff = *over_x - *under_x;

    if(under)
      *under_x = (w + *over_x - diff - under->extents().width) / 2;

    if(over)
      *over_x = (w + *over_x + diff - over->extents().width) / 2;

    return;
  }

  if(under)
    *under_x = (w - under->extents().width) / 2;
  else
    *under_x = 0;

  if(over) {
    if(base_char && over->length() == 1) {
      int16_t *val = db->top_accents.search(base->glyph_array()[0].index);

      float base_acc;
      if(val)
        base_acc = *val * pt;
      else
        base_acc = base->extents().width / 2;

      float over_acc;
      if(over->length() == 1) {
        val = db->top_accents.search(over->glyph_array()[0].index);
        if(val && *val < 0)
          over_acc = *val * pt;
        else
          over_acc = over->extents().width / 2;
      }
      else
        over_acc = over->extents().width / 2;

      *over_x = *base_x + base_acc - over_acc;
    }
    else
      *over_x = (w - over->extents().width) / 2;
  }
  else
    *over_x = 0;
}

void OTMathShaper::script_positions(
  Context           *context,
  float              base_ascent,
  float              base_descent,
  MathSequence      *sub,
  MathSequence      *super,
  float             *sub_y,
  float             *super_y
) {
  float pt = context->canvas->get_font_size() / db->units_per_em;

  *sub_y = db->consts.subscript_shift_down.value * pt;
  if(*sub_y < base_descent + db->consts.subscript_baseline_drop_min.value * pt)
    *sub_y = base_descent + db->consts.subscript_baseline_drop_min.value * pt;

  if(sub) {
    if(*sub_y < -db->consts.subscript_top_max.value * pt + sub->extents().ascent)
      *sub_y = -db->consts.subscript_top_max.value * pt + sub->extents().ascent;
  }

  if(context->script_indent > 0)
    *super_y = -db->consts.superscript_shift_up_cramped.value * pt;
  else
    *super_y = -db->consts.superscript_shift_up.value * pt;

  if(*super_y > -base_ascent + db->consts.superscript_baseline_drop_max.value * pt)
    *super_y = -base_ascent + db->consts.superscript_baseline_drop_max.value * pt;

  if(super) {
    if(*super_y > -db->consts.superscript_bottom_min.value * pt - super->extents().descent)
      *super_y = -db->consts.superscript_bottom_min.value * pt - super->extents().descent;
  }

  if(sub && super) {
    float gap = *sub_y - *super_y - sub->extents().ascent - super->extents().descent;
    if(gap < db->consts.sub_superscript_gap_min.value * pt) {
      gap = db->consts.sub_superscript_gap_min.value * pt - gap;

      *super_y -= gap;
      float max_super_bottom = -db->consts.superscript_bottom_max_with_subscript.value * pt;
      if(*super_y + super->extents().descent < max_super_bottom) {
        gap = max_super_bottom - *super_y - super->extents().descent;

        *super_y = max_super_bottom - super->extents().descent;
        *sub_y += gap;
      }
    }
  }
}

void OTMathShaper::script_corrections(
  Context           *context,
  uint16_t           base_char,
  const GlyphInfo   &base_info,
  MathSequence      *sub,
  MathSequence      *super,
  float              sub_y,
  float              super_y,
  float             *sub_x,
  float             *super_x
) {
  KernVertexObject *kern;
  float pt = context->canvas->get_font_size() / db->units_per_em;

  *sub_x = *super_x = 0;

  if(base_info.composed) {
    if(base_info.horizontal_stretch)
      return;

    uint16_t glyph = fi.char_to_glyph(base_char);

    Array<MathGlyphPartRecord> *ass = db->get_vert_assembly(base_char, glyph);
    if(ass && ass->length() > 0) {
      cairo_text_extents_t  cte;

      cairo_glyph_t cg;
      cg.x = 0;
      cg.y = 0;

      // bottom glyph
      cg.index = ass->get(0).glyph;
      context->canvas->glyph_extents(&cg, 1, &cte);

      *sub_x = cte.x_bearing + cte.width - cte.x_advance;

      // top glyph
      cg.index = ass->get(ass->length() - 1).glyph;
      context->canvas->glyph_extents(&cg, 1, &cte);

      *super_x = cte.x_bearing + cte.width - cte.x_advance;
    }
    return;
  }


  float sub_a = 0;
  if(sub)
    sub_a = sub->extents().ascent;

  kern = db->math_kern[MKE_BOTTOM_RIGHT].search(base_info.index);
  if(kern) {
    *sub_x = pt * kern->height_to_kern(
               (int16_t)((sub_a - sub_y) / pt),
               false);
  }
  else {
    *sub_x = -db->italics_correction[base_info.index] * pt;
  }


  float super_d = 0;
  if(super)
    super_d = super->extents().descent;

  kern = db->math_kern[MKE_TOP_RIGHT].search(base_info.index);
  if(kern) {
    *super_x = pt * kern->height_to_kern(
                 (int16_t)((-super_y - super_d) / pt),
                 true);
  }
}

void OTMathShaper::shape_fraction(
  Context        *context,
  const BoxSize  &num,
  const BoxSize  &den,
  float          *num_y,
  float          *den_y,
  float          *width
) {
  float rule_y1, rule_y2, num_gap, den_gap;
  float pt = context->canvas->get_font_size() / db->units_per_em;

  rule_y1 = rule_y2 = - db->consts.axis_height.value * pt;
  rule_y1 -= db->consts.fraction_rule_thickness.value * pt / 2;
  rule_y2 += db->consts.fraction_rule_thickness.value * pt / 2;

  if(context->script_indent > 0) {
    num_gap = db->consts.fraction_numerator_gap_min.value   * pt;
    den_gap = db->consts.fraction_denominator_gap_min.value * pt;

    *num_y = -db->consts.fraction_numerator_shift_up.value     * pt;
    *den_y =  db->consts.fraction_denominator_shift_down.value * pt;
  }
  else {
    num_gap = db->consts.fraction_num_display_style_gap_min.value   * pt;
    den_gap = db->consts.fraction_denom_display_style_gap_min.value * pt;

    *num_y = -db->consts.fraction_numerator_display_style_shift_up.value     * pt;
    *den_y =  db->consts.fraction_denominator_display_style_shift_down.value * pt;
  }

  if(*num_y > rule_y1 - num_gap - num.descent)
    *num_y = rule_y1 - num_gap - num.descent;

  if(*den_y < rule_y2 + den_gap + den.ascent)
    *den_y = rule_y2 + den_gap + den.ascent;

  if(num.width > den.width)
    *width = num.width + 0.2f * pt * db->units_per_em;
  else
    *width = den.width + 0.2f * pt * db->units_per_em;
}

void OTMathShaper::show_fraction(
  Context        *context,
  float           width
) {
  float pt = context->canvas->get_font_size() / db->units_per_em;
  float x1, y1, x2, y2;
  context->canvas->current_pos(&x1, &y1);

  y2 = y1 -= db->consts.axis_height.value * pt;
  y1 -= db->consts.fraction_rule_thickness.value * pt / 2;
  y2 += db->consts.fraction_rule_thickness.value * pt / 2;

  x2 = x1 + width;

  context->canvas->align_point(&x1, &y1, false);
  context->canvas->align_point(&x2, &y2, false);

  bool sot = context->canvas->show_only_text;
  context->canvas->show_only_text = false;
  if(y1 != y2) {
    context->canvas->move_to(x1, y1);
    context->canvas->line_to(x2, y1);
    context->canvas->line_to(x2, y2);
    context->canvas->line_to(x1, y2);
    context->canvas->fill();
  }
  else {
    y2 += 0.75;
    context->canvas->move_to(x1, y1);
    context->canvas->line_to(x2, y1);
    context->canvas->line_to(x2, y2);
    context->canvas->line_to(x1, y2);
    context->canvas->fill();
  }
  context->canvas->show_only_text = sot;
}

float OTMathShaper::italic_correction(
  Context          *context,
  uint16_t          ch,
  const GlyphInfo  &info
) {
  if(info.slant == FontSlantItalic || style.italic) {
    float em = context->canvas->get_font_size();
    return db->italics_correction[info.index] * em / db->units_per_em;
  }

  return 0;
}

void OTMathShaper::shape_radical(
  Context          *context,    // in
  BoxSize          *box,        // in/out
  float            *radicand_x, // out
  float            *exponent_x, // out
  float            *exponent_y, // out
  RadicalShapeInfo *info        // out
) {
  if(style.italic) {
    math_set_style(style - Italic)->shape_radical(
      context,
      box,
      radicand_x,
      exponent_x,
      exponent_y,
      info);
    return;
  }

  context->canvas->set_font_face(font(0));
  cairo_text_extents_t cte;
  cairo_glyph_t        cg;
  cg.x = 0;
  cg.y = 0;

  float em = context->canvas->get_font_size();
  float pt = em / db->units_per_em;
  float axis = db->consts.axis_height.value * pt;

  float gap_and_rule = db->consts.radical_rule_thickness.value * pt;
  if(context->script_indent > 0)
    gap_and_rule += db->consts.radical_vertical_gap.value * pt;
  else
    gap_and_rule += db->consts.radical_display_style_vertical_gap.value * pt;

  float height = box->height();// + gap_and_rule;

  info->hbar = (int)ceilf(box->width + 0.2f * em);
  *exponent_x = db->consts.radical_kern_after_degree.value * pt;
  *exponent_y = db->consts.radical_degree_bottom_raise_percent / 100.0f;

  uint16_t glyph = fi.char_to_glyph(SqrtChar);
  Array<MathGlyphVariantRecord> *var = db->get_vert_variants(SqrtChar, glyph);
  if(var) {
    for(int i = 0; i < var->length(); ++i) {
      cg.index = var->get(i).glyph;
      context->canvas->glyph_extents(&cg, 1, &cte);

      if(height + gap_and_rule <= cte.height) {
        info->y_offset = 0;
        info->size = -(int)cg.index;
        *radicand_x = cte.x_advance;
        *exponent_x += *radicand_x;

        box->descent = (cte.height - gap_and_rule) / 2 - axis;
        if(box->ascent + box->descent > cte.height - gap_and_rule)
          box->descent = cte.height - gap_and_rule - box->ascent;

        info->y_offset = box->descent - (cte.height + cte.y_bearing);
        *exponent_y = -*exponent_y * cte.height + box->descent;

        box->ascent = cte.height - box->descent;
        box->ascent += db->consts.radical_extra_ascender.value * pt;
        box->width = info->hbar + *radicand_x;
        return;
      }
    }
  }

  Array<MathGlyphPartRecord> *ass = db->get_vert_assembly(SqrtChar, glyph);
  if(ass) {
    int extenders = 0;
    float ext_h = 0;
    float non_ext_h = 0;
    float overlap  = db->min_connector_overlap * pt;

    for(int i = 0; i < ass->length(); ++i) {
      if(ass->get(i).flags & MGPRF_Extender) {
        ++extenders;
        ext_h += ass->get(i).full_advance * pt;
        ext_h -= overlap;
      }
      else {
        non_ext_h += ass->get(i).full_advance * pt;
        non_ext_h -= overlap;
      }
    }

    if(extenders) {
      int count = ceilf((height - non_ext_h) / ext_h);
      if(count >= 0)
        info->size = count;
      else
        info->size = 0;
    }

    height = 0;
    *radicand_x = 0;
    for(int i = 0; i < ass->length(); ++i) {
      cg.index = ass->get(i).glyph;
      context->canvas->glyph_extents(&cg, 1, &cte);

      if(i == 0)
        *exponent_y = -/*-*exponent_y * */cte.height;

      if(ass->get(i).flags & MGPRF_Extender) {
        height += info->size * (ass->get(i).full_advance * pt);
        height -= info->size * overlap;
      }
      else {
        height += ass->get(i).full_advance * pt;
        height -= overlap;
      }

      if(*radicand_x < cte.x_advance)
        *radicand_x = cte.x_advance;
    }

    height += overlap;
    height += db->consts.radical_extra_ascender.value * pt;
    box->ascent = height - box->descent;
    box->width = info->hbar + *radicand_x;
    info->y_offset = box->descent - height / 2 + axis;
    *exponent_x += *radicand_x;
    *exponent_y += box->descent;
  }
}

void OTMathShaper::show_radical(
  Context                *context,
  const RadicalShapeInfo &info
) {
  float x, y;
  context->canvas->current_pos(&x, &y);

  GlyphInfo gi;
  memset(&gi, 0, sizeof(gi));

  if(info.size > 0) {
    gi.composed = 1;
    gi.index    = info.size;
  }
  else {
    gi.index = -info.size;
  }

  float a = 0;
  float d = 0;
  vertical_glyph_size(context, SqrtChar, gi, &a, &d);
  y += info.y_offset - a;
  context->canvas->align_point(&x, &y, false);
  y -= info.y_offset - a;

  show_glyph(context, x, y + info.y_offset, SqrtChar, gi);

  context->canvas->set_font_face(font(0));
  cairo_text_extents_t cte;
  cairo_glyph_t        cg;
  cg.x = 0;
  cg.y = 0;

  float em = context->canvas->get_font_size();
  float overlap  = db->min_connector_overlap * em / db->units_per_em;
  float x1, y1, x2, y2;
  y1 = y + info.y_offset - a;
  if(gi.composed) {
    x1 = 0;
    Array<MathGlyphPartRecord> *ass = db->get_vert_assembly(SqrtChar, fi.char_to_glyph(SqrtChar));
    if(ass) {
      for(int i = 0; i < ass->length(); ++i) {
        cg.index = ass->get(i).glyph;
        context->canvas->glyph_extents(&cg, 1, &cte);

        if(x1 < cte.x_advance)
          x1 = cte.x_advance;
      }
    }

    x1 += x;
  }
  else {
    cg.index = gi.index;
    context->canvas->glyph_extents(&cg, 1, &cte);

    x1 = x + cte.x_advance;
  }

  x2 = x1 + info.hbar;
  if(gi.composed)
    x1 -= overlap;
  y2 = y1 + db->consts.radical_rule_thickness.value * em / db->units_per_em;

  bool sot = context->canvas->show_only_text;
  context->canvas->show_only_text = false;
  context->canvas->move_to(x1, y1);
  context->canvas->line_to(x2, y1);
  context->canvas->line_to(x2, y2);
  context->canvas->line_to(x1, y2);
  context->canvas->fill();
  context->canvas->show_only_text = sot;
}

void OTMathShaper::get_script_size_multis(Array<float> *arr) {
  arr->length(2);

  float sm1 = db->consts.script_percent_scale_down        / 100.0f;
  float sm2 = db->consts.script_script_percent_scale_down / 100.0f;

  arr->set(0, sm1);
  arr->set(1, sm1 > 0 ? sm2 / sm1 : sm2);
}

SharedPtr<TextShaper> OTMathShaper::set_style(FontStyle _style) {
  return db->find(_style);
}

float OTMathShaper::get_center_height(Context *context, uint8_t fontinfo) {
  if(fontinfo > 0)
    return text_shaper->get_center_height(context, fontinfo - 1);

  float em = context->canvas->get_font_size();
  return db->consts.axis_height.value * em / db->units_per_em;
}

//} ... class OTMathShaper
