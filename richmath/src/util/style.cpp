#include <util/style.h>

#include <gui/control-painter.h>
#include <eval/application.h>
#include <eval/dynamic.h>

#include <cmath>
#include <limits>


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
        
        if(r < 0) r = 0.0;
        else if(!(r <= 1)) r = 1.0;
        if(g < 0) g = 0.0;
        else if(!(g <= 1)) g = 1.0;
        if(b < 0) b = 0.0;
        else if(!(b <= 1)) b = 1.0;
        
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
          if(s < 0) s = 0;
          else if(!(s <= 1)) s = 1;
          
          if(obj.expr_length() >= 3) {
            v = obj[3].to_double();
            v = fmod(v, 1.0);
            if(v < 0) v = 0;
            else if(!(v <= 1)) v = 1;
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
      if(l < 0) l = 0;
      else if(!(l <= 1)) l = 1;
      
      return ((int)(l * 255 + 0.5) << 16) | ((int)(l * 255 + 0.5) << 8) | (int)(l * 255 + 0.5);
    }
  }
  
  return -2;
}

static bool keep_dynamic = false;

namespace {
  class StyleEnumConverter: public Shareable {
    public:
      StyleEnumConverter();
      
      bool is_valid_int(int val) {
        return _int_to_expr.search(val) != 0;
      }
      
      bool is_valid_expr(Expr expr) {
        return _expr_to_int.search(expr) != 0;
      }
      
      int  to_int(Expr expr) {
        return _expr_to_int[expr];
      }
      
      Expr to_expr(int val) {
        return _int_to_expr[val];
      }
      
      const Hashtable<Expr, int> &expr_to_int() { return _expr_to_int; }
      
    protected:
      void add(int val, Expr expr);
      
    protected:
      Hashtable<int, Expr, cast_hash> _int_to_expr;
      Hashtable<Expr, int>            _expr_to_int;
  };
  
  class ButtonFrameStyleEnumConverter: public StyleEnumConverter {
    public:
      ButtonFrameStyleEnumConverter()
        : StyleEnumConverter()
      {
        _int_to_expr.default_value = Symbol(PMATH_SYMBOL_AUTOMATIC);
        _expr_to_int.default_value = -1;//PushButton;
        
        add(NoContainerType,      Symbol(PMATH_SYMBOL_NONE));
        add(FramelessButton,      String("Frameless"));
        add(GenericButton,        String("Generic"));
        add(PushButton,           String("DialogBox"));
        add(DefaultPushButton,    String("Defaulted"));
        add(PaletteButton,        String("Palette"));
        add(TooltipWindow,        String("TooltipWindow"));
        add(ListViewItemSelected, String("ListViewItemSelected"));
        add(ListViewItem,         String("ListViewItem"));
      }
  };
  
  class WindowFrameStyleEnumConverter: public StyleEnumConverter {
    public:
      WindowFrameStyleEnumConverter()
        : StyleEnumConverter()
      {
        _int_to_expr.default_value = Expr();
        _expr_to_int.default_value = -1;
        
        add(WindowFrameNormal,  String("Normal"));
        add(WindowFramePalette, String("Palette"));
        add(WindowFrameDialog,  String("Dialog"));
      }
  };
  
  class FontSlantStyleEnumConverter: public StyleEnumConverter {
    public:
      FontSlantStyleEnumConverter()
        : StyleEnumConverter()
      {
        _int_to_expr.default_value = Expr();
        _expr_to_int.default_value = -1;
        
        add(FontSlantPlain,  Symbol(PMATH_SYMBOL_PLAIN));
        add(FontSlantItalic, Symbol(PMATH_SYMBOL_ITALIC));
      }
  };
  
  class FontWeightStyleEnumConverter: public StyleEnumConverter {
    public:
      FontWeightStyleEnumConverter()
        : StyleEnumConverter()
      {
        _int_to_expr.default_value = Expr();
        _expr_to_int.default_value = -1;
        
        add(FontWeightPlain, Symbol(PMATH_SYMBOL_PLAIN));
        add(FontWeightBold,  Symbol(PMATH_SYMBOL_BOLD));
      }
  };
  
  class SubRuleConverter: public StyleEnumConverter {
    public:
      void add(int val, Expr expr) {
        StyleEnumConverter::add(val, expr);
      }
  };
  
  class StyleInformation {
    public:
      StyleInformation() = delete;
      
