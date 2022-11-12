#include <graphics/config-shaper.h>

#include <cstdio>

#include <boxes/mathsequence.h>

#include <graphics/context.h>
#include <graphics/ot-math-shaper.h>

#include <util/style.h>

#include <syntax/syntax-state.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_Alternatives;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Rule;

namespace {
  static uint8_t expr_to_ui8(const Expr expr, uint8_t def = 0) {
    if(expr.is_int32() && PMATH_AS_INT32(expr.get()) >= 0) {
      unsigned res = PMATH_AS_INT32(expr.get());
      
      if((res & 0xFF) == res)
        return res;
    }
    
    return def;
  }
  
  static uint16_t expr_to_ui16(const Expr expr, uint16_t def = 0) {
    if(expr.is_int32() && PMATH_AS_INT32(expr.get()) >= 0) {
      unsigned res = PMATH_AS_INT32(expr.get());
      
      if((res & 0xFFFF) == res)
        return res;
    }
    
    return def;
  }
  
  static uint32_t expr_to_char(const Expr expr) {
    if(expr.is_string()) {
      String s(expr);
      
      if(s.length() == 1)
        return s[0];
        
      if(s.length() == 2) {
        uint32_t hi = s[0];
        uint32_t lo = s[1];
        
        if(is_utf16_high(hi)
            && is_utf16_low(lo))
          return 0x10000 + (((hi & 0x03FF) << 10) | (lo & 0x03FF));
      }
    }
    
    return -(uint32_t)1;
  }
  
  static uint16_t default_vertical_composed_glyphs[11][5] = {
    // {char, top, bottom, middle, special_center}
    {'(',                  0x239B, 0x239D, 0x239C, 0},
    {')',                  0x239E, 0x23A0, 0x239F, 0},
    {'[',                  0x23A1, 0x23A3, 0x23A2, 0},
    {']',                  0x23A4, 0x23A6, 0x23A5, 0},
    {PMATH_CHAR_PIECEWISE, 0x23A7, 0x23A9, 0x23AA, 0x23A8},
    {'{',                  0x23A7, 0x23A9, 0x23AA, 0x23A8},
    {'}',                  0x23AB, 0x23AD, 0x23AA, 0x23AC},
    {0x2308,               0x23A1, 0x23A2, 0x23A2, 0}, // left ceiling
    {0x2309,               0x23A4, 0x23A5, 0x23A5, 0}, // right ceiling
    {0x230A,               0x23A2, 0x23A3, 0x23A2, 0}, // left floor
    {0x230B,               0x23A5, 0x23A6, 0x23A5, 0}  // right floor
  };
  
  class GlyphGetter: public Base {
    public:
      GlyphGetter()
        : Base()
      {
        SET_BASE_DEBUG_TAG(typeid(*this).name());
        
        surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 1, 1);
        cr = cairo_create(surface);
        
        context.canvas_ptr = new Canvas(cr);
      }
      
      ~GlyphGetter() {
        delete context.canvas_ptr;
        
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
      }
      
      uint16_t expr_to_glyph(const Expr expr, uint8_t font) {
        if(expr.is_expr() && expr[0] == richmath_System_Alternatives) {
          for(size_t i = 1; i <= expr.expr_length(); ++i) {
            if(uint16_t res = expr_to_glyph(expr[i], font))
              return res;
          }
          
          return 0;
        }
        
        if(expr.is_string()) {
          uint16_t res = ps2g[font & (NumFontsPerGlyph - 1)][String(expr)];
          
//        if(!res){
//          pmath_debug_print_object("Unknown glyph ", expr.get(), "");
//          pmath_debug_print(" in font %d\n", font+1);
//        }

          return res;
        }
        
        return expr_to_ui16(expr);
      }
      
      void clear() {
        for(int i = 0; i < NumFontsPerGlyph; ++i)
          ps2g[i].clear();
      }
      
    public:
      Hashtable<String, uint16_t> ps2g[NumFontsPerGlyph];
      cairo_surface_t *surface;
      cairo_t         *cr;
      Context          context;
      
//    Array<SharedPtr<TextShaper> > shapers;
  };
  
  static GlyphGetter GG;
  
  class GlyphFontOffset {
    public:
      GlyphFontOffset(
        uint16_t g = 0,
        uint8_t  f = 0,
        int8_t   o = 0)
        : glyph(g), font(f), offset(o)
      {
      }
      
      explicit GlyphFontOffset(Expr expr)
        : glyph(0), font(0), offset(0)
      {
        if(expr[0] == richmath_System_List) {
          if(expr.expr_length() == 2) {
            font  = expr_to_ui8(expr[2]) - 1;
            glyph = GG.expr_to_glyph(expr[1], font);
          }
          else if(expr.expr_length() == 3) {
            font  = expr_to_ui8(expr[2]) - 1;
            glyph = GG.expr_to_glyph(expr[1], font);
            
            float o = expr[3].to_double();
            offset = (uint8_t)(o / EmPerOffset + 0.5f);
          }
        }
        else {
          glyph = GG.expr_to_glyph(expr, font);
        }
      }
      
    public:
      uint16_t glyph;
      uint8_t  font;
      int8_t   offset;
      
      static const float EmPerOffset;
  };
  
  const float GlyphFontOffset::EmPerOffset = 1 / 72.0f;
  
  class ScriptIndent {
    public:
      int8_t super;
      int8_t sub;
      int8_t center;
      
      ScriptIndent(int8_t _super = 0, int8_t _sub = 0, int8_t _center = 0)
        : super(_super), sub(_sub), center(_center)
      {
      }
      
      explicit ScriptIndent(Expr expr)
        : super(0), sub(0), center(0)
      {
        float f;
        
        if(expr[0] == richmath_System_List) {
          if(expr.expr_length() == 2) {
            f = expr[1].to_double();
            super = (uint8_t)(f / GlyphFontOffset::EmPerOffset + 0.5f);
            
            f = expr[2].to_double();
            sub = (uint8_t)(f / GlyphFontOffset::EmPerOffset + 0.5f);
          }
          else if(expr.expr_length() == 3) {
            f = expr[1].to_double();
            super = (uint8_t)(f / GlyphFontOffset::EmPerOffset + 0.5f);
            
            f = expr[2].to_double();
            sub = (uint8_t)(f / GlyphFontOffset::EmPerOffset + 0.5f);
            
            f = expr[3].to_double();
            center = (uint8_t)(f / GlyphFontOffset::EmPerOffset + 0.5f);
          }
        }
      }
  };
  
  class StretchGlyphArray {
    public:
      Array<uint16_t>  glyphs;
      Array<uint8_t>   fonts;
      bool             vertical;
  };
  
  class ComposedGlyph {
    public:
      ComposedGlyph()
        : top(0),
          bottom(0),
          middle(0),
          special_center(0),
          upper(0),
          lower(0),
          tbms_font(0),
          ul_font(0),
          vertical(false)
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
  
  class BigRadicalGlyph {
    public:
      uint16_t  bottom;
      uint16_t  vertical;
      uint16_t  edge;
      uint16_t  horizontal;
      Vector2F  rel_exponent_offset;
  };
  
  class RadicalGlyphs {
    public:
      Array<SmallRadicalGlyph>  small_glyphs;
      BigRadicalGlyph           big_glyph;
      uint8_t                   font;
  };
}

