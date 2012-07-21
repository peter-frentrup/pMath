#include <util/style.h>

#include <gui/control-painter.h>
#include <eval/dynamic.h>

#include <climits>
#include <cmath>


using namespace richmath;

double round_factor(double x, double f) {
  x = floor(x * f + 0.5);
  x = x / f;
  return x;
}

double round_to_prec(double x, int p) {
  double y = 0.0;
  double f = 1.0;
  
  for(int dmax = 10; dmax > 0; --dmax) {
    y = round_factor(x, f);
    if(fabs(y * p - x * p) < 0.5)
      return y;
    f *= 10;
  }
  
  return y;
}

Expr richmath::color_to_pmath(int color) {
  if(color < 0)
    return Symbol(PMATH_SYMBOL_NONE);
    
  int r = (color & 0xFF0000) >> 16;
  int g = (color & 0x00FF00) >>  8;
  int b =  color & 0x0000FF;
  
  if(r == g && r == b) {
    return Call(
             Symbol(PMATH_SYMBOL_GRAYLEVEL),
             Number(round_to_prec(r / 255.0, 255)));
  }
  
  return Call(
           Symbol(PMATH_SYMBOL_RGBCOLOR),
           Number(round_to_prec(r / 255.0, 255)),
           Number(round_to_prec(g / 255.0, 255)),
           Number(round_to_prec(b / 255.0, 255)));
}

int richmath::pmath_to_color(Expr obj) {
  if(obj == PMATH_SYMBOL_NONE)
    return -1;
    
  if(obj.is_expr()) {
    if(obj[0] == PMATH_SYMBOL_RGBCOLOR) {
      if( obj.expr_length() == 1 &&
          obj[1][0] == PMATH_SYMBOL_LIST)
      {
        obj = obj[1];
      }
      
      if( obj.expr_length() == 3 &&
          obj[1].is_number() &&
          obj[2].is_number() &&
          obj[3].is_number())
      {
        double r = obj[1].to_double();
        double g = obj[2].to_double();
        double b = obj[3].to_double();
        
        if(r < 0) r = 0.0; else if(!(r <= 1)) r = 1.0;
        if(g < 0) g = 0.0; else if(!(g <= 1)) g = 1.0;
        if(b < 0) b = 0.0; else if(!(b <= 1)) b = 1.0;
        
        return ((int)(r * 255 + 0.5) << 16) | ((int)(g * 255 + 0.5) << 8) | (int)(b * 255 + 0.5);
      }
    }
    
    if(obj[0] == PMATH_SYMBOL_HUE) {
      if( obj.expr_length() == 1 &&
          obj[1][0] == PMATH_SYMBOL_LIST)
      {
        obj = obj[1];
      }
      
      if(obj.expr_length() >= 1 && obj.expr_length() <= 3) {
        for(int i = obj.expr_length(); i > 0; --i)
          if(!obj[i].is_number())
            return -1;
            
        double h, s = 1, v = 1;
        
        h = obj[1].to_double();
        h = fmod(h, 1.0);
        if(h < 0)
          h += 1.0;
          
        if(!(h >= 0 && h <= 1))
          return -1;
          
        if(obj.expr_length() >= 2) {
          s = obj[2].to_double();
          if(s < 0) s = 0; else if(!(s <= 1)) s = 1;
          
          if(obj.expr_length() >= 3) {
            v = obj[3].to_double();
            v = fmod(v, 1.0);
            if(v < 0) v = 0; else if(!(v <= 1)) v = 1;
          }
        }
        
        h *= 360;
        int hi = (int)(h / 60);
        double f = h / 60 - hi;
        double p = v * (1 - s);
        double q = v * (1 - s * f);
        double t = v * (1 - s * (1 - f));
        
        switch(hi) {
          case 0:
          case 6: return ((int)(v * 255 + 0.5) << 16) | ((int)(t * 255 + 0.5) << 8) | (int)(p * 255 + 0.5);
          case 1: return ((int)(q * 255 + 0.5) << 16) | ((int)(v * 255 + 0.5) << 8) | (int)(p * 255 + 0.5);
          case 2: return ((int)(p * 255 + 0.5) << 16) | ((int)(v * 255 + 0.5) << 8) | (int)(t * 255 + 0.5);
          case 3: return ((int)(p * 255 + 0.5) << 16) | ((int)(q * 255 + 0.5) << 8) | (int)(v * 255 + 0.5);
          case 4: return ((int)(t * 255 + 0.5) << 16) | ((int)(p * 255 + 0.5) << 8) | (int)(v * 255 + 0.5);
          case 5: return ((int)(v * 255 + 0.5) << 16) | ((int)(p * 255 + 0.5) << 8) | (int)(q * 255 + 0.5);
        }
      }
    }
    
    if( obj[0] == PMATH_SYMBOL_GRAYLEVEL &&
        obj.expr_length() == 1 &&
        obj[1].is_number())
    {
      double l = obj[1].to_double();
      if(l < 0) l = 0; else if(!(l <= 1)) l = 1;
      
      return ((int)(l * 255 + 0.5) << 16) | ((int)(l * 255 + 0.5) << 8) | (int)(l * 255 + 0.5);
    }
  }
  
  return -2;
}

static int rhs_to_buttonframe(Expr rhs) {
  if(rhs.is_string()) {
    String str(rhs);
    
    if(str.equals("Generic"))
      return GenericButton;
      
    if(str.equals("DialogBox"))
      return PushButton;
      
    if(str.equals("Defaulted"))
      return DefaultPushButton;
      
    if(str.equals("Palette"))
      return PaletteButton;
      
    if(str.equals("Frameless"))
      return FramelessButton;
      
    if(str.equals("TooltipWindow"))
      return TooltipWindow;
      
    if(str.equals("ListViewItemSelected"))
      return ListViewItemSelected;
      
    if(str.equals("ListViewItem"))
      return ListViewItem;
  }
  
  return -1;
}

static Expr buttonframe_to_rhs(int value) {
  switch(value) {
    case FramelessButton:
      return String("Frameless");
      
    case GenericButton:
      return String("Generic");
      
    case PushButton:
      return String("DialogBox");
      
    case DefaultPushButton:
      return String("Defaulted");
      
    case PaletteButton:
      return String("Palette");
      
    case TooltipWindow:
      return String("TooltipWindow");
      
    case ListViewItemSelected:
      return String("ListViewItemSelected");
      
    case ListViewItem:
      return String("ListViewItem");
      
    default:
      return Symbol(PMATH_SYMBOL_AUTOMATIC);
  }
}

static bool keep_dynamic = false;

//{ class Style ...

Style::Style(): Shareable() {
}

Style::Style(Expr options): Shareable() {
  add_pmath(options);
}

void Style::clear() {
  int_float_values.clear();
  object_values.clear();
}