      static void add_style() {
        if(_num_styles++ == 0) {
          _name_to_key.default_value = -1;
          _key_to_type.default_value = StyleTypeNone;
          
          add_enum(ButtonFrame, Symbol( PMATH_SYMBOL_BUTTONFRAME), new ButtonFrameStyleEnumConverter);
          add_enum(FontSlant,   Symbol( PMATH_SYMBOL_FONTSLANT),   new FontSlantStyleEnumConverter);
          add_enum(FontWeight,  Symbol( PMATH_SYMBOL_FONTWEIGHT),  new FontWeightStyleEnumConverter);
          add_enum(WindowFrame, Symbol( PMATH_SYMBOL_WINDOWFRAME), new WindowFrameStyleEnumConverter);
          
          add_ruleset_head(DockedSections, Symbol( PMATH_SYMBOL_DOCKEDSECTIONS));
          
          
          add(StyleTypeColor,           Background,                       Symbol( PMATH_SYMBOL_BACKGROUND));
          add(StyleTypeColor,           FontColor,                        Symbol( PMATH_SYMBOL_FONTCOLOR));
          add(StyleTypeColor,           SectionFrameColor,                Symbol( PMATH_SYMBOL_SECTIONFRAMECOLOR));
          add(StyleTypeBoolAuto,        Antialiasing,                     Symbol( PMATH_SYMBOL_ANTIALIASING));
          add(StyleTypeBool,            AutoDelete,                       Symbol( PMATH_SYMBOL_AUTODELETE));
          add(StyleTypeBool,            AutoNumberFormating,              Symbol( PMATH_SYMBOL_AUTONUMBERFORMATING));
          add(StyleTypeBool,            AutoSpacing,                      Symbol( PMATH_SYMBOL_AUTOSPACING));
          add(StyleTypeBool,            ContinuousAction,                 Symbol( PMATH_SYMBOL_CONTINUOUSACTION));
          add(StyleTypeBool,            Editable,                         Symbol( PMATH_SYMBOL_EDITABLE));
          add(StyleTypeBool,            Evaluatable,                      Symbol( PMATH_SYMBOL_EVALUATABLE));
          add(StyleTypeBool,            LineBreakWithin,                  Symbol( PMATH_SYMBOL_LINEBREAKWITHIN));
          add(StyleTypeBool,            Placeholder,                      Symbol( PMATH_SYMBOL_PLACEHOLDER));
          add(StyleTypeBool,            ReturnCreatesNewSection,          Symbol( PMATH_SYMBOL_RETURNCREATESNEWSECTION));
          add(StyleTypeBool,            SectionEditDuplicate,             Symbol( PMATH_SYMBOL_SECTIONEDITDUPLICATE));
          add(StyleTypeBool,            SectionEditDuplicateMakesCopy,    Symbol( PMATH_SYMBOL_SECTIONEDITDUPLICATEMAKESCOPY));
          add(StyleTypeBool,            SectionGenerated,                 Symbol( PMATH_SYMBOL_SECTIONGENERATED));
          add(StyleTypeBool,            SectionLabelAutoDelete,           Symbol( PMATH_SYMBOL_SECTIONLABELAUTODELETE));
          add(StyleTypeBool,            Selectable,                       Symbol( PMATH_SYMBOL_SELECTABLE));
          add(StyleTypeBool,            ShowAutoStyles,                   Symbol( PMATH_SYMBOL_SHOWAUTOSTYLES));
          add(StyleTypeBool,            ShowSectionBracket,               Symbol( PMATH_SYMBOL_SHOWSECTIONBRACKET));
          add(StyleTypeBool,            ShowStringCharacters,             Symbol( PMATH_SYMBOL_SHOWSTRINGCHARACTERS));
          add(StyleTypeBool,            StripOnInput,                     Symbol( PMATH_SYMBOL_STRIPONINPUT));
          add(StyleTypeBool,            Visible,                          Symbol( PMATH_SYMBOL_VISIBLE));
          
          add(StyleTypeNumber,          AspectRatio,                      Symbol( PMATH_SYMBOL_ASPECTRATIO));
          add(StyleTypeNumber,          FontSize,                         Symbol( PMATH_SYMBOL_FONTSIZE));
          add(StyleTypeNumber,          GridBoxColumnSpacing,             Symbol( PMATH_SYMBOL_GRIDBOXCOLUMNSPACING));
          add(StyleTypeNumber,          GridBoxRowSpacing,                Symbol( PMATH_SYMBOL_GRIDBOXROWSPACING));
          add(StyleTypeNumber,          Magnification,                    Symbol( PMATH_SYMBOL_MAGNIFICATION));
          
          add(StyleTypeSize,            ImageSizeCommon,                  Symbol( PMATH_SYMBOL_IMAGESIZE));
          // ImageSizeHorizontal
          // ImageSizeVertical
          add(StyleTypeMargin,          SectionMarginLeft,                Symbol( PMATH_SYMBOL_SECTIONMARGINS));
          // SectionMarginRight
          // SectionMarginTop
          // SectionMarginBottom
          add(StyleTypeMargin,          SectionFrameLeft,                 Symbol( PMATH_SYMBOL_SECTIONFRAME));
          // SectionFrameRight
          // SectionFrameTop
          // SectionFrameBottom
          add(StyleTypeMargin,          SectionFrameMarginLeft,           Symbol( PMATH_SYMBOL_SECTIONFRAMEMARGINS));
          // SectionFrameMarginRight
          // SectionFrameMarginTop
          // SectionFrameMarginBottom
          add(StyleTypeNumber,          SectionGroupPrecedence,           Symbol( PMATH_SYMBOL_SECTIONGROUPPRECEDENCE));
          
          
          add(StyleTypeString,          BaseStyleName,                    Symbol( PMATH_SYMBOL_BASESTYLE));
          add(StyleTypeString,          Method,                           Symbol( PMATH_SYMBOL_METHOD));
          add(StyleTypeString,          LanguageCategory,                 Symbol( PMATH_SYMBOL_LANGUAGECATEGORY));
          add(StyleTypeString,          SectionLabel,                     Symbol( PMATH_SYMBOL_SECTIONLABEL));
          add(StyleTypeString,          WindowTitle,                      Symbol( PMATH_SYMBOL_WINDOWTITLE));
          
          add(StyleTypeAny,             Axes,                             Symbol( PMATH_SYMBOL_AXES));
          add(StyleTypeAny,             Ticks,                            Symbol( PMATH_SYMBOL_TICKS));
          add(StyleTypeAny,             Frame,                            Symbol( PMATH_SYMBOL_FRAME));
          add(StyleTypeAny,             FrameTicks,                       Symbol( PMATH_SYMBOL_FRAMETICKS));
          add(StyleTypeAny,             AxesOrigin,                       Symbol( PMATH_SYMBOL_AXESORIGIN));
          add(StyleTypeAny,             ButtonFunction,                   Symbol( PMATH_SYMBOL_BUTTONFUNCTION));
          add(StyleTypeAny,             ScriptSizeMultipliers,            Symbol( PMATH_SYMBOL_SCRIPTSIZEMULTIPLIERS));
          add(StyleTypeAny,             TextShadow,                       Symbol( PMATH_SYMBOL_TEXTSHADOW));
          add(StyleTypeAny,             FontFamilies,                     Symbol( PMATH_SYMBOL_FONTFAMILY));
          add(StyleTypeAny,             FontFeatures,                     Symbol( PMATH_SYMBOL_FONTFEATURES));
          add(StyleTypeAny,             BoxRotation,                      Symbol( PMATH_SYMBOL_BOXROTATION));
          add(StyleTypeAny,             BoxTransformation,                Symbol( PMATH_SYMBOL_BOXTRANSFORMATION));
          add(StyleTypeAny,             PlotRange,                        Symbol( PMATH_SYMBOL_PLOTRANGE));
          add(StyleTypeAny,             BorderRadius,                     Symbol( PMATH_SYMBOL_BORDERRADIUS));
          add(StyleTypeAny,             DefaultDuplicateSectionStyle,     Symbol( PMATH_SYMBOL_DEFAULTDUPLICATESECTIONSTYLE));
          add(StyleTypeAny,             DefaultNewSectionStyle,           Symbol( PMATH_SYMBOL_DEFAULTNEWSECTIONSTYLE));
          add(StyleTypeAny,             DefaultReturnCreatedSectionStyle, Symbol( PMATH_SYMBOL_DEFAULTRETURNCREATEDSECTIONSTYLE));
          add(StyleTypeAny,             StyleDefinitions,                 Symbol( PMATH_SYMBOL_STYLEDEFINITIONS));
          add(StyleTypeAny,             GeneratedSectionStyles,           Symbol( PMATH_SYMBOL_GENERATEDSECTIONSTYLES));
          
          add(StyleTypeAny, DockedSectionsTop,         Rule(Symbol(PMATH_SYMBOL_DOCKEDSECTIONS), String("Top")));
          add(StyleTypeAny, DockedSectionsTopGlass,    Rule(Symbol(PMATH_SYMBOL_DOCKEDSECTIONS), String("TopGlass")));
          add(StyleTypeAny, DockedSectionsBottom,      Rule(Symbol(PMATH_SYMBOL_DOCKEDSECTIONS), String("Bottom")));
          add(StyleTypeAny, DockedSectionsBottomGlass, Rule(Symbol(PMATH_SYMBOL_DOCKEDSECTIONS), String("BottomGlass")));
        }
      }
      