namespace richmath {
  class ConfigShaperTables: public Shareable {
    public:
      Hashtable<uint32_t, GlyphFontOffset>
      char_to_glyph_map;
      
      Hashtable<String, Array<GlyphFontOffset> >
      ligatures;
      
      Hashtable<uint32_t, Array<GlyphFontOffset>>
      complex_glyphs;
      
      Hashtable<uint32_t, StretchGlyphArray>
      stretched_glyphs;
      
      Hashtable<uint32_t, ComposedGlyph>
      composed_glyphs;
      
      Hashtable<uint32_t, ScriptIndent>
      script_indents;
      
      Array<float> script_size_multipliers;
      
      ScriptIndent italic_script_indent;
      
      RadicalGlyphs radical;
      
      Array<String> math_fontnames;
      Array<String> text_fontnames;
      String shaper_name;
      
    public:
      static SharedPtr<ConfigShaperTables> try_load_from_object(const Expr expr, FontStyle style) {
        SharedPtr<ConfigShaperTables> tables = new ConfigShaperTables();
        
        GG.clear();
        for(size_t i = 1; i <= expr.expr_length(); ++i) {
          if(expr[i].is_rule()) {
            String lhs = expr[i][1];
            Expr   rhs = expr[i][2];
            
            if(lhs.equals("MathFonts")) {
              if(rhs.is_string()) {
                tables->math_fontnames.length(1);
                tables->math_fontnames[0] = find_font(rhs);
                if(tables->math_fontnames[0].length() == 0)
                  return nullptr;
              }
              
              if(rhs[0] == richmath_System_List) {
                tables->math_fontnames.length(rhs.expr_length());
                
                for(int j = 0; j < tables->math_fontnames.length(); ++j) {
                  tables->math_fontnames[j] = find_font(rhs[j + 1]);
                  if(tables->math_fontnames[j].length() == 0)
                    return nullptr;
                }
              }
              
              continue;
            }
            
            if(lhs.equals("TextFonts")) {
              if(rhs.is_string()) {
                tables->text_fontnames.length(1);
                tables->text_fontnames[0] = find_font(rhs);
                if(tables->math_fontnames[0].length() == 0)
                  return nullptr;
              }
              
              if(rhs[0] == richmath_System_List) {
                tables->text_fontnames.length(rhs.expr_length());
                
                for(int j = 0; j < tables->text_fontnames.length(); ++j) {
                  tables->text_fontnames[j] = find_font(rhs[j + 1]);
                  if(tables->math_fontnames[j].length() == 0)
                    return nullptr;
                }
              }
              
              continue;
            }
            
            if(lhs.equals("RadicalFont")) {
              tables->radical.font = expr_to_ui8(rhs) - 1;
              
              continue;
            }
            
            if(lhs.equals("ScriptSizeMultipliers")) {
              if(rhs[0] == richmath_System_List) {
                tables->script_size_multipliers.length(rhs.expr_length());
                
                for(int i = 0; i < tables->script_size_multipliers.length(); ++i) {
                  tables->script_size_multipliers[i] = rhs[i + 1].to_double();
                }
              }
              else
                tables->script_size_multipliers.length(1, rhs.to_double());
                
              continue;
            }
            
            if(lhs.equals("SmallRadical")) {
              if(rhs[0] == richmath_System_List) {
                tables->radical.small_glyphs.length(rhs.expr_length() + 1);
                
                for(int j = 0; j < tables->radical.small_glyphs.length() - 1; ++j) {
                  Expr g = rhs[j + 1];
                  
                  if(g[0] == richmath_System_List && g.expr_length() == 5) {
                    tables->radical.small_glyphs[j].index          = GG.expr_to_glyph(g[1], tables->radical.font);
                    tables->radical.small_glyphs[j].hbar_index     = GG.expr_to_glyph(g[2], tables->radical.font);
                    tables->radical.small_glyphs[j].rel_ascent     = g[3].to_double();
                    tables->radical.small_glyphs[j].rel_exp_offset = Vector2F(g[4].to_double(), g[5].to_double());
                  }
                  else
                    break;
                }
                
                tables->radical.small_glyphs[tables->radical.small_glyphs.length() - 1] =
                  SmallRadicalGlyph(0, 0, 0, Vector2F());
              }
              
              continue;
            }
            
            if(lhs.equals("BigRadical")) {
              if(rhs[0] == richmath_System_List && rhs.expr_length() == 6) {
                tables->radical.big_glyph.bottom              = GG.expr_to_glyph(rhs[1], tables->radical.font);
                tables->radical.big_glyph.vertical            = GG.expr_to_glyph(rhs[2], tables->radical.font);
                tables->radical.big_glyph.edge                = GG.expr_to_glyph(rhs[3], tables->radical.font);
                tables->radical.big_glyph.horizontal          = GG.expr_to_glyph(rhs[4], tables->radical.font);
                tables->radical.big_glyph.rel_exponent_offset = Vector2F(rhs[5].to_double(), rhs[6].to_double());
              }
              
              continue;
            }
            
            if(lhs.equals("Glyphs")) {
              if(rhs[0] == richmath_System_List) {
                for(size_t j = rhs.expr_length(); j > 0; --j) {
                  Expr rule = rhs[j];
                  
                  if(rule.is_rule()) {
                    GlyphFontOffset gfo(rule[2]);
                    
                    if(gfo.glyph) {
                      tables->char_to_glyph_map.set(
                        expr_to_char(rule[1]),
                        gfo);
                    }
                  }
                }
              }
              
              continue;
            }
            
            if(lhs.equals("Ligatures")) {
              if(rhs[0] == richmath_System_List) {
                for(size_t j = rhs.expr_length(); j > 0; --j) {
                  Expr rule = rhs[j];
                  
                  if( rule[0] == richmath_System_Rule && 
                      rule.expr_length() == 2 && 
                      rule[2][0] == richmath_System_List)
                  {
                    Expr v = rule[2];
                    
                    Array<GlyphFontOffset> arr(v.expr_length());
                    
                    for(int j = 0; j < arr.length(); ++j)
                      arr[j] = GlyphFontOffset(v[j + 1]);
                      
                    uint32_t ch = expr_to_char(rule[1]);
                    if(ch + 1 == 0)
                      tables->ligatures.set(rule[1], arr);
                    else
                      tables->complex_glyphs.set(ch, arr);
                  }
                }
              }
              
              continue;
            }
            
            if( lhs.equals("VerticalStretchedGlyphes") || 
                lhs.equals("HorizontalStretchedGlyphes"))
            {
              StretchGlyphArray sga;
              sga.vertical = lhs[0] == 'V';
              
              if(rhs[0] == richmath_System_List) {
                for(size_t j = rhs.expr_length(); j > 0; --j) {
                  Expr rule = rhs[j];
                  
                  if( rule.is_rule() &&
                      rule[2][0] == richmath_System_List)
                  {
                    Expr list = rule[2];
                    
                    sga.glyphs.length(list.expr_length());
                    sga.fonts.length(sga.glyphs.length(), 0);
                    for(int k = 0; k < sga.glyphs.length(); ++k) {
                      if(list[k + 1].expr_length() == 2
                          && list[k + 1][0] == richmath_System_List) {
                        sga.fonts[k]  = expr_to_ui8(list[k + 1][2]) - 1;
                        sga.glyphs[k] = GG.expr_to_glyph(list[k + 1][1], sga.fonts[k]);
                      }
                      else {
                        sga.glyphs[k] = GG.expr_to_glyph(list[k + 1], 0);
                      }
                    }
                    
                    tables->stretched_glyphs.set(expr_to_char(rule[1]), sga);
                  }
                }
              }
              
              continue;
            }
            
            if(lhs.equals("HorizontalComposedGlyphs")) {
              ComposedGlyph cg;
              cg.vertical = false;
              
              if(rhs[0] == richmath_System_List) {
                for(size_t j = rhs.expr_length(); j > 0; --j) {
                  Expr rule = rhs[j];
                  
                  if( rule.is_rule() &&
                      rule[2][0] == richmath_System_List &&
                      rule[2].expr_length() == 2)
                  {
                    Expr list = rule[2];
                    Expr gs   = list[1];
                    
                    cg.tbms_font = expr_to_ui8(list[2]) - 1;
                    
                    if(gs[0] == richmath_System_List) {
                      switch(gs.expr_length()) {
                        case 1:
                          cg.top = cg.bottom = cg.special_center = 0;
                          cg.middle = GG.expr_to_glyph(gs[1], cg.tbms_font);
                          break;
                          
                        case 2:
                          cg.middle = cg.special_center = 0;
                          cg.top    = GG.expr_to_glyph(gs[1], cg.tbms_font);
                          cg.bottom = GG.expr_to_glyph(gs[2], cg.tbms_font);
                          break;
                          
                        case 3:
                          cg.special_center = 0;
                          cg.top    = GG.expr_to_glyph(gs[1], cg.tbms_font);
                          cg.bottom = GG.expr_to_glyph(gs[2], cg.tbms_font);
                          cg.middle = GG.expr_to_glyph(gs[3], cg.tbms_font);
                          break;
                          
                        case 4:
                          cg.top            = GG.expr_to_glyph(gs[1], cg.tbms_font);
                          cg.bottom         = GG.expr_to_glyph(gs[2], cg.tbms_font);
                          cg.middle         = GG.expr_to_glyph(gs[3], cg.tbms_font);
                          cg.special_center = GG.expr_to_glyph(gs[4], cg.tbms_font);
                          break;
                      }
                    }
                    
                    tables->composed_glyphs.set(
                      expr_to_char(rule[1]),
                      cg);
                  }
                }
              }
              
              continue;
            }
            
            if(lhs.equals("VerticalComposedGlyphs")) {
              ComposedGlyph cg;
              cg.vertical = true;
              
              if(rhs[0] == richmath_System_List) {
                for(size_t j = rhs.expr_length(); j > 0; --j) {
                  Expr rule = rhs[j];
                  
                  if( rule.is_rule() &&
                      rule[2][0] == richmath_System_List &&
                      rule[2].expr_length() == 4)
                  {
                    Expr list = rule[2];
                    Expr gs   = list[1];
                    
                    cg.tbms_font = expr_to_ui8(list[3]) - 1;
                    cg.ul_font   = expr_to_ui8(list[4]) - 1;
                    
                    if(gs[0] == richmath_System_List) {
                      cg.top            = GG.expr_to_glyph(gs[1], cg.tbms_font);
                      cg.bottom         = GG.expr_to_glyph(gs[2], cg.tbms_font);
                      cg.middle         = GG.expr_to_glyph(gs[3], cg.tbms_font);
                      cg.special_center = GG.expr_to_glyph(gs[4], cg.tbms_font);
                    }
                    
                    gs = list[2];
                    if(gs[0] == richmath_System_List) {
                      cg.upper = GG.expr_to_glyph(gs[1], cg.ul_font);
                      cg.lower = GG.expr_to_glyph(gs[2], cg.ul_font);
                    }
                    else {
                      cg.upper = 0;
                      cg.lower = 0;
                    }
                    
                    tables->composed_glyphs.set(
                      expr_to_char(rule[1]),
                      cg);
                  }
                }
              }
              
              continue;
            }
            
            if(lhs.equals("ScriptIndent")) {
              if(rhs[0] == richmath_System_List) {
                for(size_t j = rhs.expr_length(); j > 0; --j) {
                  Expr rule = rhs[j];
                  
                  if(rule.is_rule()) {
                    uint32_t key = 0;
                    Expr lhs = rule[1];
                    
                    if(lhs.is_string()) {
                      if(String(lhs).equals("Italic")) {
                        tables->italic_script_indent = ScriptIndent(rule[2]);
                        continue;
                      }
                      
                      key = expr_to_char(lhs) | (1 << 31);
                    }
                    else if(lhs[0] == richmath_System_List) {
                      if(lhs.expr_length() == 1 || lhs[2].is_string()) {
                        key = expr_to_char(lhs[1]) | (1 << 31);
                        
                        if(String(lhs[2]).equals("Composed"))
                          key |= 1 << 30;
                      }
                      else {
                        uint8_t font = expr_to_ui8(lhs[2]) - 1;
                        key = GG.expr_to_glyph(lhs[1], font);
                        key |= font << 16;
                      }
                    }
                    
                    tables->script_indents.set(key, ScriptIndent(rule[2]));
                  }
                }
              }
              
              continue;
            }
            
            if(lhs.equals("Name")) {
              tables->shaper_name = String(rhs);
              
              continue;
            }
            
            if(lhs.equals("PostScriptNames")) {
              if(rhs[0] == richmath_System_List) {
                for(size_t j = 1; j <= rhs.expr_length(); ++j) {
                  uint8_t font = expr_to_ui8(rhs[j]) - 1;
                  
                  if(font < tables->math_fontnames.length()) {
                    FontInfo(FontFace(tables->math_fontnames[font], style)).get_postscript_names(
                      &GG.ps2g[font], 0);
                  }
                }
              }
              else {
                uint8_t font = expr_to_ui8(rhs) - 1;
                
                if(font < tables->math_fontnames.length()) {
                  FontInfo(FontFace(tables->math_fontnames[font], style)).get_postscript_names(
                    &GG.ps2g[font], 0);
                }
              }
              
              continue;
            }
            
            continue;
          }
        }
        
        if(tables->math_fontnames.length() > 0) {
          SharedPtr<TextShaper> shaper = TextShaper::find(tables->math_fontnames[0], style);
          GlyphInfo glyphs[4];
          uint16_t  str[4];
          ComposedGlyph cg {};
          cg.vertical = true;
          
          for(size_t i = 0; i < sizeof(default_vertical_composed_glyphs) / sizeof(default_vertical_composed_glyphs[0]); ++i) {
            if(!tables->composed_glyphs.search(default_vertical_composed_glyphs[i][0])) {
              str[0] = default_vertical_composed_glyphs[i][1];
              str[1] = default_vertical_composed_glyphs[i][2];
              str[2] = default_vertical_composed_glyphs[i][3];
              str[3] = default_vertical_composed_glyphs[i][4];
              
              shaper->decode_token(GG.context, 4, str, glyphs);
              
              if( (str[0] && glyphs[0].index) ||
                  (str[1] && glyphs[1].index) ||
                  (str[2] && glyphs[2].index) ||
                  (str[3] && glyphs[3].index))
              {
                cg.top            = str[0] ? glyphs[0].index : 0;
                cg.bottom         = str[1] ? glyphs[1].index : 0;
                cg.middle         = str[2] ? glyphs[2].index : 0;
                cg.special_center = str[3] ? glyphs[3].index : 0;
                
                tables->composed_glyphs.set(default_vertical_composed_glyphs[i][0], cg);
              }
            }
          }
        }
        
        if(tables->verify())
          return tables;
          
        return nullptr;
      }
      
