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

extern pmath_symbol_t richmath_System_Antialiasing;
extern pmath_symbol_t richmath_System_Appearance;
extern pmath_symbol_t richmath_System_AspectRatio;
extern pmath_symbol_t richmath_System_AutoDelete;
extern pmath_symbol_t richmath_System_AutoNumberFormating;
extern pmath_symbol_t richmath_System_AutoSpacing;
extern pmath_symbol_t richmath_System_Axes;
extern pmath_symbol_t richmath_System_AxesOrigin;
extern pmath_symbol_t richmath_System_Background;
extern pmath_symbol_t richmath_System_BaselinePosition;
extern pmath_symbol_t richmath_System_BaseStyle;
extern pmath_symbol_t richmath_System_BorderRadius;
extern pmath_symbol_t richmath_System_BoxRotation;
extern pmath_symbol_t richmath_System_BoxTransformation;
extern pmath_symbol_t richmath_System_ButtonBox;
extern pmath_symbol_t richmath_System_ButtonBoxOptions;
extern pmath_symbol_t richmath_System_ButtonContents;
extern pmath_symbol_t richmath_System_ButtonData;
extern pmath_symbol_t richmath_System_ButtonFrame;
extern pmath_symbol_t richmath_System_ButtonFunction;
extern pmath_symbol_t richmath_System_ButtonSource;
extern pmath_symbol_t richmath_System_ContinuousAction;
extern pmath_symbol_t richmath_System_DefaultDuplicateSectionStyle;
extern pmath_symbol_t richmath_System_DefaultNewSectionStyle;
extern pmath_symbol_t richmath_System_DefaultReturnCreatedSectionStyle;
extern pmath_symbol_t richmath_System_DisplayFunction;
extern pmath_symbol_t richmath_System_DockedSections;
extern pmath_symbol_t richmath_System_Editable;
extern pmath_symbol_t richmath_System_Enabled;
extern pmath_symbol_t richmath_System_Evaluatable;
extern pmath_symbol_t richmath_System_FillBoxOptions;
extern pmath_symbol_t richmath_System_FillBoxWeight;
extern pmath_symbol_t richmath_System_FontColor;
extern pmath_symbol_t richmath_System_FontFamily;
extern pmath_symbol_t richmath_System_FontFeatures;
extern pmath_symbol_t richmath_System_FontSize;
extern pmath_symbol_t richmath_System_FontSlant;
extern pmath_symbol_t richmath_System_FontWeight;
extern pmath_symbol_t richmath_System_Frame;
extern pmath_symbol_t richmath_System_FrameTicks;
extern pmath_symbol_t richmath_System_FrontEndObject;
extern pmath_symbol_t richmath_System_GeneratedSectionStyles;
extern pmath_symbol_t richmath_System_GridBoxColumnSpacing;
extern pmath_symbol_t richmath_System_GridBoxRowSpacing;
extern pmath_symbol_t richmath_System_ImageSize;
extern pmath_symbol_t richmath_System_InputFieldBoxOptions;
extern pmath_symbol_t richmath_System_InterpretationFunction;
extern pmath_symbol_t richmath_System_LanguageCategory;
extern pmath_symbol_t richmath_System_LineBreakWithin;
extern pmath_symbol_t richmath_System_Magnification;
extern pmath_symbol_t richmath_System_MathFontFamily;
extern pmath_symbol_t richmath_System_Method;
extern pmath_symbol_t richmath_System_PanelBoxOptions;
extern pmath_symbol_t richmath_System_Placeholder;
extern pmath_symbol_t richmath_System_PlotRange;
extern pmath_symbol_t richmath_System_ReturnCreatesNewSection;
extern pmath_symbol_t richmath_System_Saveable;
extern pmath_symbol_t richmath_System_Scaled;
extern pmath_symbol_t richmath_System_ScriptSizeMultipliers;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_SectionDingbat;
extern pmath_symbol_t richmath_System_SectionEditDuplicate;
extern pmath_symbol_t richmath_System_SectionEditDuplicateMakesCopy;
extern pmath_symbol_t richmath_System_SectionEvaluationFunction;
extern pmath_symbol_t richmath_System_SectionFrame;
extern pmath_symbol_t richmath_System_SectionFrameColor;
extern pmath_symbol_t richmath_System_SectionFrameLabelMargins;
extern pmath_symbol_t richmath_System_SectionFrameMargins;
extern pmath_symbol_t richmath_System_SectionGenerated;
extern pmath_symbol_t richmath_System_SectionGroup;
extern pmath_symbol_t richmath_System_SectionGroupPrecedence;
extern pmath_symbol_t richmath_System_SectionLabel;
extern pmath_symbol_t richmath_System_SectionLabelAutoDelete;
extern pmath_symbol_t richmath_System_SectionMargins;
extern pmath_symbol_t richmath_System_Selectable;
extern pmath_symbol_t richmath_System_ShowAutoStyles;
extern pmath_symbol_t richmath_System_ShowSectionBracket;
extern pmath_symbol_t richmath_System_ShowStringCharacters;
extern pmath_symbol_t richmath_System_StripOnInput;
extern pmath_symbol_t richmath_System_StyleData;
extern pmath_symbol_t richmath_System_StyleDefinitions;
extern pmath_symbol_t richmath_System_SurdForm;
extern pmath_symbol_t richmath_System_SynchronousUpdating;
extern pmath_symbol_t richmath_System_SyntaxForm;
extern pmath_symbol_t richmath_System_TemplateBoxOptions;
extern pmath_symbol_t richmath_System_TextShadow;
extern pmath_symbol_t richmath_System_Ticks;
extern pmath_symbol_t richmath_System_Tooltip;
extern pmath_symbol_t richmath_System_Visible;
extern pmath_symbol_t richmath_System_WholeSectionGroupOpener;
extern pmath_symbol_t richmath_System_WindowFrame;
extern pmath_symbol_t richmath_System_WindowTitle;

using namespace richmath;

bool richmath::get_factor_of_scaled(Expr expr, double *value) {
  assert(value != nullptr);
  
  if(expr[0] != richmath_System_Scaled)
    return false;
  
  if(expr.expr_length() != 1)
    return false;
  
  Expr val = expr[1];
  if(val[0] == PMATH_SYMBOL_NCACHE)
    val = val[2];
  
  if(val[0] == PMATH_SYMBOL_LIST && val.expr_length() == 1)
    val = val[1];
  
  if(val.is_number()) {
    *value = val.to_double();
    return true;
  }
  
  return false;
}

namespace {
  class StyleEnumConverter: public Shareable {
    public:
      StyleEnumConverter();
      
      bool is_valid_key(int val) {    return _int_to_expr.search(val) != nullptr; }
      bool is_valid_expr(Expr expr) { return _expr_to_int.search(expr) != nullptr; }
      
      int to_int(Expr expr) { return _expr_to_int[expr]; }
      Expr to_expr(int val) { return _int_to_expr[val]; }
      
      const Hashtable<Expr, int> &expr_to_int() { return _expr_to_int; }
      
    protected:
      void add(int val, Expr expr);
      
    protected:
      Hashtable<int, Expr> _int_to_expr;
      Hashtable<Expr, int> _expr_to_int;
  };
  
  struct ButtonFrameStyleEnumConverter: public StyleEnumConverter {
    ButtonFrameStyleEnumConverter();
  };
  
  struct ButtonSourceStyleEnumConverter: public StyleEnumConverter {
    ButtonSourceStyleEnumConverter();
  };
  
  struct WindowFrameStyleEnumConverter: public StyleEnumConverter {
    WindowFrameStyleEnumConverter();
  };
  
  struct FontSlantStyleEnumConverter: public StyleEnumConverter {
    FontSlantStyleEnumConverter();
  };
  
  struct FontWeightStyleEnumConverter: public StyleEnumConverter {
    FontWeightStyleEnumConverter();
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
      
      static void add_style();
      static void remove_style();
      
      static bool is_window_option(StyleOptionName key);
      static bool requires_child_resize(StyleOptionName key);
      
      static enum StyleType get_type(StyleOptionName key) {
        if(key.is_literal() || key.is_volatile())
          return _key_to_type[key.to_literal()];
        else
          return StyleTypeNone;
      }
      
      static Expr get_name(StyleOptionName key) { return _key_to_name[key]; }
      
      static StyleOptionName get_key(Expr name) { return _name_to_key[name]; }
      
      static SharedPtr<StyleEnumConverter> get_enum_converter(StyleOptionName key) {
        return _key_to_enum_converter[key.to_literal()];
      }
      
    private:
      static void add(StyleType type, StyleOptionName key, const Expr &name);
      
      static void add_enum(IntStyleOptionName key, const Expr &name, SharedPtr<StyleEnumConverter> enum_converter);
      static void add_to_ruleset(StyleOptionName key, const Expr &name);
      static void add_ruleset_head(StyleOptionName key, const Expr &symbol);
      
    public:
      static Expr get_current_style_value(FrontEndObject *obj, Expr item);
      static bool put_current_style_value(FrontEndObject *obj, Expr item, Expr rhs);
      
      static bool needs_ruledelayed(Expr expr);
      
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
      
      static bool is_for_color( StyleOptionName n) { return ((int)n & 0x70000) == 0x00000; }
      static bool is_for_int(   StyleOptionName n) { return ((int)n & 0x70000) == 0x10000; }
      static bool is_for_float( StyleOptionName n) { return ((int)n & 0x70000) == 0x20000; }
      static bool is_for_string(StyleOptionName n) { return ((int)n & 0x70000) == 0x30000; }
      static bool is_for_expr(  StyleOptionName n) { return ((int)n & 0x70000) == 0x40000; }
      
    public:
      bool raw_get_color( StyleOptionName n, Color  *value) const;
      bool raw_get_int(   StyleOptionName n, int    *value) const;
      bool raw_get_float( StyleOptionName n, float  *value) const;
      bool raw_get_string(StyleOptionName n, String *value) const;
      bool raw_get_expr(  StyleOptionName n, Expr   *value) const;
      
      bool raw_set_color( StyleOptionName n, Color value);
      bool raw_set_int(   StyleOptionName n, int value);
      bool raw_set_float( StyleOptionName n, float value);
      bool raw_set_string(StyleOptionName n, String value);
      bool raw_set_expr(  StyleOptionName n, Expr value);
      
      bool raw_remove(StyleOptionName n);
      bool raw_remove_color( StyleOptionName n);
      bool raw_remove_int(   StyleOptionName n);
      bool raw_remove_float( StyleOptionName n);
      bool raw_remove_string(StyleOptionName n);
      bool raw_remove_expr(  StyleOptionName n);
      
      bool remove_dynamic(StyleOptionName n);
      
      bool remove_all_volatile();
      void collect_unused_dynamic(Hashtable<StyleOptionName, Expr> &dynamic_styles);
      