      static void remove_style() {
        if(--_num_styles == 0) {
          _key_to_enum_converter.clear();
          _key_to_type.clear();
          _key_to_name.clear();
          _name_to_key.clear();
        }
      }
      
      static bool is_window_option(int key) {
        return key == Magnification             ||
               key == Visible                   ||
               key == WindowFrame               ||
               key == WindowTitle               ||
               key == DockedSectionsTop         ||
               key == DockedSectionsTopGlass    ||
               key == DockedSectionsBottom      ||
               key == DockedSectionsBottomGlass;
      }
      
      /*static int get_number_of_keys(enum StyleType type) {
        switch(type) {
          case StyleTypeMargin:          return 4;
          case StyleTypeSize:            return 3;
          case StyleTypePointAuto:       return 2;
          case StyleTypeDockedSections4: return 4;
      
          default:
            return 1;
        }
      }*/
      
      static enum StyleType get_type(int key) {
        return _key_to_type[key];
      }
      
      static Expr get_name(int key) {
        return _key_to_name[key];
      }
      
      static int get_key(Expr name) {
        return _name_to_key[name];
      }
      
      static SharedPtr<StyleEnumConverter> get_enum_converter(int key) {
        return _key_to_enum_converter[key];
      }
      
    private:
      static void add(StyleType type, IntStyleOptionName key, const Expr &name) {
        add(type, (int)key, name);
      }
      static void add(StyleType type, FloatStyleOptionName key, const Expr &name) {
        add(type, (int)key, name);
      }
      static void add(StyleType type, StringStyleOptionName key, const Expr &name) {
        add(type, (int)key, name);
      }
      static void add(StyleType type, ObjectStyleOptionName key, const Expr &name) {
        add(type, (int)key, name);
      }
      