    private:
      ConfigShaperTables()
        : Shareable()
      {
        SET_BASE_DEBUG_TAG(typeid(*this).name());
        
        script_size_multipliers.length(1, 0.71f);
        radical.small_glyphs.length(1, SmallRadicalGlyph(0, 0, 0, Vector2F()));
      }
      
      static String find_font(Expr name) {
        if(name[0] == richmath_System_Alternatives) {
          for(size_t i = 1; i <= name.expr_length(); ++i) {
            String s = find_font(name[i]);
            if(s.length() > 0)
              return s;
          }
          
          return String();
        }
        
        String s(name);
        if(s.length() > 2 && s[0] == '<' && s[s.length() - 1] == '>') {
          s = s.part(1, s.length() - 2);
          return s;
        }
        
        if(!FontInfo::font_exists_exact(s)) {
          pmath_debug_print_object("Font ", name.get(), " not found.\n");
          return String();
        }
        
//        FontInfo fontinfo(FontFace(String(name), style));
//        if(0 == fontinfo.get_truetype_table(FONT_TABLE_NAME('c', 'm', 'a', 'p'), 0, 0, 0)) {
//          pmath_debug_print_object("Font ", name.get(), "not found.");
//          return String();
//        }

        return s;
      }
      
      bool verify() {
#define FUNC_NAME  "ConfigShaperTables::verify"
      
        if(math_fontnames.length() < 1) {
          printf("[%s, %d]", FUNC_NAME, __LINE__);
          return false;
        }
        
        if(text_fontnames.length() < 1) {
          printf("[%s, %d]", FUNC_NAME, __LINE__);
          return false;
        }
        
        if(math_fontnames.length() + text_fontnames.length() > NumFontsPerGlyph / 2) {
          printf("[%s, %d]", FUNC_NAME, __LINE__);
          return false;
        }
        
        if(radical.font >= math_fontnames.length()) {
          printf("[%s, %d]", FUNC_NAME, __LINE__);
          return false;
        }
        
        if(radical.small_glyphs.length() >= 1) {
          SmallRadicalGlyph &last = radical.small_glyphs[radical.small_glyphs.length() - 1];
          
          if(last.index != 0) {
            printf("[%s, %d]", FUNC_NAME, __LINE__);
            return false;
          }
          
          if(last.hbar_index != 0) {
            printf("[%s, %d]", FUNC_NAME, __LINE__);
            return false;
          }
          
          if(last.rel_ascent != 0) {
            printf("[%s, %d]", FUNC_NAME, __LINE__);
            return false;
          }
          
          if(last.rel_exp_offset.x != 0) {
            printf("[%s, %d]", FUNC_NAME, __LINE__);
            return false;
          }
          
          if(last.rel_exp_offset.y != 0) {
            printf("[%s, %d]", FUNC_NAME, __LINE__);
            return false;
          }
        }
        else {
          printf("[%s, %d]", FUNC_NAME, __LINE__);
          return false;
        }
        
        for(auto e : stretched_glyphs.deletable_entries()) {
          if(e.value.glyphs.length() == 0) {
            printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e.key);
            return false;
          }
          
          if(e.value.glyphs.length() != e.value.fonts.length()) {
            printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e.key);
            return false;
          }
          
