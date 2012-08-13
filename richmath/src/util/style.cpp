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

namespace {
  class StyleInformation: public Base {
    public:
      static void add_style() {
        if(_num_styles++ == 0){
          _symbol_to_key.default_value = -1;
          _key_to_type.default_value   = StyleTypeNone;
        
          add(StyleTypeColor,           Background,                       PMATH_SYMBOL_BACKGROUND);
          add(StyleTypeColor,           FontColor,                        PMATH_SYMBOL_FONTCOLOR);
          add(StyleTypeColor,           SectionFrameColor,                PMATH_SYMBOL_SECTIONFRAMECOLOR);
          add(StyleTypeBoolAuto,        Antialiasing,                     PMATH_SYMBOL_ANTIALIASING);
          add(StyleTypeFontSlant,       FontSlant,                        PMATH_SYMBOL_FONTSLANT);
          add(StyleTypeFontWeight,      FontWeight,                       PMATH_SYMBOL_FONTWEIGHT);
          add(StyleTypeBool,            AutoDelete,                       PMATH_SYMBOL_AUTODELETE);
          add(StyleTypeBool,            AutoNumberFormating,              PMATH_SYMBOL_AUTONUMBERFORMATING);
          add(StyleTypeBool,            AutoSpacing,                      PMATH_SYMBOL_AUTOSPACING);
          add(StyleTypeBool,            ContinuousAction,                 PMATH_SYMBOL_CONTINUOUSACTION);
          add(StyleTypeBool,            Editable,                         PMATH_SYMBOL_EDITABLE);
          add(StyleTypeBool,            Evaluatable,                      PMATH_SYMBOL_EVALUATABLE);
          add(StyleTypeBool,            LineBreakWithin,                  PMATH_SYMBOL_LINEBREAKWITHIN);
          add(StyleTypeBool,            Placeholder,                      PMATH_SYMBOL_PLACEHOLDER);
          add(StyleTypeBool,            ReturnCreatesNewSection,          PMATH_SYMBOL_RETURNCREATESNEWSECTION);
          add(StyleTypeBool,            SectionEditDuplicate,             PMATH_SYMBOL_SECTIONEDITDUPLICATE);
          add(StyleTypeBool,            SectionEditDuplicateMakesCopy,    PMATH_SYMBOL_SECTIONEDITDUPLICATEMAKESCOPY);
          add(StyleTypeBool,            SectionGenerated,                 PMATH_SYMBOL_SECTIONGENERATED);
          add(StyleTypeBool,            SectionLabelAutoDelete,           PMATH_SYMBOL_SECTIONLABELAUTODELETE);
          add(StyleTypeBool,            Selectable,                       PMATH_SYMBOL_SELECTABLE);
          add(StyleTypeBool,            ShowAutoStyles,                   PMATH_SYMBOL_SHOWAUTOSTYLES);
          add(StyleTypeBool,            ShowSectionBracket,               PMATH_SYMBOL_SHOWSECTIONBRACKET);
          add(StyleTypeBool,            ShowStringCharacters,             PMATH_SYMBOL_SHOWSTRINGCHARACTERS);
          add(StyleTypeBool,            StripOnInput,                     PMATH_SYMBOL_STRIPONINPUT);
          add(StyleTypeBool,            Visible,                          PMATH_SYMBOL_VISIBLE);
          add(StyleTypeButtonFrame,     ButtonFrame,                      PMATH_SYMBOL_BUTTONFRAME);
          add(StyleTypeWindowFrame,     WindowFrame,                      PMATH_SYMBOL_WINDOWFRAME);
          
          add(StyleTypeNumber,          FontSize,                         PMATH_SYMBOL_FONTSIZE);
          add(StyleTypeNumber,          AspectRatio,                      PMATH_SYMBOL_ASPECTRATIO);
          add(StyleTypeNumber,          GridBoxColumnSpacing,             PMATH_SYMBOL_GRIDBOXCOLUMNSPACING);
          add(StyleTypeNumber,          GridBoxRowSpacing,                PMATH_SYMBOL_GRIDBOXROWSPACING);
          add(StyleTypeSize,            ImageSizeCommon,                  PMATH_SYMBOL_IMAGESIZE);
          // ImageSizeHorizontal
          // ImageSizeVertical
          add(StyleTypeMargin,          SectionMarginLeft,                PMATH_SYMBOL_SECTIONMARGINS);
          // SectionMarginRight
          // SectionMarginTop
          // SectionMarginBottom
          add(StyleTypeMargin,          SectionFrameLeft,                 PMATH_SYMBOL_SECTIONFRAME);
          // SectionFrameRight
          // SectionFrameTop
          // SectionFrameBottom
          add(StyleTypeMargin,          SectionFrameMarginLeft,           PMATH_SYMBOL_SECTIONFRAMEMARGINS);
          // SectionFrameMarginRight
          // SectionFrameMarginTop
          // SectionFrameMarginBottom
          add(StyleTypeMargin,          SectionGroupPrecedence,           PMATH_SYMBOL_SECTIONGROUPPRECEDENCE);
          
          
          add(StyleTypeString,          BaseStyleName,                    PMATH_SYMBOL_BASESTYLE);
          add(StyleTypeString,          Method,                           PMATH_SYMBOL_METHOD);
          add(StyleTypeString,          LanguageCategory,                 PMATH_SYMBOL_LANGUAGECATEGORY);
          add(StyleTypeString,          SectionLabel,                     PMATH_SYMBOL_SECTIONLABEL);
          add(StyleTypeString,          WindowTitle,                      PMATH_SYMBOL_WINDOWTITLE);
          
          add(StyleTypeAny,             ButtonFunction,                   PMATH_SYMBOL_BUTTONFUNCTION);
          add(StyleTypeAny,             ScriptSizeMultipliers,            PMATH_SYMBOL_SCRIPTSIZEMULTIPLIERS);
          add(StyleTypeAny,             TextShadow,                       PMATH_SYMBOL_TEXTSHADOW);
          add(StyleTypeAny,             FontFamilies,                     PMATH_SYMBOL_FONTFAMILY);
          add(StyleTypeAny,             BoxRotation,                      PMATH_SYMBOL_BOXROTATION);
          add(StyleTypeAny,             BoxTransformation,                PMATH_SYMBOL_BOXTRANSFORMATION);
          add(StyleTypeAny,             PlotRange,                        PMATH_SYMBOL_PLOTRANGE);
          add(StyleTypeAny,             BorderRadius,                     PMATH_SYMBOL_BORDERRADIUS);
          add(StyleTypeAny,             DefaultDuplicateSectionStyle,     PMATH_SYMBOL_DEFAULTDUPLICATESECTIONSTYLE);
          add(StyleTypeAny,             DefaultNewSectionStyle,           PMATH_SYMBOL_DEFAULTNEWSECTIONSTYLE);
          add(StyleTypeAny,             DefaultReturnCreatedSectionStyle, PMATH_SYMBOL_DEFAULTRETURNCREATEDSECTIONSTYLE);
          add(StyleTypeDockedSections4, DockedSectionsTop,                PMATH_SYMBOL_DOCKEDSECTIONS);
          //DockedSectionsTopGlass
          //DockedSectionsBottom
          //DockedSectionsBottomGlass
          add(StyleTypeAny,             StyleDefinitions,                 PMATH_SYMBOL_STYLEDEFINITIONS);
          add(StyleTypeAny,             GeneratedSectionStyles,           PMATH_SYMBOL_GENERATEDSECTIONSTYLES);
        }
      }
      