      // only changes Dynamic() definitions if n.is_literal()
      bool set_pmath(StyleOptionName n, Expr obj);
      
    private:
      bool set_dynamic(StyleOptionName n, Expr value);
      
      bool set_pmath_bool_auto( StyleOptionName n, Expr obj);
      bool set_pmath_bool(      StyleOptionName n, Expr obj);
      bool set_pmath_color(     StyleOptionName n, Expr obj);
      bool set_pmath_float(     StyleOptionName n, Expr obj);
      bool set_pmath_margin(    StyleOptionName n, Expr obj); // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
      bool set_pmath_size(      StyleOptionName n, Expr obj); // n + {0,1,2} ~= {Common, Horizontal, Vertical}
      bool set_pmath_string(    StyleOptionName n, Expr obj);
      bool set_pmath_object(    StyleOptionName n, Expr obj);
      bool set_pmath_enum(      StyleOptionName n, Expr obj);
      bool set_pmath_ruleset(   StyleOptionName n, Expr obj);
      
    public:
      static Expr merge_style_values(StyleOptionName key, Expr newer, Expr older);
      
      void emit_definition(StyleOptionName key) const;
      Expr raw_get_pmath(StyleOptionName key, Expr inherited) const;
      
    private:
      static Expr merge_ruleset_members(StyleOptionName key, Expr newer, Expr older);
      static Expr merge_list_members(Expr newer, Expr older);
      static Expr merge_margin_values(Expr newer, Expr older);
      
      static Expr inherited_list_member(Expr inherited, int index);
      static Expr inherited_ruleset_member(Expr inherited, Expr key);
      static Expr inherited_margin_leftright(Expr inherited);
      static Expr inherited_margin_left(Expr inherited);
      static Expr inherited_margin_right(Expr inherited);
      static Expr inherited_margin_topbottom(Expr inherited);
      static Expr inherited_margin_top(Expr inherited);
      static Expr inherited_margin_bottom(Expr inherited);
      
