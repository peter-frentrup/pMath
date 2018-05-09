#include <util/style.h>

#include <gui/control-painter.h>
#include <eval/application.h>
#include <eval/dynamic.h>

#include <cmath>
#include <limits>


#define STYLE_ASSERT(a) \
  do{if(!(a)){ \
      assert_failed(); \
      assert(a); \
    }}while(0)

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
        for(auto item : obj.items())
          if(!item.is_number())
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

static bool needs_ruledelayed(Expr expr) {
  if(!expr.is_expr())
    return false;
    
  Expr head = expr[0];
  if(head == PMATH_SYMBOL_DYNAMIC || head == PMATH_SYMBOL_FUNCTION)
    return false;
    
  if( head == PMATH_SYMBOL_LIST ||
      head == PMATH_SYMBOL_RULE ||
      head == PMATH_SYMBOL_RULEDELAYED ||
      head == PMATH_SYMBOL_GRAYLEVEL ||
      head == PMATH_SYMBOL_HUE ||
      head == PMATH_SYMBOL_RGBCOLOR)
  {
    for(size_t i = expr.expr_length(); i > 0; --i) {
      if(needs_ruledelayed(expr[i]))
        return true;
    }
    return false;
  }
  
  return true;
}

static bool keep_dynamic = false;

namespace {
  class StyleEnumConverter: public Shareable {
    public:
      StyleEnumConverter();
      
      bool is_valid_key(int val) {
        return _int_to_expr.search(val) != nullptr;
      }
      
      bool is_valid_expr(Expr expr) {
        return _expr_to_int.search(expr) != nullptr;
      }
      
      int to_int(Expr expr) {
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
      void add(StyleOptionName val, Expr expr) {
        StyleEnumConverter::add((int)val, expr);
      }
  };
  
  class StyleInformation {
    public:
      StyleInformation() = delete;
      
