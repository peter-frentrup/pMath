#include <util/style.h>

#include <climits>
#include <cmath>

#include <gui/control-painter.h>

using namespace richmath;

pmath_t richmath::color_to_pmath(int color){
  if(color < 0)
    return pmath_ref(PMATH_SYMBOL_NONE);
  
  int r = (color & 0xFF0000) >> 16;
  int g = (color & 0x00FF00) >>  8;
  int b =  color & 0x0000FF;
  
  if(r == g && r == b)
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_GRAYLEVEL), 1,
      double_to_pmath(r / 255.));
  
  return pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_RGBCOLOR), 3,
    double_to_pmath(r / 255.),
    double_to_pmath(g / 255.),
    double_to_pmath(b / 255.));
}

pmath_t richmath::double_to_pmath(double value){
  double ival;
  
  if(modf(value, &ival) == 0.0){
    if(ival >= LONG_MIN && ival <= LONG_MAX)
      return pmath_integer_new_si((long)ival);
  }
  
  return pmath_build_value("f", value);
}

int richmath::pmath_to_color(Expr obj){
  if(obj == PMATH_SYMBOL_NONE)
    return -1;
    
  if(obj.instance_of(PMATH_TYPE_EXPRESSION)){
    if(obj[0] == PMATH_SYMBOL_RGBCOLOR
    && obj.expr_length() == 3
    && obj[1].instance_of(PMATH_TYPE_NUMBER)
    && obj[2].instance_of(PMATH_TYPE_NUMBER)
    && obj[3].instance_of(PMATH_TYPE_NUMBER)){
      double r = obj[1].to_double();
      double g = obj[2].to_double();
      double b = obj[3].to_double();
      
      if(r < 0) r = 0; else if(r > 1) r = 1;
      if(g < 0) g = 0; else if(g > 1) g = 1;
      if(b < 0) b = 0; else if(b > 1) b = 1;
      
      return ((int)(r * 255) << 16) | ((int)(g * 255) << 8) | (int)(b * 255);
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
  if(rhs.instance_of(PMATH_TYPE_STRING)){
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

static pmath_t buttonframe_to_rhs(int value){
  switch(value){
    case FramelessButton:
      return PMATH_C_STRING("Frameless");
      
    case GenericButton:
      return PMATH_C_STRING("Generic");
    
    case PushButton:
      return PMATH_C_STRING("DialogBox");
    
    case DefaultPushButton:
      return PMATH_C_STRING("Defaulted");
    
    case PaletteButton:
      return PMATH_C_STRING("Palette");
    
    default:
      return pmath_ref(PMATH_SYMBOL_AUTOMATIC);
  }
}

//{ class Style ...

Style::Style(): Shareable(){
}

Style::Style(Expr options): Shareable(){
  add_pmath(options);
}

void Style::add_pmath(Expr options){
  if(options.instance_of(PMATH_TYPE_STRING)){
    set(BaseStyleName, String(options));
  }
  else if(options.instance_of(PMATH_TYPE_EXPRESSION)){
    for(size_t i = 1;i <= options.expr_length();++i){
      Expr rule = options[i];
      
      if(rule.instance_of(PMATH_TYPE_EXPRESSION)
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
          else if(lhs == PMATH_SYMBOL_CONTENTTYPE){
            if(rhs == PMATH_SYMBOL_BOXDATA)
              set(ContentType, ContentTypeBoxData);
            else if(rhs == PMATH_SYMBOL_STRING)
              set(ContentType, ContentTypeString);
          }
          else if(lhs == PMATH_SYMBOL_EDITABLE){
            set_pmath_bool(Editable, rhs);
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
          else if(lhs == PMATH_SYMBOL_SYNCHRONOUSUPDATING){
            set_pmath_bool_auto(SynchronousUpdating, rhs);
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
  
  if(!v/*|| v->instance_of(PMATH_TYP_EXPRESSION) && (*v)[0] == PMATH_SYMBOL_DYNAMIC*/)
    return false;
  
  *value = *v;
  return true;
}

bool Style::get_dynamic(int n, Expr *value){
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
  object_values.remove(n); // remove dynamic
}

void Style::set(FloatStyleOptionName n, float value){
  IntFloatUnion v;
  v.float_value = value;
  int_float_values.set(n, v);
  object_values.remove(n); // remove dynamic
}

void Style::set(StringStyleOptionName n, String value){
  object_values.set(n, value);
}

void Style::set(ObjectStyleOptionName n, Expr value){
  object_values.set(n, value);
}

void Style::set_dynamic(int n, Expr value){
  object_values.set(n, value);
}

void Style::remove(IntStyleOptionName n){
  int_float_values.remove(n);
  object_values.remove(n); // remove dynamic
}

void Style::remove(FloatStyleOptionName n){
  int_float_values.remove(n);
  object_values.remove(n); // remove dynamic
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
  
  if(obj.instance_of(PMATH_TYPE_EXPRESSION)
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
      else if(obj[1].instance_of(PMATH_TYPE_EXPRESSION)
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
      else if(obj[2].instance_of(PMATH_TYPE_EXPRESSION)
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
  if(obj.instance_of(PMATH_TYPE_STRING))
    set(n, String(obj));
}

unsigned int Style::count(){
  return int_float_values.size() + object_values.size();
}

void Style::emit_to_pmath(
  bool for_sections, 
  bool with_inherited
){
  // A wont be freed
//  pmath_ref(pmath_is_evaluated(_emit_rule_rhs) 
//                    ? PMATH_SYMBOL_RULE 
//                    : PMATH_SYMBOL_RULEDELAYED), 
  #define EMIT_RULE(A, B) \
  do{ \
    pmath_t _emit_rule_rhs = (B); \
    pmath_emit( \
      pmath_expr_new_extended( \
        pmath_ref(PMATH_SYMBOL_RULE), \
        2, \
        pmath_ref(A), \
        _emit_rule_rhs), \
      NULL); \
  }while(0)
  
  /*
  // SYMBOL wont be freed
  #define EMIT_DYNAMIC(NAME, SYMBOL) \
    do{ \
      if(get_dynamic(NAME, &e)){ \
        EMIT_RULE(pmath_ref(SYMBOL), pmath_ref(e.get())); \
      } \
    }while(0)
  */
  
  Expr e;
  String s;
  int i;
  float f;
  
  if(get(Antialiasing, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_ANTIALIASING, 
      pmath_ref(i == 0 ? PMATH_SYMBOL_FALSE : 
        (i == 1 ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_AUTOMATIC)));
  }
  
  if(get(AutoDelete, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_AUTODELETE, 
      pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE));
  }
  
  if(get(AutoNumberFormating, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_AUTONUMBERFORMATING, 
      pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE));
  }
  
  if(get(AutoSpacing, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_AUTOSPACING, 
      pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE));
  }
  
  if(get(Background, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_BACKGROUND, 
      color_to_pmath(i));
  }
  
  if(with_inherited && get(BaseStyleName, &s)){
    EMIT_RULE(
      PMATH_SYMBOL_BASESTYLE,
      s.release());
  }
//  else 
//    EMIT_DYNAMIC(BaseStyleName, PMATH_SYMBOL_BASESTYLE);
  
  
  if(get(ButtonFrame, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_BUTTONFRAME, 
      buttonframe_to_rhs(i));
  }
  
  if(get(ButtonFunction, &e)){
    EMIT_RULE(
      PMATH_SYMBOL_BUTTONFUNCTION, 
      e.release());
  }
  
  if(get(ContentType, &i)){
    switch(i){
      case ContentTypeBoxData:
        EMIT_RULE(
          PMATH_SYMBOL_CONTENTTYPE, 
          pmath_ref(PMATH_SYMBOL_BOXDATA));
        break;
        
      case ContentTypeString:
        EMIT_RULE(
          PMATH_SYMBOL_CONTENTTYPE, 
          pmath_ref(PMATH_SYMBOL_STRING));
        break;
        
    }
  }
  
  if(get(Editable, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_EDITABLE, 
      pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE));
  }
  
  if(get(FontColor, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_FONTCOLOR, 
      color_to_pmath(i));
  }
  
  if(get(FontFamily, &s)){
    EMIT_RULE(
      PMATH_SYMBOL_FONTFAMILY,
      s.release());
  }
  
  if(get(FontSize, &f)){
    EMIT_RULE(
      PMATH_SYMBOL_FONTSIZE, 
      double_to_pmath(f));
  }
  
  if(get(FontSlant, &i)){
    switch(i){
      case FontSlantPlain:
        EMIT_RULE(
          PMATH_SYMBOL_FONTSLANT,
          pmath_ref(PMATH_SYMBOL_PLAIN));
        break;
        
      case FontSlantItalic:
        EMIT_RULE(
          PMATH_SYMBOL_FONTSLANT,
          pmath_ref(PMATH_SYMBOL_ITALIC));
        break;
    }
  }
  
  if(get(FontWeight, &i)){
    switch(i){
      case FontWeightPlain:
        EMIT_RULE(
          PMATH_SYMBOL_FONTWEIGHT,
          pmath_ref(PMATH_SYMBOL_PLAIN));
        break;
        
      case FontWeightBold:
        EMIT_RULE(
          PMATH_SYMBOL_FONTWEIGHT,
          pmath_ref(PMATH_SYMBOL_BOLD));
        break;
    }
  }
  
  if(get(GridBoxColumnSpacing, &f)){
    EMIT_RULE(
      PMATH_SYMBOL_GRIDBOXCOLUMNSPACING, 
      double_to_pmath(f));
  }
  
  if(get(GridBoxRowSpacing, &f)){
    EMIT_RULE(
      PMATH_SYMBOL_GRIDBOXROWSPACING, 
      double_to_pmath(f));
  }
  
  if(get(LineBreakWithin, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_LINEBREAKWITHIN, 
      pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE));
  }
  
  if(get(Method, &s)){
    EMIT_RULE(
      PMATH_SYMBOL_METHOD,
      s.release());
  }
  
  if(get(Placeholder, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_PLACEHOLDER, 
      pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE));
  }
  
  if(get(ScriptSizeMultipliers, &e)){
    EMIT_RULE(
      PMATH_SYMBOL_SCRIPTSIZEMULTIPLIERS, 
      e.release());
  }
  
  if(get(Selectable, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_SELECTABLE, 
      pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE));
  }
  
  if(get(ShowAutoStyles, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_SHOWAUTOSTYLES, 
      pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE));
  }
  
  if(get(ShowStringCharacters, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_SHOWSTRINGCHARACTERS, 
      pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE));
  }
  
  if(get(SynchronousUpdating, &i)){
    EMIT_RULE(
      PMATH_SYMBOL_SYNCHRONOUSUPDATING, 
      pmath_ref(i == 0 ? PMATH_SYMBOL_FALSE : 
        (i == 1 ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_AUTOMATIC)));
  }
  
  if(get(TextShadow, &e)){
    EMIT_RULE(
      PMATH_SYMBOL_TEXTSHADOW, 
      e.release());
  }
  
  if(for_sections){
    float left, right, top, bottom;
    bool have_left, have_right, have_top, have_bottom;
    
    have_left   = get(SectionFrameLeft,   &left);
    have_right  = get(SectionFrameRight,  &right);
    have_top    = get(SectionFrameTop,    &top);
    have_bottom = get(SectionFrameBottom, &bottom);
    if(have_left || have_right || have_top || have_bottom){
      pmath_t l, r, t, b;
      
      if(have_left)
        l = double_to_pmath(left);
      else
        l = pmath_ref(PMATH_SYMBOL_INHERITED);
      
      if(have_right)
        r = double_to_pmath(right);
      else
        r = pmath_ref(PMATH_SYMBOL_INHERITED);
      
      if(have_top)
        t = double_to_pmath(top);
      else
        t = pmath_ref(PMATH_SYMBOL_INHERITED);
      
      if(have_bottom)
        b = double_to_pmath(bottom);
      else
        b = pmath_ref(PMATH_SYMBOL_INHERITED);
      
      EMIT_RULE(
        PMATH_SYMBOL_SECTIONFRAME,
        pmath_build_value("(oooo)", l, r, t, b));
    }
    
    if(get(SectionFrameColor, &i)){
      EMIT_RULE(
        PMATH_SYMBOL_SECTIONFRAMECOLOR, 
        color_to_pmath(i));
    }
    
    have_left   = get(SectionFrameMarginLeft,   &left);
    have_right  = get(SectionFrameMarginRight,  &right);
    have_top    = get(SectionFrameMarginTop,    &top);
    have_bottom = get(SectionFrameMarginBottom, &bottom);
    if(have_left || have_right || have_top || have_bottom){
      pmath_t l, r, t, b;
      
      if(have_left)
        l = double_to_pmath(left);
      else
        l = pmath_ref(PMATH_SYMBOL_INHERITED);
      
      if(have_right)
        r = double_to_pmath(right);
      else
        r = pmath_ref(PMATH_SYMBOL_INHERITED);
      
      if(have_top)
        t = double_to_pmath(top);
      else
        t = pmath_ref(PMATH_SYMBOL_INHERITED);
      
      if(have_bottom)
        b = double_to_pmath(bottom);
      else
        b = pmath_ref(PMATH_SYMBOL_INHERITED);
      
      EMIT_RULE(
        PMATH_SYMBOL_SECTIONFRAMEMARGINS,
        pmath_build_value("(oooo)", l, r, t, b));
    }
    
    if(get(SectionGenerated, &i)){
      EMIT_RULE(
        PMATH_SYMBOL_SECTIONGENERATED, 
        pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE));
    }
    
    if(get(SectionGroupPrecedence, &f)){
      EMIT_RULE(
        PMATH_SYMBOL_SECTIONGROUPPRECEDENCE, 
        double_to_pmath(f));
    }
    
    have_left   = get(SectionMarginLeft,   &left);
    have_right  = get(SectionMarginRight,  &right);
    have_top    = get(SectionMarginTop,    &top);
    have_bottom = get(SectionMarginBottom, &bottom);
    if(have_left || have_right || have_top || have_bottom){
      pmath_t l, r, t, b;
      
      if(have_left)
        l = double_to_pmath(left);
      else
        l = pmath_ref(PMATH_SYMBOL_INHERITED);
      
      if(have_right)
        r = double_to_pmath(right);
      else
        r = pmath_ref(PMATH_SYMBOL_INHERITED);
      
      if(have_top)
        t = double_to_pmath(top);
      else
        t = pmath_ref(PMATH_SYMBOL_INHERITED);
      
      if(have_bottom)
        b = double_to_pmath(bottom);
      else
        b = pmath_ref(PMATH_SYMBOL_INHERITED);
      
      EMIT_RULE(
        PMATH_SYMBOL_SECTIONMARGINS,
        pmath_build_value("(oooo)", l, r, t, b));
    }
    
    if(get(SectionLabel, &s)){
      EMIT_RULE(
        PMATH_SYMBOL_SECTIONLABEL,
        s.release());
    }
    
    if(get(SectionLabelAutoDelete, &i)){
      EMIT_RULE(
        PMATH_SYMBOL_SECTIONLABELAUTODELETE, 
        pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE));
    }
    
    if(get(ShowSectionBracket, &i)){
      EMIT_RULE(
        PMATH_SYMBOL_SHOWSECTIONBRACKET, 
        pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE));
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