void Style::add_pmath(Expr options) {
  if(options.is_string()) {
    set(BaseStyleName, String(options));
  }
  else if(options.is_expr()) {
    for(size_t i = 1; i <= options.expr_length(); ++i) {
      Expr rule = options[i];
      
      if(rule.is_rule()) {
        Expr lhs = rule[1];
        Expr rhs = rule[2];
        
//        // (a->b)->c  ===>  a->{b->c}
//        if(lhs.is_rule()) {
//          rhs = List(Rule(lhs[2], rhs));
//          lhs = lhs[1];
//        }

        if(rhs != PMATH_SYMBOL_INHERITED) {
          if(lhs == PMATH_SYMBOL_ANTIALIASING) {
            set_pmath_bool_auto(Antialiasing, rhs);
          }
          else if(lhs == PMATH_SYMBOL_ASPECTRATIO) {
            set_pmath_float(AspectRatio, rhs);
          }
          else if(lhs == PMATH_SYMBOL_AUTODELETE) {
            set_pmath_bool(AutoDelete, rhs);
          }
          else if(lhs == PMATH_SYMBOL_AUTONUMBERFORMATING) {
            set_pmath_bool(AutoNumberFormating, rhs);
          }
          else if(lhs == PMATH_SYMBOL_AUTOSPACING) {
            set_pmath_bool(AutoSpacing, rhs);
          }
          else if(lhs == PMATH_SYMBOL_BACKGROUND) {
            set_pmath_color(Background, rhs);
          }
          else if(lhs == PMATH_SYMBOL_BASESTYLE) {
            if(rhs[0] == PMATH_SYMBOL_DYNAMIC)
              set_dynamic(BaseStyleName, rhs);
            else
              add_pmath(rhs);
          }
          else if(lhs == PMATH_SYMBOL_BUTTONFRAME) {
            if(rhs[0] == PMATH_SYMBOL_DYNAMIC)
              set_dynamic(ButtonFrame, rhs);
            else
              set(ButtonFrame, rhs_to_buttonframe(rhs));
          }
          else if(lhs == PMATH_SYMBOL_BOXROTATION) {
            set_pmath_object(BoxRotation, rhs);
          }
          else if(lhs == PMATH_SYMBOL_BUTTONFUNCTION) {
            set(ButtonFunction, rhs);
          }
          else if(lhs == PMATH_SYMBOL_CONTINUOUSACTION) {
            set_pmath_bool(ContinuousAction, rhs);
          }
          else if(lhs == PMATH_SYMBOL_DEFAULTDUPLICATESECTIONSTYLE) {
            set(DefaultDuplicateSectionStyle, rhs);
          }
          else if(lhs == PMATH_SYMBOL_DEFAULTNEWSECTIONSTYLE) {
            set(DefaultNewSectionStyle, rhs);
          }
          else if(lhs == PMATH_SYMBOL_DEFAULTRETURNCREATEDSECTIONSTYLE) {
            set(DefaultReturnCreatedSectionStyle, rhs);
          }
          else if(lhs == PMATH_SYMBOL_DOCKEDSECTIONS) {
            set_docked_sections(rhs);
          }
          else if(lhs == PMATH_SYMBOL_EDITABLE) {
            set_pmath_bool(Editable, rhs);
          }
          else if(lhs == PMATH_SYMBOL_EVALUATABLE) {
            set_pmath_bool(Evaluatable, rhs);
          }
          else if(lhs == PMATH_SYMBOL_FONTCOLOR) {
            set_pmath_color(FontColor, rhs);
          }
          else if(lhs == PMATH_SYMBOL_FONTFAMILY) {
            set_pmath_string(FontFamily, rhs);
          }
          else if(lhs == PMATH_SYMBOL_FONTSIZE) {
            set_pmath_float(FontSize, rhs);
          }
          else if(lhs == PMATH_SYMBOL_FONTSLANT) {
            if(rhs == PMATH_SYMBOL_PLAIN)
              set(FontSlant, FontSlantPlain);
            else if(rhs == PMATH_SYMBOL_ITALIC)
              set(FontSlant, FontSlantItalic);
            else if(rhs[0] == PMATH_SYMBOL_DYNAMIC)
              set_dynamic(FontSlant, rhs);
          }
          else if(lhs == PMATH_SYMBOL_FONTWEIGHT) {
            if(rhs == PMATH_SYMBOL_PLAIN)
              set(FontWeight, FontWeightPlain);
            else if(rhs == PMATH_SYMBOL_BOLD)
              set(FontWeight, FontWeightBold);
            else if(rhs[0] == PMATH_SYMBOL_DYNAMIC)
              set_dynamic(FontWeight, rhs);
          }
          else if(lhs == PMATH_SYMBOL_GENERATEDSECTIONSTYLES) {
            set(GeneratedSectionStyles, rhs);
          }
          else if(lhs == PMATH_SYMBOL_GRIDBOXCOLUMNSPACING) {
            set_pmath_float(GridBoxColumnSpacing, rhs);
          }
          else if(lhs == PMATH_SYMBOL_GRIDBOXROWSPACING) {
            set_pmath_float(GridBoxRowSpacing, rhs);
          }
          else if(lhs == PMATH_SYMBOL_IMAGESIZE) {
            set_pmath_size(ImageSizeCommon, rhs);
          }
          else if(lhs == PMATH_SYMBOL_LANGUAGECATEGORY) {
            set_pmath_string(LanguageCategory, rhs);
          }
          else if(lhs == PMATH_SYMBOL_LINEBREAKWITHIN) {
            set_pmath_bool(LineBreakWithin, rhs);
          }
          else if(lhs == PMATH_SYMBOL_METHOD) {
            set_pmath_string(Method, rhs);
          }
          else if(lhs == PMATH_SYMBOL_PLACEHOLDER) {
            set_pmath_bool(Placeholder, rhs);
          }
          else if(lhs == PMATH_SYMBOL_PLOTRANGE) {
            set_pmath_object(PlotRange, rhs);
          }
          else if(lhs == PMATH_SYMBOL_RETURNCREATESNEWSECTION) {
            set_pmath_bool(ReturnCreatesNewSection, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SCRIPTSIZEMULTIPLIERS) {
            set_pmath_object(ScriptSizeMultipliers, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONEDITDUPLICATE) {
            set_pmath_bool(SectionEditDuplicate, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONEDITDUPLICATEMAKESCOPY) {
            set_pmath_bool(SectionEditDuplicateMakesCopy, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONFRAME) {
            set_pmath_margin(SectionFrameLeft, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONFRAMECOLOR) {
            set_pmath_color(SectionFrameColor, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONFRAMEMARGINS) {
            set_pmath_margin(SectionFrameMarginLeft, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONGENERATED) {
            set_pmath_bool(SectionGenerated, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONGROUPPRECEDENCE) {
            set_pmath_float(SectionGroupPrecedence, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONMARGINS) {
            set_pmath_margin(SectionMarginLeft, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONLABEL) {
            if(rhs == PMATH_SYMBOL_NONE)
              set(SectionLabel, String());
            else
              set_pmath_string(SectionLabel, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONLABELAUTODELETE) {
            set_pmath_bool(SectionLabelAutoDelete, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SELECTABLE) {
            set_pmath_bool(Selectable, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SHOWAUTOSTYLES) {
            set_pmath_bool(ShowAutoStyles, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SHOWSECTIONBRACKET) {
            set_pmath_bool(ShowSectionBracket, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SHOWSTRINGCHARACTERS) {
            set_pmath_bool(ShowStringCharacters, rhs);
          }
          else if(lhs == PMATH_SYMBOL_STRIPONINPUT) {
            set_pmath_bool(StripOnInput, rhs);
          }
          else if(lhs == PMATH_SYMBOL_STYLEDEFINITIONS) {
            set_pmath_object(StyleDefinitions, rhs);
          }
          else if(lhs == PMATH_SYMBOL_TEXTSHADOW) {
            set_pmath_object(TextShadow, rhs);
          }
          else if(lhs == PMATH_SYMBOL_WINDOWFRAME) {
            String s_rhs(rhs);
            if(s_rhs.equals("Normal"))
              set(WindowFrame, WindowFrameNormal);
            else if(s_rhs.equals("Palette"))
              set(WindowFrame, WindowFramePalette);
            else
              set_dynamic(WindowFrame, rhs);
              
            set(InternalHasModifiedWindowOption, true);
          }
          else if(lhs == PMATH_SYMBOL_WINDOWTITLE) {
            if(rhs == PMATH_SYMBOL_AUTOMATIC)
              set(WindowTitle, String());
            else
              set_pmath_string(WindowTitle, rhs);
              
            set(InternalHasModifiedWindowOption, true);
          }
          else {
            pmath_debug_print_object("[unknown option ", rule.get(), "]\n");
            
            Expr sym;
            if(!get(UnknownOptions, &sym) || !sym.is_symbol()) {
              sym = Expr(pmath_symbol_create_temporary(PMATH_C_STRING("FE`Styles`unknown"), TRUE));
              set(UnknownOptions, sym);
            }
            
            rule.set(1, Call(Symbol(PMATH_SYMBOL_HOLDPATTERN), Call(sym, lhs)));
            Expr eval = Call(Symbol(PMATH_SYMBOL_ASSIGN),
                             Call(Symbol(PMATH_SYMBOL_DOWNRULES), sym),
                             Call(Symbol(PMATH_SYMBOL_APPEND),
                                  Call(Symbol(PMATH_SYMBOL_DOWNRULES), sym),
                                  rule));
                                  
            Evaluate(eval);
          }
        }
      }
    }
  }
}

void Style::merge(SharedPtr<Style> other) {
  int_float_values.merge(other->int_float_values);
  
  Expr old_unknown_sym;
  Expr new_unknown_sym;
  get(UnknownOptions, &old_unknown_sym);
  other->get(UnknownOptions, &new_unknown_sym);
  
  object_values.merge(other->object_values);
  
  if(old_unknown_sym.is_symbol() && new_unknown_sym.is_symbol()) {
    Expr eval = Parse(
                  "OwnRules(`2`):= Join("
                  "Replace(OwnRules(`1`), {HoldPattern(`1`) :> `2`}, Heads->True),"
                  "OwnRules(`2`))",
                  old_unknown_sym,
                  new_unknown_sym);
                  
    Evaluate(eval);
  }
}

bool Style::get(IntStyleOptionName n, int *value) {
  IntFloatUnion *v = int_float_values.search(n);
  
  if(!v)
    return false;
    
  *value = v->int_value;
  return true;
}

bool Style::get(FloatStyleOptionName n, float *value) {
  IntFloatUnion *v = int_float_values.search(n);
  
  if(!v)
    return false;
    
  *value = v->float_value;
  return true;
}

bool Style::get(StringStyleOptionName n, String *value) {
  Expr *v = object_values.search(n);
  
  if(!v || !v->is_string())
    return false;
    
  *value = String(*v);
  return true;
}

bool Style::get(ObjectStyleOptionName n, Expr *value) {
  Expr *v = object_values.search(n);
  
  if(!v)
    return false;
    
  *value = *v;
  return true;
}

void Style::set(IntStyleOptionName n, int value) {
  IntFloatUnion v;
  v.int_value = value;
  int_float_values.set(n, v);
  
  if(!keep_dynamic)
    remove_dynamic(n);
}

void Style::set(FloatStyleOptionName n, float value) {
  IntFloatUnion v;
  v.float_value = value;
  int_float_values.set(n, v);
  
  if(!keep_dynamic)
    remove_dynamic(n);
}

void Style::set(StringStyleOptionName n, String value) {
  object_values.set(n, value);
  
  if(!keep_dynamic)
    remove_dynamic(n);
}

void Style::set(ObjectStyleOptionName n, Expr value) {
  object_values.set(n, value);
  
  if(!keep_dynamic)
    remove_dynamic(n);
}

void Style::remove(IntStyleOptionName n) {
  int_float_values.remove(n);
}

void Style::remove(FloatStyleOptionName n) {
  int_float_values.remove(n);
}

void Style::remove(StringStyleOptionName n) {
  object_values.remove(n);
}

void Style::remove(ObjectStyleOptionName n) {
  object_values.remove(n);
}

void Style::set_pmath_bool_auto(IntStyleOptionName n, Expr obj) {
  if(obj == PMATH_SYMBOL_FALSE)
    set(n, 0);
  else if(obj == PMATH_SYMBOL_TRUE)
    set(n, 1);
  else if(obj == PMATH_SYMBOL_AUTOMATIC)
    set(n, 2);
  else if(obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
}

void Style::set_pmath_bool(IntStyleOptionName n, Expr obj) {
  if(obj == PMATH_SYMBOL_TRUE)
    set(n, true);
  else if(obj == PMATH_SYMBOL_FALSE)
    set(n, false);
  else if(obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
}

void Style::set_pmath_color(IntStyleOptionName n, Expr obj) {
  int c = pmath_to_color(obj);
  
  if(c >= -1)
    set(n, c);
  else if(obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
}

void Style::set_pmath_float(FloatStyleOptionName n, Expr obj) {
  if(obj.is_number()) {
    set(n, obj.to_double());
    return;
  }
  
  if(obj[0] == PMATH_SYMBOL_DYNAMIC) {
    set_dynamic(n, obj);
    return;
  }
  
  if(obj[0] == PMATH_SYMBOL_NCACHE) {
    set(n, obj[2].to_double());
    
    if(!keep_dynamic)
      set((ObjectStyleOptionName)(n + DynamicOffset), obj);
      
    return;
  }
}

void Style::set_pmath_size(FloatStyleOptionName n, Expr obj) {
  FloatStyleOptionName Horizontal = FloatStyleOptionName(n + 1);
  FloatStyleOptionName Vertical   = FloatStyleOptionName(n + 2);
  
  if(obj == PMATH_SYMBOL_AUTOMATIC) {
    remove(n);
    if(!keep_dynamic)
      remove_dynamic(n);
      
    set(Horizontal, ImageSizeAutomatic);
    set(Vertical,   ImageSizeAutomatic);
    return;
  }
  
  if(obj.is_number()) {
    remove(n);
    if(!keep_dynamic)
      remove_dynamic(n);
      
    float f = obj.to_double();
    set(Horizontal, f);
    set(Vertical,   ImageSizeAutomatic);
    return;
  }
  
  if(obj[0] == PMATH_SYMBOL_LIST &&
      obj.expr_length() == 2)
  {
    remove(n);
    if(!keep_dynamic)
      remove_dynamic(n);
      
    if(obj[1] == PMATH_SYMBOL_AUTOMATIC)
      set(Horizontal, ImageSizeAutomatic);
    else
      set_pmath_float(Horizontal, obj[1]);
      
    if(obj[2] == PMATH_SYMBOL_AUTOMATIC)
      set(Vertical, ImageSizeAutomatic);
    else
      set_pmath_float(Vertical, obj[2]);
    return;
  }
  
  if(obj[0] == PMATH_SYMBOL_DYNAMIC) {
    remove(n);
    remove(Horizontal);
    remove(Vertical);
    remove((ObjectStyleOptionName)(Horizontal + DynamicOffset));
    remove((ObjectStyleOptionName)(Vertical   + DynamicOffset));
    set_dynamic(n, obj);
    return;
  }
  
  if(obj[0] == PMATH_SYMBOL_NCACHE) {
    set_pmath_size(n, obj[2]);
    
    if(!keep_dynamic)
      set((ObjectStyleOptionName)(n + DynamicOffset), obj);
      
    return;
  }
}

void Style::set_pmath_margin(FloatStyleOptionName n, Expr obj) {
  FloatStyleOptionName Left   = n;
  FloatStyleOptionName Right  = FloatStyleOptionName(n + 1);
  FloatStyleOptionName Top    = FloatStyleOptionName(n + 2);
  FloatStyleOptionName Bottom = FloatStyleOptionName(n + 3);
  
  if(obj == PMATH_SYMBOL_TRUE) {
    set(Left,   1.0);
    set(Right,  1.0);
    set(Top,    1.0);
    set(Bottom, 1.0);
    return;
  }
  
  if(obj == PMATH_SYMBOL_FALSE) {
    set(Left,   0.0);
    set(Right,  0.0);
    set(Top,    0.0);
    set(Bottom, 0.0);
    return;
  }
  
  if(obj.is_number()) {
    float f = obj.to_double();
    set(Left,   f);
    set(Right,  f);
    set(Top,    f);
    set(Bottom, f);
    return;
  }
  
  if(obj.is_expr()
      && obj[0] == PMATH_SYMBOL_LIST) {
    if(obj.expr_length() == 4) {
      set_pmath_float(Left,   obj[1]);
      set_pmath_float(Right,  obj[2]);
      set_pmath_float(Top,    obj[3]);
      set_pmath_float(Bottom, obj[4]);
      return;
    }
    
    if(obj.expr_length() == 2) {
      if(obj[1].is_number()) {
        float f = obj[1].to_double();
        set(Left,  f);
        set(Right, f);
      }
      else if(obj[1].is_expr()
              && obj[1][0] == PMATH_SYMBOL_LIST
              && obj[1].expr_length() == 2) {
        set_pmath_float(Left,  obj[1][1]);
        set_pmath_float(Right, obj[1][2]);
      }
      
      if(obj[2].is_number()) {
        float f = obj[2].to_double();
        set(Top,    f);
        set(Bottom, f);
      }
      else if(obj[2].is_expr()
              && obj[2][0] == PMATH_SYMBOL_LIST
              && obj[2].expr_length() == 2) {
        set_pmath_float(Top,    obj[2][1]);
        set_pmath_float(Bottom, obj[2][2]);
      }
      
      return;
    }
  }
  
  if(obj[0] == PMATH_SYMBOL_DYNAMIC) {
    remove(Left);
    remove(Right);
    remove(Top);
    remove(Bottom);
    set_dynamic(Left, obj);
  }
}

void Style::set_pmath_string(StringStyleOptionName n, Expr obj) {
  if(obj.is_string())
    set(n, String(obj));
  else if(obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
}

void Style::set_pmath_object(ObjectStyleOptionName n, Expr obj) {
  if(obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
  else
    set(n, obj);
}

void Style::set_docked_sections(Expr obj) {

  if(obj[0] != PMATH_SYMBOL_LIST) {
    return;
  }
  
  for(size_t i = 1; i <= obj.expr_length(); ++i) {
    Expr rule = obj[i];
    
    if(!rule.is_rule())
      continue;
      
    String lhs(rule[1]);
    Expr rhs(rule[2]);
    
    if(rhs == PMATH_SYMBOL_INHERITED)
      continue;
      
    if(lhs.equals("Top"))
      set_pmath_object(DockedSectionsTop, rhs);
    else if(lhs.equals("TopGlass"))
      set_pmath_object(DockedSectionsTopGlass, rhs);
    else if(lhs.equals("Bottom"))
      set_pmath_object(DockedSectionsBottom, rhs);
    else if(lhs.equals("BottomGlass"))
      set_pmath_object(DockedSectionsBottomGlass, rhs);
      
  }
  
  set(InternalHasModifiedWindowOption, true);
}

bool Style::modifies_size(int style_name) {
  switch(style_name) {
    case Background:
    case FontColor:
    case SectionFrameColor:
    case AutoDelete:
    case ContinuousAction:
    case Editable:
    case Evaluatable:
    case InternalHasModifiedWindowOption:
    case InternalHasPendingDynamic:
    case InternalUsesCurrentValueOfMouseOver:
    case Placeholder:
    case ReturnCreatesNewSection:
    case SectionEditDuplicate:
    case SectionEditDuplicateMakesCopy:
    case SectionGenerated:
    case SectionLabelAutoDelete:
    case Selectable:
    case ShowAutoStyles: // only modifies color
    case StripOnInput:
    
    case LanguageCategory:
    case Method:
    case SectionLabel:
    case WindowTitle:
    
    case ButtonFunction:
    case ScriptSizeMultipliers:
    case TextShadow:
    case DefaultDuplicateSectionStyle:
    case DefaultNewSectionStyle:
    case DefaultReturnCreatedSectionStyle:
    case GeneratedSectionStyles:
      return false;
  }
  
  return true;
}

bool Style::update_dynamic(Box *parent) {
  if(!parent)
    return false;
    
  int i;
  if(!get(InternalHasPendingDynamic, &i) || !i)
    return false;
    
  set(InternalHasPendingDynamic, false);
  
  static Array<int> dynamic_options(100);
  
  dynamic_options.length(0);
  
  unsigned cnt = object_values.size();
  for(unsigned ui = 0; cnt > 0; ++ui) {
    Entry<int, Expr> *e = object_values.entry(ui);
    
    if(e) {
      --cnt;
      
      if(e->key >= DynamicOffset)
        dynamic_options.add(e->key - DynamicOffset);
    }
  }
  
  if(dynamic_options.length() == 0)
    return false;
    
  set(InternalHasPendingDynamic, false);
  
  bool resize = false;
  for(i = 0; i < dynamic_options.length(); ++i) {
    if(modifies_size(dynamic_options[i])) {
      resize = true;
      break;
    }
  }
  
  {
    Gather g;
    for(i = 0; i < dynamic_options.length(); ++i) {
      Expr e = object_values[DynamicOffset + dynamic_options[i]];
      Dynamic dyn(parent, e);
      
      e = dyn.get_value_now();
      
      if(e != PMATH_SYMBOL_ABORTED && e[0] != PMATH_SYMBOL_DYNAMIC)
        Gather::emit(Rule(get_symbol(dynamic_options[i]), e));
    }
    
    keep_dynamic = true;
    add_pmath(g.end());
    keep_dynamic = false;
  }
  
  if(resize)
    parent->invalidate();
  else
    parent->request_repaint_all();
    
  return true;
}

unsigned int Style::count() {
  return int_float_values.size() + object_values.size();
}

Expr Style::get_symbol(int n) {
  switch(n) {
    case Background:                          return Symbol(PMATH_SYMBOL_BACKGROUND);
    case FontColor:                           return Symbol(PMATH_SYMBOL_FONTCOLOR);
    case SectionFrameColor:                   return Symbol(PMATH_SYMBOL_SECTIONFRAMECOLOR);
    
    case Antialiasing:                        return Symbol(PMATH_SYMBOL_ANTIALIASING);
    
    case FontSlant:                           return Symbol(PMATH_SYMBOL_FONTSLANT);
    case FontWeight:                          return Symbol(PMATH_SYMBOL_FONTWEIGHT);
    
    case AutoDelete:                          return Symbol(PMATH_SYMBOL_AUTODELETE);
    case AutoNumberFormating:                 return Symbol(PMATH_SYMBOL_AUTONUMBERFORMATING);
    case AutoSpacing:                         return Symbol(PMATH_SYMBOL_AUTOSPACING);
    case ContinuousAction:                    return Symbol(PMATH_SYMBOL_CONTINUOUSACTION);
    case Editable:                            return Symbol(PMATH_SYMBOL_EDITABLE);
    case Evaluatable:                         return Symbol(PMATH_SYMBOL_EVALUATABLE);
    case InternalHasModifiedWindowOption:     return Expr();
    case InternalHasPendingDynamic:           return Expr();
    case InternalUsesCurrentValueOfMouseOver: return Expr();
    case LineBreakWithin:                     return Symbol(PMATH_SYMBOL_LINEBREAKWITHIN);
    case Placeholder:                         return Symbol(PMATH_SYMBOL_PLACEHOLDER);
    case ReturnCreatesNewSection:             return Symbol(PMATH_SYMBOL_RETURNCREATESNEWSECTION);
    case SectionEditDuplicate:                return Symbol(PMATH_SYMBOL_SECTIONEDITDUPLICATE);
    case SectionEditDuplicateMakesCopy:       return Symbol(PMATH_SYMBOL_SECTIONEDITDUPLICATEMAKESCOPY);
    case SectionGenerated:                    return Symbol(PMATH_SYMBOL_SECTIONGENERATED);
    case SectionLabelAutoDelete:              return Symbol(PMATH_SYMBOL_SECTIONLABELAUTODELETE);
    case Selectable:                          return Symbol(PMATH_SYMBOL_SELECTABLE);
    case ShowAutoStyles:                      return Symbol(PMATH_SYMBOL_SHOWAUTOSTYLES);
    case ShowSectionBracket:                  return Symbol(PMATH_SYMBOL_SHOWSECTIONBRACKET);
    case ShowStringCharacters:                return Symbol(PMATH_SYMBOL_SHOWSTRINGCHARACTERS);
    case StripOnInput:                        return Symbol(PMATH_SYMBOL_STRIPONINPUT);
    
    case ButtonFrame:                         return Symbol(PMATH_SYMBOL_BUTTONFRAME);
    case WindowFrame:                         return Symbol(PMATH_SYMBOL_WINDOWFRAME);
  }
  
  switch(n) {
    case FontSize:                 return Symbol(PMATH_SYMBOL_FONTSIZE);
    
    case AspectRatio:              return Symbol(PMATH_SYMBOL_ASPECTRATIO);
    
    case GridBoxColumnSpacing:     return Symbol(PMATH_SYMBOL_GRIDBOXCOLUMNSPACING);
    case GridBoxRowSpacing:        return Symbol(PMATH_SYMBOL_GRIDBOXROWSPACING);
    
    case ImageSizeCommon:          return Symbol(PMATH_SYMBOL_IMAGESIZE);
    case ImageSizeHorizontal:
    case ImageSizeVertical:        return Expr();
    
    case SectionMarginLeft:        return Symbol(PMATH_SYMBOL_SECTIONMARGINS);
    case SectionMarginRight:
    case SectionMarginTop:
    case SectionMarginBottom:      return Expr();
    
    case SectionFrameLeft:         return Symbol(PMATH_SYMBOL_SECTIONFRAME);
    case SectionFrameRight:
    case SectionFrameTop:
    case SectionFrameBottom:       return Expr();
    
    case SectionFrameMarginLeft:   return Symbol(PMATH_SYMBOL_SECTIONFRAMEMARGINS);
    case SectionFrameMarginRight:
    case SectionFrameMarginTop:
    case SectionFrameMarginBottom: return Expr();
    
    case SectionGroupPrecedence:   return Symbol(PMATH_SYMBOL_SECTIONGROUPPRECEDENCE);
  }
  
  switch(n) {
    case BaseStyleName:    return Symbol(PMATH_SYMBOL_BASESTYLE);
    case FontFamily:       return Symbol(PMATH_SYMBOL_FONTFAMILY);
    case Method:           return Symbol(PMATH_SYMBOL_METHOD);
    
    case LanguageCategory: return Symbol(PMATH_SYMBOL_LANGUAGECATEGORY);
    case SectionLabel:     return Symbol(PMATH_SYMBOL_SECTIONLABEL);
    
    case WindowTitle:      return Symbol(PMATH_SYMBOL_WINDOWTITLE);
  }
  
  switch(n) {
    case ButtonFunction:                   return Symbol(PMATH_SYMBOL_BUTTONFUNCTION);
    case ScriptSizeMultipliers:            return Symbol(PMATH_SYMBOL_SCRIPTSIZEMULTIPLIERS);
    case TextShadow:                       return Symbol(PMATH_SYMBOL_TEXTSHADOW);
    case UnknownOptions:                   return Expr();
    
    case BoxRotation:                      return Symbol(PMATH_SYMBOL_BOXROTATION);
    case BoxTransformation:                return Symbol(PMATH_SYMBOL_BOXTRANSFORMATION);
    case PlotRange:                        return Symbol(PMATH_SYMBOL_PLOTRANGE);
    
    case DefaultDuplicateSectionStyle:     return Symbol(PMATH_SYMBOL_DEFAULTDUPLICATESECTIONSTYLE);
    case DefaultNewSectionStyle:           return Symbol(PMATH_SYMBOL_DEFAULTNEWSECTIONSTYLE);
    case DefaultReturnCreatedSectionStyle: return Symbol(PMATH_SYMBOL_DEFAULTRETURNCREATEDSECTIONSTYLE);
    
    case DockedSectionsTop:                return Symbol(PMATH_SYMBOL_DOCKEDSECTIONS);
    case DockedSectionsTopGlass:
    case DockedSectionsBottom:
    case DockedSectionsBottomGlass:        return Expr();
    
    case StyleDefinitions:                 return Symbol(PMATH_SYMBOL_STYLEDEFINITIONS);
    case GeneratedSectionStyles:           return Symbol(PMATH_SYMBOL_GENERATEDSECTIONSTYLES);
  }
  
  return Expr();
}

void Style::emit_to_pmath(bool with_inherited) {
  Expr e;
  String s;
  int i;
  float f;
  
  if(get_dynamic(Antialiasing, &e)) {
    Gather::emit(Rule(
                   get_symbol(Antialiasing),
                   e));
  }
  else if(get(Antialiasing, &i)) {
    Gather::emit(Rule(
                   get_symbol(Antialiasing),
                   Symbol(i == 0 ? PMATH_SYMBOL_FALSE :
                          (i == 1 ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_AUTOMATIC))));
  }
  
  if(get_dynamic(AspectRatio, &e)) {
    Gather::emit(Rule(
                   get_symbol(AspectRatio),
                   e));
  }
  else if(get(AspectRatio, &f)) {
    Gather::emit(Rule(
                   get_symbol(AspectRatio),
                   Number(f)));
  }
  
  if(get_dynamic(AutoDelete, &e)) {
    Gather::emit(Rule(
                   get_symbol(AutoDelete),
                   e));
  }
  else if(get(AutoDelete, &i)) {
    Gather::emit(Rule(
                   get_symbol(AutoDelete),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(AutoNumberFormating, &e)) {
    Gather::emit(Rule(
                   get_symbol(AutoNumberFormating),
                   e));
  }
  else if(get(AutoNumberFormating, &i)) {
    Gather::emit(Rule(
                   get_symbol(AutoNumberFormating),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(AutoSpacing, &e)) {
    Gather::emit(Rule(
                   get_symbol(AutoSpacing),
                   e));
  }
  else if(get(AutoSpacing, &i)) {
    Gather::emit(Rule(
                   get_symbol(AutoSpacing),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(Background, &e)) {
    Gather::emit(Rule(
                   get_symbol(Background),
                   e));
  }
  else if(get(Background, &i)) {
    Gather::emit(Rule(
                   get_symbol(Background),
                   color_to_pmath(i)));
  }
  
  if(with_inherited) {
    if(get_dynamic(BaseStyleName, &e)) {
      Gather::emit(Rule(
                     get_symbol(BaseStyleName),
                     e));
    }
    else if(get(BaseStyleName, &s)) {
      Gather::emit(Rule(
                     get_symbol(BaseStyleName),
                     s));
    }
  }
  
  if(get_dynamic(BoxRotation, &e)) {
    Gather::emit(Rule(
                   get_symbol(BoxRotation),
                   e));
  }
  else if(get(BoxRotation, &e)) {
    Gather::emit(Rule(
                   get_symbol(BoxRotation),
                   e));
  }
  
  if(get_dynamic(BoxTransformation, &e)) {
    Gather::emit(Rule(
                   get_symbol(BoxTransformation),
                   e));
  }
  else if(get(BoxTransformation, &e)) {
    Gather::emit(Rule(
                   get_symbol(BoxTransformation),
                   e));
  }
  
  if(get_dynamic(ButtonFrame, &e)) {
    Gather::emit(Rule(
                   get_symbol(ButtonFrame),
                   e));
  }
  else if(get(ButtonFrame, &i)) {
    Gather::emit(Rule(
                   get_symbol(ButtonFrame),
                   buttonframe_to_rhs(i)));
  }
  
  if(get_dynamic(ButtonFunction, &e)) {
    Gather::emit(Rule(
                   get_symbol(ButtonFunction),
                   e));
  }
  else if(get(ButtonFunction, &e)) {
    Gather::emit(Rule(
                   get_symbol(ButtonFunction),
                   e));
  }
  
  if(get_dynamic(ContinuousAction, &e)) {
    Gather::emit(Rule(
                   get_symbol(ContinuousAction),
                   e));
  }
  else if(get(ContinuousAction, &i)) {
    Gather::emit(Rule(
                   get_symbol(ContinuousAction),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(DefaultDuplicateSectionStyle, &e)) {
    Gather::emit(Rule(
                   get_symbol(DefaultDuplicateSectionStyle),
                   e));
  }
  else if(get(DefaultDuplicateSectionStyle, &e)) {
    Gather::emit(Rule(
                   get_symbol(DefaultDuplicateSectionStyle),
                   e));
  }
  
  if(get_dynamic(DefaultNewSectionStyle, &e)) {
    Gather::emit(Rule(
                   get_symbol(DefaultNewSectionStyle),
                   e));
  }
  else if(get(DefaultNewSectionStyle, &e)) {
    Gather::emit(Rule(
                   get_symbol(DefaultNewSectionStyle),
                   e));
  }
  
  if(get_dynamic(DefaultReturnCreatedSectionStyle, &e)) {
    Gather::emit(Rule(
                   get_symbol(DefaultReturnCreatedSectionStyle),
                   e));
  }
  else if(get(DefaultReturnCreatedSectionStyle, &e)) {
    Gather::emit(Rule(
                   get_symbol(DefaultReturnCreatedSectionStyle),
                   e));
  }
  
  if(get_dynamic(DockedSectionsTop, &e)) {
    Gather::emit(Rule(
                   get_symbol(DockedSectionsTop),
                   e));
  }
  else {
    Expr top, top_glass, bottom, bottom_glass;
    bool have_top, have_top_glass, have_bottom, have_bottom_glass;
    
    have_top          = get(DockedSectionsTop,         &top);
    have_top_glass    = get(DockedSectionsTopGlass,    &top_glass);
    have_bottom       = get(DockedSectionsBottom,      &bottom);
    have_bottom_glass = get(DockedSectionsBottomGlass, &bottom_glass);
    
    if(have_top || have_top_glass || have_bottom || have_bottom_glass) {
      Gather g;
      
      if(have_top)
        Gather::emit(Rule(String("Top"), top));
        
      if(have_top_glass)
        Gather::emit(Rule(String("TopGlass"), top_glass));
        
      if(have_bottom)
        Gather::emit(Rule(String("Bottom"), bottom));
        
      if(have_bottom_glass)
        Gather::emit(Rule(String("BottomGlass"), bottom_glass));
        
      Expr all = g.end();
      
      Gather::emit(Rule(
                     get_symbol(DockedSectionsTop),
                     all));
    }
  }
  
  if(get_dynamic(Editable, &e)) {
    Gather::emit(Rule(
                   get_symbol(Editable),
                   e));
  }
  else if(get(Editable, &i)) {
    Gather::emit(Rule(
                   get_symbol(Editable),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(Evaluatable, &e)) {
    Gather::emit(Rule(
                   get_symbol(Evaluatable),
                   e));
  }
  else if(get(Evaluatable, &i)) {
    Gather::emit(Rule(
                   get_symbol(Evaluatable),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(FontColor, &e)) {
    Gather::emit(Rule(
                   get_symbol(FontColor),
                   e));
  }
  else if(get(FontColor, &i)) {
    Gather::emit(Rule(
                   get_symbol(FontColor),
                   color_to_pmath(i)));
  }
  
  if(get_dynamic(FontFamily, &e)) {
    Gather::emit(Rule(
                   get_symbol(FontFamily),
                   e));
  }
  else if(get(FontFamily, &s)) {
    Gather::emit(Rule(
                   get_symbol(FontFamily),
                   s));
  }
  
  if(get_dynamic(FontSize, &e)) {
    Gather::emit(Rule(
                   get_symbol(FontSize),
                   e));
  }
  else if(get(FontSize, &f)) {
    Gather::emit(Rule(
                   get_symbol(FontSize),
                   Number(f)));
  }
  
  if(get_dynamic(FontSlant, &e)) {
    Gather::emit(Rule(
                   get_symbol(FontSlant),
                   e));
  }
  else if(get(FontSlant, &i)) {
    switch(i) {
      case FontSlantPlain:
        Gather::emit(Rule(
                       get_symbol(FontSlant),
                       Symbol(PMATH_SYMBOL_PLAIN)));
        break;
        
      case FontSlantItalic:
        Gather::emit(Rule(
                       get_symbol(FontSlant),
                       Symbol(PMATH_SYMBOL_ITALIC)));
        break;
    }
  }
  
  if(get_dynamic(FontWeight, &e)) {
    Gather::emit(Rule(
                   get_symbol(FontWeight),
                   e));
  }
  else if(get(FontWeight, &i)) {
    switch(i) {
      case FontWeightPlain:
        Gather::emit(Rule(
                       get_symbol(FontWeight),
                       Symbol(PMATH_SYMBOL_PLAIN)));
        break;
        
      case FontWeightBold:
        Gather::emit(Rule(
                       get_symbol(FontWeight),
                       Symbol(PMATH_SYMBOL_BOLD)));
        break;
    }
  }
  
  if(get_dynamic(GeneratedSectionStyles, &e)) {
    Gather::emit(Rule(
                   get_symbol(GeneratedSectionStyles),
                   e));
  }
  else if(get(GeneratedSectionStyles, &e)) {
    Gather::emit(Rule(
                   get_symbol(GeneratedSectionStyles),
                   e));
  }
  
  if(get_dynamic(GridBoxColumnSpacing, &e)) {
    Gather::emit(Rule(
                   get_symbol(GridBoxColumnSpacing),
                   e));
  }
  else if(get(GridBoxColumnSpacing, &f)) {
    Gather::emit(Rule(
                   get_symbol(GridBoxColumnSpacing),
                   Number(f)));
  }
  
  if(get_dynamic(GridBoxRowSpacing, &e)) {
    Gather::emit(Rule(
                   get_symbol(GridBoxRowSpacing),
                   e));
  }
  else if(get(GridBoxRowSpacing, &f)) {
    Gather::emit(Rule(
                   get_symbol(GridBoxRowSpacing),
                   Number(f)));
  }
  
  if(get_dynamic(ImageSizeCommon, &e)) {
    Gather::emit(Rule(
                   get_symbol(ImageSizeCommon),
                   e));
  }
  else {
    bool have_horz, have_vert;
    Expr horz, vert;
    
    have_horz = get_dynamic(ImageSizeHorizontal, &horz);
    if(!have_horz) {
      float h;
      have_horz = get(ImageSizeHorizontal, &h);
      
      if(have_horz) {
        if(h > 0)
          horz = Number(h);
        else
          horz = Symbol(PMATH_SYMBOL_AUTOMATIC);
      }
      else
        horz = Symbol(PMATH_SYMBOL_INHERITED);
    }
    
    have_vert = get_dynamic(ImageSizeVertical, &vert);
    if(!have_vert) {
      float v;
      have_vert = get(ImageSizeVertical, &v);
      
      if(have_vert) {
        if(v > 0)
          vert = Number(v);
        else
          vert = Symbol(PMATH_SYMBOL_AUTOMATIC);
      }
      else
        vert = Symbol(PMATH_SYMBOL_INHERITED);
    }
    
    if(have_horz || have_vert) {
      Gather::emit(Rule(
                     get_symbol(ImageSizeCommon),
                     List(horz, vert)));
    }
  }
  
  if(get_dynamic(LanguageCategory, &e)) {
    Gather::emit(Rule(
                   get_symbol(LanguageCategory),
                   e));
  }
  else if(get(LanguageCategory, &s)) {
    Gather::emit(Rule(
                   get_symbol(LanguageCategory),
                   s));
  }
  
  if(get_dynamic(LineBreakWithin, &e)) {
    Gather::emit(Rule(
                   get_symbol(LineBreakWithin),
                   e));
  }
  else if(get(LineBreakWithin, &i)) {
    Gather::emit(Rule(
                   get_symbol(LineBreakWithin),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(Method, &e)) {
    Gather::emit(Rule(
                   get_symbol(Method),
                   e));
  }
  else if(get(Method, &s)) {
    Gather::emit(Rule(
                   get_symbol(Method),
                   s));
  }
  
  if(get_dynamic(Placeholder, &e)) {
    Gather::emit(Rule(
                   get_symbol(Placeholder),
                   e));
  }
  else if(get(Placeholder, &i)) {
    Gather::emit(Rule(
                   get_symbol(Placeholder),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(PlotRange, &e)) {
    Gather::emit(Rule(
                   get_symbol(PlotRange),
                   e));
  }
  else if(get(PlotRange, &e)) {
    Gather::emit(Rule(
                   get_symbol(PlotRange),
                   e));
  }
  
  if(get_dynamic(ReturnCreatesNewSection, &e)) {
    Gather::emit(Rule(
                   get_symbol(ReturnCreatesNewSection),
                   e));
  }
  else if(get(ReturnCreatesNewSection, &i)) {
    Gather::emit(Rule(
                   get_symbol(ReturnCreatesNewSection),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(ScriptSizeMultipliers, &e)) {
    Gather::emit(Rule(
                   get_symbol(ScriptSizeMultipliers),
                   e));
  }
  else if(get(ScriptSizeMultipliers, &e)) {
    Gather::emit(Rule(
                   get_symbol(ScriptSizeMultipliers),
                   e));
  }
  
  if(get_dynamic(SectionEditDuplicate, &e)) {
    Gather::emit(Rule(
                   get_symbol(SectionEditDuplicate),
                   e));
  }
  else if(get(SectionEditDuplicate, &i)) {
    Gather::emit(Rule(
                   get_symbol(SectionEditDuplicate),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(SectionEditDuplicateMakesCopy, &e)) {
    Gather::emit(Rule(
                   get_symbol(SectionEditDuplicateMakesCopy),
                   e));
  }
  else if(get(SectionEditDuplicateMakesCopy, &i)) {
    Gather::emit(Rule(
                   get_symbol(SectionEditDuplicateMakesCopy),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(SectionFrameLeft, &e)) {
    Gather::emit(Rule(
                   get_symbol(SectionFrameLeft),
                   e));
  }
  else {
    float left, right, top, bottom;
    bool have_left, have_right, have_top, have_bottom;
    
    have_left   = get(SectionFrameLeft,   &left);
    have_right  = get(SectionFrameRight,  &right);
    have_top    = get(SectionFrameTop,    &top);
    have_bottom = get(SectionFrameBottom, &bottom);
    if(have_left || have_right || have_top || have_bottom) {
      Expr l, r, t, b;
      
      if(have_left)
        l = Number(left);
      else
        l = Symbol(PMATH_SYMBOL_INHERITED);
        
      if(have_right)
        r = Number(right);
      else
        r = Symbol(PMATH_SYMBOL_INHERITED);
        
      if(have_top)
        t = Number(top);
      else
        t = Symbol(PMATH_SYMBOL_INHERITED);
        
      if(have_bottom)
        b = Number(bottom);
      else
        b = Symbol(PMATH_SYMBOL_INHERITED);
        
      Gather::emit(Rule(
                     get_symbol(SectionFrameLeft),
                     List(l, r, t, b)));
    }
  }
  
  if(get_dynamic(SectionFrameColor, &e)) {
    Gather::emit(Rule(
                   get_symbol(SectionFrameColor),
                   e));
  }
  else if(get(SectionFrameColor, &i)) {
    Gather::emit(Rule(
                   get_symbol(SectionFrameColor),
                   color_to_pmath(i)));
  }
  
  if(get_dynamic(SectionFrameMarginLeft, &e)) {
    Gather::emit(Rule(
                   get_symbol(SectionFrameMarginLeft),
                   e));
  }
  else {
    float left, right, top, bottom;
    bool have_left, have_right, have_top, have_bottom;
    
    have_left   = get(SectionFrameMarginLeft,   &left);
    have_right  = get(SectionFrameMarginRight,  &right);
    have_top    = get(SectionFrameMarginTop,    &top);
    have_bottom = get(SectionFrameMarginBottom, &bottom);
    if(have_left || have_right || have_top || have_bottom) {
      Expr l, r, t, b;
      
      if(have_left)
        l = Number(left);
      else
        l = Symbol(PMATH_SYMBOL_INHERITED);
        
      if(have_right)
        r = Number(right);
      else
        r = Symbol(PMATH_SYMBOL_INHERITED);
        
      if(have_top)
        t = Number(top);
      else
        t = Symbol(PMATH_SYMBOL_INHERITED);
        
      if(have_bottom)
        b = Number(bottom);
      else
        b = Symbol(PMATH_SYMBOL_INHERITED);
        
      Gather::emit(Rule(
                     get_symbol(SectionFrameMarginLeft),
                     List(l, r, t, b)));
    }
  }
  
  if(get_dynamic(SectionGenerated, &e)) {
    Gather::emit(Rule(
                   get_symbol(SectionGenerated),
                   e));
  }
  else if(get(SectionGenerated, &i)) {
    Gather::emit(Rule(
                   get_symbol(SectionGenerated),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(SectionGroupPrecedence, &e)) {
    Gather::emit(Rule(
                   get_symbol(SectionGroupPrecedence),
                   e));
  }
  else if(get(SectionGroupPrecedence, &f)) {
    Gather::emit(Rule(
                   get_symbol(SectionGroupPrecedence),
                   Number(f)));
  }
  
  if(get_dynamic(SectionMarginLeft, &e)) {
    Gather::emit(Rule(
                   get_symbol(SectionMarginLeft),
                   e));
  }
  else {
    float left, right, top, bottom;
    bool have_left, have_right, have_top, have_bottom;
    
    have_left   = get(SectionMarginLeft,   &left);
    have_right  = get(SectionMarginRight,  &right);
    have_top    = get(SectionMarginTop,    &top);
    have_bottom = get(SectionMarginBottom, &bottom);
    if(have_left || have_right || have_top || have_bottom) {
      Expr l, r, t, b;
      
      if(have_left)
        l = Number(left);
      else
        l = Symbol(PMATH_SYMBOL_INHERITED);
        
      if(have_right)
        r = Number(right);
      else
        r = Symbol(PMATH_SYMBOL_INHERITED);
        
      if(have_top)
        t = Number(top);
      else
        t = Symbol(PMATH_SYMBOL_INHERITED);
        
      if(have_bottom)
        b = Number(bottom);
      else
        b = Symbol(PMATH_SYMBOL_INHERITED);
        
      Gather::emit(Rule(
                     get_symbol(SectionMarginLeft),
                     List(l, r, t, b)));
    }
  }
  
  if(get_dynamic(SectionLabel, &e)) {
    Gather::emit(Rule(
                   get_symbol(SectionLabel),
                   e));
  }
  else if(get(SectionLabel, &s)) {
    Gather::emit(Rule(
                   get_symbol(SectionLabel),
                   s.is_null() ? Symbol(PMATH_SYMBOL_NONE) : s));
  }
  
  if(get_dynamic(SectionLabelAutoDelete, &e)) {
    Gather::emit(Rule(
                   get_symbol(SectionLabelAutoDelete),
                   e));
  }
  else if(get(SectionLabelAutoDelete, &i)) {
    Gather::emit(Rule(
                   get_symbol(SectionLabelAutoDelete),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(Selectable, &e)) {
    Gather::emit(Rule(
                   get_symbol(Selectable),
                   e));
  }
  else if(get(Selectable, &i)) {
    Gather::emit(Rule(
                   get_symbol(Selectable),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(ShowAutoStyles, &e)) {
    Gather::emit(Rule(
                   get_symbol(ShowAutoStyles),
                   e));
  }
  else if(get(ShowAutoStyles, &i)) {
    Gather::emit(Rule(
                   get_symbol(ShowAutoStyles),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(ShowSectionBracket, &e)) {
    Gather::emit(Rule(
                   get_symbol(ShowSectionBracket),
                   e));
  }
  else if(get(ShowSectionBracket, &i)) {
    Gather::emit(Rule(
                   get_symbol(ShowSectionBracket),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(ShowStringCharacters, &e)) {
    Gather::emit(Rule(
                   get_symbol(ShowStringCharacters),
                   e));
  }
  else if(get(ShowStringCharacters, &i)) {
    Gather::emit(Rule(
                   get_symbol(ShowStringCharacters),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(StripOnInput, &e)) {
    Gather::emit(Rule(
                   get_symbol(StripOnInput),
                   e));
  }
  else if(get(StripOnInput, &i)) {
    Gather::emit(Rule(
                   get_symbol(StripOnInput),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get_dynamic(StyleDefinitions, &e)) {
    Gather::emit(Rule(
                   get_symbol(StyleDefinitions),
                   e));
  }
  else if(get(StyleDefinitions, &e)) {
    Gather::emit(Rule(
                   get_symbol(StyleDefinitions),
                   e));
  }
  
  if(get_dynamic(TextShadow, &e)) {
    Gather::emit(Rule(
                   get_symbol(TextShadow),
                   e));
  }
  else if(get(TextShadow, &e)) {
    Gather::emit(Rule(
                   get_symbol(TextShadow),
                   e));
  }
  
  if(get_dynamic(WindowFrame, &e)) {
    Gather::emit(Rule(
                   get_symbol(WindowFrame),
                   e));
  }
  else if(get(WindowFrame, &i)) {
    switch((WindowFrameType)i) {
      case WindowFrameNormal:
        Gather::emit(Rule(
                       get_symbol(WindowFrame),
                       String("Normal")));
        break;
        
      case WindowFramePalette:
        Gather::emit(Rule(
                       get_symbol(WindowFrame),
                       String("Palette")));
        break;
    }
  }
  
  if(get_dynamic(WindowTitle, &e)) {
    Gather::emit(Rule(
                   get_symbol(WindowTitle),
                   e));
  }
  else if(get(WindowTitle, &s)) {
    Gather::emit(Rule(
                   get_symbol(WindowTitle),
                   s.is_null() ? Symbol(PMATH_SYMBOL_AUTOMATIC) : s));
  }
  
  if(get(UnknownOptions, &e)) {
    Expr rules = Evaluate(Call(Symbol(PMATH_SYMBOL_DOWNRULES), e));
    
    for(size_t i = 1; i <= rules.expr_length(); ++i) {
      Expr rule = rules[i];
      Expr lhs = rule[1]; // HoldPattern(symbol(x))
      lhs = lhs[1];       //             symbol(x)
      lhs = lhs[1];       //                    x
      
      if(rule[2].is_evaluated())
        rule.set(0, Symbol(PMATH_SYMBOL_RULE));
        
      rule.set(1, lhs);
      Gather::emit(rule);
    }
  }
}

//} ... class Style

//{ class Stylesheet ...

SharedPtr<Stylesheet> Stylesheet::Default;

bool Stylesheet::get(SharedPtr<Style> s, IntStyleOptionName n, int *value) {
  for(int count = 20; count && s; --count) {
    if(s->get(n, value))
      return true;
      
    String inherited;
    if(s->get(BaseStyleName, &inherited))
      s = styles[inherited];
    else
      break;
  }
  
  return false;
}

bool Stylesheet::get(SharedPtr<Style> s, FloatStyleOptionName n, float *value) {
  for(int count = 20; count && s; --count) {
    if(s->get(n, value))
      return true;
      
    String inherited;
    if(s->get(BaseStyleName, &inherited))
      s = styles[inherited];
    else
      break;
  }
  
  return false;
}

bool Stylesheet::get(SharedPtr<Style> s, StringStyleOptionName n, String *value) {
  for(int count = 20; count && s; --count) {
    if(s->get(n, value))
      return true;
      
    String inherited;
    if(s->get(BaseStyleName, &inherited))
      s = styles[inherited];
    else
      break;
  }
  
  return false;
}

bool Stylesheet::get(SharedPtr<Style> s, ObjectStyleOptionName n, Expr *value) {
  for(int count = 20; count && s; --count) {
    if(s->get(n, value))
      return true;
      
    String inherited;
    if(s->get(BaseStyleName, &inherited))
      s = styles[inherited];
    else
      break;
  }
  
  return false;
}

int Stylesheet::get_with_base(SharedPtr<Style> s, IntStyleOptionName n) {
  int value = 0;
  
  if(!get(s, n, &value))
    base->get(n, &value);
    
  return value;
}

float Stylesheet::get_with_base(SharedPtr<Style> s, FloatStyleOptionName n) {
  float value = 0.0;
  
  if(!get(s, n, &value))
    base->get(n, &value);
    
  return value;
}

String Stylesheet::get_with_base(SharedPtr<Style> s, StringStyleOptionName n) {
  String value;
  
  if(!get(s, n, &value))
    base->get(n, &value);
    
  return value;
}

Expr Stylesheet::get_with_base(SharedPtr<Style> s, ObjectStyleOptionName n) {
  Expr value;
  
  if(!get(s, n, &value))
    base->get(n, &value);
    
  return value;
}

//} ... class Stylesheet