      static void add_style() {
        if(_num_styles++ == 0) {
          _name_to_key.default_value = StyleOptionName{ -1};
          _key_to_type.default_value = StyleTypeNone;
          
          add_enum(ButtonFrame, Symbol( PMATH_SYMBOL_BUTTONFRAME), new ButtonFrameStyleEnumConverter);
          add_enum(FontSlant,   Symbol( PMATH_SYMBOL_FONTSLANT),   new FontSlantStyleEnumConverter);
          add_enum(FontWeight,  Symbol( PMATH_SYMBOL_FONTWEIGHT),  new FontWeightStyleEnumConverter);
          add_enum(WindowFrame, Symbol( PMATH_SYMBOL_WINDOWFRAME), new WindowFrameStyleEnumConverter);
          
          add_ruleset_head(DockedSections,     Symbol( PMATH_SYMBOL_DOCKEDSECTIONS));
          add_ruleset_head(TemplateBoxOptions, Symbol( PMATH_SYMBOL_TEMPLATEBOXOPTIONS));
          
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
          add(StyleTypeAny,             DisplayFunction,                  Symbol( PMATH_SYMBOL_DISPLAYFUNCTION));
          add(StyleTypeAny,             InterpretationFunction,           Symbol( PMATH_SYMBOL_INTERPRETATIONFUNCTION));
          add(StyleTypeAny,             SyntaxForm,                       Symbol( PMATH_SYMBOL_SYNTAXFORM));
          add(StyleTypeAny,             StyleDefinitions,                 Symbol( PMATH_SYMBOL_STYLEDEFINITIONS));
          add(StyleTypeAny,             GeneratedSectionStyles,           Symbol( PMATH_SYMBOL_GENERATEDSECTIONSTYLES));
          
          add(StyleTypeAny, DockedSectionsTop,         Rule(Symbol(PMATH_SYMBOL_DOCKEDSECTIONS), String("Top")));
          add(StyleTypeAny, DockedSectionsTopGlass,    Rule(Symbol(PMATH_SYMBOL_DOCKEDSECTIONS), String("TopGlass")));
          add(StyleTypeAny, DockedSectionsBottom,      Rule(Symbol(PMATH_SYMBOL_DOCKEDSECTIONS), String("Bottom")));
          add(StyleTypeAny, DockedSectionsBottomGlass, Rule(Symbol(PMATH_SYMBOL_DOCKEDSECTIONS), String("BottomGlass")));
          
          add(StyleTypeAny, TemplateBoxDefaultDisplayFunction,        Rule(Symbol(PMATH_SYMBOL_TEMPLATEBOXOPTIONS), Symbol(PMATH_SYMBOL_DISPLAYFUNCTION)));
          add(StyleTypeAny, TemplateBoxDefaultInterpretationFunction, Rule(Symbol(PMATH_SYMBOL_TEMPLATEBOXOPTIONS), Symbol(PMATH_SYMBOL_INTERPRETATIONFUNCTION)));
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
      
      static bool is_window_option(StyleOptionName key) {
        StyleOptionName literal_key = key.to_literal();
        return literal_key == Magnification             ||
               literal_key == StyleDefinitions          ||
               literal_key == Visible                   ||
               literal_key == WindowFrame               ||
               literal_key == WindowTitle               ||
               literal_key == DockedSectionsTop         ||
               literal_key == DockedSectionsTopGlass    ||
               literal_key == DockedSectionsBottom      ||
               literal_key == DockedSectionsBottomGlass;
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
      
      static enum StyleType get_type(StyleOptionName key) {
        if(key.is_literal() || key.is_volatile())
          return _key_to_type[key.to_literal()];
        else
          return StyleTypeNone;
      }
      
      static Expr get_name(StyleOptionName key) {
        return _key_to_name[key];
      }
      
      static StyleOptionName get_key(Expr name) {
        return _name_to_key[name];
      }
      
      static SharedPtr<StyleEnumConverter> get_enum_converter(StyleOptionName key) {
        return _key_to_enum_converter[key];
      }
      
    private:
      static void add(StyleType type, StyleOptionName key, const Expr &name) {
        assert(type != StyleTypeEnum);
        
        _key_to_type.set(key, type);
        _key_to_name.set(key, name);
        _name_to_key.set(name, key);
        
        Application::register_currentvalue_provider(name, get_current_style_value);
        
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
      
      static void add_to_ruleset(StyleOptionName key, const Expr &name) {
        if(name.is_rule()) {
          Expr super_name = name[1];
          Expr sub_name   = name[2];
          
          StyleOptionName super_key = _name_to_key[super_name];
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
        StyleOptionName  key,
        const Expr      &symbol
      ) {
        _key_to_enum_converter.set(key, new SubRuleConverter);
        _key_to_type.set(          key, StyleTypeRuleSet);
        _key_to_name.set(          key, symbol);
        _name_to_key.set(          symbol, key);
      }
      
      static Expr get_current_style_value(FrontEndObject *obj, Expr item) {
        Box *box = dynamic_cast<Box*>(obj);
        if(!box)
          return Symbol(PMATH_SYMBOL_FAILED);
          
        StyleOptionName key = Style::get_key(item);
        if(key.is_valid())
          return box->get_pmath_style(key);
          
        return Symbol(PMATH_SYMBOL_FAILED);
      }
      
    private:
      static int _num_styles;
      
      static Hashtable<StyleOptionName, SharedPtr<StyleEnumConverter>> _key_to_enum_converter;
      static Hashtable<StyleOptionName, enum StyleType>                _key_to_type;
      static Hashtable<StyleOptionName, Expr>                          _key_to_name;
      static Hashtable<Expr, StyleOptionName>                          _name_to_key;
  };
  
  int                                                       StyleInformation::_num_styles = 0;
  Hashtable<StyleOptionName, SharedPtr<StyleEnumConverter>> StyleInformation::_key_to_enum_converter;
  Hashtable<StyleOptionName, enum StyleType>                StyleInformation::_key_to_type;
  Hashtable<StyleOptionName, Expr>                          StyleInformation::_key_to_name;
  Hashtable<Expr, StyleOptionName>                          StyleInformation::_name_to_key;
}

namespace richmath {
  class StyleImpl {
    private:
      StyleImpl(Style &_self) : self(_self) {
      }
      
    public:
      static StyleImpl of(Style &_self);
      static const StyleImpl of(const Style &_self);
      
      static bool is_for_int(   StyleOptionName n) { return ((int)n & 0x30000) == 0x00000; }
      static bool is_for_float( StyleOptionName n) { return ((int)n & 0x30000) == 0x10000; }
      static bool is_for_string(StyleOptionName n) { return ((int)n & 0x30000) == 0x20000; }
      static bool is_for_expr(  StyleOptionName n) { return ((int)n & 0x30000) == 0x30000; }
      
    public:
      bool raw_get_int(   StyleOptionName n, int *value) const;
      bool raw_get_float( StyleOptionName n, float *value) const;
      bool raw_get_string(StyleOptionName n, String *value) const;
      bool raw_get_expr(  StyleOptionName n, Expr *value) const;
      
      void raw_set_int(   StyleOptionName n, int value);
      void raw_set_float( StyleOptionName n, float value);
      void raw_set_string(StyleOptionName n, String value);
      void raw_set_expr(  StyleOptionName n, Expr value);
      
      void raw_remove(StyleOptionName n);
      void raw_remove_int(   StyleOptionName n);
      void raw_remove_float( StyleOptionName n);
      void raw_remove_string(StyleOptionName n);
      void raw_remove_expr(  StyleOptionName n);
      
      void remove_dynamic(StyleOptionName n);
      
      // only changes Dynamic() definitions if n.is_literal()
      void set_pmath(StyleOptionName n, Expr obj);
      
    private:
      void set_dynamic(StyleOptionName n, Expr value);
      
      void set_pmath_bool_auto( StyleOptionName n, Expr obj);
      void set_pmath_bool(      StyleOptionName n, Expr obj);
      void set_pmath_color(     StyleOptionName n, Expr obj);
      void set_pmath_float(     StyleOptionName n, Expr obj);
      void set_pmath_margin(    StyleOptionName n, Expr obj); // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
      void set_pmath_size(      StyleOptionName n, Expr obj); // n + {0,1,2} ~= {Common, Horizontal, Vertical}
      void set_pmath_string(    StyleOptionName n, Expr obj);
      void set_pmath_object(    StyleOptionName n, Expr obj);
      void set_pmath_enum(      StyleOptionName n, Expr obj);
      void set_pmath_ruleset(   StyleOptionName n, Expr obj);
      
    private:
      Style &self;
  };
}

StyleImpl StyleImpl::of(Style &_self) {
  return StyleImpl(_self);
}

const StyleImpl StyleImpl::of(const Style &_self) {
  return StyleImpl(const_cast<Style&>(_self));
}

bool StyleImpl::raw_get_int(StyleOptionName n, int *value) const {
  const IntFloatUnion *v = self.int_float_values.search(n);
  
  if(!v)
    return false;
    
  *value = v->int_value;
  return true;
}

bool StyleImpl::raw_get_float(StyleOptionName n, float *value) const {
  const IntFloatUnion *v = self.int_float_values.search(n);
  
  if(!v)
    return false;
    
  *value = v->float_value;
  return true;
}

bool StyleImpl::raw_get_string(StyleOptionName n, String *value) const {
  const Expr *v = self.object_values.search(n);
  
  if(!v || !v->is_string())
    return false;
    
  *value = String(*v);
  return true;
}

bool StyleImpl::raw_get_expr(StyleOptionName n, Expr *value) const {
  const Expr *v = self.object_values.search(n);
  
  if(!v)
    return false;
    
  *value = *v;
  return true;
}

void StyleImpl::raw_set_int(StyleOptionName n, int value) {
  IntFloatUnion v;
  v.int_value = value;
  self.int_float_values.set(n, v);
}

void StyleImpl::raw_set_float(StyleOptionName n, float value) {
  IntFloatUnion v;
  v.float_value = value;
  self.int_float_values.set(n, v);
}

void StyleImpl::raw_set_string(StyleOptionName n, String value) {
  self.object_values.set(n, value);
}

void StyleImpl::raw_set_expr(StyleOptionName n, Expr value) {
  self.object_values.set(n, value);
}

void StyleImpl::raw_remove(StyleOptionName n) {
  self.int_float_values.remove(n);
  self.object_values.remove(n);
}

void StyleImpl::raw_remove_int(StyleOptionName n) {
  STYLE_ASSERT(!n.is_dynamic());
  STYLE_ASSERT(is_for_int(n));
  
  self.int_float_values.remove(n);
}

void StyleImpl::raw_remove_float(StyleOptionName n) {
  STYLE_ASSERT(!n.is_dynamic());
  STYLE_ASSERT(is_for_float(n));
  
  self.int_float_values.remove(n);
}

void StyleImpl::raw_remove_string(StyleOptionName n) {
  STYLE_ASSERT(!n.is_dynamic());
  STYLE_ASSERT(is_for_string(n));
  
  self.object_values.remove(n);
}

void StyleImpl::raw_remove_expr(StyleOptionName n) {
  STYLE_ASSERT(is_for_expr(n) || n.is_dynamic());
  
  self.object_values.remove(n);
}

void StyleImpl::remove_dynamic(StyleOptionName n) {
  STYLE_ASSERT(n.is_literal());
  
  raw_remove_expr(n.to_dynamic());
}

void StyleImpl::set_pmath(StyleOptionName n, Expr obj) {
  if(StyleInformation::is_window_option(n))
    raw_set_int(InternalHasModifiedWindowOption, true);
    
  enum StyleType type = StyleInformation::get_type(n);
  
  switch(type) {
    case StyleTypeBool:
      set_pmath_bool(n, obj);
      break;
      
    case StyleTypeBoolAuto:
      set_pmath_bool_auto(n, obj);
      break;
      
    case StyleTypeColor:
      set_pmath_color(n, obj);
      break;
      
    case StyleTypeNumber:
      set_pmath_float(n, obj);
      break;
      
    case StyleTypeMargin:
      set_pmath_margin(n, obj);
      break;
      
    case StyleTypeSize:
      set_pmath_size(n, obj);
      break;
      
    case StyleTypeString:
      set_pmath_string(n, obj);
      break;
      
    case StyleTypeNone:
    case StyleTypeAny:
      set_pmath_object(n, obj);
      break;
      
    case StyleTypeEnum:
      set_pmath_enum(n, obj);
      break;
      
    case StyleTypeRuleSet:
      set_pmath_ruleset(n, obj);
      break;
  }
}

void StyleImpl::set_dynamic(StyleOptionName n, Expr value) {
  STYLE_ASSERT(n.is_literal());
  
  raw_remove(n);
  raw_set_expr(n.to_dynamic(), value);
  raw_set_int(InternalHasPendingDynamic, true);
}

void StyleImpl::set_pmath_bool_auto(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_int(n));
  
  if(!keep_dynamic && n.is_literal())
    remove_dynamic(n);
    
  if(obj == PMATH_SYMBOL_FALSE)
    raw_set_int(n, 0);
  else if(obj == PMATH_SYMBOL_TRUE)
    raw_set_int(n, 1);
  else if(obj == PMATH_SYMBOL_AUTOMATIC)
    raw_set_int(n, 2);
  else if(obj == PMATH_SYMBOL_INHERITED)
    raw_remove_int(n);
  else if(n.is_literal() && obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
}

void StyleImpl::set_pmath_bool(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_int(n));
  
  if(!keep_dynamic && n.is_literal())
    remove_dynamic(n);
    
  if(obj == PMATH_SYMBOL_FALSE)
    raw_set_int(n, false);
  else if(obj == PMATH_SYMBOL_TRUE)
    raw_set_int(n, true);
  else if(obj == PMATH_SYMBOL_INHERITED)
    raw_remove_int(n);
  else if(n.is_literal() && obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
}

void StyleImpl::set_pmath_color(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_int(n));
  
  if(!keep_dynamic && n.is_literal())
    remove_dynamic(n);
    
  int c = pmath_to_color(obj);
  
  if(c >= -1)
    raw_set_int(n, c);
  else if(obj == PMATH_SYMBOL_INHERITED)
    raw_remove_int(n);
  else if(n.is_literal() && obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
}

void set_pmath_margin(    StyleOptionName n, Expr obj); // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
void set_pmath_size(      StyleOptionName n, Expr obj); // n + {0,1,2} ~= {Common, Horizontal, Vertical}
void set_pmath_string(    StyleOptionName n, Expr obj);
void set_pmath_object(    StyleOptionName n, Expr obj);
void set_pmath_enum(      StyleOptionName n, Expr obj);
void set_pmath_ruleset(   StyleOptionName n, Expr obj);

void StyleImpl::set_pmath_float(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_float(n));
  
  if(!keep_dynamic && n.is_literal())
    remove_dynamic(n);
    
  if(obj.is_number())
    raw_set_float(n, obj.to_double());
  else if(obj == PMATH_SYMBOL_INHERITED)
    raw_remove_float(n);
  else if(n.is_literal() && obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
  else if(obj[0] == PMATH_SYMBOL_NCACHE) {
    raw_set_float(n, obj[2].to_double());
    
    if(!keep_dynamic && n.is_literal()) // TODO: do not treat NCache as Dynamic, but as Definition
      raw_set_expr(n.to_dynamic(), obj);
  }
}

void StyleImpl::set_pmath_margin(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_float(n));
  
  StyleOptionName Left   = n;
  StyleOptionName Right  = StyleOptionName((int)n + 1);
  StyleOptionName Top    = StyleOptionName((int)n + 2);
  StyleOptionName Bottom = StyleOptionName((int)n + 3);
  
  if(!keep_dynamic && n.is_literal()) {
    remove_dynamic(Left);
    remove_dynamic(Right);
    remove_dynamic(Top);
    remove_dynamic(Bottom);
  }
  
  if(obj == PMATH_SYMBOL_TRUE) {
    raw_set_float(Left,   1.0);
    raw_set_float(Right,  1.0);
    raw_set_float(Top,    1.0);
    raw_set_float(Bottom, 1.0);
  }
  else if(obj == PMATH_SYMBOL_FALSE) {
    raw_set_float(Left,   0.0);
    raw_set_float(Right,  0.0);
    raw_set_float(Top,    0.0);
    raw_set_float(Bottom, 0.0);
  }
  else if(obj.is_number()) {
    float f = obj.to_double();
    raw_set_float(Left,   f);
    raw_set_float(Right,  f);
    raw_set_float(Top,    f);
    raw_set_float(Bottom, f);
  }
  else if( obj.is_expr() && obj[0] == PMATH_SYMBOL_LIST) {
    if(obj.expr_length() == 4) {
      set_pmath_float(Left,   obj[1]);
      set_pmath_float(Right,  obj[2]);
      set_pmath_float(Top,    obj[3]);
      set_pmath_float(Bottom, obj[4]);
    }
    else if(obj.expr_length() == 2) {
      if(obj[1].is_number()) {
        float f = obj[1].to_double();
        raw_set_float(Left,  f);
        raw_set_float(Right, f);
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
        raw_set_float(Top,    f);
        raw_set_float(Bottom, f);
      }
      else if(obj[2].is_expr() &&
              obj[2][0] == PMATH_SYMBOL_LIST &&
              obj[2].expr_length() == 2)
      {
        set_pmath_float(Top,    obj[2][1]);
        set_pmath_float(Bottom, obj[2][2]);
      }
    }
  }
  else if(obj == PMATH_SYMBOL_INHERITED) {
    raw_remove_float(Left);
    raw_remove_float(Right);
    raw_remove_float(Top);
    raw_remove_float(Bottom);
  }
  else if(n.is_literal() && obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
}

void StyleImpl::set_pmath_size(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_float(n));
  
  StyleOptionName Horizontal = StyleOptionName((int)n + 1);
  StyleOptionName Vertical   = StyleOptionName((int)n + 2);
  
  if(!keep_dynamic && n.is_literal()) {
    remove_dynamic(n);
    remove_dynamic(Horizontal);
    remove_dynamic(Vertical);
  }
  
  if(obj == PMATH_SYMBOL_AUTOMATIC) {
    raw_remove_float(n);
    
    raw_set_float(Horizontal, ImageSizeAutomatic);
    raw_set_float(Vertical,   ImageSizeAutomatic);
  }
  else if(obj.is_number()) {
    raw_remove_float(n);
    
    float f = obj.to_double();
    raw_set_float(Horizontal, f);
    raw_set_float(Vertical,   ImageSizeAutomatic);
  }
  else if(obj[0] == PMATH_SYMBOL_LIST && obj.expr_length() == 2) {
    raw_remove_float(n);
    
    if(obj[1] == PMATH_SYMBOL_AUTOMATIC)
      raw_set_float(Horizontal, ImageSizeAutomatic);
    else
      set_pmath_float(Horizontal, obj[1]);
      
    if(obj[2] == PMATH_SYMBOL_AUTOMATIC)
      raw_set_float(Vertical, ImageSizeAutomatic);
    else
      set_pmath_float(Vertical, obj[2]);
  }
  else if(obj == PMATH_SYMBOL_INHERITED) {
    raw_remove_float(n);
    raw_remove_float(Horizontal);
    raw_remove_float(Vertical);
    if(!keep_dynamic && !StyleOptionName{n} .is_dynamic()) {
      remove_dynamic(n);
      remove_dynamic(Horizontal);
      remove_dynamic(Vertical);
    }
  }
  else if(n.is_literal() && obj[0] == PMATH_SYMBOL_DYNAMIC) {
    raw_remove_float(n);
    raw_remove_float(Horizontal);
    raw_remove_float(Vertical);
    remove_dynamic(Horizontal);
    remove_dynamic(Vertical);
    set_dynamic(n, obj);
  }
  else if(obj[0] == PMATH_SYMBOL_NCACHE) {
    set_pmath_size(n, obj[2]);
    
    if(!keep_dynamic && n.is_literal()) // TODO: do not treat NCache as Dynamic, but as Definition
      raw_set_expr(n.to_dynamic(), obj);
  }
}

void StyleImpl::set_pmath_string(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_string(n));
  
  if(!keep_dynamic && n.is_literal())
    remove_dynamic(n);
    
  if(obj.is_string())
    raw_set_expr(n, obj);
  else if(obj == PMATH_SYMBOL_INHERITED)
    raw_remove_string(n);
  else if(n.is_literal() && obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
}

void StyleImpl::set_pmath_object(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_expr(n));
  
  if(!keep_dynamic && n.is_literal())
    remove_dynamic(n);
    
  if(obj == PMATH_SYMBOL_INHERITED)
    raw_remove_expr(n);
  else if(n.is_literal() && obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
  else
    raw_set_expr(n, obj);
}

void StyleImpl::set_pmath_enum(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_int(n));
  
  if(!keep_dynamic && n.is_literal())
    remove_dynamic(n);
    
  if(obj == PMATH_SYMBOL_INHERITED)
    raw_remove_int(n);
  else if(n.is_literal() && obj[0] == PMATH_SYMBOL_DYNAMIC)
    set_dynamic(n, obj);
  else {
    SharedPtr<StyleEnumConverter> enum_converter = StyleInformation::get_enum_converter(n);
    
    assert(enum_converter.is_valid());
    
    if(enum_converter->is_valid_expr(obj))
      raw_set_int(n, enum_converter->to_int(obj));
  }
}

void StyleImpl::set_pmath_ruleset(StyleOptionName n, Expr obj) {
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

        StyleOptionName sub_key = StyleOptionName{key_converter->to_int(lhs)};
        if(sub_key.is_valid())
          self.set_pmath(sub_key, rhs);
        else
          pmath_debug_print_object("[ignoring unknown sub-option ", lhs.get(), "]\n");
      }
    }
  }
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
    