      Expr raw_get_pmath_bool_auto( StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_bool(      StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_color(     StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_float(     StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_margin(    StyleOptionName n, Expr inherited) const; // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
      Expr raw_get_pmath_size(      StyleOptionName n, Expr inherited) const; // n + {0,1,2} ~= {Common, Horizontal, Vertical}
      Expr raw_get_pmath_string(    StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_object(    StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_enum(      StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_ruleset(   StyleOptionName n, Expr inherited) const;
      
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

bool StyleImpl::raw_get_color(StyleOptionName n, Color *value) const {
  const IntFloatUnion *v = self.int_float_values.search(n);
  
  if(!v)
    return false;
    
  *value = Color::decode(v->int_value);
  return true;
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

bool StyleImpl::raw_set_color(StyleOptionName n, Color value) {
  IntFloatUnion v;
  v.int_value = value.encode();
  return self.int_float_values.modify(n, v, [](IntFloatUnion v1, IntFloatUnion v2) { return v1.int_value == v2.int_value; });
}

bool StyleImpl::raw_set_int(StyleOptionName n, int value) {
  IntFloatUnion v;
  v.int_value = value;
  return self.int_float_values.modify(n, v, [](IntFloatUnion v1, IntFloatUnion v2) { return v1.int_value == v2.int_value; });
}

bool StyleImpl::raw_set_float(StyleOptionName n, float value) {
  IntFloatUnion v;
  v.float_value = value;
  return self.int_float_values.modify(n, v, [](IntFloatUnion v1, IntFloatUnion v2) { return v1.int_value == v2.int_value; });
}

bool StyleImpl::raw_set_string(StyleOptionName n, String value) {
  return self.object_values.modify(n, value, [](const Expr &v1, const Expr &v2) { return v1 == v2; });
}

bool StyleImpl::raw_set_expr(StyleOptionName n, Expr value) {
  return self.object_values.modify(n, value, [](const Expr &v1, const Expr &v2) { return v1 == v2; });
}

bool StyleImpl::raw_remove(StyleOptionName n) {
  bool any_change = false;
  any_change = self.int_float_values.remove(n) || any_change;
  any_change = self.object_values.remove(n)    || any_change;
  return any_change;
}

bool StyleImpl::raw_remove_color(StyleOptionName n) {
  STYLE_ASSERT(!n.is_dynamic());
  STYLE_ASSERT(is_for_color(n));
  
  return self.int_float_values.remove(n);
}

bool StyleImpl::raw_remove_int(StyleOptionName n) {
  STYLE_ASSERT(!n.is_dynamic());
  STYLE_ASSERT(is_for_int(n));
  
  return self.int_float_values.remove(n);
}

bool StyleImpl::raw_remove_float(StyleOptionName n) {
  STYLE_ASSERT(!n.is_dynamic());
  STYLE_ASSERT(is_for_float(n));
  
  return self.int_float_values.remove(n);
}

bool StyleImpl::raw_remove_string(StyleOptionName n) {
  STYLE_ASSERT(!n.is_dynamic());
  STYLE_ASSERT(is_for_string(n));
  
  return self.object_values.remove(n);
}

bool StyleImpl::raw_remove_expr(StyleOptionName n) {
  STYLE_ASSERT(is_for_expr(n) || n.is_dynamic());
  
  return self.object_values.remove(n);
}

bool StyleImpl::remove_dynamic(StyleOptionName n) {
  STYLE_ASSERT(n.is_literal());
  
  return raw_remove_expr(n.to_dynamic());
}

bool StyleImpl::remove_all_volatile() {
  static Array<StyleOptionName> volatile_int_float_options(100);
  static Array<StyleOptionName> volatile_object_options(100);
  
  volatile_int_float_options.length(0);
  volatile_object_options.length(0);
  
  for(const auto &e : self.int_float_values.entries()) {
    if(e.key.is_volatile())
      volatile_int_float_options.add(e.key);
  }
  
  for(const auto &e : self.object_values.entries()) {
    if(e.key.is_volatile())
      volatile_object_options.add(e.key);
  }
  
  for(const auto &key : volatile_int_float_options)
    self.int_float_values.remove(key);
  for(const auto &key : volatile_object_options)
    self.object_values.remove(key);
  
  return volatile_int_float_options.length() > 0 || volatile_object_options.length() > 0;
}

void StyleImpl::collect_unused_dynamic(Hashtable<StyleOptionName, Expr> &dynamic_styles) {
  for(const auto &e : self.object_values.entries()) {
    if(e.key.is_dynamic() && !dynamic_styles.search(e.key)) {
      dynamic_styles.set(e.key, e.value);
    }
  }
}

bool StyleImpl::set_pmath(StyleOptionName n, Expr obj) {
  enum StyleType type = StyleInformation::get_type(n);
  
  bool any_change = false;
  switch(type) {
    case StyleTypeNone:
      pmath_debug_print("[Cannot encode style %u]\n", (unsigned)(int)n);
      break;
      
    case StyleTypeBool:
      any_change = set_pmath_bool(n, obj);
      break;
      
    case StyleTypeBoolAuto:
      any_change = set_pmath_bool_auto(n, obj);
      break;
      
    case StyleTypeColor:
      any_change = set_pmath_color(n, obj);
      break;
      
    case StyleTypeNumber:
      any_change = set_pmath_float(n, obj);
      break;
      
    case StyleTypeMargin:
      any_change = set_pmath_margin(n, obj);
      break;
      
    case StyleTypeSize:
      any_change = set_pmath_size(n, obj);
      break;
      
    case StyleTypeString:
      any_change = set_pmath_string(n, obj);
      break;
      
    case StyleTypeAny:
      any_change = set_pmath_object(n, obj);
      break;
      
    case StyleTypeEnum:
      any_change = set_pmath_enum(n, obj);
      break;
      
    case StyleTypeRuleSet:
      any_change = set_pmath_ruleset(n, obj);
      break;
  }
  
  if(any_change) {  
    if(StyleInformation::is_window_option(n))
      raw_set_int(InternalHasModifiedWindowOption, true);
      
    if(StyleInformation::requires_child_resize(n))
      raw_set_int(InternalRequiresChildResize, true);
  }
  return any_change;
}

bool StyleImpl::set_dynamic(StyleOptionName n, Expr value) {
  STYLE_ASSERT(n.is_literal());
  
  bool any_change = false;
  any_change = raw_remove(n) || any_change;
  any_change = raw_set_expr(n.to_dynamic(), value) || any_change;
  
  raw_set_int(InternalHasPendingDynamic, true);
  return any_change;
}

bool StyleImpl::set_pmath_bool_auto(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_int(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
    
  if(obj == PMATH_SYMBOL_FALSE)
    return raw_set_int(n, AutoBoolFalse) || any_change;
  
  if(obj == PMATH_SYMBOL_TRUE)
    return raw_set_int(n, AutoBoolTrue) || any_change;
  
  if(obj == PMATH_SYMBOL_AUTOMATIC)
    return raw_set_int(n, AutoBoolAutomatic) || any_change;
  
  if(obj == PMATH_SYMBOL_INHERITED)
    return raw_remove_int(n) || any_change;
    
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  return any_change;
}

bool StyleImpl::set_pmath_bool(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_int(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
    
  if(obj == PMATH_SYMBOL_FALSE)
    return raw_set_int(n, false) || any_change;
    
  if(obj == PMATH_SYMBOL_TRUE)
    return raw_set_int(n, true) || any_change;
    
  if(obj == PMATH_SYMBOL_INHERITED)
    return raw_remove_int(n) || any_change;
    
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  return any_change;
}

bool StyleImpl::set_pmath_color(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_color(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
    
  Color c = Color::from_pmath(obj);
  
  if(c.is_valid() || c.is_none())
    return raw_set_color(n, c) || any_change;
  
  if(obj == PMATH_SYMBOL_INHERITED)
    return raw_remove_color(n) || any_change;
    
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  return any_change;
}

bool StyleImpl::set_pmath_float(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_float(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
    
  if(obj.is_number())
    return raw_set_float(n, obj.to_double()) || any_change;
  
  if(obj == PMATH_SYMBOL_INHERITED)
    return raw_remove_float(n) || any_change;
  
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  if(obj[0] == PMATH_SYMBOL_NCACHE) {
    any_change = raw_set_float(n, obj[2].to_double()) || any_change;
    
    if(n.is_literal()) // TODO: do not treat NCache as Dynamic, but as Definition
      any_change = raw_set_expr(n.to_dynamic(), obj) || any_change;
    
    return any_change;
  }
  
  return any_change;
}

bool StyleImpl::set_pmath_margin(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_float(n));
  
  StyleOptionName Left   = n;
  StyleOptionName Right  = StyleOptionName((int)n + 1);
  StyleOptionName Top    = StyleOptionName((int)n + 2);
  StyleOptionName Bottom = StyleOptionName((int)n + 3);
  
  bool any_change = false;
  if(n.is_literal()) {
    any_change = remove_dynamic(Left)   || any_change;
    any_change = remove_dynamic(Right)  || any_change;
    any_change = remove_dynamic(Top)    || any_change;
    any_change = remove_dynamic(Bottom) || any_change;
  }
  
  if(obj == PMATH_SYMBOL_TRUE) {
    any_change = raw_set_float(Left,   1.0) || any_change;
    any_change = raw_set_float(Right,  1.0) || any_change;
    any_change = raw_set_float(Top,    1.0) || any_change;
    any_change = raw_set_float(Bottom, 1.0) || any_change;
    return any_change;
  }
  
  if(obj == PMATH_SYMBOL_FALSE) {
    any_change = raw_set_float(Left,   0.0) || any_change;
    any_change = raw_set_float(Right,  0.0) || any_change;
    any_change = raw_set_float(Top,    0.0) || any_change;
    any_change = raw_set_float(Bottom, 0.0) || any_change;
    return any_change;
  }
  
  if(obj.is_number()) {
    float f = obj.to_double();
    any_change = raw_set_float(Left,   f) || any_change;
    any_change = raw_set_float(Right,  f) || any_change;
    any_change = raw_set_float(Top,    f) || any_change;
    any_change = raw_set_float(Bottom, f) || any_change;
    return any_change;
  }
  
  if( obj.is_expr() && obj[0] == PMATH_SYMBOL_LIST) {
    if(obj.expr_length() == 4) {
      any_change = set_pmath_float(Left,   obj[1]) || any_change;
      any_change = set_pmath_float(Right,  obj[2]) || any_change;
      any_change = set_pmath_float(Top,    obj[3]) || any_change;
      any_change = set_pmath_float(Bottom, obj[4]) || any_change;
      return any_change;
    }
    
    if(obj.expr_length() == 2) {
      if(obj[1].is_number()) {
        float f = obj[1].to_double();
        any_change = raw_set_float(Left,  f) || any_change;
        any_change = raw_set_float(Right, f) || any_change;
      }
      else if(obj[1].is_expr() &&
              obj[1][0] == PMATH_SYMBOL_LIST &&
              obj[1].expr_length() == 2)
      {
        any_change = set_pmath_float(Left,  obj[1][1]) || any_change;
        any_change = set_pmath_float(Right, obj[1][2]) || any_change;
      }
      
      if(obj[2].is_number()) {
        float f = obj[2].to_double();
        any_change = raw_set_float(Top,    f) || any_change;
        any_change = raw_set_float(Bottom, f) || any_change;
      }
      else if(obj[2].is_expr() &&
              obj[2][0] == PMATH_SYMBOL_LIST &&
              obj[2].expr_length() == 2)
      {
        any_change = set_pmath_float(Top,    obj[2][1]) || any_change;
        any_change = set_pmath_float(Bottom, obj[2][2]) || any_change;
      }
      
      return any_change;
    }
    return any_change;
  }
  
  if(obj == PMATH_SYMBOL_INHERITED) {
    any_change = raw_remove_float(Left)   || any_change;
    any_change = raw_remove_float(Right)  || any_change;
    any_change = raw_remove_float(Top)    || any_change;
    any_change = raw_remove_float(Bottom) || any_change;
    return any_change;
  }
  
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  return any_change;
}

bool StyleImpl::set_pmath_size(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_float(n));
  
  StyleOptionName Horizontal = StyleOptionName((int)n + 1);
  StyleOptionName Vertical   = StyleOptionName((int)n + 2);
  
  bool any_change = false;
  if(n.is_literal()) {
    any_change = remove_dynamic(n)          || any_change;
    any_change = remove_dynamic(Horizontal) || any_change;
    any_change = remove_dynamic(Vertical)   || any_change;
  }
  
  if(obj == PMATH_SYMBOL_AUTOMATIC) {
    any_change = raw_remove_float(n) || any_change;
    
    any_change = raw_set_float(Horizontal, ImageSizeAutomatic) || any_change;
    any_change = raw_set_float(Vertical,   ImageSizeAutomatic) || any_change;
    return any_change;
  }
  
  if(obj.is_number()) {
    any_change = raw_remove_float(n) || any_change;
    
    float f = obj.to_double();
    any_change = raw_set_float(Horizontal, f)                  || any_change;
    any_change = raw_set_float(Vertical,   ImageSizeAutomatic) || any_change;
    return any_change;
  }
  
  if(obj[0] == PMATH_SYMBOL_LIST && obj.expr_length() == 2) {
    any_change = raw_remove_float(n) || any_change;
    
    if(obj[1] == PMATH_SYMBOL_AUTOMATIC)
      any_change = raw_set_float(Horizontal, ImageSizeAutomatic) || any_change;
    else
      any_change = set_pmath_float(Horizontal, obj[1]) || any_change;
      
    if(obj[2] == PMATH_SYMBOL_AUTOMATIC)
      any_change = raw_set_float(Vertical, ImageSizeAutomatic) || any_change;
    else
      any_change = set_pmath_float(Vertical, obj[2]) || any_change;
      
    return any_change;
  }
  
  if(obj == PMATH_SYMBOL_INHERITED) {
    any_change = raw_remove_float(n)          || any_change;
    any_change = raw_remove_float(Horizontal) || any_change;
    any_change = raw_remove_float(Vertical)   || any_change;
    if(!StyleOptionName{n} .is_dynamic()) {
      any_change = remove_dynamic(n)          || any_change;
      any_change = remove_dynamic(Horizontal) || any_change;
      any_change = remove_dynamic(Vertical)   || any_change;
    }
    return any_change;
  }
  
  if(n.is_literal() && Dynamic::is_dynamic(obj)) {
    any_change = raw_remove_float(n)          || any_change;
    any_change = raw_remove_float(Horizontal) || any_change;
    any_change = raw_remove_float(Vertical)   || any_change;
    any_change = remove_dynamic(Horizontal)   || any_change;
    any_change = remove_dynamic(Vertical)     || any_change;
    any_change = set_dynamic(n, obj)          || any_change;
    return any_change;
  }
  
  if(obj[0] == PMATH_SYMBOL_NCACHE) {
    any_change = set_pmath_size(n, obj[2]) || any_change;
    
    if(n.is_literal()) // TODO: do not treat NCache as Dynamic, but as Definition
      any_change = raw_set_expr(n.to_dynamic(), obj) || any_change;
      
    return any_change;
  }
  
  return any_change;
}

bool StyleImpl::set_pmath_string(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_string(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
    
  if(obj.is_string()) {
    if(n == BaseStyleName) {
      if(raw_set_string(n, String(obj))) {
        any_change = true;
        raw_set_int(InternalHasPendingDynamic, true);
        raw_set_int(InternalHasNewBaseStyle, true);
      }
      return any_change;
    }
    
    return raw_set_string(n, String(obj)) || any_change;
  }
  
  if(obj == PMATH_SYMBOL_INHERITED)
    return raw_remove_string(n) || any_change;
  
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  return any_change;
}

bool StyleImpl::set_pmath_object(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_expr(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
  
  if(obj == PMATH_SYMBOL_INHERITED)
    return raw_remove_expr(n) || any_change;
  
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
    
  if(n == StyleDefinitions) {
    if(raw_set_expr(n, obj)) {
      any_change = true;
      raw_set_int(InternalHasPendingDynamic, true);
    }
    return any_change;
  }
  
  return raw_set_expr(n, obj) || any_change;
}

bool StyleImpl::set_pmath_enum(StyleOptionName n, Expr obj) {
  STYLE_ASSERT(is_for_int(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
    
  if(obj == PMATH_SYMBOL_INHERITED)
    return raw_remove_int(n) || any_change;
  
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  SharedPtr<StyleEnumConverter> enum_converter = StyleInformation::get_enum_converter(n);
  assert(enum_converter.is_valid());
  
  if(enum_converter->is_valid_expr(obj))
    return raw_set_int(n, enum_converter->to_int(obj)) || any_change;
  
  return any_change;
}

bool StyleImpl::set_pmath_ruleset(StyleOptionName n, Expr obj) {
  SharedPtr<StyleEnumConverter> key_converter = StyleInformation::get_enum_converter(n);
  assert(key_converter.is_valid());
  
  bool any_change = false;
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
          any_change = set_pmath(sub_key, rhs) || any_change;
        else
          pmath_debug_print_object("[ignoring unknown sub-option ", lhs.get(), "]\n");
      }
    }
  }
  
  return any_change;
}

Expr StyleImpl::merge_style_values(StyleOptionName key, Expr newer, Expr older) {
  if(newer == PMATH_SYMBOL_INHERITED)
    return older;
    
  if(older == PMATH_SYMBOL_INHERITED)
    return newer;
    
  enum StyleType type = StyleInformation::get_type(key);
  
  switch(type) {
    case StyleTypeMargin:
      return merge_margin_values(std::move(newer), std::move(older));
      
    case StyleTypeSize:
      return merge_list_members(std::move(newer), std::move(older));
      break;
      
    case StyleTypeRuleSet:
      return merge_ruleset_members(key, std::move(newer), std::move(older));
      
    default:
      break;
  }
  return newer;
}

Expr StyleImpl::merge_ruleset_members(StyleOptionName key, Expr newer, Expr older) { // ignores all rules in older, whose keys do not appear in newer
  if(newer == PMATH_SYMBOL_INHERITED)
    return older;
    
  if(older == PMATH_SYMBOL_INHERITED)
    return newer;
    
  SharedPtr<StyleEnumConverter> key_converter = StyleInformation::get_enum_converter(key);
  assert(key_converter.is_valid());
  
  if(newer[0] == PMATH_SYMBOL_LIST) {
    for(size_t i = newer.expr_length(); i > 0; --i) {
      Expr rule = newer[i];
      
      if(!rule.is_rule())
        continue;
        
      Expr lhs = rule[1];
      Expr rhs = rule[2];
      
      Expr new_rhs = rhs;
      StyleOptionName sub_key = StyleOptionName{key_converter->to_int(lhs)};
      if(sub_key.is_valid()) {
        new_rhs = merge_style_values(sub_key, rhs, inherited_ruleset_member(older, lhs));
      }
      else {
        pmath_debug_print_object("[unknown sub-option ", lhs.get(), "]\n");
        if(rhs == PMATH_SYMBOL_INHERITED)
          new_rhs = inherited_ruleset_member(older, lhs);
      }
      
      if(new_rhs != rhs) {
        rule[2] = std::move(new_rhs);
        newer.set(i, std::move(rule));
      }
    }
  }
  
  return newer;
}

Expr StyleImpl::merge_list_members(Expr newer, Expr older) {
  if(newer == PMATH_SYMBOL_INHERITED)
    return older;
    
  if(older == PMATH_SYMBOL_INHERITED)
    return newer;
    
  if(newer[0] == PMATH_SYMBOL_LIST && older[0] == PMATH_SYMBOL_LIST && newer.expr_length() == older.expr_length()) {
    for(size_t i = newer.expr_length(); i > 0; --i) {
      Expr item = newer[i];
      if(item == PMATH_SYMBOL_INHERITED)
        newer.set(i, older[i]);
      else if(item[0] == PMATH_SYMBOL_LIST)
        newer.set(i, merge_list_members(item, older[i]));
    }
    return newer;
  }
  
  pmath_debug_print_object("[Warning: cannot merge non-equally-sized lists ", newer.get(), "");
  pmath_debug_print_object(" and ", older.get(), "]\n");
  return newer;
}

Expr StyleImpl::merge_margin_values(Expr newer, Expr older) {
  if(newer[0] == PMATH_SYMBOL_LIST) {
    if(newer.expr_length() == 2)
      return merge_list_members(newer, List(inherited_margin_leftright(older), inherited_margin_topbottom(older)));
      
    return merge_list_members(std::move(newer), std::move(older));
  }
  return newer;
}

Expr StyleImpl::inherited_list_member(Expr inherited, int index) {
  assert(index >= 1);
  
  if(inherited[0] == PMATH_SYMBOL_LIST) {
    if((size_t)index <= inherited.expr_length())
      return inherited[index];
  }
  
  if(inherited != PMATH_SYMBOL_INHERITED) {
    pmath_debug_print("[Warning: partial redefition of item %d", index);
    pmath_debug_print_object(" of ", inherited.get(), "]\n");
  }
  return Symbol(PMATH_SYMBOL_INHERITED);
}

Expr StyleImpl::inherited_ruleset_member(Expr inherited, Expr key) {
  if(inherited[0] == PMATH_SYMBOL_LIST) {
    size_t len = inherited.expr_length();
    for(size_t i = 1; i <= len; ++i) {
      Expr item = inherited[i];
      if(item.is_rule() && item[1] == key)
        return item[2];
    }
  }
  else if(inherited != PMATH_SYMBOL_INHERITED) {
    pmath_debug_print_object("[Warning: partial redefition of rule ", key.get(), "");
    pmath_debug_print_object(" of ", inherited.get(), "]\n");
  }
  
  return Symbol(PMATH_SYMBOL_INHERITED);
}

Expr StyleImpl::inherited_margin_leftright(Expr inherited) {
  if(inherited[0] == PMATH_SYMBOL_LIST) {
    if(inherited.expr_length() == 2)
      return inherited_list_member(std::move(inherited), 1);
  }
  
  return List(inherited_list_member(inherited, 1), inherited_list_member(inherited, 2));
}

Expr StyleImpl::inherited_margin_left(Expr inherited) {
  if(inherited[0] == PMATH_SYMBOL_LIST) {
    if(inherited.expr_length() == 2)
      return inherited_list_member(inherited_list_member(std::move(inherited), 1), 1);
  }
  
  return inherited_list_member(std::move(inherited), 1);
}

Expr StyleImpl::inherited_margin_right(Expr inherited) {
  if(inherited[0] == PMATH_SYMBOL_LIST) {
    if(inherited.expr_length() == 2)
      return inherited_list_member(inherited_list_member(std::move(inherited), 1), 2);
  }
  
  return inherited_list_member(std::move(inherited), 2);
}

Expr StyleImpl::inherited_margin_topbottom(Expr inherited) {
  if(inherited[0] == PMATH_SYMBOL_LIST) {
    if(inherited.expr_length() == 2)
      return inherited_list_member(std::move(inherited), 2);
  }
  
  return List(inherited_list_member(inherited, 3), inherited_list_member(inherited, 4));
}

Expr StyleImpl::inherited_margin_top(Expr inherited) {
  if(inherited[0] == PMATH_SYMBOL_LIST) {
    if(inherited.expr_length() == 2)
      return inherited_list_member(inherited_list_member(std::move(inherited), 2), 1);
  }
  
  return inherited_list_member(std::move(inherited), 3);
}

Expr StyleImpl::inherited_margin_bottom(Expr inherited) {
  if(inherited[0] == PMATH_SYMBOL_LIST) {
    if(inherited.expr_length() == 2)
      return inherited_list_member(inherited_list_member(std::move(inherited), 2), 2);
  }
  
  return inherited_list_member(std::move(inherited), 4);
}

void StyleImpl::emit_definition(StyleOptionName n) const {
  STYLE_ASSERT(n.is_literal());
  
  Expr e;
  if(raw_get_expr(n.to_dynamic(), &e)) {
    Gather::emit(Rule(StyleInformation::get_name(n), e));
    return;
  }
  
  e = Symbol(PMATH_SYMBOL_INHERITED);
  
  StyleType type = StyleInformation::get_type(n);
  switch(type) {
    case StyleTypeSize:
      {
        StyleOptionName Horizontal = StyleOptionName((int)n + 1);
        StyleOptionName Vertical = StyleOptionName((int)n + 2);
        
        Expr horz;
        bool have_horz = raw_get_expr(Horizontal.to_dynamic(), &horz);
        if(!have_horz)
          horz = Symbol(PMATH_SYMBOL_INHERITED);
          
        Expr vert;
        bool have_vert = raw_get_expr(Vertical.to_dynamic(), &vert);
        if(!have_vert)
          vert = Symbol(PMATH_SYMBOL_INHERITED);
          
        if(have_horz || have_vert)
          e = List(horz, vert);
      }
      break;
      
      //case StyleTypeMargin: ....
  }
  
  e = raw_get_pmath(n, std::move(e));
  if(e == PMATH_SYMBOL_INHERITED)
    return;
    
  if(StyleInformation::needs_ruledelayed(e))
    Gather::emit(RuleDelayed(StyleInformation::get_name(n), e));
  else
    Gather::emit(Rule(StyleInformation::get_name(n), e));
}

Expr StyleImpl::raw_get_pmath(StyleOptionName key, Expr inherited) const {
  enum StyleType type = StyleInformation::get_type(key);
  
  switch(type) {
    case StyleTypeNone:
      break;
      
    case StyleTypeBool:
      return raw_get_pmath_bool(key, std::move(inherited));
      
    case StyleTypeBoolAuto:
      return raw_get_pmath_bool_auto(key, std::move(inherited));
      
    case StyleTypeColor:
      return raw_get_pmath_color(key, std::move(inherited));
      
    case StyleTypeNumber:
      return raw_get_pmath_float(key, std::move(inherited));
      
    case StyleTypeMargin:
      return raw_get_pmath_margin(key, std::move(inherited));
      
    case StyleTypeSize:
      return raw_get_pmath_size(key, std::move(inherited));
      
    case StyleTypeString:
      return raw_get_pmath_string(key, std::move(inherited));
      
    case StyleTypeAny:
      return raw_get_pmath_object(key, std::move(inherited));
      
    case StyleTypeEnum:
      return raw_get_pmath_enum(key, std::move(inherited));
      
    case StyleTypeRuleSet:
      return raw_get_pmath_ruleset(key, std::move(inherited));
  }
  
  return inherited;
}

Expr StyleImpl::raw_get_pmath_bool_auto(StyleOptionName n, Expr inherited) const {
  STYLE_ASSERT(is_for_int(n));
  
  int i;
  if(raw_get_int(n, &i)) {
    switch(i) {
      case AutoBoolFalse:
        return Symbol(PMATH_SYMBOL_FALSE);
        
      case AutoBoolTrue:
        return Symbol(PMATH_SYMBOL_TRUE);
        
      case AutoBoolAutomatic:
      default:
        return Symbol(PMATH_SYMBOL_AUTOMATIC);
    }
  }
  
  return inherited;
}

Expr StyleImpl::raw_get_pmath_bool(StyleOptionName n, Expr inherited) const {
  STYLE_ASSERT(is_for_int(n));
  
  int i;
  if(raw_get_int(n, &i)) {
    if(i)
      return Symbol(PMATH_SYMBOL_TRUE);
      
    return Symbol(PMATH_SYMBOL_FALSE);
  }
  
  return inherited;
}

Expr StyleImpl::raw_get_pmath_color(StyleOptionName n, Expr inherited) const {
  STYLE_ASSERT(is_for_color(n));
  
  Color c;
  if(raw_get_color(n, &c))
    return c.to_pmath();
    
  return inherited;
}

Expr StyleImpl::raw_get_pmath_float(StyleOptionName n, Expr inherited) const {
  STYLE_ASSERT(is_for_float(n));
  
  float f;
  
  if(raw_get_float(n, &f))
    return Number(f);
    
  return inherited;
}

Expr StyleImpl::raw_get_pmath_margin(StyleOptionName n, Expr inherited) const { // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
  STYLE_ASSERT(is_for_float(n));
  
  StyleOptionName Left   = n;
  StyleOptionName Right  = StyleOptionName((int)n + 1);
  StyleOptionName Top    = StyleOptionName((int)n + 2);
  StyleOptionName Bottom = StyleOptionName((int)n + 3);
  
  float left, right, top, bottom;
  bool have_left, have_right, have_top, have_bottom;
  
  have_left   = raw_get_float(Left,   &left);
  have_right  = raw_get_float(Right,  &right);
  have_top    = raw_get_float(Top,    &top);
  have_bottom = raw_get_float(Bottom, &bottom);
  
  if(have_left || have_right || have_top || have_bottom) {
    Expr l, r, t, b;
    
    if(have_left)
      l = Number(left);
    else
      l = inherited_margin_left(inherited);
      
    if(have_right)
      r = Number(right);
    else
      r = inherited_margin_right(inherited);
      
    if(have_top)
      t = Number(top);
    else
      t = inherited_margin_top(inherited);
      
    if(have_bottom)
      b = Number(bottom);
    else
      b = inherited_margin_bottom(std::move(inherited));
      
    return List(l, r, t, b);
  }
  
  return inherited;
}

Expr StyleImpl::raw_get_pmath_size(StyleOptionName n, Expr inherited) const { // n + {0,1,2} ~= {Common, Horizontal, Vertical}
  STYLE_ASSERT(is_for_float(n));
  
  bool have_horz, have_vert;
  Expr horz, vert;
  
  StyleOptionName Horizontal = StyleOptionName((int)n + 1);
  StyleOptionName Vertical   = StyleOptionName((int)n + 2);
  
//  have_horz = get_dynamic(Horizontal, &horz);
//  if(!have_horz) {
  float h;
  have_horz = raw_get_float(Horizontal, &h);
  
  if(have_horz) {
    if(h > 0)
      horz = Number(h);
    else
      horz = Symbol(PMATH_SYMBOL_AUTOMATIC);
  }
  else
    horz = inherited_list_member(inherited, 1);
//  }

//  have_vert = get_dynamic(Vertical, &vert);
//  if(!have_vert) {
  float v;
  have_vert = raw_get_float(Vertical, &v);
  
  if(have_vert) {
    if(v > 0)
      vert = Number(v);
    else
      vert = Symbol(PMATH_SYMBOL_AUTOMATIC);
  }
  else
    vert = inherited_list_member(inherited, 2);
//  }

  if(have_horz || have_vert)
    return List(horz, vert);
    
  return inherited;
}

Expr StyleImpl::raw_get_pmath_string(StyleOptionName n, Expr inherited) const {
  STYLE_ASSERT(is_for_string(n));
  
  String s;
  if(raw_get_string(n, &s))
    return s;
    
  return inherited;
}

Expr StyleImpl::raw_get_pmath_object(StyleOptionName n, Expr inherited) const {
  STYLE_ASSERT(is_for_expr(n));
  
  Expr e;
  if(raw_get_expr(n, &e))
    return e;
    
  return inherited;
}

Expr StyleImpl::raw_get_pmath_enum(StyleOptionName n, Expr inherited) const {
  STYLE_ASSERT(is_for_int(n));
  
  int i;
  if(raw_get_int(n, &i)) {
    SharedPtr<StyleEnumConverter> enum_converter = StyleInformation::get_enum_converter(n);
    
    assert(enum_converter.is_valid());
    
    return enum_converter->to_expr(i);
  }
  
  return inherited;
}

Expr StyleImpl::raw_get_pmath_ruleset(StyleOptionName n, Expr inherited) const {
  SharedPtr<StyleEnumConverter> key_converter = StyleInformation::get_enum_converter(n);
  
  assert(key_converter.is_valid());
  
  const Hashtable<Expr, int> &table = key_converter->expr_to_int();
  
  bool all_inherited = true;
  Gather g;
  
  for(auto &entry : table.entries()) {
    Expr inherited_value = inherited_ruleset_member(inherited, entry.key);
    Expr value = raw_get_pmath(StyleOptionName{entry.value}, inherited_value);
    
    if(value != PMATH_SYMBOL_INHERITED) {
      Gather::emit(Rule(entry.key, value));
      
      if(value != inherited_value)
        all_inherited = false;
    }
  }
  
  Expr e = g.end();
  if(all_inherited)
    return inherited;
    
  e.sort();
  return e;
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
  SET_EXPLICIT_BASE_DEBUG_TAG(Observable, typeid(*this).name());
  SET_EXPLICIT_BASE_DEBUG_TAG(Shareable,  typeid(*this).name());
  
  StyleInformation::add_style();
}

Style::Style(Expr options)
  : Shareable()
{
  SET_EXPLICIT_BASE_DEBUG_TAG(Observable, typeid(*this).name());
  SET_EXPLICIT_BASE_DEBUG_TAG(Shareable,  typeid(*this).name());
  
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

void Style::reset(SharedPtr<Style> &style, String base_style_name) {
  if(!style) {
    style = new Style();
    style->set(BaseStyleName, std::move(base_style_name));
    return;
  }
  
  int old_has_pending_dynamic = 0;
  style->get(InternalHasPendingDynamic, &old_has_pending_dynamic);
  
  String old_base_style_name;
  style->get(BaseStyleName, &old_base_style_name);
  
  style->clear();
  style->set(BaseStyleName, base_style_name);
  if(!base_style_name.is_null())
    style->set(InternalHasNewBaseStyle, true);
  if(!old_has_pending_dynamic && old_base_style_name == base_style_name)
    style->remove(InternalHasPendingDynamic);
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
  notify_all();
}

bool Style::contains_inherited(Expr expr) {
  if(expr == PMATH_SYMBOL_INHERITED)
    return true;
    
  if(expr.is_expr() && !expr.is_packed_array()) {
    size_t len = expr.expr_length();
    for(size_t i = 0; i <= len; ++i) {
      if(contains_inherited(expr[i]))
        return true;
    }
    return false;
  }
  
  return false;
}

Expr Style::merge_style_values(StyleOptionName n, Expr newer, Expr older) {
  return StyleImpl::merge_style_values(n, std::move(newer), std::move(older));
}

bool Style::get(ColorStyleOptionName n, Color *value) const {
  register_observer();
  if(StyleImpl::of(*this).raw_get_color(n, value)) 
    return true;
  if(StyleImpl::of(*this).raw_get_color(StyleOptionName(n).to_volatile(), value))
    return true;
  return false;
}

bool Style::get(IntStyleOptionName n, int *value) const {
  register_observer();
  if(StyleImpl::of(*this).raw_get_int(n, value))
    return true;
  if(StyleImpl::of(*this).raw_get_int(StyleOptionName(n).to_volatile(), value))
    return true;
  return false;
}

bool Style::get(FloatStyleOptionName n, float *value) const {
  register_observer();
  if(StyleImpl::of(*this).raw_get_float(n, value))
    return true;
  if(StyleImpl::of(*this).raw_get_float(StyleOptionName(n).to_volatile(), value))
    return true;
  return false;
}

bool Style::get(StringStyleOptionName n, String *value) const {
  register_observer();
  if(StyleImpl::of(*this).raw_get_string(n, value))
    return true;
  if(StyleImpl::of(*this).raw_get_string(StyleOptionName(n).to_volatile(), value))
    return true;
  return false;
}

bool Style::get(ObjectStyleOptionName n, Expr *value) const {
  register_observer();
  if(StyleImpl::of(*this).raw_get_expr(n, value))
    return true;
  if(StyleImpl::of(*this).raw_get_expr(StyleOptionName(n).to_volatile(), value))
    return true;
  return false;
}

void Style::set(ColorStyleOptionName n, Color value) {
  StyleOptionName key{n};
  STYLE_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_set_color(key, value) || any_change;
  any_change = StyleImpl::of(*this).remove_dynamic(key)       || any_change;
  
  if(any_change)
    notify_all();
}

void Style::set(IntStyleOptionName n, int value) {
  StyleOptionName key{n};
  STYLE_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_set_int(key, value) || any_change;
  any_change = StyleImpl::of(*this).remove_dynamic(key)     || any_change;
  
  if(any_change)
    notify_all();
}

void Style::set(FloatStyleOptionName n, float value) {
  StyleOptionName key{n};
  STYLE_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_set_float(key, value) || any_change;
  any_change = StyleImpl::of(*this).remove_dynamic(key)       || any_change;
  
  if(any_change)
    notify_all();
}

void Style::set(StringStyleOptionName n, String value) {
  StyleOptionName key{n};
  STYLE_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_set_string(key, value) || any_change;
  any_change = StyleImpl::of(*this).remove_dynamic(key)        || any_change;
  
  if(any_change)
    notify_all();
  
  if(key == BaseStyleName)
    StyleImpl::of(*this).raw_set_int(InternalHasPendingDynamic, true);
}

void Style::set(ObjectStyleOptionName n, Expr value) {
  StyleOptionName key{n};
  STYLE_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_set_expr(n, value) || any_change;
  any_change = StyleImpl::of(*this).remove_dynamic(key)    || any_change;
  
  if(any_change)
    notify_all();
}

void Style::remove(ColorStyleOptionName n) {
  StyleOptionName key{n};
  STYLE_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_remove_color(key)               || any_change;
  any_change = StyleImpl::of(*this).raw_remove_color(key.to_volatile()) || any_change;
  
  if(any_change)
    notify_all();
}

void Style::remove(IntStyleOptionName n) {
  StyleOptionName key{n};
  STYLE_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_remove_int(key)               || any_change;
  any_change = StyleImpl::of(*this).raw_remove_int(key.to_volatile()) || any_change;
  
  if(any_change)
    notify_all();
}

void Style::remove(FloatStyleOptionName n) {
  StyleOptionName key{n};
  STYLE_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_remove_float(key)               || any_change;
  any_change = StyleImpl::of(*this).raw_remove_float(key.to_volatile()) || any_change;
  
  if(any_change)
    notify_all();
}

void Style::remove(StringStyleOptionName n) {
  StyleOptionName key{n};
  STYLE_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_remove_string(key)               || any_change;
  any_change = StyleImpl::of(*this).raw_remove_string(key.to_volatile()) || any_change;
  
  if(any_change)
    notify_all();
}

void Style::remove(ObjectStyleOptionName n) {
  StyleOptionName key{n};
  STYLE_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_remove_expr(key)               || any_change;
  any_change = StyleImpl::of(*this).raw_remove_expr(key.to_volatile()) || any_change;
  
  if(any_change)
    notify_all();
}


bool Style::modifies_size(StyleOptionName style_name) {
  switch((int)style_name) {
    case Background:
    case FontColor:
    case SectionFrameColor:
    case AutoDelete:
    case ContinuousAction:
    case Editable:
    case Enabled:
    case Evaluatable:
    case InternalHasModifiedWindowOption:
    case InternalHasPendingDynamic:
    case InternalHasNewBaseStyle:
    case InternalUsesCurrentValueOfMouseOver:
    case Placeholder:
    case ReturnCreatesNewSection:
    case Saveable:
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

Expr Style::get_current_style_value(FrontEndObject *obj, Expr item) {
  return StyleInformation::get_current_style_value(obj, std::move(item));
}

bool Style::put_current_style_value(FrontEndObject *obj, Expr item, Expr rhs) {
  return StyleInformation::put_current_style_value(obj, std::move(item), std::move(rhs));
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

bool Style::set_pmath(StyleOptionName n, Expr obj) {
  if(StyleImpl::of(*this).set_pmath(n, obj)) {
    notify_all();
    return true;
  }
  return false;
}

Expr Style::get_pmath(StyleOptionName key) const {
  // STYLE_ASSERT(key.is_literal())
  
  register_observer();
  Expr result = Symbol(PMATH_SYMBOL_INHERITED);
  result = StyleImpl::of(*this).raw_get_pmath(key.to_volatile(), result);
  result = StyleImpl::of(*this).raw_get_pmath(key, result);
  return result;
}

void Style::emit_pmath(StyleOptionName n) const {
  register_observer();
  return StyleImpl::of(*this).emit_definition(n);
}

void Style::emit_to_pmath(bool with_inherited) const {
  auto impl = StyleImpl::of(*this);
  
  register_observer();
  impl.emit_definition(Antialiasing);
  impl.emit_definition(Appearance);
  impl.emit_definition(AspectRatio);
  impl.emit_definition(AutoDelete);
  impl.emit_definition(AutoNumberFormating);
  impl.emit_definition(AutoSpacing);
  impl.emit_definition(Axes);
  impl.emit_definition(AxesOrigin);
  impl.emit_definition(Background);
  impl.emit_definition(BaselinePosition);
  
  if(with_inherited)
    impl.emit_definition(BaseStyleName);
    
  impl.emit_definition(BorderRadius);
  impl.emit_definition(BoxRotation);
  impl.emit_definition(BoxTransformation);
  impl.emit_definition(ButtonBoxOptions);
  impl.emit_definition(ButtonData);
  impl.emit_definition(ButtonFrame);
  impl.emit_definition(ButtonFunction);
  impl.emit_definition(ButtonSource);
  impl.emit_definition(ContinuousAction);
  impl.emit_definition(DefaultDuplicateSectionStyle);
  impl.emit_definition(DefaultNewSectionStyle);
  impl.emit_definition(DefaultReturnCreatedSectionStyle);
  impl.emit_definition(DisplayFunction);
  impl.emit_definition(DockedSections);
  impl.emit_definition(Editable);
  impl.emit_definition(Enabled);
  impl.emit_definition(Evaluatable);
  impl.emit_definition(FillBoxOptions);
  impl.emit_definition(FillBoxWeight);
  impl.emit_definition(FontColor);
  impl.emit_definition(FontFamilies);
  impl.emit_definition(FontFeatures);
  impl.emit_definition(FontSize);
  impl.emit_definition(FontSlant);
  impl.emit_definition(FontWeight);
  impl.emit_definition(Frame);
  impl.emit_definition(FrameTicks);
  impl.emit_definition(GeneratedSectionStyles);
  impl.emit_definition(GridBoxColumnSpacing);
  impl.emit_definition(GridBoxRowSpacing);
  impl.emit_definition(ImageSizeCommon);
  impl.emit_definition(InputFieldBoxOptions);
  impl.emit_definition(InterpretationFunction);
  impl.emit_definition(LanguageCategory);
  impl.emit_definition(LineBreakWithin);
  impl.emit_definition(Magnification);
  impl.emit_definition(MathFontFamily);
  impl.emit_definition(Method);
  impl.emit_definition(PanelBoxOptions);
  impl.emit_definition(Placeholder);
  impl.emit_definition(PlotRange);
  impl.emit_definition(ReturnCreatesNewSection);
  impl.emit_definition(Saveable);
  impl.emit_definition(ScriptSizeMultipliers);
  impl.emit_definition(SectionDingbat);
  impl.emit_definition(SectionEditDuplicate);
  impl.emit_definition(SectionEditDuplicateMakesCopy);
  impl.emit_definition(SectionEvaluationFunction);
  impl.emit_definition(SectionFrameLeft);
  impl.emit_definition(SectionFrameColor);
  impl.emit_definition(SectionFrameMarginLeft);
  impl.emit_definition(SectionFrameLabelMarginLeft);
  impl.emit_definition(SectionGenerated);
  impl.emit_definition(SectionGroupPrecedence);
  impl.emit_definition(SectionMarginLeft);
  impl.emit_definition(SectionLabel);
  impl.emit_definition(SectionLabelAutoDelete);
  impl.emit_definition(Selectable);
  impl.emit_definition(ShowAutoStyles);
  impl.emit_definition(ShowSectionBracket);
  impl.emit_definition(ShowStringCharacters);
  impl.emit_definition(StripOnInput);
  impl.emit_definition(StyleDefinitions);
  impl.emit_definition(SurdForm);
  impl.emit_definition(SynchronousUpdating);
  impl.emit_definition(SyntaxForm);
  impl.emit_definition(TemplateBoxOptions);
  impl.emit_definition(TextShadow);
  impl.emit_definition(Ticks);
  impl.emit_definition(Tooltip);
  impl.emit_definition(TrackedSymbols);
  impl.emit_definition(Visible);
  impl.emit_definition(WholeSectionGroupOpener);
  impl.emit_definition(WindowFrame);
  impl.emit_definition(WindowTitle);
  
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
      void reload(Expr expr) {
        self.styles.clear();
        self.used_stylesheets.clear();
        self.users.clear();
        self._loaded_definition = expr;
        add(expr);
      }
      
      void add(Expr expr) {
        if(self._name.is_valid()) {
          if(currently_loading.search(self._name)) {
            // TODO: warn about recursive dependency
            return;
          }
          currently_loading.add(self._name);
        }
        
        internal_add(expr);
        
        if(self._name.is_valid())
          currently_loading.remove(self._name);
      }
      
      bool update_dynamic(SharedPtr<Style> s, Box *parent);
      
    private:
      static Hashset<Expr> currently_loading;
      
      void internal_add(Expr expr) {
        // TODO: detect stack overflow/infinite recursion
        
        while(expr.is_expr()) {
          if(expr[0] == PMATH_SYMBOL_DOCUMENT) {
            expr = expr[1];
            continue;
          }
          
          if(expr[0] == richmath_System_SectionGroup) {
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
          
          if(expr[0] == richmath_System_Section) {
            add_section(expr);
            return;
          }
          
          break;
        }
      }
      
      void add_section(Expr expr) {
        Expr name = expr[1];
        if(name[0] == richmath_System_StyleData) {
          Expr data = name[1];
          if(data.is_string()) {
            Expr options(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
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
          
          if(expr.expr_length() == 1 && data.is_rule() && data[1] == richmath_System_StyleDefinitions) {
            SharedPtr<Stylesheet> stylesheet = Stylesheet::try_load(data[2]);
            if(stylesheet) {
              self.used_stylesheets.add(stylesheet);
              stylesheet->users.add(self.id());
              
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
  
  Hashset<Expr> StylesheetImpl::currently_loading;
}

bool StylesheetImpl::update_dynamic(SharedPtr<Style> s, Box *parent) {
  if(!s || !parent)
    return false;
    
  StyleImpl s_impl = StyleImpl::of(*s.ptr());
  
  int i;
  if(!s_impl.raw_get_int(InternalHasPendingDynamic, &i) || !i) {
    bool has_parent_pending_dynamic = false;
    
    if(s_impl.raw_get_int(InternalHasNewBaseStyle, &i) && i) {
      s_impl.raw_set_int(InternalHasNewBaseStyle, false);
      
      SharedPtr<Style> tmp = self.find_parent_style(s);
      for(int count = 20; count && tmp; --count) {
        if(StyleImpl::of(*tmp.ptr()).raw_get_int(InternalHasPendingDynamic, &i) && i) {
          has_parent_pending_dynamic = true;
          break;
        }
        
        tmp = self.find_parent_style(tmp);
      }
    }
    
    if(!has_parent_pending_dynamic)
      return false;
  }
  s_impl.raw_set_int(InternalHasNewBaseStyle, false);
  s_impl.raw_set_int(InternalHasPendingDynamic, false);
  
  s_impl.remove_all_volatile();
  
  Hashtable<StyleOptionName, Expr> dynamic_styles;
  
  SharedPtr<Style> tmp = s;
  for(int count = 20; count && tmp; --count) {
    StyleImpl::of(*tmp.ptr()).collect_unused_dynamic(dynamic_styles);
    
    tmp = self.find_parent_style(tmp);
  }
  
  if(dynamic_styles.size() == 0)
    return false;
    
  bool resize = false;
  for(const auto &e : dynamic_styles.entries()) {
    if(Style::modifies_size(e.key.to_literal())) {
      resize = true;
      break;
    }
  }
  
  for(auto &e : dynamic_styles.entries()) {
    Dynamic dyn(parent, e.value);
    e.value = dyn.get_value_now();
  }
  
  for(const auto &e : dynamic_styles.entries()) {
    if(e.value != PMATH_SYMBOL_ABORTED && !Dynamic::is_dynamic(e.value)) {
      StyleOptionName key = e.key.to_volatile(); // = e.key.to_literal().to_volatile()
      s_impl.set_pmath(key, e.value);
    }
  }
  
  if(resize)
    parent->invalidate();
  else
    parent->request_repaint_all();
    
  return true;
}

Stylesheet::Stylesheet() : Shareable() {
}

Stylesheet::~Stylesheet() {
  users.clear();
  for(auto &e : used_stylesheets.entries())
    e.key->users.remove(id());
  
  used_stylesheets.clear();
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
                        Parse(
                          "Get(`1`, Head->HoldComplete)", 
                          Application::stylesheet_path_base + expr),
                        Application::button_timeout);
                        
    if(held_boxes.expr_length() == 1 && held_boxes[0] == PMATH_SYMBOL_HOLDCOMPLETE) {
      stylesheet = new Stylesheet();
      stylesheet->base = Stylesheet::Default->base;
      stylesheet->register_as(expr);
      stylesheet->reload(held_boxes[1]);
      return stylesheet;
    }
    
    return nullptr;
  }
  
  if(expr[0] == PMATH_SYMBOL_DOCUMENT) {
    SharedPtr<Stylesheet> stylesheet = new Stylesheet();
    stylesheet->base = Stylesheet::Default->base;
    stylesheet->reload(expr);
    return stylesheet;
  }
  
  return nullptr;
}

Expr Stylesheet::name_from_path(String filename) {
  filename = String(pmath_to_absolute_file_name(filename.release()));
  
  if(filename.starts_with(Application::stylesheet_path_base)) 
    return filename.part(Application::stylesheet_path_base.length());
  
  return String();
}

void Stylesheet::add(Expr expr) {
  StylesheetImpl(*this).add(expr);
}

void Stylesheet::reload() {
  reload(_loaded_definition);
}

void Stylesheet::reload(Expr expr) {
  StylesheetImpl(*this).reload(expr);
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

bool Stylesheet::get(SharedPtr<Style> s, ColorStyleOptionName n, Color *value) {
  return Stylesheet_get(this, s, n, value);
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
  Expr result = Symbol(PMATH_SYMBOL_INHERITED);
  
  for(int count = 20; count && s && Style::contains_inherited(result); --count) {
    result = Style::merge_style_values(n, std::move(result), s->get_pmath(n));
    
    s = find_parent_style(s);
  }
  
  return result;
}

bool Stylesheet::update_dynamic(SharedPtr<Style> s, Box *parent) {
  return StylesheetImpl(*this).update_dynamic(s, parent);
}

Color Stylesheet::get_with_base(SharedPtr<Style> s, ColorStyleOptionName n) {
  Color value = Color::None;
  
  if(!get(s, n, &value))
    base->get(n, &value);
    
  return value;
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

//{ class StyleInformation ...

void StyleInformation::add_style() {
  if(_num_styles++ == 0) {
    _name_to_key.default_value = StyleOptionName{ -1};
    _key_to_type.default_value = StyleTypeNone;
    
    add_ruleset_head(DockedSections,       Symbol( richmath_System_DockedSections));
    add_ruleset_head(ButtonBoxOptions,     Symbol( richmath_System_ButtonBoxOptions));
    add_ruleset_head(FillBoxOptions,       Symbol( richmath_System_FillBoxOptions));
    add_ruleset_head(InputFieldBoxOptions, Symbol( richmath_System_InputFieldBoxOptions));
    add_ruleset_head(PanelBoxOptions,      Symbol( richmath_System_PanelBoxOptions));
    add_ruleset_head(TemplateBoxOptions,   Symbol( richmath_System_TemplateBoxOptions));
    
    {
      SharedPtr<StyleEnumConverter> converter{new ButtonFrameStyleEnumConverter};
      add_enum(
        ButtonFrame, 
        Symbol( richmath_System_ButtonFrame), 
        converter);
      add_enum(
        ButtonBoxDefaultButtonFrame, 
        Rule(Symbol( richmath_System_ButtonBoxOptions), Symbol( richmath_System_ButtonFrame)),
        converter);
    }
    
    {
      SharedPtr<StyleEnumConverter> converter{new ButtonSourceStyleEnumConverter};
      add_enum(
        ButtonSource, 
        Symbol( richmath_System_ButtonSource), 
        converter);
      add_enum(
        ButtonBoxDefaultButtonSource, 
        Rule(Symbol( richmath_System_ButtonBoxOptions), Symbol( richmath_System_ButtonSource)),
        converter);
    }
    
    add_enum(FontSlant,   Symbol( richmath_System_FontSlant),   new FontSlantStyleEnumConverter);
    add_enum(FontWeight,  Symbol( richmath_System_FontWeight),  new FontWeightStyleEnumConverter);
    add_enum(WindowFrame, Symbol( richmath_System_WindowFrame), new WindowFrameStyleEnumConverter);
    
    add(StyleTypeColor,           Background,                       Symbol( richmath_System_Background));
    add(StyleTypeColor,           FontColor,                        Symbol( richmath_System_FontColor));
    add(StyleTypeColor,           SectionFrameColor,                Symbol( richmath_System_SectionFrameColor));
    add(StyleTypeBoolAuto,        Antialiasing,                     Symbol( richmath_System_Antialiasing));
    add(StyleTypeBool,            AutoDelete,                       Symbol( richmath_System_AutoDelete));
    add(StyleTypeBool,            AutoNumberFormating,              Symbol( richmath_System_AutoNumberFormating));
    add(StyleTypeBool,            AutoSpacing,                      Symbol( richmath_System_AutoSpacing));
    add(StyleTypeBool,            ContinuousAction,                 Symbol( richmath_System_ContinuousAction));
    add(StyleTypeBool,            Editable,                         Symbol( richmath_System_Editable));
    add(StyleTypeBoolAuto,        Enabled,                          Symbol( richmath_System_Enabled));
    add(StyleTypeBool,            Evaluatable,                      Symbol( richmath_System_Evaluatable));
    add(StyleTypeBool,            LineBreakWithin,                  Symbol( richmath_System_LineBreakWithin));
    add(StyleTypeBool,            Placeholder,                      Symbol( richmath_System_Placeholder));
    add(StyleTypeBool,            ReturnCreatesNewSection,          Symbol( richmath_System_ReturnCreatesNewSection));
    add(StyleTypeBool,            Saveable,                         Symbol( richmath_System_Saveable));
    add(StyleTypeBool,            SectionEditDuplicate,             Symbol( richmath_System_SectionEditDuplicate));
    add(StyleTypeBool,            SectionEditDuplicateMakesCopy,    Symbol( richmath_System_SectionEditDuplicateMakesCopy));
    add(StyleTypeBool,            SectionGenerated,                 Symbol( richmath_System_SectionGenerated));
    add(StyleTypeBool,            SectionLabelAutoDelete,           Symbol( richmath_System_SectionLabelAutoDelete));
    add(StyleTypeBoolAuto,        Selectable,                       Symbol( richmath_System_Selectable));
    add(StyleTypeBool,            ShowAutoStyles,                   Symbol( richmath_System_ShowAutoStyles));
    add(StyleTypeBoolAuto,        ShowSectionBracket,               Symbol( richmath_System_ShowSectionBracket));
    add(StyleTypeBool,            ShowStringCharacters,             Symbol( richmath_System_ShowStringCharacters));
    add(StyleTypeBool,            StripOnInput,                     Symbol( richmath_System_StripOnInput));
    add(StyleTypeBool,            SurdForm,                         Symbol( richmath_System_SurdForm));
    add(StyleTypeBoolAuto,        SynchronousUpdating,              Symbol( richmath_System_SynchronousUpdating));
    add(StyleTypeBool,            Visible,                          Symbol( richmath_System_Visible));
    add(StyleTypeBool,            WholeSectionGroupOpener,          Symbol( richmath_System_WholeSectionGroupOpener));
    
    add(StyleTypeBoolAuto,        ButtonBoxDefaultEnabled,          Rule(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_Enabled)));
    
    add(StyleTypeBool,            InputFieldBoxDefaultContinuousAction, Rule(Symbol(richmath_System_InputFieldBoxOptions), Symbol( richmath_System_ContinuousAction)));
    add(StyleTypeBool,            InputFieldBoxDefaultEnabled,          Rule(Symbol(richmath_System_InputFieldBoxOptions), Symbol( richmath_System_Enabled)));
    
    add(StyleTypeBool,            PanelBoxDefaultEnabled,           Rule(Symbol(richmath_System_PanelBoxOptions), Symbol( richmath_System_Enabled)));
    
    add(StyleTypeNumber,          AspectRatio,                      Symbol( richmath_System_AspectRatio));
    add(StyleTypeNumber,          FillBoxWeight,                    Symbol( richmath_System_FillBoxWeight));
    add(StyleTypeNumber,          FontSize,                         Symbol( richmath_System_FontSize));
    add(StyleTypeNumber,          GridBoxColumnSpacing,             Symbol( richmath_System_GridBoxColumnSpacing));
    add(StyleTypeNumber,          GridBoxRowSpacing,                Symbol( richmath_System_GridBoxRowSpacing));
    add(StyleTypeNumber,          Magnification,                    Symbol( richmath_System_Magnification));
    
    add(StyleTypeSize,            ImageSizeCommon,                  Symbol( richmath_System_ImageSize));
    // ImageSizeHorizontal
    // ImageSizeVertical
    add(StyleTypeMargin,          SectionMarginLeft,                Symbol( richmath_System_SectionMargins));
    // SectionMarginRight
    // SectionMarginTop
    // SectionMarginBottom
    add(StyleTypeMargin,          SectionFrameLeft,                 Symbol( richmath_System_SectionFrame));
    // SectionFrameRight
    // SectionFrameTop
    // SectionFrameBottom
    add(StyleTypeMargin,          SectionFrameMarginLeft,           Symbol( richmath_System_SectionFrameMargins));
    // SectionFrameMarginRight
    // SectionFrameMarginTop
    // SectionFrameMarginBottom
    add(StyleTypeMargin,          SectionFrameLabelMarginLeft,      Symbol( richmath_System_SectionFrameLabelMargins));
    // SectionFrameLabelMarginRight
    // SectionFrameLabelMarginTop
    // SectionFrameLabelMarginBottom
    add(StyleTypeNumber,          SectionGroupPrecedence,           Symbol( richmath_System_SectionGroupPrecedence));
    
    add(StyleTypeNumber,          FillBoxDefaultFillBoxWeight,      Rule(Symbol(richmath_System_FillBoxOptions), Symbol( richmath_System_FillBoxWeight)));
    add(StyleTypeBool,            FillBoxDefaultStripOnInput,       Rule(Symbol(richmath_System_FillBoxOptions), Symbol( richmath_System_StripOnInput)));
    
    add(StyleTypeString,          BaseStyleName,                    Symbol( richmath_System_BaseStyle));
    add(StyleTypeString,          Method,                           Symbol( richmath_System_Method));
    add(StyleTypeString,          LanguageCategory,                 Symbol( richmath_System_LanguageCategory));
    add(StyleTypeString,          SectionLabel,                     Symbol( richmath_System_SectionLabel));
    add(StyleTypeString,          WindowTitle,                      Symbol( richmath_System_WindowTitle));
    
    add(StyleTypeString,          ButtonBoxDefaultMethod,           Rule(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_Method)));
    
    add(StyleTypeAny,             Appearance,                       Symbol( richmath_System_Appearance));
    add(StyleTypeAny,             Axes,                             Symbol( richmath_System_Axes));
    add(StyleTypeAny,             Ticks,                            Symbol( richmath_System_Ticks));
    add(StyleTypeAny,             Frame,                            Symbol( richmath_System_Frame));
    add(StyleTypeAny,             FrameTicks,                       Symbol( richmath_System_FrameTicks));
    add(StyleTypeAny,             AxesOrigin,                       Symbol( richmath_System_AxesOrigin));
    add(StyleTypeAny,             BaselinePosition,                 Symbol( richmath_System_BaselinePosition));
    add(StyleTypeAny,             ButtonData,                       Symbol( richmath_System_ButtonData));
    add(StyleTypeAny,             ButtonFunction,                   Symbol( richmath_System_ButtonFunction));
    add(StyleTypeAny,             ScriptSizeMultipliers,            Symbol( richmath_System_ScriptSizeMultipliers));
    add(StyleTypeAny,             TextShadow,                       Symbol( richmath_System_TextShadow));
    add(StyleTypeAny,             FontFamilies,                     Symbol( richmath_System_FontFamily));
    add(StyleTypeAny,             FontFeatures,                     Symbol( richmath_System_FontFeatures));
    add(StyleTypeAny,             MathFontFamily,                   Symbol( richmath_System_MathFontFamily));
    add(StyleTypeAny,             TrackedSymbols,                   Symbol( PMATH_SYMBOL_TRACKEDSYMBOLS));
    add(StyleTypeAny,             BoxRotation,                      Symbol( richmath_System_BoxRotation));
    add(StyleTypeAny,             BoxTransformation,                Symbol( richmath_System_BoxTransformation));
    add(StyleTypeAny,             PlotRange,                        Symbol( richmath_System_PlotRange));
    add(StyleTypeAny,             BorderRadius,                     Symbol( richmath_System_BorderRadius));
    add(StyleTypeAny,             DefaultDuplicateSectionStyle,     Symbol( richmath_System_DefaultDuplicateSectionStyle));
    add(StyleTypeAny,             DefaultNewSectionStyle,           Symbol( richmath_System_DefaultNewSectionStyle));
    add(StyleTypeAny,             DefaultReturnCreatedSectionStyle, Symbol( richmath_System_DefaultReturnCreatedSectionStyle));
    add(StyleTypeAny,             DisplayFunction,                  Symbol( richmath_System_DisplayFunction));
    add(StyleTypeAny,             InterpretationFunction,           Symbol( richmath_System_InterpretationFunction));
    add(StyleTypeAny,             SyntaxForm,                       Symbol( richmath_System_SyntaxForm));
    add(StyleTypeAny,             StyleDefinitions,                 Symbol( richmath_System_StyleDefinitions));
    add(StyleTypeAny,             Tooltip,                          Symbol( richmath_System_Tooltip));
    add(StyleTypeAny,             GeneratedSectionStyles,           Symbol( richmath_System_GeneratedSectionStyles));
    add(StyleTypeAny,             SectionDingbat,                   Symbol( richmath_System_SectionDingbat));
    add(StyleTypeAny,             SectionEvaluationFunction,        Symbol( richmath_System_SectionEvaluationFunction));
    
    add(StyleTypeAny, DockedSectionsTop,         Rule(Symbol(richmath_System_DockedSections), String("Top")));
    add(StyleTypeAny, DockedSectionsTopGlass,    Rule(Symbol(richmath_System_DockedSections), String("TopGlass")));
    add(StyleTypeAny, DockedSectionsBottom,      Rule(Symbol(richmath_System_DockedSections), String("Bottom")));
    add(StyleTypeAny, DockedSectionsBottomGlass, Rule(Symbol(richmath_System_DockedSections), String("BottomGlass")));
    
    add(StyleTypeAny, ButtonBoxDefaultAppearance,       Rule(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_Appearance)));
    add(StyleTypeAny, ButtonBoxDefaultBaselinePosition, Rule(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_BaselinePosition)));
    add(StyleTypeAny, ButtonBoxDefaultButtonData,       Rule(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_ButtonData)));
    add(StyleTypeAny, ButtonBoxDefaultButtonFunction,   Rule(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_ButtonFunction)));
    
    add(StyleTypeAny, InputFieldBoxDefaultAppearance,       Rule(Symbol(richmath_System_InputFieldBoxOptions), Symbol(richmath_System_Appearance)));
    add(StyleTypeAny, InputFieldBoxDefaultBaselinePosition, Rule(Symbol(richmath_System_InputFieldBoxOptions), Symbol(richmath_System_BaselinePosition)));
    
    add(StyleTypeAny, PanelBoxDefaultAppearance,       Rule(Symbol(richmath_System_PanelBoxOptions), Symbol(richmath_System_Appearance)));
    add(StyleTypeAny, PanelBoxDefaultBaselinePosition, Rule(Symbol(richmath_System_PanelBoxOptions), Symbol(richmath_System_BaselinePosition)));
    
    add(StyleTypeAny, TemplateBoxDefaultDisplayFunction,        Rule(Symbol(richmath_System_TemplateBoxOptions), Symbol(richmath_System_DisplayFunction)));
    add(StyleTypeAny, TemplateBoxDefaultInterpretationFunction, Rule(Symbol(richmath_System_TemplateBoxOptions), Symbol(richmath_System_InterpretationFunction)));
    add(StyleTypeAny, TemplateBoxDefaultTooltip,                Rule(Symbol(richmath_System_TemplateBoxOptions), Symbol(richmath_System_Tooltip)));
  }
}

void StyleInformation::remove_style() {
  if(--_num_styles == 0) {
    _key_to_enum_converter.clear();
    _key_to_type.clear();
    _key_to_name.clear();
    _name_to_key.clear();
  }
}

bool StyleInformation::is_window_option(StyleOptionName key) {
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

bool StyleInformation::requires_child_resize(StyleOptionName key) {
  StyleOptionName literal_key = key.to_literal();
  return literal_key == Antialiasing ||
         literal_key == FontSlant ||
         literal_key == FontWeight ||
         literal_key == AutoSpacing ||
         literal_key == LineBreakWithin ||
         literal_key == ShowAutoStyles ||
         literal_key == ShowSectionBracket ||
         literal_key == ShowStringCharacters ||
         literal_key == FontSize ||
         literal_key == Magnification ||
         literal_key == ScriptSizeMultipliers ||
         literal_key == FontFamilies ||
         literal_key == FontFeatures ||
         literal_key == MathFontFamily ||
         literal_key == StyleDefinitions;
}

void StyleInformation::add(StyleType type, StyleOptionName key, const Expr &name) {
  assert(type != StyleTypeEnum);
  
  _key_to_type.set(key, type);
  if(type == StyleTypeSize) { // {horz, vert} = {key+1, key+2}
    StyleOptionName Horizontal = StyleOptionName((int)key + 1);
    StyleOptionName Vertical   = StyleOptionName((int)key + 2);
    _key_to_type.set(Horizontal, StyleTypeNumber);
    _key_to_type.set(Vertical,   StyleTypeNumber);
  }
//  else if(type == StyleTypeMargin) { // {left, right, top, bottom} = {key, key+1, key+2, key+3}
//    //StyleOptionName Left   = StyleOptionName((int)key + 1);
//    StyleOptionName Right  = StyleOptionName((int)key + 1);
//    StyleOptionName Top    = StyleOptionName((int)key + 2);
//    StyleOptionName Bottom = StyleOptionName((int)key + 3);
//    //_key_to_type.set(Left,   StyleTypeNumber);
//    _key_to_type.set(Right,  StyleTypeNumber);
//    _key_to_type.set(Top,    StyleTypeNumber);
//    _key_to_type.set(Bottom, StyleTypeNumber);
//  }

  _key_to_name.set(key, name);
  _name_to_key.set(name, key);
  
  Application::register_currentvalue_provider(name, get_current_style_value, put_current_style_value);
  
  add_to_ruleset(key, name);
}

void StyleInformation::add_enum(IntStyleOptionName key, const Expr &name, SharedPtr<StyleEnumConverter> enum_converter) {
  _key_to_enum_converter.set(key, enum_converter);
  _key_to_type.set(          key, StyleTypeEnum);
  _key_to_name.set(          key, name);
  _name_to_key.set(          name, key);
  
  Application::register_currentvalue_provider(name, get_current_style_value);
  
  add_to_ruleset(key, name);
}

void StyleInformation::add_to_ruleset(StyleOptionName key, const Expr &name) {
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

void StyleInformation::add_ruleset_head(StyleOptionName key, const Expr &symbol) {
  _key_to_enum_converter.set(key, new SubRuleConverter);
  _key_to_type.set(          key, StyleTypeRuleSet);
  _key_to_name.set(          key, symbol);
  _name_to_key.set(          symbol, key);
}

Expr StyleInformation::get_current_style_value(FrontEndObject *obj, Expr item) {
  Box *box = dynamic_cast<Box*>(obj);
  if(!box)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  StyleOptionName key = Style::get_key(item);
  if(key.is_valid()) 
    return box->get_pmath_style(key);
    
  return Symbol(PMATH_SYMBOL_FAILED);
}

bool StyleInformation::put_current_style_value(FrontEndObject *obj, Expr item, Expr rhs) {
  Box *box = dynamic_cast<Box*>(obj);
  if(!box)
    return false;
  
  Expr head = box->to_pmath_symbol();
  if(!head.is_symbol())
    return false;
  
  StyleOptionName key = Style::get_key(item);
  if(!key.is_valid())
    return false;
  
  Expr opts = Call(Symbol(PMATH_SYMBOL_OPTIONS), std::move(head));
  opts = Application::interrupt_wait_cached(std::move(opts));
  if(opts.lookup(std::move(item), Expr{PMATH_UNDEFINED}) == PMATH_UNDEFINED)
    return false;
  
  if(!box->style) {
    if(rhs == PMATH_SYMBOL_INHERITED)
      return true;
    
    box->style = new Style();
  }
  
  if(box->style->set_pmath(key, std::move(rhs)))
    box->invalidate_options();
  
  return true;
}

bool StyleInformation::needs_ruledelayed(Expr expr) {
  if(expr.is_symbol()) {
    if(!(pmath_symbol_get_attributes(expr.get()) & PMATH_SYMBOL_ATTRIBUTE_PROTECTED))
      return true;
    
    // TODO: white-list allowed symbols
    
    return false;
  }
  
  if(!expr.is_expr())
    return false;
    
  Expr head = expr[0];
  if(head == richmath_System_Dynamic || head == PMATH_SYMBOL_FUNCTION)
    return false;
    
  if( head == PMATH_SYMBOL_LIST ||
      head == PMATH_SYMBOL_RANGE ||
      head == PMATH_SYMBOL_NCACHE ||
      head == PMATH_SYMBOL_PUREARGUMENT ||
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

//} ... class StyleInformation

ButtonFrameStyleEnumConverter::ButtonFrameStyleEnumConverter() : StyleEnumConverter() {
  _int_to_expr.default_value = Symbol(PMATH_SYMBOL_AUTOMATIC);
  _expr_to_int.default_value = -1;//PushButton;
  
  add(NoContainerType,      Symbol(PMATH_SYMBOL_NONE));
  add(FramelessButton,      String("Frameless"));
  add(GenericButton,        String("Generic"));
  add(PushButton,           String("DialogBox"));
  add(DefaultPushButton,    String("Defaulted"));
  add(PaletteButton,        String("Palette"));
  add(AddressBandGoButton,  String("AddressBandGo"));
  add(ListViewItemSelected, String("ListViewItemSelected"));
  add(ListViewItem,         String("ListViewItem"));
  
  add(OpenerTriangleClosed, String("OpenerTriangleClosed"));
  add(OpenerTriangleOpened, String("OpenerTriangleOpened"));
  
  add(NavigationBack,       String("NavigationBack"));
  add(NavigationForward,    String("NavigationForward"));
  
  add(TabHeadAbuttingRight,     String("TabHeadAbuttingRight"));
  add(TabHeadAbuttingLeftRight, String("TabHeadAbuttingLeftRight"));
  add(TabHeadAbuttingLeft,      String("TabHeadAbuttingLeft"));
  add(TabHead,                  String("TabHead"));
}

ButtonSourceStyleEnumConverter::ButtonSourceStyleEnumConverter() : StyleEnumConverter() {
  _int_to_expr.default_value = Expr();
  _expr_to_int.default_value = -1;
  
  add(ButtonSourceAutomatic,      Symbol(PMATH_SYMBOL_AUTOMATIC));
  add(ButtonSourceButtonBox,      Symbol(richmath_System_ButtonBox));
  add(ButtonSourceButtonContents, Symbol(richmath_System_ButtonContents));
  add(ButtonSourceButtonData,     Symbol(richmath_System_ButtonData));
  add(ButtonSourceFrontEndObject, Symbol(richmath_System_FrontEndObject));
}

WindowFrameStyleEnumConverter::WindowFrameStyleEnumConverter() : StyleEnumConverter() {
  _int_to_expr.default_value = Expr();
  _expr_to_int.default_value = -1;
  
  add(WindowFrameNormal,  String("Normal"));
  add(WindowFramePalette, String("Palette"));
  add(WindowFrameDialog,  String("Dialog"));
}

FontSlantStyleEnumConverter::FontSlantStyleEnumConverter() : StyleEnumConverter() {
  _int_to_expr.default_value = Expr();
  _expr_to_int.default_value = -1;
  
  add(FontSlantPlain,  Symbol(PMATH_SYMBOL_PLAIN));
  add(FontSlantItalic, Symbol(PMATH_SYMBOL_ITALIC));
}

FontWeightStyleEnumConverter::FontWeightStyleEnumConverter() : StyleEnumConverter() {
  _int_to_expr.default_value = Expr();
  _expr_to_int.default_value = -1;
  
  add(FontWeightPlain, Symbol(PMATH_SYMBOL_PLAIN));
  add(FontWeightBold,  Symbol(PMATH_SYMBOL_BOLD));
}