          for(int j = 0; j < e.value.glyphs.length(); ++j) {
            if(e.value.glyphs[j] == 0) {
              //printf("[%s, %d, %x, %d]", FUNC_NAME, __LINE__, e.key, j);
              //return false;
              printf("streched glyph %x failed.\n", e.key);
              e.delete_self();
              goto NEXT_STRETCHED;
            }
          }
          
          for(int j = 0; j < e.value.fonts.length(); ++j) {
            if(e.value.fonts[j] >= math_fontnames.length()) {
              printf("[%s, %d, %x, %d]", FUNC_NAME, __LINE__, e.key, j);
              return false;
            }
          }
          
        NEXT_STRETCHED: ;
        }
        
        for(auto &e : composed_glyphs.entries()) {
          if(e.value.tbms_font >= math_fontnames.length()) {
            printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e.key);
            return false;
          }
          
          if(e.value.ul_font >= math_fontnames.length()) {
            printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e.key);
            return false;
          }
          
          if(e.value.vertical) {
            if(e.value.top == 0 && e.value.bottom != 0) {
              printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e.key);
              return false;
            }
            
            if(e.value.top != 0 && e.value.bottom == 0) {
              printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e.key);
              return false;
            }
          }
          
          if(e.value.middle == 0 && e.value.special_center != 0) {
            printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e.key);
            return false;
          }
          
          if(e.value.upper == 0 && e.value.lower != 0) {
            printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e.key);
            return false;
          }
          
          if(e.value.upper != 0 && e.value.lower == 0) {
            printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e.key);
            return false;
          }
        }
        
        for(auto e : char_to_glyph_map.deletable_entries()) {
          if(e.value.glyph == 0) {
            printf("not found: U+%04x\n", (int)e.key);
            e.delete_self();
            continue;
          }
          
          if(e.value.font >= math_fontnames.length()) {
            printf("[%s, %d]", FUNC_NAME, __LINE__);
            return false;
          }
        }
        
        for(auto &e : ligatures.entries()) {
          if(e.value.length() > e.key.length()) {
            printf("[%s, %d]", FUNC_NAME, __LINE__);
            return false;
          }
          
          for(int j = 0; j < e.value.length(); ++j) {
            if(e.value[j].font >= math_fontnames.length()) {
              printf("[%s, %d]", FUNC_NAME, __LINE__);
              return false;
            }
          }
        }
        
        for(auto &e : complex_glyphs.entries()) {
          for(int j = 0; j < e.value.length(); ++j) {
            if(e.value[j].font >= math_fontnames.length()) {
              printf("[%s, %d]", FUNC_NAME, __LINE__);
              return false;
            }
          }
        }
        
        return true;
      }
  };
  
  class ConfigShaperDB: public Shareable {
    public:
      virtual ~ConfigShaperDB() {
      }
      
      static SharedPtr<ConfigShaper> try_register(const Expr expr) {
        SharedPtr<ConfigShaperTables> tables = ConfigShaperTables::try_load_from_object(expr, NoStyle);
        if(!tables.is_valid())
          return nullptr;
          
        SharedPtr<ConfigShaper> plain_shaper = new ConfigShaper(tables, NoStyle);
        
        SharedPtr<ConfigShaperDB> db = new ConfigShaperDB();
        db->definition = expr;
        db->shapers[(int)NoStyle] = plain_shaper;
        
        registered.set(tables->shaper_name, db);
        
        return plain_shaper;
      }
      
      static void dispose_all() {
        registered.clear();
        GG.clear();
      }
      
      static SharedPtr<ConfigShaper> try_find(String name, FontStyle style) {
        SharedPtr<ConfigShaperDB> *db = registered.search(name);
        
        if(!db || !db->is_valid()) 
          return nullptr;
        
        return (*db)->find(style);
      }
      
    private:
      ConfigShaperDB()
        : Shareable()
      {
        SET_BASE_DEBUG_TAG(typeid(*this).name());
      }
      
      SharedPtr<ConfigShaper> find(FontStyle style) {
        int i = (int)style;
        
        if(shapers[i].is_valid())
          return shapers[i];
          
        SharedPtr<ConfigShaperTables> tables = ConfigShaperTables::try_load_from_object(definition, style);
        
        if(!tables.is_valid()) {
          printf("ConfigShaperTables::try_load_from_object failed for style %x\n", (unsigned)style);
          
          if(!shapers[(int)NoStyle].is_valid()) {
            RICHMATH_ASSERT(0 && "invalid ConfigShaperDB");
          }
          
          tables = shapers[(int)NoStyle]->tables;
        }
        
        shapers[i] = new ConfigShaper(tables, style);
        return shapers[i];
      }
      
    private:
      SharedPtr<ConfigShaper> shapers[FontStyle::Permutations];
      Expr definition;
      
      static Hashtable<String, SharedPtr<ConfigShaperDB> > registered;
  };
  
  Hashtable<String, SharedPtr<ConfigShaperDB> > ConfigShaperDB::registered;
  
}