    set_pmath_by_unknown_key(lhs, rhs);
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
  if(StyleImpl::of(*this).raw_get_int(n, value))
    return true;
  if(StyleImpl::of(*this).raw_get_int(StyleOptionName(n).to_volatile(), value))
    return true;
  return false;
}

bool Style::get(FloatStyleOptionName n, float *value) const {
  if(StyleImpl::of(*this).raw_get_float(n, value))
    return true;
  if(StyleImpl::of(*this).raw_get_float(StyleOptionName(n).to_volatile(), value))
    return true;
  return false;
}

bool Style::get(StringStyleOptionName n, String *value) const {
  if(StyleImpl::of(*this).raw_get_string(n, value))
    return true;
  if(StyleImpl::of(*this).raw_get_string(StyleOptionName(n).to_volatile(), value))
    return true;
  return false;
}

bool Style::get(ObjectStyleOptionName n, Expr *value) const {
  if(StyleImpl::of(*this).raw_get_expr(n, value))
    return true;
  if(StyleImpl::of(*this).raw_get_expr(StyleOptionName(n).to_volatile(), value))
    return true;
  return false;
}

void Style::set(IntStyleOptionName n, int value) {
  StyleImpl::of(*this).raw_set_int(n, value);
  
  if(!keep_dynamic && StyleOptionName(n).is_literal())
    StyleImpl::of(*this).remove_dynamic(n);
}

void Style::set(FloatStyleOptionName n, float value) {
  StyleImpl::of(*this).raw_set_float(n, value);
  
  if(!keep_dynamic && StyleOptionName(n).is_literal())
    StyleImpl::of(*this).remove_dynamic(n);
}

void Style::set(StringStyleOptionName n, String value) {
  StyleImpl::of(*this).raw_set_string(n, value);
  
  if(!keep_dynamic && StyleOptionName(n).is_literal())
    StyleImpl::of(*this).remove_dynamic(n);
}

void Style::set(ObjectStyleOptionName n, Expr value) {
  StyleImpl::of(*this).raw_set_expr(n, value);
  
  if(!keep_dynamic && StyleOptionName(n).is_literal())
    StyleImpl::of(*this).remove_dynamic(n);
}

void Style::remove(IntStyleOptionName n) {
  StyleImpl::of(*this).raw_remove_int(n);
  StyleImpl::of(*this).raw_remove_int(StyleOptionName(n).to_volatile());
}

void Style::remove(FloatStyleOptionName n) {
  StyleImpl::of(*this).raw_remove_float(n);
  StyleImpl::of(*this).raw_remove_float(StyleOptionName(n).to_volatile());
}

void Style::remove(StringStyleOptionName n) {
  StyleImpl::of(*this).raw_remove_string(n);
  StyleImpl::of(*this).raw_remove_string(StyleOptionName(n).to_volatile());
}

void Style::remove(ObjectStyleOptionName n) {
  StyleImpl::of(*this).raw_remove_expr(n);
  StyleImpl::of(*this).raw_remove_expr(StyleOptionName(n).to_volatile());
}


bool Style::modifies_size(StyleOptionName style_name) {
  switch((int)style_name) {
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
  
  static Array<StyleOptionName> dynamic_options(100);
  
  dynamic_options.length(0);
  
  for(const auto &e : object_values.entries()) {
    if(e.key.is_dynamic())
      dynamic_options.add(e.key.to_literal());
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
    for(i = 0; i < dynamic_options.length(); ++i) {
      StyleOptionName key = dynamic_options[i];
      Expr e = object_values[key.to_dynamic()];
      Dynamic dyn(parent, e);
      
      e = dyn.get_value_now();
      
      if(e != PMATH_SYMBOL_ABORTED && e[0] != PMATH_SYMBOL_DYNAMIC) {
        StyleOptionName eval_key = key.to_volatile();
        keep_dynamic = true;
        set_pmath(eval_key, e);
        keep_dynamic = false;
      }
    }
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

bool Style::is_style_name(Expr n) {
  return StyleInformation::get_key(n).is_valid();
}

StyleOptionName Style::get_key(Expr n) {
  return StyleInformation::get_key(n);
}

Expr Style::get_name(StyleOptionName n) {
  return StyleInformation::get_name(n);
}

enum StyleType Style::get_type(StyleOptionName n) {
  return StyleInformation::get_type(n);
}

void Style::set_pmath_by_unknown_key(Expr lhs, Expr rhs) {
  StyleOptionName key = StyleInformation::get_key(lhs);
  
  if(!key.is_valid()) {
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

void Style::set_pmath(StyleOptionName n, Expr obj) {
  StyleImpl::of(*this).set_pmath(n, obj);
}

Expr Style::get_pmath(StyleOptionName key) const {
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
  
  FloatStyleOptionName horz_name = FloatStyleOptionName(n + 1);
  FloatStyleOptionName vert_name = FloatStyleOptionName(n + 2);
  
  have_horz = get_dynamic(horz_name, &horz);
  if(!have_horz) {
    float h;
    have_horz = get(horz_name, &h);
    
    if(have_horz) {
      if(h > 0)
        horz = Number(h);
      else
        horz = Symbol(PMATH_SYMBOL_AUTOMATIC);
    }
    else
      horz = Symbol(PMATH_SYMBOL_INHERITED);
  }
  
  have_vert = get_dynamic(vert_name, &vert);
  if(!have_vert) {
    float v;
    have_vert = get(vert_name, &v);
    
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
  
  for(auto &entry : table.entries()) {
    Expr value = get_pmath(StyleOptionName{entry.value});
    Gather::emit(Rule(entry.key, value));
    
    if(value != PMATH_SYMBOL_INHERITED)
      all_inherited = false;
  }
  
  Expr e = g.end();
  if(all_inherited)
    return Symbol(PMATH_SYMBOL_INHERITED);
    
  return e;
}

void Style::emit_pmath(StyleOptionName n) const {
  Expr e;
  
  if(get_dynamic(n, &e)) {
    Gather::emit(Rule(get_name(n), e));
    return;
  }
  
  e = get_pmath(n);
  if(e == PMATH_SYMBOL_INHERITED)
    return;
    
  if(needs_ruledelayed(e))
    Gather::emit(RuleDelayed(get_name(n), e));
  else
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
  emit_pmath(DisplayFunction);
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
  emit_pmath(InterpretationFunction);
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
  emit_pmath(SyntaxForm);
  emit_pmath(TemplateBoxOptions);
  emit_pmath(TextShadow);
  emit_pmath(Ticks);
  emit_pmath(Tooltip);
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

static Hashtable<Expr, Stylesheet*> registered_stylesheets;

namespace richmath {
  class StylesheetImpl {
    public:
      StylesheetImpl(Stylesheet &_self) : self(_self) {}
      
    public:
      void add(Expr expr) {
        if(self._name.is_valid()) {
          if(currently_loading.search(self._name)) {
            // TODO: warn about recursive dependency
            return;
          }
          currently_loading.set(self._name, Void());
        }
        
        internal_add(expr);
        
        if(self._name.is_valid())
          currently_loading.remove(self._name);
      }
      
    private:
      static Hashtable<Expr, Void> currently_loading;
      
      void internal_add(Expr expr) {
        // TODO: detect stack overflow/infinite recursion
        
        while(expr.is_expr()) {
          if(expr[0] == PMATH_SYMBOL_DOCUMENT) {
            expr = expr[1];
            continue;
          }
          
          if(expr[0] == PMATH_SYMBOL_SECTIONGROUP) {
            expr = expr[1];
            continue;
          }
          
          if(expr[0] == PMATH_SYMBOL_LIST) {
            size_t len = expr.expr_length();
            for(size_t i = 1; i < len; ++i) {
              internal_add(expr[i]);
            }
            expr = expr[len];
            continue;
          }
          
          if(expr[0] == PMATH_SYMBOL_SECTION) {
            add_section(expr);
            return;
          }
          
          break;
        }
      }
      
      void add_section(Expr expr) {
        Expr name = expr[1];
        if(name[0] == PMATH_SYMBOL_STYLEDATA) {
          Expr data = name[1];
          if(data.is_string()) {
            Expr options(pmath_options_extract(expr.get(), 1));
            if(options.is_null())
              return;
              
            String stylename{data};
            SharedPtr<Style> *style_ptr = self.styles.search(stylename);
            if(style_ptr) {
              (*style_ptr)->add_pmath(options);
            }
            else {
              SharedPtr<Style> style = new Style(options);
              self.styles.set(stylename, style);
            }
            return;
          }
          
          if(expr.expr_length() == 1 && data.is_rule() && data[1] == PMATH_SYMBOL_STYLEDEFINITIONS) {
            SharedPtr<Stylesheet> stylesheet = Stylesheet::try_load(data[2]);
            if(stylesheet) {
              for(auto &other : stylesheet->styles.entries()) {
                SharedPtr<Style> *mine = self.styles.search(other.key);
                if(mine) {
                  (*mine)->merge(other.value);
                }
                else {
                  SharedPtr<Style> copy = new Style();
                  copy->merge(other.value);
                  self.styles.set(other.key, copy);
                }
              }
            }
          }
        }
      }
      
    private:
      Stylesheet &self;
  };
  
  Hashtable<Expr, Void> StylesheetImpl::currently_loading;
}


Stylesheet::~Stylesheet() {
  unregister();
}

void Stylesheet::unregister() {
  if(_name.is_valid()) {
    pmath_debug_print_object("Unregistering stylesheet `", _name.get(), "`\n");
    // TODO: check that registered_stylesheets[_name] == this
    registered_stylesheets.remove(_name);
    _name = Expr();
  }
}

bool Stylesheet::register_as(Expr name) {
  unregister();
  if(!name.is_valid())
    return false;
    
  Stylesheet **old = registered_stylesheets.search(name);
  if(old)
    return false;
    
  _name = name;
  registered_stylesheets.set(_name, this);
  return true;
}

SharedPtr<Stylesheet> Stylesheet::find_registered(Expr name) {
  Stylesheet *stylesheet = registered_stylesheets[name];
  if(!stylesheet)
    return nullptr;
    
  stylesheet->ref();
  return stylesheet;
}

SharedPtr<Stylesheet> Stylesheet::try_load(Expr expr) {
  if(expr.is_string()) {
    SharedPtr<Stylesheet> stylesheet = find_registered(expr);
    if(stylesheet)
      return stylesheet;
      
    Expr held_boxes = Application::interrupt_wait(
                        Parse("Get(ToFileName({FE`$FrontEndDirectory,\"resources\",\"StyleSheets\"},`1`), Head->HoldComplete)", expr),
                        Application::button_timeout);
                        
    if(held_boxes.expr_length() == 1 && held_boxes[0] == PMATH_SYMBOL_HOLDCOMPLETE) {
      stylesheet = new Stylesheet();
      stylesheet->base = Stylesheet::Default->base;
      stylesheet->register_as(expr);
      stylesheet->add(held_boxes[1]);
      return stylesheet;
    }
    
    return nullptr;
  }
  
  if(expr[0] == PMATH_SYMBOL_DOCUMENT) {
    SharedPtr<Stylesheet> stylesheet = new Stylesheet();
    stylesheet->base = Stylesheet::Default->base;
    stylesheet->add(expr);
    return stylesheet;
  }
  
  return nullptr;
}

void Stylesheet::add(Expr expr) {
  StylesheetImpl(*this).add(expr);
}

SharedPtr<Style> Stylesheet::find_parent_style(SharedPtr<Style> s) {
  if(!s.is_valid())
    return nullptr;
    
  String inherited;
  if(s->get(BaseStyleName, &inherited))
    return styles[inherited];
    
  return nullptr;
}

template<typename N, typename T>
static bool Stylesheet_get(Stylesheet *self, SharedPtr<Style> s, N n, T *value) {
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
  return Stylesheet_get(this, s, n, value);
}

Expr Stylesheet::get_pmath(SharedPtr<Style> s, StyleOptionName n) {
  /* TODO: merge structure styles from the whole style hierarchy
   */
  for(int count = 20; count && s; --count) {
    Expr e = s->get_pmath(n);
    if(e != PMATH_SYMBOL_INHERITED)
      return e;
      
    s = find_parent_style(s);
  }
  
  return Symbol(PMATH_SYMBOL_INHERITED);
}

bool Stylesheet::update_dynamic(SharedPtr<Style> s, Box *parent) {
  if(!s)
    return false;
  return s->update_dynamic(parent);
//  bool any_change = false;
//  for(int count = 20; count && s; --count) {
//    any_change = s->update_dynamic(parent) || any_change;
//
//    s = find_parent_style(s);
//  }
//  return any_change;
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

Expr Stylesheet::get_pmath_with_base(SharedPtr<Style> s, StyleOptionName n) {
  Expr e = get_pmath(s, n);
  if(e != PMATH_SYMBOL_INHERITED)
    return e;
    
  return get_pmath(base, n);
}

//} ... class Stylesheet