      static void remove_style() {
        if(--_num_styles == 0){
          _key_to_symbol.clear();
          _symbol_to_key.clear();
        }
      }
      
      static bool is_window_option(int key){
        return key == Visible     ||
               key == WindowFrame ||
               key == WindowTitle;
      }
      
      static int get_number_of_keys(enum StyleType type){
        switch(type) {
          case StyleTypeMargin:          return 4;
          case StyleTypeSize:            return 3;
          case StyleTypeDockedSections4: return 4;
          
          default:
            return 1;
        }
      }
      
      static enum StyleType get_type(int key) {
        return _key_to_type[key];
      }
    
      static Expr get_symbol(int key) {
        return _key_to_symbol[key];
      }
    
      static int get_key(Expr symbol) {
        return _symbol_to_key[symbol];
      }
    
    private:
      static void add(StyleType type, IntStyleOptionName key, pmath_symbol_t symbol){
        add(type, (int)key, symbol);
      }
      static void add(StyleType type, FloatStyleOptionName key, pmath_symbol_t symbol){
        add(type, (int)key, symbol);
      }
      static void add(StyleType type, StringStyleOptionName key, pmath_symbol_t symbol){
        add(type, (int)key, symbol);
      }
      static void add(StyleType type, ObjectStyleOptionName key, pmath_symbol_t symbol){
        add(type, (int)key, symbol);
      }
      
