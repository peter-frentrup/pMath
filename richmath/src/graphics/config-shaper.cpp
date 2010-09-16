#include <graphics/config-shaper.h>

#include <cstdio>

#include <boxes/mathsequence.h>

#include <graphics/context.h>
#include <graphics/ot-math-shaper.h>

#include <util/style.h>
#include <util/syntax-state.h>

using namespace richmath;

static uint8_t expr_to_ui8(const Expr expr, uint8_t def = 0){
  if(expr.instance_of(PMATH_TYPE_INTEGER)
  && pmath_integer_fits_ui(expr.get())){
    unsigned long res = pmath_integer_get_ui(expr.get());
    
    if((res & 0xFF) == res)
      return res;
  }
  
  return def;
}

static uint16_t expr_to_ui16(const Expr expr, uint16_t def = 0){
  if(expr.instance_of(PMATH_TYPE_INTEGER)
  && pmath_integer_fits_ui(expr.get())){
    unsigned long res = pmath_integer_get_ui(expr.get());
    
    if((res & 0xFFFF) == res)
      return res;
  }
  
  return def;
}

static uint32_t expr_to_char(const Expr expr){
  if(expr.instance_of(PMATH_TYPE_STRING)){
    String s(expr);
    
    if(s.length() == 1)
      return s[0];
    
    if(s.length() == 2){
      uint32_t hi = s[0];
      uint32_t lo = s[1];
      
      if(is_utf16_high(hi)
      && is_utf16_low(lo))
        return 0x10000 + (((hi & 0x03FF) << 10) | (lo & 0x03FF));
    }
  }
  
  return -(uint32_t)1;
}

class GlyphGetter: public Base{
  public:
    GlyphGetter(){
      surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 1, 1);
      cr = cairo_create(surface);
      
      context.canvas = new Canvas(cr);
    }
    
    ~GlyphGetter(){
      delete context.canvas;
      
      cairo_destroy(cr);
      cairo_surface_destroy(surface);
    }
    
    uint16_t expr_to_glyph(const Expr expr, uint8_t font){
      if(expr.instance_of(PMATH_TYPE_STRING)){
        uint16_t res = ps2g[font & (FontsPerGlyphCount-1)][String(expr)];
        
        if(!res){
          pmath_debug_print_object("Unknown glyph ", expr.get(), "");
          pmath_debug_print(" in font %d\n", font);
        }
        
        return res;
      }
      
      return expr_to_ui16(expr);
    }
    
  public:
    Hashtable<String, uint16_t> ps2g[FontsPerGlyphCount];
    cairo_surface_t *surface;
    cairo_t         *cr;
    Context          context;
    
//    Array<SharedPtr<TextShaper> > shapers;
};

static GlyphGetter GG;

//{ class GlyphFontOffset ...

const float GlyphFontOffset::EmPerOffset = 1/72.0f;

GlyphFontOffset::GlyphFontOffset(Expr expr)
: glyph(0), font(0), offset(0)
{
  if(expr[0] == PMATH_SYMBOL_LIST){
    if(expr.expr_length() == 2){
      font  = expr_to_ui8(expr[2]) - 1;
      glyph = GG.expr_to_glyph(expr[1], font);
    }
    else if(expr.expr_length() == 3){
      font  = expr_to_ui8(expr[2]) - 1;
      glyph = GG.expr_to_glyph(expr[1], font);
      
      float o = expr[3].to_double();
      offset = (uint8_t)(o / EmPerOffset + 0.5f);
    }
  }
  else{
    glyph = GG.expr_to_glyph(expr, font);
  }
}

//} ... class GlyphFontOffset

//{ class ScriptIndent ...

ScriptIndent::ScriptIndent(Expr expr)
: super(0), sub(0), center(0)
{
  float f;
  
  if(expr[0] == PMATH_SYMBOL_LIST){
    if(expr.expr_length() == 2){
      f = expr[1].to_double();
      super = (uint8_t)(f / GlyphFontOffset::EmPerOffset + 0.5f); 
      
      f = expr[2].to_double();
      sub = (uint8_t)(f / GlyphFontOffset::EmPerOffset + 0.5f); 
    }
    else if(expr.expr_length() == 3){
      f = expr[1].to_double();
      super = (uint8_t)(f / GlyphFontOffset::EmPerOffset + 0.5f); 
      
      f = expr[2].to_double();
      sub = (uint8_t)(f / GlyphFontOffset::EmPerOffset + 0.5f); 
      
      f = expr[3].to_double();
      center = (uint8_t)(f / GlyphFontOffset::EmPerOffset + 0.5f); 
    }
  }
}

//} ... class ScriptIndent

//{ class ConfigShaperDB ...

Hashtable<String, SharedPtr<ConfigShaperDB> > ConfigShaperDB::registered;