//{ class ConfigShaper ...

ConfigShaper::ConfigShaper(SharedPtr<ConfigShaperTables> _tables, FontStyle _style)
  : SimpleMathShaper(_tables->radical.font),
    tables(_tables),
    text_shaper(new FallbackTextShaper(TextShaper::find(tables->text_fontnames[0], _style))),
    math_font_faces(_tables->math_fontnames.length()),
    style(_style)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  for(int i = 1; i < tables->text_fontnames.length(); ++i)
    text_shaper->add(TextShaper::find(tables->text_fontnames[i], style));
    
  text_shaper->add_default();
  
  for(int i = 0; i < math_font_faces.length(); ++i)
    math_font_faces[i] = FontFace(tables->math_fontnames[i], style);
}

ConfigShaper::~ConfigShaper() {
}

String ConfigShaper::name() {
  return tables->shaper_name;
}

SharedPtr<ConfigShaper> ConfigShaper::try_register(const Expr expr) {
  return ConfigShaperDB::try_register(expr);
}

void ConfigShaper::dispose_all() {
  ConfigShaperDB::dispose_all();
}

uint8_t ConfigShaper::num_fonts() {
  return (uint8_t)math_font_faces.length() + text_shaper->num_fonts();
}

FontFace ConfigShaper::font(uint8_t fontinfo) {
  if(fontinfo >= math_font_faces.length())
    return text_shaper->font(fontinfo - (uint8_t)math_font_faces.length());
    
  return math_font_faces[fontinfo];
}