      static void add(StyleType type, int key, pmath_symbol_t symbol){
        Expr sym = Symbol(symbol);
        
        _key_to_type.set(  key, type);
        _key_to_symbol.set(key, sym);
        _symbol_to_key.set(sym, key);
      }
      
    private:
      static int _num_styles;
      
      static Hashtable<int, enum StyleType> _key_to_type;
      static Hashtable<int, Expr>           _key_to_symbol;
      static Hashtable<Expr, int>           _symbol_to_key;
  };
  
  int StyleInformation::_num_styles = 0;
}

//{ class Style ...

Style::Style(): Shareable() {
  StyleInformation::add_style();
}

Style::Style(Expr options): Shareable() {
  StyleInformation::add_style();
  
  add_pmath(options);
}

Style::~Style() {
  StyleInformation::remove_style();
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
          int key;
          enum StyleType type;
            
          key  = StyleInformation::get_key(lhs);
          type = StyleInformation::get_type(key);
              
          if(key < 0 || type == StyleTypeNone){
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
            continue;
          }
          
          if(StyleInformation::is_window_option(key))
            set(InternalHasModifiedWindowOption, true);
            
          switch(type){
            case StyleTypeBool:
              set_pmath_bool((IntStyleOptionName)key, rhs);
              break;
              
            case StyleTypeBoolAuto:
              set_pmath_bool_auto((IntStyleOptionName)key, rhs);
              break;
              
            case StyleTypeColor:
              set_pmath_color((IntStyleOptionName)key, rhs);
              break;
              
            case StyleTypeNumber:
              set_pmath_float((FloatStyleOptionName)key, rhs);
              break;
              
            case StyleTypeMargin:
              set_pmath_margin((FloatStyleOptionName)key, rhs);
              break;
              
            case StyleTypeSize:
              set_pmath_size((FloatStyleOptionName)key, rhs);
              break;
              
            case StyleTypeString:
              set_pmath_string((StringStyleOptionName)key, rhs);
              break;
            
            case StyleTypeAny: 
              set_pmath_object((ObjectStyleOptionName)key, rhs);
              break;
              
            case StyleTypeDockedSections4:
              set_docked_sections(/*(ObjectStyleOptionName)key,*/ rhs);
              break;
              
            case StyleTypeFontSlant: {
              if(rhs == PMATH_SYMBOL_PLAIN)
                set((FloatStyleOptionName)key, FontSlantPlain);
              else if(rhs == PMATH_SYMBOL_ITALIC)
                set((FloatStyleOptionName)key, FontSlantItalic);
              else if(rhs[0] == PMATH_SYMBOL_DYNAMIC)
                set_dynamic(key, rhs);
            } break;
              
            case StyleTypeFontWeight: {
              if(rhs == PMATH_SYMBOL_PLAIN)
                set((FloatStyleOptionName)key, FontWeightPlain);
              else if(rhs == PMATH_SYMBOL_BOLD)
                set((FloatStyleOptionName)key, FontWeightBold);
              else if(rhs[0] == PMATH_SYMBOL_DYNAMIC)
                set_dynamic(key, rhs);
            } break;
              
            case StyleTypeButtonFrame: {
              if(rhs[0] == PMATH_SYMBOL_DYNAMIC)
                set_dynamic(key, rhs);
              else
                set((FloatStyleOptionName)key, rhs_to_buttonframe(rhs));
            } break;
              
            case StyleTypeWindowFrame: {
              String s_rhs(rhs);
              if(s_rhs.equals("Normal"))
                set((FloatStyleOptionName)key, WindowFrameNormal);
              else if(s_rhs.equals("Palette"))
                set((FloatStyleOptionName)key, WindowFramePalette);
              else if(s_rhs.equals("Dialog"))
                set((FloatStyleOptionName)key, WindowFrameDialog);
              else
                set_dynamic(key, rhs);
            } break;
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
  
  if( obj.is_expr() &&
      obj[0] == PMATH_SYMBOL_LIST)
  {
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
      else if(obj[1].is_expr() &&
              obj[1][0] == PMATH_SYMBOL_LIST &&
              obj[1].expr_length() == 2)
      {
        set_pmath_float(Left,  obj[1][1]);
        set_pmath_float(Right, obj[1][2]);
      }
      
      if(obj[2].is_number()) {
        float f = obj[2].to_double();
        set(Top,    f);
        set(Bottom, f);
      }
      else if(obj[2].is_expr() &&
              obj[2][0] == PMATH_SYMBOL_LIST &&
              obj[2].expr_length() == 2)
      {
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
  return StyleInformation::get_symbol(n);
  }
  
void Style::emit_pmath_bool_auto(IntStyleOptionName n) { // 0/1=false/true, 2=auto
  Expr e;
  int i;
  
  if(get_dynamic(n, &e)) {
    Gather::emit(Rule(
                   get_symbol(n),
                   e));
  }
  else if(get(n, &i)) {
    switch(i) {
      case 0:
        Gather::emit(Rule(get_symbol(n), Symbol(PMATH_SYMBOL_FALSE)));
        break;
        
      case 1:
        Gather::emit(Rule(get_symbol(n), Symbol(PMATH_SYMBOL_TRUE)));
        break;
        
      default:
        Gather::emit(Rule(get_symbol(n), Symbol(PMATH_SYMBOL_AUTOMATIC)));
        break;
    }
  }
}

void Style::emit_pmath_bool(IntStyleOptionName n) {
  Expr e;
  int i;
  
  if(get_dynamic(n, &e)) {
    Gather::emit(Rule(
                   get_symbol(n),
                   e));
  }
  else if(get(n, &i)) {
    Gather::emit(Rule(
                   get_symbol(n),
                   Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
}

void Style::emit_pmath_color(IntStyleOptionName n) {
  Expr e;
  int i;
  
  if(get_dynamic(n, &e)) {
    Gather::emit(Rule(
                   get_symbol(n),
                   e));
  }
  else if(get(n, &i)) {
    Gather::emit(Rule(
                   get_symbol(n),
                   color_to_pmath(i)));
  }
}

void Style::emit_pmath_float(FloatStyleOptionName n) {
  Expr e;
  float f;
  
  if(get_dynamic(n, &e)) {
    Gather::emit(Rule(
                   get_symbol(n),
                   e));
  }
  else if(get(n, &f)) {
    Gather::emit(Rule(
                   get_symbol(n),
                   Number(f)));
  }
}

void Style::emit_pmath_margin(FloatStyleOptionName n) { // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
  Expr e;
  
  if(get_dynamic(n, &e)) {
    Gather::emit(Rule(
                   get_symbol(n),
                   e));
  }
  else {
    float left, right, top, bottom;
    bool have_left, have_right, have_top, have_bottom;
    
    have_left   = get(                     n,      &left);
    have_right  = get(FloatStyleOptionName(n + 1), &right);
    have_top    = get(FloatStyleOptionName(n + 2), &top);
    have_bottom = get(FloatStyleOptionName(n + 3), &bottom);
    
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
                     get_symbol(n),
                     List(l, r, t, b)));
    }
  }
}

void Style::emit_pmath_size(FloatStyleOptionName  n) { // n + {0,1,2} ~= {Common, Horizontal, Vertical}
  Expr e;
  
  if(get_dynamic(n, &e)) {
    Gather::emit(Rule(
                   get_symbol(n),
                   e));
  }
  else {
    bool have_horz, have_vert;
    Expr horz, vert;
    
    have_horz = get_dynamic(n + 1, &horz);
    if(!have_horz) {
      float h;
      have_horz = get(FloatStyleOptionName(n + 1), &h);
      
      if(have_horz) {
        if(h > 0)
          horz = Number(h);
        else
          horz = Symbol(PMATH_SYMBOL_AUTOMATIC);
      }
      else
        horz = Symbol(PMATH_SYMBOL_INHERITED);
    }
    
    have_vert = get_dynamic(n + 2, &vert);
    if(!have_vert) {
      float v;
      have_vert = get(FloatStyleOptionName(n + 2), &v);
      
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
                     get_symbol(n),
                     List(horz, vert)));
    }
  }
}

void Style::emit_pmath_string(StringStyleOptionName n) {
  Expr e;
  String s;
  
  if(get_dynamic(n, &e)) {
    Gather::emit(Rule(
                   get_symbol(n),
                   e));
  }
  else if(get(n, &s)) {
    Gather::emit(Rule(
                   get_symbol(n),
                   s));
  }
}

void Style::emit_pmath_object(ObjectStyleOptionName n) {
  Expr e;
  
  if(get_dynamic(n, &e)) {
    Gather::emit(Rule(
                   get_symbol(n),
                   e));
  }
  else if(get(n, &e)) {
    Gather::emit(Rule(
                   get_symbol(n),
                   e));
  }
}

void Style::emit_to_pmath(bool with_inherited) {
  Expr e;
  String s;
  int i;
  
  emit_pmath_bool_auto(Antialiasing);
  emit_pmath_float(    AspectRatio);
  emit_pmath_bool(     AutoDelete);
  emit_pmath_bool(     AutoNumberFormating);
  emit_pmath_bool(     AutoSpacing);
  emit_pmath_color(    Background);
  
  if(with_inherited) 
    emit_pmath_string(BaseStyleName);
  
  emit_pmath_object(BorderRadius);
  emit_pmath_object(BoxRotation);
  emit_pmath_object(BoxTransformation);
  
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
  
  emit_pmath_object(ButtonFunction);
  emit_pmath_bool(  ContinuousAction);
  emit_pmath_object(DefaultDuplicateSectionStyle);
  emit_pmath_object(DefaultNewSectionStyle);
  emit_pmath_object(DefaultReturnCreatedSectionStyle);
  
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
  
  emit_pmath_bool(  Editable);
  emit_pmath_bool(  Evaluatable);
  emit_pmath_color( FontColor);
  emit_pmath_object(FontFamilies);
  emit_pmath_float( FontSize);
  
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
  
  emit_pmath_object(GeneratedSectionStyles);
  emit_pmath_float( GridBoxColumnSpacing);
  emit_pmath_float( GridBoxRowSpacing);
  emit_pmath_size(  ImageSizeCommon);
  emit_pmath_string(LanguageCategory);
  emit_pmath_bool(  LineBreakWithin);
  emit_pmath_string(Method);
  emit_pmath_bool(  Placeholder);
  emit_pmath_object(PlotRange);
  emit_pmath_bool(  ReturnCreatesNewSection);
  emit_pmath_object(ScriptSizeMultipliers);
  emit_pmath_bool(  SectionEditDuplicate);
  emit_pmath_bool(  SectionEditDuplicateMakesCopy);
  emit_pmath_margin(SectionFrameLeft);
  emit_pmath_color( SectionFrameColor);
  emit_pmath_margin(SectionFrameMarginLeft);
  emit_pmath_bool(  SectionGenerated);
  emit_pmath_float( SectionGroupPrecedence);
  emit_pmath_margin(SectionMarginLeft);
  
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
  
  emit_pmath_bool(  SectionLabelAutoDelete);
  emit_pmath_bool(  Selectable);
  emit_pmath_bool(  ShowAutoStyles);
  emit_pmath_bool(  ShowSectionBracket);
  emit_pmath_bool(  ShowStringCharacters);
  emit_pmath_bool(  StripOnInput);
  emit_pmath_object(StyleDefinitions);
  emit_pmath_object(TextShadow);
  emit_pmath_bool(  Visible);
  
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
        
      case WindowFrameDialog:
        Gather::emit(Rule(
                       get_symbol(WindowFrame),
                       String("Dialog")));
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