ConfigShaperDB::ConfigShaperDB(): Shareable(){
  script_size_multipliers.length(1, 0.71f);
  radical.small.length(1, SmallRadicalGlyph(0, 0, 0, 0, 0));
}

ConfigShaperDB::~ConfigShaperDB(){
}

void ConfigShaperDB::clear_cache(){
  for(int i = 0;i < FontStyle::Permutations;++i)
    shapers[i] = 0;
}

void ConfigShaperDB::clear_all(){
  int c = registered.size();
  for(int i = 0;c > 0;++i){
    Entry<String, SharedPtr<ConfigShaperDB> > *e = registered.entry(i);
    
    if(e){
      --c;
      
      e->value->clear_cache();
    }
  }
  
  registered.clear();
  
  for(int i = 0;i < (int)(sizeof(GG.ps2g)/sizeof(GG.ps2g[0]));++i)
    GG.ps2g[i].clear();
}

bool ConfigShaperDB::verify(){
#define FUNC_NAME  "ConfigShaperDB::verify"

  if(math_fontnames.length() < 1){
    printf("[%s, %d]", FUNC_NAME, __LINE__);
    return false;
  }
  
  if(text_fontnames.length() < 1){
    printf("[%s, %d]", FUNC_NAME, __LINE__);
    return false;
  }
  
  if(math_fontnames.length() + text_fontnames.length() > FontsPerGlyphCount){
    printf("[%s, %d]", FUNC_NAME, __LINE__);
    return false;
  }
  
  if(radical.font >= math_fontnames.length()){
    printf("[%s, %d]", FUNC_NAME, __LINE__);
    return false;
  }
  
  if(radical.small.length() >= 1){
    SmallRadicalGlyph &last = radical.small[radical.small.length()-1];
    
    if(last.index != 0){
      printf("[%s, %d]", FUNC_NAME, __LINE__);
      return false;
    }
      
    if(last.hbar_index != 0){
      printf("[%s, %d]", FUNC_NAME, __LINE__);
      return false;
    }
      
    if(last.rel_ascent != 0){
      printf("[%s, %d]", FUNC_NAME, __LINE__);
      return false;
    }
      
    if(last.rel_exp_x != 0){
      printf("[%s, %d]", FUNC_NAME, __LINE__);
      return false;
    }
      
    if(last.rel_exp_y != 0){
      printf("[%s, %d]", FUNC_NAME, __LINE__);
      return false;
    }
  }
  else{
    printf("[%s, %d]", FUNC_NAME, __LINE__);
    return false;
  }
  
  int c = stretched_glyphs.size();
  for(int i = 0;c > 0;++i){
    Entry<uint32_t, StretchGlyphArray> *e = stretched_glyphs.entry(i);
    
    if(e){
      --c;
      
      if(e->value.glyphs.length() == 0){
        printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e->key);
        return false;
      }
        
      if(e->value.glyphs.length() != e->value.fonts.length()){
        printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e->key);
        return false;
      }
      
      for(int j = 0;j < e->value.glyphs.length();++j){
        if(e->value.glyphs[j] == 0){
          printf("[%s, %d, %x, %d]", FUNC_NAME, __LINE__, e->key, j);
          return false;
        }
      }
      
      for(int j = 0;j < e->value.fonts.length();++j){
        if(e->value.fonts[j] >= math_fontnames.length()){
          printf("[%s, %d, %x, %d]", FUNC_NAME, __LINE__, e->key, j);
          return false;
        }
      }
    }
  }
  
  c = composed_glyphs.size();
  for(int i = 0;c > 0;++i){
    Entry<uint32_t, ComposedGlyph> *e = composed_glyphs.entry(i);
    
    if(e){
      --c;
      
      if(e->value.tbms_font >= math_fontnames.length()){
        printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e->key);
        return false;
      }
      
      if(e->value.ul_font >= math_fontnames.length()){
        printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e->key);
        return false;
      }
      
      if(e->value.vertical){
        if(e->value.top == 0 && e->value.bottom != 0){
          printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e->key);
          return false;
        }
          
        if(e->value.top != 0 && e->value.bottom == 0){
          printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e->key);
          return false;
        }
      }
      
      if(e->value.middle == 0 && e->value.special_center != 0){
        printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e->key);
        return false;
      }
      
      if(e->value.upper == 0 && e->value.lower != 0){
        printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e->key);
        return false;
      }
        
      if(e->value.upper != 0 && e->value.lower == 0){
        printf("[%s, %d, %x]", FUNC_NAME, __LINE__, e->key);
        return false;
      }
    }
  }
  
  c = char_to_glyph_map.size();
  for(int i = 0;c > 0;++i){
    Entry<uint32_t, GlyphFontOffset> *e = char_to_glyph_map.entry(i);
    
    if(e){
      --c;
      
      if(e->value.glyph == 0){
        printf("not found: U+%04x\n", (int)e->key);
      }
      
      if(e->value.font >= math_fontnames.length()){
        printf("[%s, %d]", FUNC_NAME, __LINE__);
        return false;
      }
    }
  }
  
  c = ligatures.size();
  for(int i = 0;c > 0;++i){
    Entry<String, Array<GlyphFontOffset> > *e = ligatures.entry(i);
    
    if(e){
      --c;
      
      if(e->value.length() > e->key.length()){
        printf("[%s, %d]", FUNC_NAME, __LINE__);
        return false;
      }
      
      for(int j = 0;j < e->value.length();++j){
        if(e->value[j].font >= math_fontnames.length()){
          printf("[%s, %d]", FUNC_NAME, __LINE__);
          return false;
        }
      }
    }
  }
  
  c = complex_glyphs.size();
  for(int i = 0;c > 0;++i){
    Entry<uint32_t, Array<GlyphFontOffset> > *e = complex_glyphs.entry(i);
    
    if(e){
      --c;
      
      for(int j = 0;j < e->value.length();++j){
        if(e->value[j].font >= math_fontnames.length()){
          printf("[%s, %d]", FUNC_NAME, __LINE__);
          return false;
        }
      }
    }
  }
  
  return true;
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