String ConfigShaper::font_name(uint8_t fontinfo) {
  if(fontinfo >= tables->math_fontnames.length())
    return text_shaper->font_name(fontinfo - (uint8_t)math_font_faces.length());
    
  return tables->math_fontnames[fontinfo];
}

void ConfigShaper::decode_token(
  Context        &context,
  int             len,
  const uint16_t *str,
  GlyphInfo      *result
) {
  if(len == 1 && context.single_letter_italics) {
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
      if(GlyphFontOffset *gfo = tables->char_to_glyph_map.search(ch)) {
        if(style.italic) {
          result->slant = FontSlantPlain;
          math_set_style(style - Italic)->decode_token(context, len, str, result);
          return;
        }
        
        result->index    = gfo->glyph;
        result->fontinfo = gfo->font;
        
        context.canvas().set_font_face(font(result->fontinfo));
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = 0;
        cg.y = 0;
        cg.index = result->index;
        context.canvas().glyph_extents(&cg, 1, &cte);
        
        result->x_offset = 0;
        result->right = cte.x_advance;
        
        if(gfo && gfo->offset) {
          result->right += gfo->offset
                           * context.canvas().get_font_size()
                           * GlyphFontOffset::EmPerOffset;
        }
        
        return;
      }
      
//      uint16_t utf16[2];
//      GlyphInfo r[2];
//      memset(r, 0, sizeof(r));
//
//      if(ch <= 0xFFFF){
//        utf16[0] = ch;
//      }
//      else{
//        ch-= 0x10000;
//
//        utf16[0] = 0xD800 | (ch >> 10);
//        utf16[1] = 0xDC00 | (ch & 0x03FF);
//
//        ch+= 0x10000;
//      }
//
//      bool old_boxchar_fallback_enabled = context.boxchar_fallback_enabled;
//      context.boxchar_fallback_enabled = false;
//      text_shaper->decode_token(
//        context,
//        ch <= 0xFFFF ? 1 : 2,
//        utf16,
//        r);
//      context.boxchar_fallback_enabled = old_boxchar_fallback_enabled;
//
//      if(r->index != 0
//      && r->index != UnknownGlyph
//      && r->fontinfo < math_font_faces.length()){
//        memcpy(result, r, sizeof(GlyphInfo));
//        result->fontinfo+= math_font_faces.length();
//        return;
//      }

      if(!style.italic) {
        math_set_style(style + Italic)->decode_token(context, len, str, result);
        result->slant = FontSlantItalic;
        return;
      }
    }
  }
  
  if(len == 1 && tables->complex_glyphs.size() > 0) {
    Array<GlyphFontOffset> *arr = tables->complex_glyphs.search(*str);
    
    if(arr && arr->length() > 0) {
      cairo_text_extents_t cte;
      cairo_glyph_t        cg;
      
      result->composed = 1;
      result->index    = arr->get(0).glyph; // ext.num_extenders???
      result->fontinfo = arr->get(0).font;
      result->right    = 0;
      result->x_offset = 0;
      
      for(const auto &part : *arr) {
        context.canvas().set_font_face(font(part.font));
        cg.x = 0;
        cg.y = 0;
        cg.index = part.glyph;
        context.canvas().glyph_extents(&cg, 1, &cte);
        
        result->right += cte.x_advance;
        
        if(part.offset) {
          result->right += part.offset
                           * context.canvas().get_font_size()
                           * GlyphFontOffset::EmPerOffset;
        }
      }
      
      return;
    }
  }
  
  if(len > 1 && tables->ligatures.size() > 0) {
    Array<GlyphFontOffset> *arr = tables->ligatures.search(String::FromUcs2(str, len));
    
    if(arr && arr->length() <= len) {
      int i;
      for(i = 0; i < arr->length(); ++i) {
        result[i].index    = arr->get(i).glyph;
        result[i].fontinfo = arr->get(i).font;
        
        context.canvas().set_font_face(font(result[i].fontinfo));
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = 0;
        cg.y = 0;
        cg.index = result[i].index;
        context.canvas().glyph_extents(&cg, 1, &cte);
        
        result[i].x_offset = 0;
        result[i].right = cte.x_advance;
        
        if(arr->get(i).offset) {
          result[i].right += arr->get(i).offset
                             * context.canvas().get_font_size()
                             * GlyphFontOffset::EmPerOffset;
        }
      }
      
      for(; i < len; ++i) {
        result[i].index    = 0;
        result[i].fontinfo = 0;
      }
      
      return;
    }
  }
  
  while(len > 0) {
    int sub_len = 0;
    int char_len = 1;
    GlyphFontOffset *gfo = nullptr;
    
    while(sub_len < len && !gfo) {
      char_len = 1;
      
      if( sub_len + 1 < len &&
          is_utf16_high(str[sub_len]) &&
          is_utf16_low(str[sub_len + 1]))
      {
        uint32_t hi = str[sub_len];
        uint32_t lo = str[sub_len + 1];
        uint32_t ch = 0x10000 + (((hi & 0x03FF) << 10) | (lo & 0x03FF));
        
        char_len = 2;
        gfo = tables->char_to_glyph_map.search(ch);
      }
      else
        gfo = tables->char_to_glyph_map.search(str[sub_len]);
        
      if(gfo)
        break;
        
      sub_len += char_len;
    }
    
    if(sub_len > 0) {
      text_shaper->decode_token(
        context,
        sub_len,
        str,
        result);
        
      for(int i = 0; i < sub_len; ++i)
        result[i].fontinfo += math_font_faces.length();
        
      str    += sub_len;
      result += sub_len;
      len    -= sub_len;
    }
    
    if(gfo) {
      result->index    = gfo->glyph;
      result->fontinfo = gfo->font;
      
      if(char_len == 2) {
        result[1].index    = 0;
        result[1].fontinfo = 0;
      }
      
      context.canvas().set_font_face(font(result->fontinfo));
      cairo_text_extents_t cte;
      cairo_glyph_t cg;
      cg.x = 0;
      cg.y = 0;
      cg.index = result->index;
      context.canvas().glyph_extents(&cg, 1, &cte);
      
      result->x_offset = 0;
      result->right = cte.x_advance;
      
      if(gfo && gfo->offset) {
        result->right += gfo->offset
                         * context.canvas().get_font_size()
                         * GlyphFontOffset::EmPerOffset;
      }
      
      str    += char_len;
      result += char_len;
      len    -= char_len;
    }
    else {
      RICHMATH_ASSERT(len == 0);
    }
  }
}

