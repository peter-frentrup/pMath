#include <util/style.h>

#include <climits>
#include <cmath>

#include <gui/control-painter.h>

using namespace richmath;

Expr richmath::color_to_pmath(int color){
  if(color < 0)
    return Symbol(PMATH_SYMBOL_NONE);
  
  int r = (color & 0xFF0000) >> 16;
  int g = (color & 0x00FF00) >>  8;
  int b =  color & 0x0000FF;
  
  if(r == g && r == b)
    return Call(
      Symbol(PMATH_SYMBOL_GRAYLEVEL),
      Number(r / 255.));
  
  return Call(
    Symbol(PMATH_SYMBOL_RGBCOLOR), 
    Number(r / 255.),
    Number(g / 255.),
    Number(b / 255.));
}

int richmath::pmath_to_color(Expr obj){
  if(obj == PMATH_SYMBOL_NONE)
    return -1;
    
  if(obj.is_expr()){
    if(obj[0] == PMATH_SYMBOL_RGBCOLOR){
      if(obj.expr_length() == 1
      && obj[1][0] == PMATH_SYMBOL_LIST){
        obj = obj[1];
      }
      
      if(obj.expr_length() == 3
      && obj[1].instance_of(PMATH_TYPE_NUMBER)
      && obj[2].instance_of(PMATH_TYPE_NUMBER)
      && obj[3].instance_of(PMATH_TYPE_NUMBER)){
        double r = obj[1].to_double();
        double g = obj[2].to_double();
        double b = obj[3].to_double();
        
        if(r < 0){ r = 0; }else if(r > 1){ r = 1; }
        if(g < 0){ g = 0; }else if(g > 1){ g = 1; }
        if(b < 0){ b = 0; }else if(b > 1){ b = 1; }
        
        return ((int)(r * 255) << 16) | ((int)(g * 255) << 8) | (int)(b * 255);
      }
    }
    
    if(obj[0] == PMATH_SYMBOL_GRAYLEVEL
    && obj.expr_length() == 1
    && obj[1].instance_of(PMATH_TYPE_NUMBER)){
      double l = obj[1].to_double();
      
      if(l < 0) l = 0; else if(l > 1) l = 1;
      
      return ((int)(l * 255) << 16) | ((int)(l * 255) << 8) | (int)(l * 255);
    }
  }
  
  return -2;
}

static int rhs_to_buttonframe(Expr rhs){
  if(rhs.is_string()){
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
  }
  
  return -1;
}

static Expr buttonframe_to_rhs(int value){
  switch(value){
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
    
    default:
      return Symbol(PMATH_SYMBOL_AUTOMATIC);
  }
}

//{ class Style ...

Style::Style(): Shareable(){
}

Style::Style(Expr options): Shareable(){
  add_pmath(options);
}