SharedPtr<ConfigShaperDB> ConfigShaperDB::load_from_object(const Expr expr){
  SharedPtr<ConfigShaperDB> db = new ConfigShaperDB();
  
  for(int i = 0;i < (int)(sizeof(GG.ps2g)/sizeof(GG.ps2g[0]));++i)
    GG.ps2g[i].clear();
    
  for(size_t i = 1;i <= expr.expr_length();++i){
    if(expr[i][0] == PMATH_SYMBOL_RULE
    && expr[i].expr_length() == 2){
      String lhs = expr[i][1];
      Expr   rhs = expr[i][2];
      
      if(lhs.equals("MathFonts")){
        if(rhs.instance_of(PMATH_TYPE_STRING)){
          db->math_fontnames.length(1);
          db->math_fontnames[0] = String(rhs);
        }
        
        if(rhs[0] == PMATH_SYMBOL_LIST){
          db->math_fontnames.length(rhs.expr_length());
          
          for(int j = 0;j < db->math_fontnames.length();++j){
            db->math_fontnames[j] = String(rhs[j+1]);
          }
        }
        
        continue;
      }
      
      if(lhs.equals("TextFonts")){
        if(rhs.instance_of(PMATH_TYPE_STRING)){
          db->text_fontnames.length(1);
          db->text_fontnames[0] = String(rhs);
        }
        
        if(rhs[0] == PMATH_SYMBOL_LIST){
          db->text_fontnames.length(rhs.expr_length());
          
          for(int j = 0;j < db->text_fontnames.length();++j){
            db->text_fontnames[j] = String(rhs[j+1]);
          }
        }
        
        continue;
      }
      
      if(lhs.equals("RadicalFont")){
        db->radical.font = expr_to_ui8(rhs) - 1;
        
        continue;
      }
      
      if(lhs.equals("ScriptSizeMultipliers")){
        if(rhs[0] == PMATH_SYMBOL_LIST){
          db->script_size_multipliers.length(rhs.expr_length());
          
          for(int i = 0;i < db->script_size_multipliers.length();++i){
            db->script_size_multipliers[i] = rhs[i+1].to_double();
          }
        }
        else
          db->script_size_multipliers.length(1, rhs.to_double());
          
        continue;
      }
      
      if(lhs.equals("SmallRadical")){
        if(rhs[0] == PMATH_SYMBOL_LIST){
          db->radical.small.length(rhs.expr_length() + 1);
          
          for(int j = 0;j < db->radical.small.length()-1;++j){
            Expr g = rhs[j+1];
            
            if(g[0] == PMATH_SYMBOL_LIST
            && g.expr_length() == 5){
              db->radical.small[j].index      = GG.expr_to_glyph(g[1], db->radical.font);
              db->radical.small[j].hbar_index = GG.expr_to_glyph(g[2], db->radical.font);
              db->radical.small[j].rel_ascent = g[3].to_double();
              db->radical.small[j].rel_exp_x  = g[4].to_double();
              db->radical.small[j].rel_exp_y  = g[5].to_double();
            }
            else
              break;
          }
          
          db->radical.small[db->radical.small.length()-1] = 
            SmallRadicalGlyph(0, 0, 0, 0, 0);
        }
        
        continue;
      }
      
      if(lhs.equals("BigRadical")){
        if(rhs[0] == PMATH_SYMBOL_LIST
        && rhs.expr_length() == 6){
          db->radical.big.bottom     = GG.expr_to_glyph(rhs[1], db->radical.font);
          db->radical.big.vertical   = GG.expr_to_glyph(rhs[2], db->radical.font);
          db->radical.big.edge       = GG.expr_to_glyph(rhs[3], db->radical.font);
          db->radical.big.horizontal = GG.expr_to_glyph(rhs[4], db->radical.font);
          db->radical.big.rel_exp_x  = rhs[5].to_double();
          db->radical.big.rel_exp_y  = rhs[6].to_double();
        }
        
        continue;
      }
      
      if(lhs.equals("Glyphs")){
        if(rhs[0] == PMATH_SYMBOL_LIST){
          for(size_t j = rhs.expr_length();j > 0;--j){
            Expr rule = rhs[j];
            
            if(rule[0] == PMATH_SYMBOL_RULE
            && rule.expr_length() == 2){
              db->char_to_glyph_map.set(
                expr_to_char(rule[1]), 
                GlyphFontOffset(rule[2]));
            }
          }
        }
        
        continue;
      }
      
      if(lhs.equals("Ligatures")){
        if(rhs[0] == PMATH_SYMBOL_LIST){
          for(size_t j = rhs.expr_length();j > 0;--j){
            Expr rule = rhs[j];
            
            if(rule[0] == PMATH_SYMBOL_RULE
            && rule.expr_length() == 2
            && rule[2][0] == PMATH_SYMBOL_LIST){
              Expr v = rule[2];
              
              Array<GlyphFontOffset> arr(v.expr_length());
              
              for(int j = 0;j < arr.length();++j)
                arr[j] = GlyphFontOffset(v[j + 1]);
              
              uint32_t ch = expr_to_char(rule[1]);
              if(ch + 1 == 0)
                db->ligatures.set(rule[1], arr);
              else
                db->complex_glyphs.set(ch, arr);
            }
          }
        }
        
        continue;
      }
      
      if(lhs.equals("VerticalStretchedGlyphes")
      || lhs.equals("HorizontalStretchedGlyphes")){
        StretchGlyphArray sga;
        sga.vertical = lhs[0] == 'V';
        
        if(rhs[0] == PMATH_SYMBOL_LIST){
          for(size_t j = rhs.expr_length();j > 0;--j){
            Expr rule = rhs[j];
            
            if(rule[0] == PMATH_SYMBOL_RULE
            && rule.expr_length() == 2
            && rule[2][0] == PMATH_SYMBOL_LIST){
              Expr list = rule[2];
              
              sga.glyphs.length(list.expr_length());
              sga.fonts.length(sga.glyphs.length(), 0);
              for(int k = 0;k < sga.glyphs.length();++k){
                if(list[k+1].expr_length() == 2
                && list[k+1][0] == PMATH_SYMBOL_LIST){
                  sga.fonts[k]  = expr_to_ui8(list[k+1][2]) - 1;
                  sga.glyphs[k] = GG.expr_to_glyph(list[k+1][1], sga.fonts[k]);
                }
                else{
                  sga.glyphs[k] = GG.expr_to_glyph(list[k+1], 0);
                }
              }
              
              db->stretched_glyphs.set(expr_to_char(rule[1]), sga);
            }
          }
        }
        
        continue;
      }
      
      if(lhs.equals("HorizontalComposedGlyphs")){
        ComposedGlyph cg;
        cg.vertical = false;
        
        if(rhs[0] == PMATH_SYMBOL_LIST){
          for(size_t j = rhs.expr_length();j > 0;--j){
            Expr rule = rhs[j];
            
            if(rule[0] == PMATH_SYMBOL_RULE
            && rule.expr_length() == 2
            && rule[2][0] == PMATH_SYMBOL_LIST
            && rule[2].expr_length() == 2){
              Expr list = rule[2];
              Expr gs   = list[1];
              
              cg.tbms_font = expr_to_ui8(list[2]) - 1;
              
              if(gs[0] == PMATH_SYMBOL_LIST){
                switch(gs.expr_length()){
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
              
              db->composed_glyphs.set(
                expr_to_char(rule[1]),
                cg);
            }
          }
        }
        
        continue;
      }
      
      if(lhs.equals("VerticalComposedGlyphs")){
        ComposedGlyph cg;
        cg.vertical = true;
        
        if(rhs[0] == PMATH_SYMBOL_LIST){
          for(size_t j = rhs.expr_length();j > 0;--j){
            Expr rule = rhs[j];
            
            if(rule[0] == PMATH_SYMBOL_RULE
            && rule.expr_length() == 2
            && rule[2][0] == PMATH_SYMBOL_LIST
            && rule[2].expr_length() == 4){
              Expr list = rule[2];
              Expr gs   = list[1];
              
              cg.tbms_font = expr_to_ui8(list[3]) - 1;
              cg.ul_font   = expr_to_ui8(list[4]) - 1;
              
              if(gs[0] == PMATH_SYMBOL_LIST){
                cg.top            = GG.expr_to_glyph(gs[1], cg.tbms_font);
                cg.bottom         = GG.expr_to_glyph(gs[2], cg.tbms_font);
                cg.middle         = GG.expr_to_glyph(gs[3], cg.tbms_font);
                cg.special_center = GG.expr_to_glyph(gs[4], cg.tbms_font);
              }
              
              gs = list[2];
              if(gs[0] == PMATH_SYMBOL_LIST){
                cg.upper = GG.expr_to_glyph(gs[1], cg.ul_font);
                cg.lower = GG.expr_to_glyph(gs[2], cg.ul_font);
              }
              else{
                cg.upper = 0;
                cg.lower = 0;
              }
              
              db->composed_glyphs.set(
                expr_to_char(rule[1]),
                cg);
            }
          }
        }
        
        continue;
      }
    
      if(lhs.equals("ScriptIndent")){
        if(rhs[0] == PMATH_SYMBOL_LIST){
          for(size_t j = rhs.expr_length();j > 0;--j){
            Expr rule = rhs[j];
            
            if(rule[0] == PMATH_SYMBOL_RULE
            && rule.expr_length() == 2){
              uint32_t key = 0;
              Expr lhs = rule[1];
              
              if(lhs.instance_of(PMATH_TYPE_STRING)){
                if(String(lhs).equals("Italic")){
                  db->italic_script_indent = ScriptIndent(rule[2]);
                  continue;
                }
                
                key = expr_to_char(lhs) | (1 << 31);
              }
              else if(lhs[0] == PMATH_SYMBOL_LIST){
                if(lhs.expr_length() == 1 || lhs[2].instance_of(PMATH_TYPE_STRING)){
                  key = expr_to_char(lhs[1]) | (1 << 31);
                  
                  if(String(lhs[2]).equals("Composed"))
                    key |= 1 << 30;
                }
                else{
                  uint8_t font = expr_to_ui8(lhs[2]) - 1;
                  key = GG.expr_to_glyph(lhs[1], font); 
                  key |= font << 16;
                }
              }
              
              db->script_indents.set(key, ScriptIndent(rule[2]));
            }
          }
        }
        
        continue;
      }
    
      if(lhs.equals("Name")){
        db->shaper_name = String(rhs);
        
        continue;
      }
      
      if(lhs.equals("PostScriptNames")){
        if(rhs[0] == PMATH_SYMBOL_LIST){
          for(size_t j = 1;j <= rhs.expr_length();++j){
            uint8_t font = expr_to_ui8(rhs[j]) - 1;
            
            if(font < db->math_fontnames.length()){
              FontInfo(FontFace(db->math_fontnames[font], NoStyle)).get_postscript_names(
                &GG.ps2g[font], 0);
            }
          }
        }
        else{
          uint8_t font = expr_to_ui8(rhs) - 1;
          
          if(font < db->math_fontnames.length()){
            FontInfo(FontFace(db->math_fontnames[font], NoStyle)).get_postscript_names(
              &GG.ps2g[font], 0);
          }
        }
        
        continue;
      }
      
      continue;
    }
  }
  
  if(db->math_fontnames.length() > 0){
    
    SharedPtr<TextShaper> shaper = TextShaper::find(db->math_fontnames[0], NoStyle);
    GlyphInfo glyphs[4];
    uint16_t  str[4];
    ComposedGlyph cg;
    
    memset(&cg, 0, sizeof(ComposedGlyph));
    cg.vertical = true;
    
    for(size_t i = 0;i < sizeof(default_vertical_composed_glyphs) / sizeof(default_vertical_composed_glyphs[0]);++i){
      if(!db->composed_glyphs.search(default_vertical_composed_glyphs[i][0])){
        str[0] = default_vertical_composed_glyphs[i][1];
        str[1] = default_vertical_composed_glyphs[i][2];
        str[2] = default_vertical_composed_glyphs[i][3];
        str[3] = default_vertical_composed_glyphs[i][4];
        
        shaper->decode_token(&GG.context, 4, str, glyphs);
        
        if((str[0] && glyphs[0].index)
        || (str[1] && glyphs[1].index)
        || (str[2] && glyphs[2].index)
        || (str[3] && glyphs[3].index)){
          cg.top            = str[0] ? glyphs[0].index : 0;
          cg.bottom         = str[1] ? glyphs[1].index : 0;
          cg.middle         = str[2] ? glyphs[2].index : 0;
          cg.special_center = str[3] ? glyphs[3].index : 0;
          
          db->composed_glyphs.set(default_vertical_composed_glyphs[i][0], cg);
        }
      }
    }
  }
  
  if(db->verify())
    return db;
  
  return 0;
}

SharedPtr<ConfigShaper> ConfigShaperDB::find(FontStyle style){
  int i = (int)style;
  
  if(shapers[i].is_valid())
    return shapers[i];
  
  ref();
  shapers[i] = new ConfigShaper(this, style);
  
  return shapers[i];
}

//} ... class ConfigShaperDB

//{ class ConfigShaper ...

ConfigShaper::ConfigShaper(SharedPtr<ConfigShaperDB> _db, FontStyle _style)
: SimpleMathShaper(_db->radical.font),
  db(_db),
  text_shaper(new FallbackTextShaper(TextShaper::find(db->text_fontnames[0], _style))),
  math_font_faces(_db->math_fontnames.length()),
  style(_style)
{
  for(int i = 1;i < db->text_fontnames.length();++i)
    text_shaper->add(TextShaper::find(db->text_fontnames[i], style));
  
  text_shaper->add_default();

  for(int i = 0;i < math_font_faces.length();++i)
    math_font_faces[i] = FontFace(db->math_fontnames[i], style);
} 

ConfigShaper::~ConfigShaper(){
}

uint8_t ConfigShaper::num_fonts(){
  return (uint8_t)math_font_faces.length() + text_shaper->num_fonts();
}

FontFace ConfigShaper::font(uint8_t fontinfo){
  if(fontinfo >= math_font_faces.length())
    return text_shaper->font(fontinfo - (uint8_t)math_font_faces.length());
  
  return math_font_faces[fontinfo];
}

String ConfigShaper::font_name(uint8_t fontinfo){
  if(fontinfo >= db->math_fontnames.length())
    return text_shaper->font_name(fontinfo - (uint8_t)math_font_faces.length());
  
  return db->math_fontnames[fontinfo];
}

void ConfigShaper::decode_token(
  Context        *context,
  int             len,
  const uint16_t *str, 
  GlyphInfo      *result
){
  if(len == 1 && context->single_letter_italics){
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
    
    if(ch){
      GlyphFontOffset *gfo = db->char_to_glyph_map.search(ch);
      
      if(gfo){
        if(style.italic){
          result->slant = FontSlantPlain;
          math_set_style(style - Italic)->decode_token(context, len, str, result);
          return;
        }
        
        result->index    = gfo->glyph;
        result->fontinfo = gfo->font;
        
        context->canvas->set_font_face(font(result->fontinfo));
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = 0;
        cg.y = 0;
        cg.index = result->index;
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
        
        result->x_offset = 0;
        result->right = cte.x_advance;
        
        if(gfo && gfo->offset){
          result->right+= gfo->offset 
            * context->canvas->get_font_size()
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
//      bool old_boxchar_fallback_enabled = context->boxchar_fallback_enabled;
//      context->boxchar_fallback_enabled = false;
//      text_shaper->decode_token(
//        context, 
//        ch <= 0xFFFF ? 1 : 2, 
//        utf16, 
//        r);
//      context->boxchar_fallback_enabled = old_boxchar_fallback_enabled;
//      
//      if(r->index != 0
//      && r->index != UnknownGlyph
//      && r->fontinfo < math_font_faces.length()){
//        memcpy(result, r, sizeof(GlyphInfo));
//        result->fontinfo+= math_font_faces.length();
//        return;
//      }
      
      if(!style.italic){
        result->slant = FontSlantItalic;
        math_set_style(style + Italic)->decode_token(context, len, str, result);
        return;
      }
    }
  }
  
  if(len == 1 && db->complex_glyphs.size() > 0){
    Array<GlyphFontOffset> *arr = db->complex_glyphs.search(*str);
    
    if(arr && arr->length() > 0){
      cairo_text_extents_t cte;
      cairo_glyph_t        cg;
      
      result->composed = true;
      result->index    = arr->get(0).glyph;
      result->fontinfo = arr->get(0).font;
      result->right    = 0;
      result->x_offset = 0;
      
      for(int i = 0;i < arr->length();++i){
        context->canvas->set_font_face(font(arr->get(i).font));
        cg.x = 0;
        cg.y = 0;
        cg.index = arr->get(i).glyph;
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
        
        result->right+= cte.x_advance;
        
        if(arr->get(i).offset){
          result->right+= arr->get(i).offset 
            * context->canvas->get_font_size()
            * GlyphFontOffset::EmPerOffset;
        }
      }
      
      return;
    }
  }
  
  if(len > 1 && db->ligatures.size() > 0){
    Array<GlyphFontOffset> *arr = db->ligatures.search(String::FromUcs2(str, len));
    
    if(arr && arr->length() <= len){
      int i;
      for(i = 0;i < arr->length();++i){
        result[i].index    = arr->get(i).glyph;
        result[i].fontinfo = arr->get(i).font;
        
        context->canvas->set_font_face(font(result[i].fontinfo));
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = 0;
        cg.y = 0;
        cg.index = result[i].index;
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
        
        result[i].x_offset = 0;
        result[i].right = cte.x_advance;
        
        if(arr->get(i).offset){
          result[i].right+= arr->get(i).offset 
            * context->canvas->get_font_size()
            * GlyphFontOffset::EmPerOffset;
        }
      }
      
      for(;i < len;++i){
        result[i].index    = 0;
        result[i].fontinfo = 0;
      }
      
      return;
    }
  }
  
  while(len > 0){
    int sub_len = 0;
    int char_len = 1;
    GlyphFontOffset *gfo = 0;
    
    while(sub_len < len && !gfo){
      char_len = 1;
      
      if(sub_len + 1 < len 
      && is_utf16_high(str[sub_len])
      && is_utf16_low( str[sub_len+1])){
        uint32_t hi = str[sub_len];
        uint32_t lo = str[sub_len+1];
        uint32_t ch = 0x10000 + (((hi & 0x03FF) << 10) | (lo & 0x03FF));
        
        char_len = 2;
        gfo = db->char_to_glyph_map.search(ch);
      }
      else
        gfo = db->char_to_glyph_map.search(str[sub_len]);
      
      if(gfo)
        break;
        
      sub_len+= char_len;
    }
    
    if(sub_len > 0){
      text_shaper->decode_token(
        context, 
        sub_len, 
        str, 
        result);
      
      for(int i = 0;i < sub_len;++i)
        result[i].fontinfo+= math_font_faces.length();
    
      str   += sub_len;
      result+= sub_len;
      len   -= sub_len;
    }
    
    if(gfo){
      result->index    = gfo->glyph;
      result->fontinfo = gfo->font;
      
      if(char_len == 2){
        result[1].index    = 0;
        result[1].fontinfo = 0;
      }
      
      context->canvas->set_font_face(font(result->fontinfo));
      cairo_text_extents_t cte;
      cairo_glyph_t cg;
      cg.x = 0;
      cg.y = 0;
      cg.index = result->index;
      cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
      
      result->x_offset = 0;
      result->right = cte.x_advance;
      
      if(gfo && gfo->offset){
        result->right+= gfo->offset 
          * context->canvas->get_font_size()
          * GlyphFontOffset::EmPerOffset;
      }
      
      str   += char_len;
      result+= char_len;
      len   -= char_len;
    }
    else{
      assert(len == 0);
    }
  }
}

void ConfigShaper::vertical_glyph_size(
  Context         *context,
  const uint16_t   ch,
  const GlyphInfo &info,
  float           *ascent,
  float           *descent
){
  if(info.fontinfo >= math_font_faces.length()){
    GlyphInfo gi;
    memcpy(&gi, &info, sizeof(GlyphInfo));
    
    gi.fontinfo-= math_font_faces.length();
    text_shaper->vertical_glyph_size(context, ch, gi, ascent, descent);
    
    return;
  }
  
  if(info.composed){
    Array<GlyphFontOffset> *arr = db->complex_glyphs.search(ch);
    
    if(arr && arr->length() > 0){
      cairo_text_extents_t cte;
      cairo_glyph_t        cg;
      
      for(int i = 0;i < arr->length();++i){
        context->canvas->set_font_face(font(arr->get(i).font));
        cg.x = 0;
        cg.y = 0;
        cg.index = arr->get(i).glyph;
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
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
  Context         *context, 
  float            x,
  float            y,
  const uint16_t   ch,
  const GlyphInfo &info
){
  if(info.fontinfo >= (unsigned)math_font_faces.length()){
    GlyphInfo gi;
    memcpy(&gi, &info, sizeof(GlyphInfo));
    
    gi.fontinfo-= math_font_faces.length();
    text_shaper->show_glyph(context, x, y, ch, gi);
    
    return;
  }
  
  if(info.composed){
    Array<GlyphFontOffset> *arr = db->complex_glyphs.search(ch);
    
    if(arr && arr->length() > 0){
      cairo_text_extents_t cte;
      cairo_glyph_t        cg;
      
      cg.x = x + info.x_offset;
      cg.y = y;
      
      for(int i = 0;i < arr->length();++i){
        context->canvas->set_font_face(font(arr->get(i).font));
        cg.index = arr->get(i).glyph;
        context->canvas->show_glyphs(&cg, 1);
        
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
        cg.x+= cte.x_advance;
        
        if(arr->get(i).offset){
          cg.x+= arr->get(i).offset 
            * context->canvas->get_font_size()
            * GlyphFontOffset::EmPerOffset;
        }
      }
      
      return;
    }
  }
  
  SimpleMathShaper::show_glyph(context, x, y, ch, info);
}

float ConfigShaper::italic_correction(
  Context          *context,
  uint16_t          ch,
  const GlyphInfo  &info
){
  float result = db->italic_script_indent.center * GlyphFontOffset::EmPerOffset;
  
  uint32_t key;
  if(info.composed)
    key = (3 << 30) | ch;
  else
    key = (uint32_t)info.index | (((uint32_t)info.fontinfo) << 16);
  
  ScriptIndent *si = db->script_indents.search(key);
  if(si)
    return result + si->center * GlyphFontOffset::EmPerOffset;
  
  if(!info.composed){
    key = (1 << 31) | ch;
    
    si = db->script_indents.search(key);
    if(si)
      return result + si->center * GlyphFontOffset::EmPerOffset;
  }
  
  return result;
}

void ConfigShaper::script_corrections(
  Context           *context,
  uint16_t           base_char, 
  const GlyphInfo   &base_info,
  MathSequence          *sub,
  MathSequence          *super,
  float              sub_y,
  float              super_y,
  float             *sub_x,
  float             *super_x
){
  switch(base_info.slant){
    case FontSlantItalic: 
      if(!style.italic){
        math_set_style(style + Italic)->script_corrections(
          context, base_char, base_info, sub, super, sub_y, super_y, 
          sub_x, super_x);
        return;
      }
      break;
      
    case FontSlantPlain: 
      if(style.italic){
        math_set_style(style - Italic)->script_corrections(
          context, base_char, base_info, sub, super, sub_y, super_y, 
          sub_x, super_x);
        return;
      }
      break;
  }
  
  float em = context->canvas->get_font_size();
  *sub_x = *super_x = 0;
  
  if(style.italic){
    *super_x += db->italic_script_indent.super  * GlyphFontOffset::EmPerOffset * em;
    *sub_x   += db->italic_script_indent.center * GlyphFontOffset::EmPerOffset * em;
  }
  
  uint32_t key;
  if(base_info.composed)
    key = (3 << 30) | base_char;
  else
    key = (uint32_t)base_info.index | (((uint32_t)base_info.fontinfo) << 16);
  
  ScriptIndent *si = db->script_indents.search(key);
  if(si){
    *super_x += si->super  * GlyphFontOffset::EmPerOffset * em;
//    *center+= si->center * GlyphFontOffset::EmPerOffset * em;
    *sub_x   += si->sub    * GlyphFontOffset::EmPerOffset * em;
    
    return;
  }
  else if(!base_info.composed){
    key = (1 << 31) | base_char;
    
    si = db->script_indents.search(key);
    if(si){
      *super_x += si->super  * GlyphFontOffset::EmPerOffset * em;
//      *center+= si->center * GlyphFontOffset::EmPerOffset * em;
      *sub_x   += si->sub    * GlyphFontOffset::EmPerOffset * em;
      
      return;
    }
  }
}

void ConfigShaper::get_script_size_multis(Array<float> *arr){
  arr->operator=(db->script_size_multipliers);
}

SharedPtr<TextShaper> ConfigShaper::set_style(FontStyle _style){
  return db->find(_style);
}

int ConfigShaper::h_stretch_glyphs(
  uint16_t         ch,
  const uint8_t  **fonts, 
  const uint16_t **glyphs
){
  StretchGlyphArray *arr = db->stretched_glyphs.search(ch);
  
  if(arr && !arr->vertical){
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
){
  ComposedGlyph *cg = db->composed_glyphs.search(ch);
  
  if(cg && !cg->vertical){
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
){
  StretchGlyphArray *arr = db->stretched_glyphs.search(ch);
  
  if(arr && arr->vertical){
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
){
  ComposedGlyph *cg = db->composed_glyphs.search(ch);
  
  if(cg && cg->vertical){
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
){
  ComposedGlyph *cg = db->composed_glyphs.search(ch);
  
  if(cg && cg->vertical){
    *top            = cg->top;
    *middle         = cg->middle;
    *bottom         = cg->bottom;
    *special_center = cg->special_center;
    return cg->tbms_font;
  }
  
  *top = *middle = *bottom = *special_center = 0;
  return 0;
}

const SmallRadicalGlyph *ConfigShaper::small_radical_glyphs(){ // zero terminated
  return db->radical.small.items();
}

void ConfigShaper::big_radical_glyphs(
  uint16_t     *bottom,
  uint16_t     *vertical,
  uint16_t     *edge,
  uint16_t     *horizontal,
  float        *_rel_exp_x,
  float        *_rel_exp_y
){
  *bottom     = db->radical.big.bottom;
  *vertical   = db->radical.big.vertical;
  *edge       = db->radical.big.edge;
  *horizontal = db->radical.big.horizontal;
  *_rel_exp_x = db->radical.big.rel_exp_x;
  *_rel_exp_y = db->radical.big.rel_exp_y;
}
  
//} ... class ConfigShaper