      static void add(StyleType type, int key, const Expr &name) {
        assert(type != StyleTypeEnum);
        
        _key_to_type.set(  key, type);
        _key_to_name.set(key, name);
        _name_to_key.set(name, key);
        
        add_to_ruleset(key, name);
      }
      
      static void add_enum(
        IntStyleOptionName             key,
        const Expr                    &name,
        SharedPtr<StyleEnumConverter>  enum_converter
      ) {
        _key_to_enum_converter.set(key, enum_converter);
        _key_to_type.set(          key, StyleTypeEnum);
        _key_to_name.set(          key, name);
        _name_to_key.set(          name, key);
        
        add_to_ruleset(key, name);
      }
      
      static void add_to_ruleset(int key, const Expr &name) {
        if(name.is_rule()) {
          Expr super_name = name[1];
          Expr sub_name   = name[2];
          
          int super_key = _name_to_key[super_name];
          if(_key_to_type[super_key] != StyleTypeRuleSet) {
            pmath_debug_print_object("[not a StyleTypeRuleSet: ", super_name.get(), "]\n");
            return;
          }
          
          SharedPtr<StyleEnumConverter> sec = _key_to_enum_converter[super_key];
          auto sur = dynamic_cast<SubRuleConverter*>(sec.ptr());
          if(!sur) {
            pmath_debug_print_object("[invalid StyleEnumConverter: ", super_name.get(), "]\n");
            return;
          }
          
          sur->add(key, sub_name);
        }
      }
      
      static void add_ruleset_head(
        int         key,
        const Expr &symbol
      ) {
        _key_to_enum_converter.set(key, new SubRuleConverter);
        _key_to_type.set(          key, StyleTypeRuleSet);
        _key_to_name.set(          key, symbol);
        _name_to_key.set(          symbol, key);
      }
      
    private:
      static int _num_styles;
      
      static Hashtable<int, SharedPtr<StyleEnumConverter>, cast_hash> _key_to_enum_converter;
      static Hashtable<int, enum StyleType, cast_hash>                _key_to_type;
      static Hashtable<int, Expr,           cast_hash>                _key_to_name;
      static Hashtable<Expr, int>                                     _name_to_key;
  };
  
  int                                                      StyleInformation::_num_styles = 0;
  Hashtable<int, SharedPtr<StyleEnumConverter>, cast_hash> StyleInformation::_key_to_enum_converter;
  Hashtable<int, enum StyleType, cast_hash>                StyleInformation::_key_to_type;
  Hashtable<int, Expr,           cast_hash>                StyleInformation::_key_to_name;
  Hashtable<Expr, int>                                     StyleInformation::_name_to_key;
}

//{ class StyleEnumConverter ...

StyleEnumConverter::StyleEnumConverter()
  : Shareable()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  _expr_to_int.default_value = -1;
}

void StyleEnumConverter::add(int val, Expr expr) {
  _int_to_expr.set(val, expr);
  _expr_to_int.set(expr, val);
}

//} ... class StyleEnumConverter

//{ class Style ...

Style::Style()
  : Shareable()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  StyleInformation::add_style();
}