void ConfigShaper::vertical_glyph_size(
  Context         &context,
  const uint16_t   ch,
  const GlyphInfo &info,
  float           *ascent,
  float           *descent
) {
  if((int)info.fontinfo >= math_font_faces.length()) {
    GlyphInfo gi;
    memcpy(&gi, &info, sizeof(GlyphInfo));
    
    gi.fontinfo -= math_font_faces.length();
    text_shaper->vertical_glyph_size(context, ch, gi, ascent, descent);
    
    return;
  }
  
  if(info.composed) {
    Array<GlyphFontOffset> *arr = tables->complex_glyphs.search(ch);
    
    if(arr && arr->length() > 0) {
      cairo_text_extents_t cte;
      cairo_glyph_t        cg;
      
      for(const auto &part : *arr) {
        context.canvas().set_font_face(font(part.font));
        cg.x = 0;
        cg.y = 0;
        cg.index = part.glyph;
        context.canvas().glyph_extents(&cg, 1, &cte);
        if(*ascent < -cte.y_bearing)
          *ascent = -cte.y_bearing;
        if(*descent < cte.height + cte.y_bearing)
          *descent = cte.height + cte.y_bearing;
      }
      
      return;
    }
  }
  
  SimpleMathShaper::vertical_glyph_size(context, ch, info, ascent, descent);
}

void ConfigShaper::show_glyph(
  Context         &context,
  Point            pos,
  const uint16_t   ch,
  const GlyphInfo &info
) {
  if(info.fontinfo >= (unsigned)math_font_faces.length()) {
    GlyphInfo gi;
    memcpy(&gi, &info, sizeof(GlyphInfo));
    
    gi.fontinfo -= math_font_faces.length();
    text_shaper->show_glyph(context, pos, ch, gi);
    
    return;
  }
  
  if(info.composed) {
    Array<GlyphFontOffset> *arr = tables->complex_glyphs.search(ch);
    
    if(arr && arr->length() > 0) {
      cairo_text_extents_t cte;
      cairo_glyph_t        cg;
      
      cg.x = pos.x + info.x_offset;
      cg.y = pos.y;
      
      for(const auto &part : *arr) {
        context.canvas().set_font_face(font(part.font));
        cg.index = part.glyph;
        context.canvas().show_glyphs(&cg, 1);
        
        context.canvas().glyph_extents(&cg, 1, &cte);
        cg.x += cte.x_advance;
        
        if(part.offset) {
          cg.x += part.offset
                  * context.canvas().get_font_size()
                  * GlyphFontOffset::EmPerOffset;
        }
      }
      
      return;
    }
  }
  
  SimpleMathShaper::show_glyph(context, pos, ch, info);
}

float ConfigShaper::italic_correction(
  Context          &context,
  uint16_t          ch,
  const GlyphInfo  &info
) {
  float result = tables->italic_script_indent.center * GlyphFontOffset::EmPerOffset;
  
  uint32_t key;
  if(info.composed)
    key = (3 << 30) | ch;
  else
    key = (uint32_t)info.index | (((uint32_t)info.fontinfo) << 16);
    
  if(ScriptIndent *si = tables->script_indents.search(key))
    return result + si->center * GlyphFontOffset::EmPerOffset;
    
  if(!info.composed) {
    key = (1 << 31) | ch;
    
    if(ScriptIndent *si = tables->script_indents.search(key))
      return result + si->center * GlyphFontOffset::EmPerOffset;
  }
  
  return result;
}