void Style::add_pmath(Expr options){
  if(options.is_string()){
    set(BaseStyleName, String(options));
  }
  else if(options.is_expr()){
    for(size_t i = 1;i <= options.expr_length();++i){
      Expr rule = options[i];
      
      if(rule.is_expr()
      && rule.expr_length() == 2
      && (rule[0] == PMATH_SYMBOL_RULE || rule[0] == PMATH_SYMBOL_RULEDELAYED)){
        Expr lhs = rule[1];
        Expr rhs = rule[2];
        
        if(rhs != PMATH_SYMBOL_INHERITED){
          if(lhs == PMATH_SYMBOL_ANTIALIASING){
            set_pmath_bool_auto(Antialiasing, rhs);
          }
          if(lhs == PMATH_SYMBOL_AUTODELETE){
            set_pmath_bool(AutoDelete, rhs);
          }
          else if(lhs == PMATH_SYMBOL_AUTONUMBERFORMATING){
            set_pmath_bool(AutoNumberFormating, rhs);
          }
          else if(lhs == PMATH_SYMBOL_AUTOSPACING){
            set_pmath_bool(AutoSpacing, rhs);
          }
          else if(lhs == PMATH_SYMBOL_BACKGROUND){
            set_pmath_color(Background, rhs);
          }
          else if(lhs == PMATH_SYMBOL_BASESTYLE){
            add_pmath(rhs);
          }
          else if(lhs == PMATH_SYMBOL_BUTTONFRAME){
            set(ButtonFrame, rhs_to_buttonframe(rhs));
          }
          else if(lhs == PMATH_SYMBOL_BUTTONFUNCTION){
            set(ButtonFunction, rhs);
          }
          else if(lhs == PMATH_SYMBOL_EDITABLE){
            set_pmath_bool(Editable, rhs);
          }
          else if(lhs == PMATH_SYMBOL_EVALUATABLE){
            set_pmath_bool(Evaluatable, rhs);
          }
          else if(lhs == PMATH_SYMBOL_FONTCOLOR){
            set_pmath_color(FontColor, rhs);
          }
          else if(lhs == PMATH_SYMBOL_FONTFAMILY){
            set_pmath_string(FontFamily, rhs);
          }
          else if(lhs == PMATH_SYMBOL_FONTSIZE){
            set_pmath_float(FontSize, rhs);
          }
          else if(lhs == PMATH_SYMBOL_FONTSLANT){
            if(rhs == PMATH_SYMBOL_PLAIN)
              set(FontSlant, FontSlantPlain);
            else if(rhs == PMATH_SYMBOL_ITALIC)
              set(FontSlant, FontSlantItalic);
          }
          else if(lhs == PMATH_SYMBOL_FONTWEIGHT){
            if(rhs == PMATH_SYMBOL_PLAIN)
              set(FontWeight, FontWeightPlain);
            else if(rhs == PMATH_SYMBOL_BOLD)
              set(FontWeight, FontWeightBold);
          }
          else if(lhs == PMATH_SYMBOL_GRIDBOXCOLUMNSPACING){
            set_pmath_float(GridBoxColumnSpacing, rhs);
          }
          else if(lhs == PMATH_SYMBOL_GRIDBOXROWSPACING){
            set_pmath_float(GridBoxRowSpacing, rhs);
          }
          else if(lhs == PMATH_SYMBOL_LINEBREAKWITHIN){
            set_pmath_bool(LineBreakWithin, rhs);
          }
          else if(lhs == PMATH_SYMBOL_METHOD){
            set_pmath_string(Method, rhs);
          }
          else if(lhs == PMATH_SYMBOL_PLACEHOLDER){
            set_pmath_bool(Placeholder, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SCRIPTSIZEMULTIPLIERS){
            set(ScriptSizeMultipliers, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONFRAME){
            set_pmath_margin(SectionFrameLeft, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONFRAMECOLOR){
            set_pmath_color(SectionFrameColor, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONFRAMEMARGINS){
            set_pmath_margin(SectionFrameMarginLeft, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONGENERATED){
            set_pmath_bool(SectionGenerated, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONGROUPPRECEDENCE){
            set_pmath_float(SectionGroupPrecedence, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONMARGINS){
            set_pmath_margin(SectionMarginLeft, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONLABEL){
            if(rhs == PMATH_SYMBOL_NONE)
              set(SectionLabel, String());
            else
              set_pmath_string(SectionLabel, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SECTIONLABELAUTODELETE){
            set_pmath_bool(SectionLabelAutoDelete, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SELECTABLE){
            set_pmath_bool(Selectable, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SHOWAUTOSTYLES){
            set_pmath_bool(ShowAutoStyles, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SHOWSECTIONBRACKET){
            set_pmath_bool(ShowSectionBracket, rhs);
          }
          else if(lhs == PMATH_SYMBOL_SHOWSTRINGCHARACTERS){
            set_pmath_bool(ShowStringCharacters, rhs);
          }
          else if(lhs == PMATH_SYMBOL_STRIPONINPUT){
            set_pmath_bool(StripOnInput, rhs);
          }
          else if(lhs == PMATH_SYMBOL_TEXTSHADOW){
            set(TextShadow, rhs);
          }
        }
      }
    }
  }
}

void Style::merge(SharedPtr<Style> other){
  int_float_values.merge(other->int_float_values);
  
  object_values.merge(other->object_values);
}

bool Style::get(IntStyleOptionName n, int *value){
  IntFloatUnion *v = int_float_values.search(n);
  
  if(!v)
    return false;
  
  *value = v->int_value;
  return true;
}

bool Style::get(FloatStyleOptionName n, float *value){
  IntFloatUnion *v = int_float_values.search(n);
  
  if(!v)
    return false;
  
  *value = v->float_value;
  return true;
}

bool Style::get(StringStyleOptionName n, String *value){
  Expr *v = object_values.search(n);
  
  if(!v || !v->instance_of(PMATH_TYPE_STRING))
    return false;
  
  *value = String(*v);
  return true;
}

bool Style::get(ObjectStyleOptionName n, Expr *value){
  Expr *v = object_values.search(n);
  
  if(!v)
    return false;
  
  *value = *v;
  return true;
}

void Style::set(IntStyleOptionName n, int value){
  IntFloatUnion v;
  v.int_value = value;
  int_float_values.set(n, v);
}

void Style::set(FloatStyleOptionName n, float value){
  IntFloatUnion v;
  v.float_value = value;
  int_float_values.set(n, v);
}

void Style::set(StringStyleOptionName n, String value){
  object_values.set(n, value);
}

void Style::set(ObjectStyleOptionName n, Expr value){
  object_values.set(n, value);
}

void Style::remove(IntStyleOptionName n){
  int_float_values.remove(n);
}

void Style::remove(FloatStyleOptionName n){
  int_float_values.remove(n);
}

void Style::remove(StringStyleOptionName n){
  object_values.remove(n);
}

void Style::remove(ObjectStyleOptionName n){
  object_values.remove(n);
}

void Style::set_pmath_bool_auto(IntStyleOptionName n, Expr obj){
  if(obj == PMATH_SYMBOL_FALSE)
    set(n, 0);
  else if(obj == PMATH_SYMBOL_TRUE)
    set(n, 1);
  else if(obj == PMATH_SYMBOL_AUTOMATIC)
    set(n, 2);
}

void Style::set_pmath_bool(IntStyleOptionName n, Expr obj){
  if(obj == PMATH_SYMBOL_TRUE)
    set(n, true);
  else if(obj == PMATH_SYMBOL_FALSE)
    set(n, false);
}

void Style::set_pmath_color(IntStyleOptionName n, Expr obj){
  int c = pmath_to_color(obj);
  
  if(c >= -1)
    set(n, c);
}

void Style::set_pmath_float(FloatStyleOptionName n, Expr obj){
  if(obj.instance_of(PMATH_TYPE_NUMBER))
    set(n, obj.to_double());
}

void Style::set_pmath_margin(FloatStyleOptionName n, Expr obj){
  FloatStyleOptionName Left   = n;
  FloatStyleOptionName Right  = FloatStyleOptionName(n + 1);
  FloatStyleOptionName Top    = FloatStyleOptionName(n + 2);
  FloatStyleOptionName Bottom = FloatStyleOptionName(n + 3);
  
  if(obj == PMATH_SYMBOL_TRUE){
    set(Left,   1.0);
    set(Right,  1.0);
    set(Top,    1.0);
    set(Bottom, 1.0);
    return;
  }
  
  if(obj == PMATH_SYMBOL_FALSE){
    set(Left,   0.0);
    set(Right,  0.0);
    set(Top,    0.0);
    set(Bottom, 0.0);
    return;
  }
  
  if(obj.instance_of(PMATH_TYPE_NUMBER)){
    float f = obj.to_double();
    set(Left,   f);
    set(Right,  f);
    set(Top,    f);
    set(Bottom, f);
    return;
  }
  
  if(obj.is_expr()
  && obj[0] == PMATH_SYMBOL_LIST){
    if(obj.expr_length() == 4){
      set_pmath_float(Left,   obj[1]);
      set_pmath_float(Right,  obj[2]);
      set_pmath_float(Top,    obj[3]);
      set_pmath_float(Bottom, obj[4]);
      return;
    }
    
    if(obj.expr_length() == 2){
      if(obj[1].instance_of(PMATH_TYPE_NUMBER)){
        float f = obj[1].to_double();
        set(Left,  f);
        set(Right, f);
      }
      else if(obj[1].is_expr()
      && obj[1][0] == PMATH_SYMBOL_LIST
      && obj[1].expr_length() == 2){
        set_pmath_float(Left,  obj[1][1]);
        set_pmath_float(Right, obj[1][2]);
      }
      
      if(obj[2].instance_of(PMATH_TYPE_NUMBER)){
        float f = obj[2].to_double();
        set(Top,    f);
        set(Bottom, f);
      }
      else if(obj[2].is_expr()
      && obj[2][0] == PMATH_SYMBOL_LIST
      && obj[2].expr_length() == 2){
        set_pmath_float(Top,    obj[2][1]);
        set_pmath_float(Bottom, obj[2][2]);
      }
      
      return;
    }
  }
}

void Style::set_pmath_string(StringStyleOptionName n, Expr obj){
  if(obj.is_string())
    set(n, String(obj));
}

unsigned int Style::count(){
  return int_float_values.size() + object_values.size();
}

void Style::emit_to_pmath(
  bool for_sections, 
  bool with_inherited
){
  Expr e;
  String s;
  int i;
  float f;
  
  if(get(Antialiasing, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_ANTIALIASING), 
      Symbol(i == 0 ? PMATH_SYMBOL_FALSE : 
        (i == 1 ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_AUTOMATIC))));
  }
  
  if(get(AutoDelete, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_AUTODELETE), 
      Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get(AutoNumberFormating, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_AUTONUMBERFORMATING), 
      Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get(AutoSpacing, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_AUTOSPACING), 
      Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get(Background, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_BACKGROUND), 
      color_to_pmath(i)));
  }
  
  if(with_inherited && get(BaseStyleName, &s)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_BASESTYLE),
      s));
  }
  
  if(get(ButtonFrame, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_BUTTONFRAME), 
      buttonframe_to_rhs(i)));
  }
  
  if(get(ButtonFunction, &e)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_BUTTONFUNCTION), 
      e));
  }
  
  if(get(Editable, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_EDITABLE), 
      Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get(Evaluatable, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_EVALUATABLE), 
      Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get(FontColor, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_FONTCOLOR), 
      color_to_pmath(i)));
  }
  
  if(get(FontFamily, &s)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_FONTFAMILY),
      s));
  }
  
  if(get(FontSize, &f)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_FONTSIZE), 
      Number(f)));
  }
  
  if(get(FontSlant, &i)){
    switch(i){
      case FontSlantPlain:
        Gather::emit(Rule(
          Symbol(PMATH_SYMBOL_FONTSLANT),
          Symbol(PMATH_SYMBOL_PLAIN)));
        break;
        
      case FontSlantItalic:
        Gather::emit(Rule(
          Symbol(PMATH_SYMBOL_FONTSLANT),
          Symbol(PMATH_SYMBOL_ITALIC)));
        break;
    }
  }
  
  if(get(FontWeight, &i)){
    switch(i){
      case FontWeightPlain:
        Gather::emit(Rule(
          Symbol(PMATH_SYMBOL_FONTWEIGHT),
          Symbol(PMATH_SYMBOL_PLAIN)));
        break;
        
      case FontWeightBold:
        Gather::emit(Rule(
          Symbol(PMATH_SYMBOL_FONTWEIGHT),
          Symbol(PMATH_SYMBOL_BOLD)));
        break;
    }
  }
  
  if(get(GridBoxColumnSpacing, &f)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_GRIDBOXCOLUMNSPACING), 
      Number(f)));
  }
  
  if(get(GridBoxRowSpacing, &f)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_GRIDBOXROWSPACING), 
      Number(f)));
  }
  
  if(get(LineBreakWithin, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_LINEBREAKWITHIN), 
      Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get(Method, &s)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_METHOD),
      s));
  }
  
  if(get(Placeholder, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_PLACEHOLDER), 
      Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get(ScriptSizeMultipliers, &e)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_SCRIPTSIZEMULTIPLIERS), 
      e));
  }
  
  if(get(Selectable, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_SELECTABLE), 
      Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get(ShowAutoStyles, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_SHOWAUTOSTYLES), 
      Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get(ShowStringCharacters, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_SHOWSTRINGCHARACTERS), 
      Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get(StripOnInput, &i)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_STRIPONINPUT), 
      Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(get(TextShadow, &e)){
    Gather::emit(Rule(
      Symbol(PMATH_SYMBOL_TEXTSHADOW), 
      e));
  }
  
  if(for_sections){
    float left, right, top, bottom;
    bool have_left, have_right, have_top, have_bottom;
    
    have_left   = get(SectionFrameLeft,   &left);
    have_right  = get(SectionFrameRight,  &right);
    have_top    = get(SectionFrameTop,    &top);
    have_bottom = get(SectionFrameBottom, &bottom);
    if(have_left || have_right || have_top || have_bottom){
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
        Symbol(PMATH_SYMBOL_SECTIONFRAME),
        List(l, r, t, b)));
    }
    
    if(get(SectionFrameColor, &i)){
      Gather::emit(Rule(
        Symbol(PMATH_SYMBOL_SECTIONFRAMECOLOR), 
        color_to_pmath(i)));
    }
    
    have_left   = get(SectionFrameMarginLeft,   &left);
    have_right  = get(SectionFrameMarginRight,  &right);
    have_top    = get(SectionFrameMarginTop,    &top);
    have_bottom = get(SectionFrameMarginBottom, &bottom);
    if(have_left || have_right || have_top || have_bottom){
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
        Symbol(PMATH_SYMBOL_SECTIONFRAMEMARGINS),
        List(l, r, t, b)));
    }
    
    if(get(SectionGenerated, &i)){
      Gather::emit(Rule(
        Symbol(PMATH_SYMBOL_SECTIONGENERATED), 
        Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
    }
    
    if(get(SectionGroupPrecedence, &f)){
      Gather::emit(Rule(
        Symbol(PMATH_SYMBOL_SECTIONGROUPPRECEDENCE), 
        Number(f)));
    }
    
    have_left   = get(SectionMarginLeft,   &left);
    have_right  = get(SectionMarginRight,  &right);
    have_top    = get(SectionMarginTop,    &top);
    have_bottom = get(SectionMarginBottom, &bottom);
    if(have_left || have_right || have_top || have_bottom){
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
        Symbol(PMATH_SYMBOL_SECTIONMARGINS),
        List(l, r, t, b)));
    }
    
    if(get(SectionLabel, &s)){
      Gather::emit(Rule(
        Symbol(PMATH_SYMBOL_SECTIONLABEL),
        s));
    }
    
    if(get(SectionLabelAutoDelete, &i)){
      Gather::emit(Rule(
        Symbol(PMATH_SYMBOL_SECTIONLABELAUTODELETE), 
        Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
    }
    
    if(get(ShowSectionBracket, &i)){
      Gather::emit(Rule(
        Symbol(PMATH_SYMBOL_SHOWSECTIONBRACKET), 
        Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
    }
    
  }
}

//} ... class Style

//{ class Stylesheet ...

SharedPtr<Stylesheet> Stylesheet::Default;

bool Stylesheet::get(SharedPtr<Style> s, IntStyleOptionName n, int *value){
  for(int count = 20;count && s;--count){
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

bool Stylesheet::get(SharedPtr<Style> s, FloatStyleOptionName n, float *value){
  for(int count = 20;count && s;--count){
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

bool Stylesheet::get(SharedPtr<Style> s, StringStyleOptionName n, String *value){
  for(int count = 20;count && s;--count){
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

bool Stylesheet::get(SharedPtr<Style> s, ObjectStyleOptionName n, Expr *value){
  for(int count = 20;count && s;--count){
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

int Stylesheet::get_with_base(SharedPtr<Style> s, IntStyleOptionName n){
  int value = 0;
  
  if(!get(s, n, &value))
    base->get(n, &value);
  
  return value;
}

float Stylesheet::get_with_base(SharedPtr<Style> s, FloatStyleOptionName n){
  float value = 0.0;
  
  if(!get(s, n, &value))
    base->get(n, &value);
  
  return value;
}

String Stylesheet::get_with_base(SharedPtr<Style> s, StringStyleOptionName n){
  String value;
  
  if(!get(s, n, &value))
    base->get(n, &value);
  
  return value;
}

Expr Stylesheet::get_with_base(SharedPtr<Style> s, ObjectStyleOptionName n){
  Expr value;
  
  if(!get(s, n, &value))
    base->get(n, &value);
  
  return value;
}

//} ... class Stylesheet