Style::Style(Expr options)
  : Shareable()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
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
  static bool allow_strings = true;
  
  if(allow_strings && options.is_string()) {
    set(BaseStyleName, String(options));
  }
  else if(options[0] == PMATH_SYMBOL_LIST) {
    bool old_allow_strings = allow_strings;
    allow_strings = false;
    
    for(size_t i = options.expr_length(); i > 0; --i) {
      add_pmath(options[i]);
    }
    
    allow_strings = old_allow_strings;
  }
  else if(options.is_rule()) {
    Expr lhs = options[1];
    Expr rhs = options[2];
    
    set_pmath(lhs, rhs);
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

bool Style::get(IntStyleOptionName n, int *value) const {
  const IntFloatUnion *v = int_float_values.search(n);
  
  if(!v)
    return false;
    
  *value = v->int_value;
  return true;
}

bool Style::get(FloatStyleOptionName n, float *value) const {
  const IntFloatUnion *v = int_float_values.search(n);
  
  if(!v)
    return false;
    
  *value = v->float_value;
  return true;
}

bool Style::get(StringStyleOptionName n, String *value) const {
  const Expr *v = object_values.search(n);
  
  if(!v || !v->is_string())
    return false;
    
  *value = String(*v);
  return true;
}

bool Style::get(ObjectStyleOptionName n, Expr *value) const {
  const Expr *v = object_values.search(n);
  
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
    if(auto e = object_values.entry(ui)) {
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
        Gather::emit(Rule(get_name(dynamic_options[i]), e));
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

unsigned int Style::count() const {
  return int_float_values.size() + object_values.size();
}

Expr Style::get_name(int n) {
  return StyleInformation::get_name(n);
}

enum StyleType Style::get_type(int n) {
  return StyleInformation::get_type(n);
}

void Style::set_pmath(Expr lhs, Expr rhs) {
  int key = StyleInformation::get_key(lhs);
  
  if(key < 0) {
    pmath_debug_print_object("[unknown option ", lhs.get(), "]\n");
    
    Expr sym;
    if(!get(UnknownOptions, &sym) || !sym.is_symbol()) {
      sym = Expr(pmath_symbol_create_temporary(PMATH_C_STRING("FE`Styles`unknown"), TRUE));
      
      pmath_symbol_set_attributes(sym.get(),
                                  PMATH_SYMBOL_ATTRIBUTE_TEMPORARY | PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE);
                                  
      set(UnknownOptions, sym);
    }
    
    Expr eval;
    if(rhs == PMATH_SYMBOL_INHERITED) {
      eval = Call(Symbol(PMATH_SYMBOL_UNASSIGN),
                  Call(sym, lhs));
    }
    else {
      eval = Call(Symbol(PMATH_SYMBOL_ASSIGN),
                  Call(sym, lhs), rhs);
    }
    
//    Expr eval = Call(Symbol(PMATH_SYMBOL_ASSIGN),
//                     Call(Symbol(PMATH_SYMBOL_DOWNRULES), sym),
//                     Call(Symbol(PMATH_SYMBOL_APPEND),
//                          Call(Symbol(PMATH_SYMBOL_DOWNRULES), sym),
//                          Rule(
//                            Call(Symbol(PMATH_SYMBOL_HOLDPATTERN),
//                                 Call(sym, lhs)),
//                            rhs)));

    Evaluate(eval);
    return;
  }
  
  set_pmath(key, rhs);
}

void Style::set_pmath(int n, Expr obj) {
  if(StyleInformation::is_window_option(n))
    set(InternalHasModifiedWindowOption, true);
    
  enum StyleType type = StyleInformation::get_type(n);
  
  switch(type) {
    case StyleTypeBool:
      set_pmath_bool((IntStyleOptionName)n, obj);
      break;
      
    case StyleTypeBoolAuto:
      set_pmath_bool_auto((IntStyleOptionName)n, obj);
      break;
      
    case StyleTypeColor:
      set_pmath_color((IntStyleOptionName)n, obj);
      break;
      
    case StyleTypeNumber:
      set_pmath_float((FloatStyleOptionName)n, obj);
      break;
      
    case StyleTypeMargin:
      set_pmath_margin((FloatStyleOptionName)n, obj);
      break;
      
    case StyleTypeSize:
      set_pmath_size((FloatStyleOptionName)n, obj);
      break;
      
    case StyleTypeString:
      set_pmath_string((StringStyleOptionName)n, obj);
      break;
      
    case StyleTypeNone:
    case StyleTypeAny:
      set_pmath_object((ObjectStyleOptionName)n, obj);
      break;
      
    case StyleTypeEnum:
      set_pmath_enum((IntStyleOptionName)n, obj);
      break;
      
    case StyleTypeRuleSet:
      set_pmath_ruleset((ObjectStyleOptionName)n, obj);
      break;
  }
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
  else if(obj == PMATH_SYMBOL_INHERITED) {
    remove(n);
    if(!keep_dynamic)
      remove_dynamic(n);
  }
}

void Style::set_pmath_bool(IntStyleOptionName n, Expr obj) {
  if(obj == PMATH_SYMBOL_TRUE)
    set(n, true);
  else if(obj == PMATH_SYMBOL_FALSE)
    set(n, false);
  else if(obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
  else if(obj == PMATH_SYMBOL_INHERITED) {
    remove(n);
    if(!keep_dynamic)
      remove_dynamic(n);
  }
}

void Style::set_pmath_color(IntStyleOptionName n, Expr obj) {
  int c = pmath_to_color(obj);
  
  if(c >= -1)
    set(n, c);
  else if(obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
  else if(obj == PMATH_SYMBOL_INHERITED) {
    remove(n);
    if(!keep_dynamic)
      remove_dynamic(n);
  }
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
  
  if(obj == PMATH_SYMBOL_INHERITED) {
    remove(n);
    if(!keep_dynamic)
      remove_dynamic(n);
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
    remove_dynamic(Horizontal);
    remove_dynamic(Vertical);
    set_dynamic(n, obj);
    return;
  }
  
  if(obj[0] == PMATH_SYMBOL_NCACHE) {
    set_pmath_size(n, obj[2]);
    
    if(!keep_dynamic)
      set((ObjectStyleOptionName)(n + DynamicOffset), obj);
      
    return;
  }
  
  if(obj == PMATH_SYMBOL_INHERITED) {
    remove(n);
    remove(Horizontal);
    remove(Vertical);
    if(!keep_dynamic) {
      remove_dynamic(n);
      remove_dynamic(Horizontal);
      remove_dynamic(Vertical);
    }
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
  
  if(obj == PMATH_SYMBOL_INHERITED) {
    remove(Left);
    remove(Right);
    remove(Top);
    remove(Bottom);
    if(!keep_dynamic) {
      remove_dynamic(Left);
      remove_dynamic(Right);
      remove_dynamic(Top);
      remove_dynamic(Bottom);
    }
  }
}

void Style::set_pmath_string(StringStyleOptionName n, Expr obj) {
  if(obj.is_string())
    set(n, String(obj));
  else if(obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
  else if(obj == PMATH_SYMBOL_INHERITED) {
    remove(n);
    if(!keep_dynamic)
      remove_dynamic(n);
  }
}

void Style::set_pmath_object(ObjectStyleOptionName n, Expr obj) {
  if(obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
  else if(obj == PMATH_SYMBOL_INHERITED) {
    remove(n);
    if(!keep_dynamic)
      remove_dynamic(n);
  }
  else
    set(n, obj);
}

void Style::set_pmath_enum(IntStyleOptionName n, Expr obj) {
  if(obj[0] == PMATH_SYMBOL_DYNAMIC) {
    set_dynamic(n, obj);
  }
  else if(obj == PMATH_SYMBOL_INHERITED) {
    remove(n);
    if(!keep_dynamic)
      remove_dynamic(n);
  }
  else {
    SharedPtr<StyleEnumConverter> enum_converter = StyleInformation::get_enum_converter(n);
    
    assert(enum_converter.is_valid());
    
    if(enum_converter->is_valid_expr(obj))
      set((IntStyleOptionName)n, enum_converter->to_int(obj));
  }
}

void Style::set_pmath_ruleset(ObjectStyleOptionName n, Expr obj) {
  SharedPtr<StyleEnumConverter> key_converter = StyleInformation::get_enum_converter(n);
  assert(key_converter.is_valid());
  
  if(obj[0] == PMATH_SYMBOL_LIST) {
    for(size_t i = 1; i <= obj.expr_length(); ++i) {
      Expr rule = obj[i];
      
      if(rule.is_rule()) {
        Expr lhs = rule[1];
        Expr rhs = rule[2];
        
//        if(rhs == PMATH_SYMBOL_INHERITED)
//          continue;

        int sub_key = key_converter->to_int(lhs);
        if(sub_key < 0) {
          pmath_debug_print_object("[ignoring unknown sub-option ", lhs.get(), "]\n");
        }
        else {
          set_pmath(sub_key, rhs);
        }
      }
    }
  }
  
}


Expr Style::get_pmath(Expr lhs) const {
  int key = StyleInformation::get_key(lhs);
  
  if(key >= 0)
    return get_pmath(key);
    
  return Symbol(PMATH_SYMBOL_INHERITED);
}

Expr Style::get_pmath(int key) const {
  enum StyleType type = StyleInformation::get_type(key);
  
  switch(type) {
    case StyleTypeNone:
      break;
      
    case StyleTypeBool:
      return get_pmath_bool((IntStyleOptionName)key);
      
    case StyleTypeBoolAuto:
      return get_pmath_bool_auto((IntStyleOptionName)key);
      
    case StyleTypeColor:
      return get_pmath_color((IntStyleOptionName)key);
      
    case StyleTypeNumber:
      return get_pmath_float((FloatStyleOptionName)key);
      
    case StyleTypeMargin:
      return get_pmath_margin((FloatStyleOptionName)key);
      
    case StyleTypeSize:
      return get_pmath_size((FloatStyleOptionName)key);
      
    case StyleTypeString:
      return get_pmath_string((StringStyleOptionName)key);
      
    case StyleTypeAny:
      return get_pmath_object((ObjectStyleOptionName)key);
      
    case StyleTypeEnum:
      return get_pmath_enum((IntStyleOptionName)key);
      
    case StyleTypeRuleSet:
      return get_pmath_ruleset((ObjectStyleOptionName)key);
  }
  
  return Symbol(PMATH_SYMBOL_INHERITED);
}

Expr Style::get_pmath_bool_auto(IntStyleOptionName n) const {
  int i;
  
  if(get(n, &i)) {
    switch(i) {
      case 0:
        return Symbol(PMATH_SYMBOL_FALSE);
        
      case 1:
        return Symbol(PMATH_SYMBOL_TRUE);
        
      default:
        return Symbol(PMATH_SYMBOL_AUTOMATIC);
    }
  }
  
  return Symbol(PMATH_SYMBOL_INHERITED);
}

Expr Style::get_pmath_bool(IntStyleOptionName n) const {
  int i;
  if(get(n, &i)) {
    if(i)
      return Symbol(PMATH_SYMBOL_TRUE);
      
    return Symbol(PMATH_SYMBOL_FALSE);
  }
  
  return Symbol(PMATH_SYMBOL_INHERITED);
}

Expr Style::get_pmath_color(IntStyleOptionName n) const {
  int i;
  
  if(get(n, &i))
    return color_to_pmath(i);
    
  return Symbol(PMATH_SYMBOL_INHERITED);
}

Expr Style::get_pmath_float(FloatStyleOptionName n) const {
  float f;
  
  if(get(n, &f))
    return Number(f);
    
  return Symbol(PMATH_SYMBOL_INHERITED);
}

Expr Style::get_pmath_margin(FloatStyleOptionName n) const { // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
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
      
    return List(l, r, t, b);
  }
  
  return Symbol(PMATH_SYMBOL_INHERITED);
}

Expr Style::get_pmath_size(FloatStyleOptionName n) const { // n + {0,1,2} ~= {Common, Horizontal, Vertical}
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
  
  if(have_horz || have_vert)
    return List(horz, vert);
    
  return Symbol(PMATH_SYMBOL_INHERITED);
}

Expr Style::get_pmath_string(StringStyleOptionName n) const {
  String s;
  
  if(get(n, &s))
    return s;
    
  return Symbol(PMATH_SYMBOL_INHERITED);
}

Expr Style::get_pmath_object(ObjectStyleOptionName n) const {
  Expr e;
  
  if(get(n, &e))
    return e;
    
  return Symbol(PMATH_SYMBOL_INHERITED);
}

Expr Style::get_pmath_enum(IntStyleOptionName n) const {
  int i;
  
  if(get(n, &i)) {
    SharedPtr<StyleEnumConverter> enum_converter = StyleInformation::get_enum_converter(n);
    
    assert(enum_converter.is_valid());
    
    return enum_converter->to_expr(i);
  }
  
  return Symbol(PMATH_SYMBOL_INHERITED);
}

Expr Style::get_pmath_ruleset(ObjectStyleOptionName n) const {
  SharedPtr<StyleEnumConverter> key_converter = StyleInformation::get_enum_converter(n);
  
  assert(key_converter.is_valid());
  
  const Hashtable<Expr, int> &table = key_converter->expr_to_int();
  
  bool all_inherited = true;
  Gather g;
  
  for(unsigned int i = 0, count = table.size(); count > 0; ++i) {
    const Entry<Expr, int> *entry = table.entry(i);
    
    if(entry) {
      --count;
      
      Expr value = get_pmath(entry->value);
      Gather::emit(Rule(entry->key, value));
      
      if(value != PMATH_SYMBOL_INHERITED)
        all_inherited = false;
    }
  }
  
  Expr e = g.end();
  if(all_inherited)
    return Symbol(PMATH_SYMBOL_INHERITED);
    
  return e;
}


void Style::emit_pmath(Expr lhs) const {
  int key = StyleInformation::get_key(lhs);
  
  if(key >= 0)
    return emit_pmath(key);
}

void Style::emit_pmath(int n) const {
  Expr e;
  
  if(get_dynamic(n, &e)) {
    Gather::emit(Rule(get_name(n), e));
    return;
  }
  
  e = get_pmath(n);
  if(e != PMATH_SYMBOL_INHERITED)
    Gather::emit(Rule(get_name(n), e));
}

void Style::emit_to_pmath(bool with_inherited) const {
  emit_pmath(Antialiasing);
  emit_pmath(AspectRatio);
  emit_pmath(AutoDelete);
  emit_pmath(AutoNumberFormating);
  emit_pmath(AutoSpacing);
  emit_pmath(Axes);
  emit_pmath(AxesOrigin);
  emit_pmath(Background);
  
  if(with_inherited)
    emit_pmath(BaseStyleName);
    
  emit_pmath(BorderRadius);
  emit_pmath(BoxRotation);
  emit_pmath(BoxTransformation);
  emit_pmath(ButtonFrame);
  emit_pmath(ButtonFunction);
  emit_pmath(ContinuousAction);
  emit_pmath(DefaultDuplicateSectionStyle);
  emit_pmath(DefaultNewSectionStyle);
  emit_pmath(DefaultReturnCreatedSectionStyle);
  emit_pmath(DockedSections);
  emit_pmath(Editable);
  emit_pmath(Evaluatable);
  emit_pmath(FontColor);
  emit_pmath(FontFamilies);
  emit_pmath(FontFeatures);
  emit_pmath(FontSize);
  emit_pmath(FontSlant);
  emit_pmath(FontWeight);
  emit_pmath(Frame);
  emit_pmath(FrameTicks);
  emit_pmath(GeneratedSectionStyles);
  emit_pmath(GridBoxColumnSpacing);
  emit_pmath(GridBoxRowSpacing);
  emit_pmath(ImageSizeCommon);
  emit_pmath(LanguageCategory);
  emit_pmath(LineBreakWithin);
  emit_pmath(Magnification);
  emit_pmath(Method);
  emit_pmath(Placeholder);
  emit_pmath(PlotRange);
  emit_pmath(ReturnCreatesNewSection);
  emit_pmath(ScriptSizeMultipliers);
  emit_pmath(SectionEditDuplicate);
  emit_pmath(SectionEditDuplicateMakesCopy);
  emit_pmath(SectionFrameLeft);
  emit_pmath(SectionFrameColor);
  emit_pmath(SectionFrameMarginLeft);
  emit_pmath(SectionGenerated);
  emit_pmath(SectionGroupPrecedence);
  emit_pmath(SectionMarginLeft);
  emit_pmath(SectionLabel);
  emit_pmath(SectionLabelAutoDelete);
  emit_pmath(Selectable);
  emit_pmath(ShowAutoStyles);
  emit_pmath(ShowSectionBracket);
  emit_pmath(ShowStringCharacters);
  emit_pmath(StripOnInput);
  emit_pmath(StyleDefinitions);
  emit_pmath(TextShadow);
  emit_pmath(Ticks);
  emit_pmath(Visible);
  emit_pmath(WindowFrame);
  emit_pmath(WindowTitle);
  
  Expr e;
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

SharedPtr<Style> Stylesheet::find_parent_style(SharedPtr<Style> s) {
  if(!s.is_valid())
    return nullptr;
    
  String inherited;
  if(s->get(BaseStyleName, &inherited))
    return styles[inherited];
    
  return nullptr;
}

template<typename N, typename T>
bool Stylesheet_get(Stylesheet *self, SharedPtr<Style> s, N n, T *value) {
  for(int count = 20; count && s; --count) {
    if(s->get(n, value))
      return true;
      
    s = self->find_parent_style(s);
  }
  
  return false;
}

bool Stylesheet::get(SharedPtr<Style> s, IntStyleOptionName n, int *value) {
  return Stylesheet_get(this, s, n, value);
}

bool Stylesheet::get(SharedPtr<Style> s, FloatStyleOptionName n, float *value) {
  return Stylesheet_get(this, s, n, value);
}

bool Stylesheet::get(SharedPtr<Style> s, StringStyleOptionName n, String *value) {
  return Stylesheet_get(this, s, n, value);
}

bool Stylesheet::get(SharedPtr<Style> s, ObjectStyleOptionName n, Expr *value) {
  for(int count = 20; count && s; --count) {
    if(s->get(n, value))
      return true;
      
    s = find_parent_style(s);
  }
  
  return false;
}

Expr Stylesheet::get_pmath(SharedPtr<Style> s, Expr n) {
  for(int count = 20; count && s; --count) {
    Expr e = s->get_pmath(n);
    if(e != PMATH_SYMBOL_INHERITED)
      return e;
      
    s = find_parent_style(s);
  }
  
  return Symbol(PMATH_SYMBOL_INHERITED);
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

Expr Stylesheet::get_pmath_with_base(SharedPtr<Style> s, Expr n) {
  Expr e = get_pmath(s, n);
  if(e != PMATH_SYMBOL_INHERITED)
    return e;
    
  return get_pmath(base, n);
}

//} ... class Stylesheet