void ConfigShaper::script_corrections(
  Context           &context,
  uint16_t           base_char,
  const GlyphInfo   &base_info,
  MathSequence          *sub,
  MathSequence          *super,
  float              sub_y,
  float              super_y,
  float             *sub_x,
  float             *super_x
) {
  switch(base_info.slant) {
    case FontSlantItalic:
      if(!style.italic) {
        math_set_style(style + Italic)->script_corrections(
          context, base_char, base_info, sub, super, sub_y, super_y,
          sub_x, super_x);
        return;
      }
      break;
      
    case FontSlantPlain:
      if(style.italic) {
        math_set_style(style - Italic)->script_corrections(
          context, base_char, base_info, sub, super, sub_y, super_y,
          sub_x, super_x);
        return;
      }
      break;
  }
  
  float em = context.canvas().get_font_size();
  *sub_x = *super_x = 0;
  
  if(style.italic) {
    *super_x += tables->italic_script_indent.super * GlyphFontOffset::EmPerOffset * em;
    *sub_x   += tables->italic_script_indent.sub * GlyphFontOffset::EmPerOffset * em;
  }
  
  uint32_t key;
  if(base_info.composed)
    key = (3 << 30) | base_char;
  else
    key = (uint32_t)base_info.index | (((uint32_t)base_info.fontinfo) << 16);
    
  if(ScriptIndent *si = tables->script_indents.search(key)) {
    *super_x += si->super  * GlyphFontOffset::EmPerOffset * em;
//    *center+= si->center * GlyphFontOffset::EmPerOffset * em;
    *sub_x   += si->sub    * GlyphFontOffset::EmPerOffset * em;
    return;
  }
  else if(!base_info.composed) {
    key = (1 << 31) | base_char;
    
    if(ScriptIndent *si = tables->script_indents.search(key)) {
      *super_x += si->super  * GlyphFontOffset::EmPerOffset * em;
//      *center+= si->center * GlyphFontOffset::EmPerOffset * em;
      *sub_x   += si->sub    * GlyphFontOffset::EmPerOffset * em;
      return;
    }
  }
}

void ConfigShaper::get_script_size_multis(Array<float> *arr) {
  arr->operator=(tables->script_size_multipliers);
}

SharedPtr<TextShaper> ConfigShaper::set_style(FontStyle _style) {
  SharedPtr<ConfigShaper> result = ConfigShaperDB::try_find(name(), _style);
  if(!result.is_valid()) {
    pmath_debug_print_object("Lost ConfigShaperDB for ", name().get(), "\n");
    
    style = _style; // HACK!!! Otherwise we cause stack overflow elsewhere: code assumes that the returned shaper has the given style.
    
    ref();
    return this;
  }
  
  return result;
}

int ConfigShaper::h_stretch_glyphs(
  uint16_t         ch,
  const uint8_t  **fonts,
  const uint16_t **glyphs
) {
  StretchGlyphArray *arr = tables->stretched_glyphs.search(ch);
  
  if(arr && !arr->vertical) {
    *fonts  = arr->fonts.items();
    *glyphs = arr->glyphs.items();
    return arr->glyphs.length();
  }
  
  return 0;
}

int ConfigShaper::h_stretch_big_glyphs(
  uint16_t  ch,
  uint16_t *left,
  uint16_t *middle,
  uint16_t *right,
  uint16_t *special_center
) {
  ComposedGlyph *cg = tables->composed_glyphs.search(ch);
  
  if(cg && !cg->vertical) {
    *left           = cg->top;
    *middle         = cg->middle;
    *right          = cg->bottom;
    *special_center = cg->special_center;
    return cg->tbms_font;
  }
  
  *left = *middle = *right = *special_center = 0;
  return 0;
}

int ConfigShaper::v_stretch_glyphs(
  uint16_t         ch,
  bool             full_stretch,
  const uint8_t  **fonts,
  const uint16_t **glyphs
) {
  StretchGlyphArray *arr = tables->stretched_glyphs.search(ch);
  
  if(arr && arr->vertical) {
    *fonts  = arr->fonts.items();
    *glyphs = arr->glyphs.items();
    return arr->glyphs.length();
  }
  
  return 0;
}

int ConfigShaper::v_stretch_pair_glyphs(
  uint16_t  ch,
  uint16_t *upper,
  uint16_t *lower
) {
  ComposedGlyph *cg = tables->composed_glyphs.search(ch);
  
  if(cg && cg->vertical) {
    *upper = cg->upper;
    *lower = cg->lower;
    return cg->ul_font;
  }
  
  *upper = *lower = 0;
  return 0;
}

int ConfigShaper::v_stretch_big_glyphs(
  uint16_t  ch,
  uint16_t *top,
  uint16_t *middle,
  uint16_t *bottom,
  uint16_t *special_center
) {
  ComposedGlyph *cg = tables->composed_glyphs.search(ch);
  
  if(cg && cg->vertical) {
    *top            = cg->top;
    *middle         = cg->middle;
    *bottom         = cg->bottom;
    *special_center = cg->special_center;
    return cg->tbms_font;
  }
  
  *top = *middle = *bottom = *special_center = 0;
  return 0;
}

const SmallRadicalGlyph *ConfigShaper::small_radical_glyphs() { // zero terminated
  return tables->radical.small_glyphs.items();
}

void ConfigShaper::big_radical_glyphs(
  uint16_t     *bottom,
  uint16_t     *vertical,
  uint16_t     *edge,
  uint16_t     *horizontal,
  Vector2F     *rel_exponent_offset
) {
  *bottom              = tables->radical.big_glyph.bottom;
  *vertical            = tables->radical.big_glyph.vertical;
  *edge                = tables->radical.big_glyph.edge;
  *horizontal          = tables->radical.big_glyph.horizontal;
  *rel_exponent_offset = tables->radical.big_glyph.rel_exponent_offset;
}

//} ... class ConfigShaper


Expr richmath_eval_FrontEnd_AddConfigShaper(Expr expr) {
  Expr data = expr[1];
  expr = Expr();
  
  SharedPtr<ConfigShaper> shaper = ConfigShaper::try_register(PMATH_CPP_MOVE(data));
  
  if(shaper) {
    MathShaper::available_shapers.set(shaper->name(), shaper);
    
    pmath_debug_print_object("loaded ", shaper->name().get(), "\n");
    return shaper->name();
  }
  
  pmath_debug_print("adding config shaper failed.\n");
  return Symbol(richmath_System_DollarFailed);
}


