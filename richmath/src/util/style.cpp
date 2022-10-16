#include <util/style.h>

#include <boxes/box.h>

#include <eval/application.h>
#include <eval/current-value.h>
#include <eval/dynamic.h>

#include <gui/control-painter.h>

#include <util/filesystem.h>

#include <cmath>
#include <limits>


#ifdef _MSC_VER
#  define isfinite(x)  (_finite(x))
#endif

using namespace std;


extern pmath_symbol_t richmath_System_DollarAborted;
extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_AllowScriptLevelChange;
extern pmath_symbol_t richmath_System_Antialiasing;
extern pmath_symbol_t richmath_System_Appearance;
extern pmath_symbol_t richmath_System_AspectRatio;
extern pmath_symbol_t richmath_System_Assign;
extern pmath_symbol_t richmath_System_AutoDelete;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_AutoNumberFormating;
extern pmath_symbol_t richmath_System_AutoSpacing;
extern pmath_symbol_t richmath_System_Axes;
extern pmath_symbol_t richmath_System_AxesOrigin;
extern pmath_symbol_t richmath_System_Background;
extern pmath_symbol_t richmath_System_BaselinePosition;
extern pmath_symbol_t richmath_System_BaseStyle;
extern pmath_symbol_t richmath_System_Bold;
extern pmath_symbol_t richmath_System_BorderRadius;
extern pmath_symbol_t richmath_System_Bottom;
extern pmath_symbol_t richmath_System_BoxID;
extern pmath_symbol_t richmath_System_BoxRotation;
extern pmath_symbol_t richmath_System_BoxTransformation;
extern pmath_symbol_t richmath_System_ButtonBox;
extern pmath_symbol_t richmath_System_ButtonBoxOptions;
extern pmath_symbol_t richmath_System_ButtonContents;
extern pmath_symbol_t richmath_System_ButtonData;
extern pmath_symbol_t richmath_System_ButtonFrame;
extern pmath_symbol_t richmath_System_ButtonFunction;
extern pmath_symbol_t richmath_System_ButtonSource;
extern pmath_symbol_t richmath_System_CapForm;
extern pmath_symbol_t richmath_System_ContentPadding;
extern pmath_symbol_t richmath_System_ContextMenu;
extern pmath_symbol_t richmath_System_ContinuousAction;
extern pmath_symbol_t richmath_System_ControlPlacement;
extern pmath_symbol_t richmath_System_Dashing;
extern pmath_symbol_t richmath_System_DefaultDuplicateSectionStyle;
extern pmath_symbol_t richmath_System_DefaultNewSectionStyle;
extern pmath_symbol_t richmath_System_DefaultReturnCreatedSectionStyle;
extern pmath_symbol_t richmath_System_Deinitialization;
extern pmath_symbol_t richmath_System_DisplayFunction;
extern pmath_symbol_t richmath_System_DockedSections;
extern pmath_symbol_t richmath_System_Document;
extern pmath_symbol_t richmath_System_DownRules;
extern pmath_symbol_t richmath_System_DynamicBoxOptions;
extern pmath_symbol_t richmath_System_DynamicLocalBoxOptions;
extern pmath_symbol_t richmath_System_DynamicLocalValues;
extern pmath_symbol_t richmath_System_Editable;
extern pmath_symbol_t richmath_System_Enabled;
extern pmath_symbol_t richmath_System_Evaluatable;
extern pmath_symbol_t richmath_System_EvaluationContext;
extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_FillBoxOptions;
extern pmath_symbol_t richmath_System_FillBoxWeight;
extern pmath_symbol_t richmath_System_FontColor;
extern pmath_symbol_t richmath_System_FontFamily;
extern pmath_symbol_t richmath_System_FontFeatures;
extern pmath_symbol_t richmath_System_FontSize;
extern pmath_symbol_t richmath_System_FontSlant;
extern pmath_symbol_t richmath_System_FontWeight;
extern pmath_symbol_t richmath_System_FractionBoxOptions;
extern pmath_symbol_t richmath_System_Frame;
extern pmath_symbol_t richmath_System_FrameBoxOptions;
extern pmath_symbol_t richmath_System_FrameMargins;
extern pmath_symbol_t richmath_System_FrameStyle;
extern pmath_symbol_t richmath_System_FrameTicks;
extern pmath_symbol_t richmath_System_FrontEndObject;
extern pmath_symbol_t richmath_System_Function;
extern pmath_symbol_t richmath_System_GeneratedSectionStyles;
extern pmath_symbol_t richmath_System_GridBoxColumnSpacing;
extern pmath_symbol_t richmath_System_GridBoxOptions;
extern pmath_symbol_t richmath_System_GridBoxRowSpacing;
extern pmath_symbol_t richmath_System_GrayLevel;
extern pmath_symbol_t richmath_System_HoldComplete;
extern pmath_symbol_t richmath_System_Hue;
extern pmath_symbol_t richmath_System_ImageSize;
extern pmath_symbol_t richmath_System_ImageSizeAction;
extern pmath_symbol_t richmath_System_Inherited;
extern pmath_symbol_t richmath_System_Initialization;
extern pmath_symbol_t richmath_System_InputAliases;
extern pmath_symbol_t richmath_System_InputAutoReplacements;
extern pmath_symbol_t richmath_System_InputFieldBoxOptions;
extern pmath_symbol_t richmath_System_InterpretationFunction;
extern pmath_symbol_t richmath_System_Italic;
extern pmath_symbol_t richmath_System_JoinForm;
extern pmath_symbol_t richmath_System_LanguageCategory;
extern pmath_symbol_t richmath_System_Left;
extern pmath_symbol_t richmath_System_LineBreakWithin;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Magnification;
extern pmath_symbol_t richmath_System_MathFontFamily;
extern pmath_symbol_t richmath_System_MenuCommandKey;
extern pmath_symbol_t richmath_System_MenuSortingValue;
extern pmath_symbol_t richmath_System_Method;
extern pmath_symbol_t richmath_System_NCache;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_Opacity;
extern pmath_symbol_t richmath_System_PaneBoxOptions;
extern pmath_symbol_t richmath_System_PanelBoxOptions;
extern pmath_symbol_t richmath_System_Placeholder;
extern pmath_symbol_t richmath_System_Plain;
extern pmath_symbol_t richmath_System_PlotRange;
extern pmath_symbol_t richmath_System_PlotRangePadding;
extern pmath_symbol_t richmath_System_PointSize;
extern pmath_symbol_t richmath_System_PureArgument;
extern pmath_symbol_t richmath_System_Range;
extern pmath_symbol_t richmath_System_RemovalConditions;
extern pmath_symbol_t richmath_System_ReturnCreatesNewSection;
extern pmath_symbol_t richmath_System_RGBColor;
extern pmath_symbol_t richmath_System_Right;
extern pmath_symbol_t richmath_System_Rule;
extern pmath_symbol_t richmath_System_RuleDelayed;
extern pmath_symbol_t richmath_System_Saveable;
extern pmath_symbol_t richmath_System_Scaled;
extern pmath_symbol_t richmath_System_ScriptLevel;
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
extern pmath_symbol_t richmath_System_SetterBoxOptions;
extern pmath_symbol_t richmath_System_ShowAutoStyles;
extern pmath_symbol_t richmath_System_ShowContents;
extern pmath_symbol_t richmath_System_ShowSectionBracket;
extern pmath_symbol_t richmath_System_ShowStringCharacters;
extern pmath_symbol_t richmath_System_SliderBoxOptions;
extern pmath_symbol_t richmath_System_StripOnInput;
extern pmath_symbol_t richmath_System_StyleData;
extern pmath_symbol_t richmath_System_StyleDefinitions;
extern pmath_symbol_t richmath_System_SurdForm;
extern pmath_symbol_t richmath_System_SynchronousUpdating;
extern pmath_symbol_t richmath_System_SyntaxForm;
extern pmath_symbol_t richmath_System_TemplateBoxOptions;
extern pmath_symbol_t richmath_System_TextShadow;
extern pmath_symbol_t richmath_System_Thickness;
extern pmath_symbol_t richmath_System_Ticks;
extern pmath_symbol_t richmath_System_Tooltip;
extern pmath_symbol_t richmath_System_Top;
extern pmath_symbol_t richmath_System_TrackedSymbols;
extern pmath_symbol_t richmath_System_True;
extern pmath_symbol_t richmath_System_Unassign;
extern pmath_symbol_t richmath_System_UnsavedVariables;
extern pmath_symbol_t richmath_System_Visible;
extern pmath_symbol_t richmath_System_WholeSectionGroupOpener;
extern pmath_symbol_t richmath_System_WindowFrame;
extern pmath_symbol_t richmath_System_WindowTitle;

namespace richmath { namespace strings {
  extern String Butt;
  extern String CharacterNameStyle;
  extern String ClosingAction;
  extern String Color;
  extern String CommentStyle;
  extern String Delete;
  extern String DragDropContextMenu;
  extern String ExcessOrMissingArgumentStyle;
  extern String Frameless;
  extern String FunctionLocalVariableStyle;
  extern String FunctionNameStyle;
  extern String Hide;
  extern String ImplicitOperatorStyle;
  extern String InlineAutoCompletionStyle;
  extern String InlineSectionEditingStyle;
  extern String KeywordSymbolStyle;
  extern String LocalScopeConflictStyle;
  extern String LocalVariableStyle;
  extern String MatchingBracketHighlightStyle;
  extern String Normal;
  extern String OccurenceHighlightStyle;
  extern String Palette;
  extern String PatternVariableStyle;
  extern String Round;
  extern String SectionInsertionPointColor;
  extern String Square;
  extern String StringStyle;
  extern String SymbolShadowingStyle;
  extern String SyntaxErrorStyle;
  extern String TabHead;
  extern String UndefinedSymbolStyle;
  extern String UnknownOptionStyle;
}}

using namespace richmath;

static MultiMap<Expr, FrontEndReference> box_registry;

bool richmath::get_factor_of_scaled(Expr expr, double *value) {
  RICHMATH_ASSERT(value != nullptr);
  
  if(expr[0] != richmath_System_Scaled)
    return false;
  
  if(expr.expr_length() != 1)
    return false;
  
  Expr val = expr[1];
  if(val[0] == richmath_System_NCache)
    val = val[2];
  
  if(val[0] == richmath_System_List && val.expr_length() == 1)
    val = val[1];
  
  if(val.is_number()) {
    *value = val.to_double();
    return true;
  }
  
  return false;
}

double richmath::convert_float_to_nice_double(float f) {
  char buf[16];
  
  if(f == floorf(f))
    return f;
    
  if(!isfinite(f))
    return f;
  
  snprintf(buf, sizeof(buf), "%.6e", f);
  
  double val = strtod(buf, nullptr);
  if((float)val == f)
    return val;
  
  return f;
}

namespace {
  class EnumStyleConverter: public Shareable {
    public:
      EnumStyleConverter();
      
      virtual bool is_valid_key(int val) {    return _int_to_expr.search(val) != nullptr; }
      virtual bool is_valid_expr(Expr expr) { return _expr_to_int.search(expr) != nullptr; }
      
      virtual int to_int(Expr expr) { return _expr_to_int[std::move(expr)]; }
      virtual Expr to_expr(int val) { return _int_to_expr[val]; }
      
      const Hashtable<Expr, int> &expr_to_int() { return _expr_to_int; }
      
    protected:
      void add(int val, Expr expr);
      
    protected:
      Hashtable<int, Expr> _int_to_expr;
      Hashtable<Expr, int> _expr_to_int;
  };
  
  class FlagsStyleConverter: public EnumStyleConverter {
    public:
      FlagsStyleConverter();
      
      virtual bool is_valid_key(int val) override;
      virtual bool is_valid_expr(Expr expr) override;
      
      virtual int to_int(Expr expr) override;
      virtual Expr to_expr(int val) override;
  };
  
  struct ButtonFrameStyleConverter: public EnumStyleConverter {
    ButtonFrameStyleConverter();
    
    protected:
      void add(ContainerType val, Expr expr) { EnumStyleConverter::add((int)val, std::move(expr)); }
  };
  
  struct ButtonSourceStyleConverter: public EnumStyleConverter {
    ButtonSourceStyleConverter();
  };
  
  struct CapFormStyleConverter: public EnumStyleConverter {
    CapFormStyleConverter();
  };
  
  struct ClosingActionStyleConverter: public EnumStyleConverter {
    ClosingActionStyleConverter();
  };
  
  struct ControlPlacementStyleConverter: public EnumStyleConverter {
    ControlPlacementStyleConverter();
  };
  
  struct FontSlantStyleConverter: public EnumStyleConverter {
    FontSlantStyleConverter();
  };
  
  struct FontWeightStyleConverter: public EnumStyleConverter {
    FontWeightStyleConverter();
  };
  
  struct ImageSizeActionStyleConverter: public EnumStyleConverter {
    ImageSizeActionStyleConverter();
  };
  
  struct MenuCommandKeyStyleConverter: public EnumStyleConverter {
    MenuCommandKeyStyleConverter();
  };
  
  struct MenuSortingValueStyleConverter: public EnumStyleConverter {
    MenuSortingValueStyleConverter();

    virtual bool is_valid_key(int val) override;
    virtual bool is_valid_expr(Expr expr) override;
    
    virtual int to_int(Expr expr) override;
    virtual Expr to_expr(int val) override;
    
  };
  
  struct RemovalConditionsStyleConverter: public FlagsStyleConverter {
    RemovalConditionsStyleConverter();
  };
  
  struct WindowFrameStyleConverter: public EnumStyleConverter {
    WindowFrameStyleConverter();
  };
  
  class SubRuleConverter: public EnumStyleConverter {
    public:
      void add(StyleOptionName val, Expr expr) {
        EnumStyleConverter::add((int)val, expr);
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
          return StyleType::None;
      }
      
      static Expr get_name(StyleOptionName key) { return _key_to_name[key]; }
      
      static StyleOptionName get_key(Expr name) { return _name_to_key[name]; }
      
      static SharedPtr<EnumStyleConverter> get_enum_converter(StyleOptionName key) {
        return _key_to_enum_converter[key.to_literal()];
      }
      
    private:
      static void add(StyleType type, StyleOptionName key, const Expr &name);
      
      static void add_enum(IntStyleOptionName key, const Expr &name, SharedPtr<EnumStyleConverter> enum_converter);
      static void add_to_ruleset(StyleOptionName key, const Expr &name);
      static void add_ruleset_head(StyleOptionName key, const Expr &symbol);
      
    public:
      static Expr get_current_style_value(FrontEndObject *obj, Expr item);
      static bool put_current_style_value(FrontEndObject *obj, Expr item, Expr rhs);
      
      static bool is_list_with_inherited(Expr expr);
      static bool needs_ruledelayed(Expr expr);
      
    private:
      static int _num_styles;
      
      static Hashtable<StyleOptionName, SharedPtr<EnumStyleConverter>> _key_to_enum_converter;
      static Hashtable<StyleOptionName, enum StyleType>                _key_to_type;
      static Hashtable<StyleOptionName, Expr>                          _key_to_name;
      static Hashtable<Expr, StyleOptionName>                          _name_to_key;
  };
  
  int                                                       StyleInformation::_num_styles = 0;
  Hashtable<StyleOptionName, SharedPtr<EnumStyleConverter>> StyleInformation::_key_to_enum_converter;
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
      static bool is_for_length(StyleOptionName n) { return ((int)n & 0x70000) == 0x30000; }
      static bool is_for_string(StyleOptionName n) { return ((int)n & 0x70000) == 0x40000; }
      static bool is_for_expr(  StyleOptionName n) { return ((int)n & 0x70000) == 0x50000; }
      
    public:
      bool raw_get_color( StyleOptionName n, Color  *value) const;
      bool raw_get_int(   StyleOptionName n, int    *value) const;
      bool raw_get_float( StyleOptionName n, float  *value) const;
      bool raw_get_length(StyleOptionName n, Length *value) const;
      bool raw_get_string(StyleOptionName n, String *value) const;
      bool raw_get_expr(  StyleOptionName n, Expr   *value) const;
      
      bool raw_set_color( StyleOptionName n, Color value);
      bool raw_set_int(   StyleOptionName n, int value);
      bool raw_set_float( StyleOptionName n, float value);
      bool raw_set_length(StyleOptionName n, Length value);
      bool raw_set_string(StyleOptionName n, String value);
      bool raw_set_expr(  StyleOptionName n, Expr value);
      
      bool raw_remove(StyleOptionName n);
      bool raw_remove_color( StyleOptionName n);
      bool raw_remove_int(   StyleOptionName n);
      bool raw_remove_float( StyleOptionName n);
      bool raw_remove_length(StyleOptionName n);
      bool raw_remove_string(StyleOptionName n);
      bool raw_remove_expr(  StyleOptionName n);
      
      bool remove_dynamic(StyleOptionName n);
      
      bool remove_all_volatile();
      void collect_unused_dynamic(Hashtable<StyleOptionName, Expr> &dynamic_styles);
      
      // only changes Dynamic() definitions if n.is_literal()
      bool set_pmath(StyleOptionName n, Expr obj);
      
      bool add_pmath(Expr options, bool amend);
      
    private:
      bool set_dynamic(StyleOptionName n, Expr value);
      
      bool set_pmath_bool_auto(  StyleOptionName n, Expr obj);
      bool set_pmath_bool(       StyleOptionName n, Expr obj);
      bool set_pmath_color(      StyleOptionName n, Expr obj);
      bool set_pmath_int(        StyleOptionName n, Expr obj);
      bool set_pmath_float(      StyleOptionName n, Expr obj);
      bool set_pmath_posnum_auto(StyleOptionName n, Expr obj);
      bool set_pmath_length(     StyleOptionName n, Expr obj);
      bool set_pmath_margin(     StyleOptionName n, Expr obj); // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
      bool set_pmath_size(       StyleOptionName n, Expr obj); // n + {0,1,2} ~= {Common, Horizontal, Vertical}
      bool set_pmath_string(     StyleOptionName n, Expr obj);
      bool set_pmath_object(     StyleOptionName n, Expr obj);
      bool set_pmath_flatlist(   StyleOptionName n, Expr obj);
      bool set_pmath_enum(       StyleOptionName n, Expr obj);
      bool set_pmath_ruleset(    StyleOptionName n, Expr obj);
      
      bool set_pmath_by_unknown_key(Expr lhs, Expr rhs, bool amend);
      
    public:
      static Expr merge_style_values(StyleOptionName key, Expr newer, Expr older);
      static Expr finish_style_merge(StyleOptionName key, Expr value);
      
      void emit_definition(StyleOptionName key) const;
      Expr raw_get_pmath(StyleOptionName key, Expr inherited) const;
      
    private:
      static Expr merge_ruleset_members(StyleOptionName key, Expr newer, Expr older);
      static Expr merge_tuple_members(Expr newer, Expr older);
      static Expr merge_margin_values(Expr newer, Expr older);
      static Expr merge_flatlist_members(Expr newer, Expr older);

      static Expr finish_ruleset_merge(StyleOptionName key, Expr value);
      static Expr finish_flatlist_merge(StyleOptionName key, Expr value);
      
      static Expr inherited_tuple_member(Expr inherited, int index);
      static Expr inherited_ruleset_member(Expr inherited, Expr key);
      static Expr inherited_margin_leftright(Expr inherited);
      static Expr inherited_margin_left(     Expr inherited);
      static Expr inherited_margin_right(    Expr inherited);
      static Expr inherited_margin_topbottom(Expr inherited);
      static Expr inherited_margin_top(      Expr inherited);
      static Expr inherited_margin_bottom(   Expr inherited);

      Expr prepare_inherited(StyleOptionName n) const;
      Expr prepare_inherited_size(StyleOptionName n) const;
      Expr prepare_inherited_ruleset(StyleOptionName n) const;
      
      Expr raw_get_pmath_bool_auto(  StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_bool(       StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_color(      StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_int(        StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_float(      StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_posnum_auto(StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_length(     StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_margin(     StyleOptionName n, Expr inherited) const; // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
      Expr raw_get_pmath_size(       StyleOptionName n, Expr inherited) const; // n + {0,1,2} ~= {Common, Horizontal, Vertical}
      Expr raw_get_pmath_string(     StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_object(     StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_flatlist(   StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_enum(       StyleOptionName n, Expr inherited) const;
      Expr raw_get_pmath_ruleset(    StyleOptionName n, Expr inherited) const;
      
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

bool StyleImpl::raw_get_length(StyleOptionName n, Length *value) const {
  const IntFloatUnion *v = self.int_float_values.search(n);
  
  if(!v)
    return false;
    
  *value = Length(v->float_value);
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

bool StyleImpl::raw_set_length(StyleOptionName n, Length value) {
  IntFloatUnion v;
  v.float_value = value.raw_value();
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
  RICHMATH_ASSERT(!n.is_dynamic());
  RICHMATH_ASSERT(is_for_color(n));
  
  return self.int_float_values.remove(n);
}

bool StyleImpl::raw_remove_int(StyleOptionName n) {
  RICHMATH_ASSERT(!n.is_dynamic());
  RICHMATH_ASSERT(is_for_int(n));
  
  return self.int_float_values.remove(n);
}

bool StyleImpl::raw_remove_float(StyleOptionName n) {
  RICHMATH_ASSERT(!n.is_dynamic());
  RICHMATH_ASSERT(is_for_float(n));
  
  return self.int_float_values.remove(n);
}

bool StyleImpl::raw_remove_length(StyleOptionName n) {
  RICHMATH_ASSERT(!n.is_dynamic());
  RICHMATH_ASSERT(is_for_length(n));
  
  return self.int_float_values.remove(n);
}

bool StyleImpl::raw_remove_string(StyleOptionName n) {
  RICHMATH_ASSERT(!n.is_dynamic());
  RICHMATH_ASSERT(is_for_string(n));
  
  return self.object_values.remove(n);
}

bool StyleImpl::raw_remove_expr(StyleOptionName n) {
  RICHMATH_ASSERT(is_for_expr(n) || n.is_dynamic());
  
  return self.object_values.remove(n);
}

bool StyleImpl::remove_dynamic(StyleOptionName n) {
  RICHMATH_ASSERT(n.is_literal());
  
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

int Style::decode_enum(Expr expr, IntStyleOptionName n, int def) {
  StyleType type = StyleInformation::get_type(n);
  
  switch(type) {
//    case StyleType::AutoBool:
//      if(expr == richmath_System_Automatic) return AutoBoolAutomatic;
//      if(expr == richmath_System_True)      return AutoBoolTrue;
//      if(expr == richmath_System_False)     return AutoBoolFalse;
//      break;
//      
//    case StyleType::Bool:
//      if(expr == richmath_System_True)      return true;
//      if(expr == richmath_System_False)     return false;
//      break;
//      
//    case StyleType::Integer:
//      if(expr.is_int32())
//        return PMATH_AS_INT32(expr.get());
      
    case StyleType::Enum: {
      SharedPtr<EnumStyleConverter> enum_converter = StyleInformation::get_enum_converter(n);
      RICHMATH_ASSERT(enum_converter.is_valid());
      
      if(enum_converter->is_valid_expr(expr))
        return enum_converter->to_int(expr);
    } break;
  }
  
  return def;
}

bool StyleImpl::set_pmath(StyleOptionName n, Expr obj) {
  StyleType type = StyleInformation::get_type(n);
  
  bool any_change = false;
  switch(type) {
    case StyleType::None:
      pmath_debug_print("[Cannot encode style %u]\n", (unsigned)(int)n);
      break;
      
    case StyleType::Bool:
      any_change = set_pmath_bool(n, obj);
      break;
      
    case StyleType::AutoBool:
      any_change = set_pmath_bool_auto(n, obj);
      break;
      
    case StyleType::Color:
      any_change = set_pmath_color(n, obj);
      break;
      
    case StyleType::Integer:
      any_change = set_pmath_int(n, obj);
      break;
      
    case StyleType::Number:
      any_change = set_pmath_float(n, obj);
      break;
      
    case StyleType::AutoPositive:
      any_change = set_pmath_posnum_auto(n, obj);
      break;
      
    case StyleType::Length:
      any_change = set_pmath_length(n, obj);
      break;
      
    case StyleType::Margin:
      any_change = set_pmath_margin(n, obj);
      break;
      
    case StyleType::Size:
      any_change = set_pmath_size(n, obj);
      break;
      
    case StyleType::String:
      any_change = set_pmath_string(n, obj);
      break;
      
    case StyleType::Any:
      any_change = set_pmath_object(n, obj);
      break;
      
    case StyleType::AnyFlatList:
      any_change = set_pmath_flatlist(n, obj);
      break;
      
    case StyleType::Enum:
      any_change = set_pmath_enum(n, obj);
      break;
      
    case StyleType::RuleSet:
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

bool StyleImpl::add_pmath(Expr options, bool amend) {
  static bool allow_strings = true;
  
  if(allow_strings && options.is_string()) 
    return set_pmath_string(BaseStyleName, String(options));
  
  if(options[0] == richmath_System_List) {
    bool old_allow_strings = allow_strings;
    allow_strings = false;
    
    bool any_change = false;
    
    for(size_t i = options.expr_length(); i > 0; --i) {
      any_change = add_pmath(options[i], amend) || any_change;
    }
    
    allow_strings = old_allow_strings;
    return any_change;
  }
  
  if(options.is_rule()) {
    Expr lhs = options[1];
    Expr rhs = options[2];
    
    return set_pmath_by_unknown_key(lhs, rhs, amend);
  }
  
  return false;
}

bool StyleImpl::set_dynamic(StyleOptionName n, Expr value) {
  RICHMATH_ASSERT(n.is_literal());
  
  bool any_change = false;
  any_change = raw_remove(n) || any_change;
  any_change = raw_set_expr(n.to_dynamic(), value) || any_change;
  
  raw_set_int(InternalHasPendingDynamic, true);
  return any_change;
}

bool StyleImpl::set_pmath_bool_auto(StyleOptionName n, Expr obj) {
  RICHMATH_ASSERT(is_for_int(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
    
  if(obj == richmath_System_False)
    return raw_set_int(n, AutoBoolFalse) || any_change;
  
  if(obj == richmath_System_True)
    return raw_set_int(n, AutoBoolTrue) || any_change;
  
  if(obj == richmath_System_Automatic)
    return raw_set_int(n, AutoBoolAutomatic) || any_change;
  
  if(obj == richmath_System_Inherited)
    return raw_remove_int(n) || any_change;
    
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  return any_change;
}

bool StyleImpl::set_pmath_bool(StyleOptionName n, Expr obj) {
  RICHMATH_ASSERT(is_for_int(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
    
  if(obj == richmath_System_False)
    return raw_set_int(n, false) || any_change;
    
  if(obj == richmath_System_True)
    return raw_set_int(n, true) || any_change;
    
  if(obj == richmath_System_Inherited)
    return raw_remove_int(n) || any_change;
    
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  return any_change;
}

bool StyleImpl::set_pmath_color(StyleOptionName n, Expr obj) {
  RICHMATH_ASSERT(is_for_color(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
    
  Color c = Color::from_pmath(obj);
  
  if(c.is_valid() || c.is_none())
    return raw_set_color(n, c) || any_change;
  
  if(obj == richmath_System_Inherited)
    return raw_remove_color(n) || any_change;
    
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  return any_change;
}

bool StyleImpl::set_pmath_int(StyleOptionName n, Expr obj) {
  RICHMATH_ASSERT(is_for_int(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
  
  if(obj.is_int32())
    return raw_set_int(n, PMATH_AS_INT32(obj.get())) || any_change;
  
  if(obj == richmath_System_Inherited)
    return raw_remove_int(n) || any_change;
    
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  return any_change;
}

bool StyleImpl::set_pmath_float(StyleOptionName n, Expr obj) {
  RICHMATH_ASSERT(is_for_float(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
    
  if(obj.is_number())
    return raw_set_float(n, obj.to_double()) || any_change;
  
  if(obj == richmath_System_Inherited)
    return raw_remove_float(n) || any_change;
  
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  if(obj[0] == richmath_System_NCache) {
    any_change = raw_set_float(n, obj[2].to_double()) || any_change;
    
    if(n.is_literal()) // TODO: do not treat NCache as Dynamic, but as Definition
      any_change = raw_set_expr(n.to_dynamic(), obj) || any_change;
    
    return any_change;
  }
  
  return any_change;
}

bool StyleImpl::set_pmath_posnum_auto(StyleOptionName n, Expr obj) {
  RICHMATH_ASSERT(is_for_float(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
  
  if(obj == richmath_System_Automatic)
    return raw_set_float(n, 0) || any_change;
  
  if(obj.is_number()) {
    double val = obj.to_double();
    if(val > 0)
      return raw_set_float(n, val) || any_change;
    return any_change;
  }
  
  if(obj == richmath_System_Inherited)
    return raw_remove_float(n) || any_change;
  
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  if(obj[0] == richmath_System_NCache) {
    double val = obj[2].to_double();
    if(val > 0 && isfinite(val))
      any_change = raw_set_float(n, val) || any_change;
    
    if(n.is_literal()) // TODO: do not treat NCache as Dynamic, but as Definition
      any_change = raw_set_expr(n.to_dynamic(), obj) || any_change;
    
    return any_change;
  }
  
  return any_change;
}

bool StyleImpl::set_pmath_length(StyleOptionName n, Expr obj) {
  RICHMATH_ASSERT(is_for_length(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
    
  Length dim = Length::from_pmath(obj);
  
  if(dim.is_valid())
    return raw_set_length(n, dim) || any_change;
  
  if(obj == richmath_System_Inherited)
    return raw_remove_length(n) || any_change;
  
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  if(obj[0] == richmath_System_NCache) {
    any_change = raw_set_length(n, Length(obj[2].to_double())) || any_change;
    
    if(n.is_literal()) // TODO: do not treat NCache as Dynamic, but as Definition
      any_change = raw_set_expr(n.to_dynamic(), obj) || any_change;
    
    return any_change;
  }
  
  return any_change;
}

bool StyleImpl::set_pmath_margin(StyleOptionName n, Expr obj) {
  RICHMATH_ASSERT(is_for_length(n));
  
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
  
  Length dim = Length::from_pmath(obj);
  if(!dim.is_valid()) {
    if(obj == richmath_System_True) {
      dim = Length(1.0);
    }
    else if(obj == richmath_System_False || obj == richmath_System_None) {
      dim = Length(0.0);
    }
  }
  
  if(dim.is_valid()) {
    any_change = raw_set_length(Left,   dim) || any_change;
    any_change = raw_set_length(Right,  dim) || any_change;
    any_change = raw_set_length(Top,    dim) || any_change;
    any_change = raw_set_length(Bottom, dim) || any_change;
    return any_change;
  }
  
  if( obj.is_expr() && obj[0] == richmath_System_List) {
    if(obj.expr_length() == 4) {
      any_change = set_pmath_length(Left,   obj[1]) || any_change;
      any_change = set_pmath_length(Right,  obj[2]) || any_change;
      any_change = set_pmath_length(Top,    obj[3]) || any_change;
      any_change = set_pmath_length(Bottom, obj[4]) || any_change;
      return any_change;
    }
    
    if(obj.expr_length() == 2) {
      {
        Expr horz = obj[1];
        if( horz.is_expr() &&
            horz[0] == richmath_System_List &&
            horz.expr_length() == 2)
        {
          any_change = set_pmath_length(Left,  horz[1]) || any_change;
          any_change = set_pmath_length(Right, horz[2]) || any_change;
        }
        else {
          any_change = set_pmath_length(Left,  horz) || any_change;
          any_change = set_pmath_length(Right, horz) || any_change;
        }
      }
      
      {
        Expr vert = obj[2];
        if( vert.is_expr() &&
            vert[0] == richmath_System_List &&
            vert.expr_length() == 2)
        {
          any_change = set_pmath_length(Top,    vert[1]) || any_change;
          any_change = set_pmath_length(Bottom, vert[2]) || any_change;
        }
        else {
          any_change = set_pmath_length(Top,    vert) || any_change;
          any_change = set_pmath_length(Bottom, vert) || any_change;
        }
      }
      
      return any_change;
    }
    return any_change;
  }
  
  if(obj == richmath_System_Inherited) {
    any_change = raw_remove_length(Left)   || any_change;
    any_change = raw_remove_length(Right)  || any_change;
    any_change = raw_remove_length(Top)    || any_change;
    any_change = raw_remove_length(Bottom) || any_change;
    return any_change;
  }
  
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  return any_change;
}

bool StyleImpl::set_pmath_size(StyleOptionName n, Expr obj) {
  RICHMATH_ASSERT(is_for_length(n));
  
  StyleOptionName Horizontal = StyleOptionName((int)n + 1);
  StyleOptionName Vertical   = StyleOptionName((int)n + 2);
  
  bool any_change = false;
  if(n.is_literal()) {
    any_change = remove_dynamic(n)          || any_change;
    any_change = remove_dynamic(Horizontal) || any_change;
    any_change = remove_dynamic(Vertical)   || any_change;
  }
  
  Length dim = Length::from_pmath(obj);
  if(dim.is_valid()) {
    any_change = raw_remove_length(n) || any_change;
    
    any_change = raw_set_length(Horizontal, dim) || any_change;
    any_change = raw_set_length(Vertical, SymbolicSize::Automatic) || any_change;
    return any_change;
  }
  
  if(obj[0] == richmath_System_List && obj.expr_length() == 2) {
    any_change = raw_remove_length(n) || any_change;
    
    any_change = set_pmath_length(Horizontal, obj[1]) || any_change;
    any_change = set_pmath_length(Vertical,   obj[2]) || any_change;
      
    return any_change;
  }
  
  if(obj == richmath_System_Inherited) {
    any_change = raw_remove_length(n)          || any_change;
    any_change = raw_remove_length(Horizontal) || any_change;
    any_change = raw_remove_length(Vertical)   || any_change;
    if(!StyleOptionName{n} .is_dynamic()) {
      any_change = remove_dynamic(n)          || any_change;
      any_change = remove_dynamic(Horizontal) || any_change;
      any_change = remove_dynamic(Vertical)   || any_change;
    }
    return any_change;
  }
  
  if(n.is_literal() && Dynamic::is_dynamic(obj)) {
    any_change = raw_remove_length(n)          || any_change;
    any_change = raw_remove_length(Horizontal) || any_change;
    any_change = raw_remove_length(Vertical)   || any_change;
    any_change = remove_dynamic(Horizontal)    || any_change;
    any_change = remove_dynamic(Vertical)      || any_change;
    any_change = set_dynamic(n, obj)           || any_change;
    return any_change;
  }
  
  if(obj[0] == richmath_System_NCache) {
    any_change = set_pmath_size(n, obj[2]) || any_change;
    
    if(n.is_literal()) // TODO: do not treat NCache as Dynamic, but as Definition
      any_change = raw_set_expr(n.to_dynamic(), obj) || any_change;
      
    return any_change;
  }
  
  return any_change;
}

bool StyleImpl::set_pmath_string(StyleOptionName n, Expr obj) {
  RICHMATH_ASSERT(is_for_string(n));
  
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
  
  if(obj == richmath_System_Inherited)
    return raw_remove_string(n) || any_change;
  
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  return any_change;
}

bool StyleImpl::set_pmath_object(StyleOptionName n, Expr obj) {
  RICHMATH_ASSERT(is_for_expr(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
  
  if(obj == richmath_System_Inherited)
    return raw_remove_expr(n) || any_change;
  
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
    
  if(n == StyleDefinitions /*|| n == BoxID*/) {
    if(raw_set_expr(n, obj)) {
      any_change = true;
      raw_set_int(InternalHasPendingDynamic, true);
    }
    return any_change;
  }
  
  return raw_set_expr(n, obj) || any_change;
}

bool StyleImpl::set_pmath_flatlist(StyleOptionName n, Expr obj) {
  RICHMATH_ASSERT(is_for_expr(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
  
  if(obj == richmath_System_Inherited)
    return raw_remove_expr(n) || any_change;
  
  if(n.is_literal()) {
    if(Dynamic::is_dynamic(obj))
      return set_dynamic(n, obj) || any_change;
    
    if(StyleInformation::is_list_with_inherited(obj)) {
      Expr inherited;
      if(raw_get_expr(n, &inherited)) {
        obj = merge_flatlist_members(std::move(obj), std::move(inherited));
      }
    }
  }
  
  return raw_set_expr(n, obj) || any_change;
}

bool StyleImpl::set_pmath_enum(StyleOptionName n, Expr obj) {
  RICHMATH_ASSERT(is_for_int(n));
  
  bool any_change = false;
  if(n.is_literal())
    any_change = remove_dynamic(n);
    
  if(obj == richmath_System_Inherited)
    return raw_remove_int(n) || any_change;
  
  if(n.is_literal() && Dynamic::is_dynamic(obj))
    return set_dynamic(n, obj) || any_change;
  
  SharedPtr<EnumStyleConverter> enum_converter = StyleInformation::get_enum_converter(n);
  RICHMATH_ASSERT(enum_converter.is_valid());
  
  if(enum_converter->is_valid_expr(obj))
    return raw_set_int(n, enum_converter->to_int(obj)) || any_change;
  
  return any_change;
}

bool StyleImpl::set_pmath_ruleset(StyleOptionName n, Expr obj) {
  SharedPtr<EnumStyleConverter> key_converter = StyleInformation::get_enum_converter(n);
  RICHMATH_ASSERT(key_converter.is_valid());
  
  bool any_change = false;
  if(obj[0] == richmath_System_List) {
    for(size_t i = 1; i <= obj.expr_length(); ++i) {
      Expr rule = obj[i];
      
      if(rule.is_rule()) {
        Expr lhs = rule[1];
        Expr rhs = rule[2];
        
//        if(rhs == richmath_System_Inherited)
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

bool StyleImpl::set_pmath_by_unknown_key(Expr lhs, Expr rhs, bool amend) {
  StyleOptionName key = StyleInformation::get_key(lhs);
  
  if(!key.is_valid()) {
    pmath_debug_print_object("[unknown option ", lhs.get(), "]\n");
    
    Expr sym;
    if(!raw_get_expr(UnknownOptions, &sym) || !sym.is_symbol()) {
      sym = Expr(pmath_symbol_create_temporary(PMATH_C_STRING("FE`Styles`unknown"), TRUE));
      
      pmath_symbol_set_attributes(sym.get(),
                                  PMATH_SYMBOL_ATTRIBUTE_TEMPORARY | PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE);
                                  
      raw_set_expr(UnknownOptions, sym);
    }
    
    Expr eval;
    if(rhs == richmath_System_Inherited) {
      eval = Call(Symbol(richmath_System_Unassign), Call(sym, lhs));
    }
    else {
      eval = Call(Symbol(richmath_System_Assign), Call(sym, lhs), rhs);
    }
    
    Evaluate(eval);
    return true;
  }
  
  bool any_change = false;
  if(!amend) {
    if(StyleInformation::get_type(key) == StyleType::AnyFlatList) {
      // Reset the style so that calling CurrentValue(...):= {..., Inherited, ...} multiple times
      // will not repeatedly fill in the previous value.
      if(rhs != richmath_System_Inherited) {
        any_change = raw_remove_expr(key) || any_change;
      }
    }
  }
  return set_pmath(key, rhs) || any_change;
}

Expr StyleImpl::merge_style_values(StyleOptionName key, Expr newer, Expr older) {
  if(newer == richmath_System_Inherited)
    return older;
    
  if(older == richmath_System_Inherited)
    return newer;
    
  StyleType type = StyleInformation::get_type(key);
  
  switch(type) {
    case StyleType::Margin:
      return merge_margin_values(std::move(newer), std::move(older));
      
    case StyleType::Size:
      return merge_tuple_members(std::move(newer), std::move(older));
      
    case StyleType::RuleSet:
      return merge_ruleset_members(key, std::move(newer), std::move(older));
      
    case StyleType::AnyFlatList:
      return merge_flatlist_members(std::move(newer), std::move(older));
      
    default:
      break;
  }
  return newer;
}

Expr StyleImpl::merge_ruleset_members(StyleOptionName key, Expr newer, Expr older) { // ignores all rules in older, whose keys do not appear in newer
  if(newer == richmath_System_Inherited)
    return older;
    
  if(older == richmath_System_Inherited)
    return newer;
    
  SharedPtr<EnumStyleConverter> key_converter = StyleInformation::get_enum_converter(key);
  RICHMATH_ASSERT(key_converter.is_valid());
  
  if(newer[0] == richmath_System_List) {
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
        if(rhs == richmath_System_Inherited)
          new_rhs = inherited_ruleset_member(older, lhs);
      }
      
      if(new_rhs != rhs) {
        rule.set(2, std::move(new_rhs));
        newer.set(i, std::move(rule));
      }
    }
  }
  
  return newer;
}

Expr StyleImpl::merge_tuple_members(Expr newer, Expr older) {
  if(newer == richmath_System_Inherited)
    return older;
    
  if(older == richmath_System_Inherited)
    return newer;
    
  if(newer[0] == richmath_System_List && older[0] == richmath_System_List && newer.expr_length() == older.expr_length()) {
    for(size_t i = newer.expr_length(); i > 0; --i) {
      Expr item = newer[i];
      if(item == richmath_System_Inherited)
        newer.set(i, older[i]);
      else if(item[0] == richmath_System_List)
        newer.set(i, merge_tuple_members(item, older[i]));
    }
    return newer;
  }
  
  pmath_debug_print_object("[Warning: cannot merge tuples ", newer.get(), "");
  pmath_debug_print_object(" and ", older.get(), "]\n");
  return newer;
}

Expr StyleImpl::merge_margin_values(Expr newer, Expr older) {
  if(newer[0] == richmath_System_List) {
    if(newer.expr_length() == 2)
      return merge_tuple_members(newer, List(inherited_margin_leftright(older), inherited_margin_topbottom(older)));
      
    return merge_tuple_members(std::move(newer), std::move(older));
  }
  return newer;
}

Expr StyleImpl::merge_flatlist_members(Expr newer, Expr older) {
  if(newer[0] == richmath_System_List) {
    bool need_flatten = false;
    for(size_t i = newer.expr_length(); i > 0; --i) {
      Expr item = newer[i];
      if(item == richmath_System_Inherited) {
        if(older[0] == richmath_System_List) {
          if(older.expr_length() == 1) {
            newer.set(i, older[1]);
          }
          else {
            need_flatten = true;
            newer.set(i, older);
          }
        }
        else 
          newer.set(i, older);
        
        continue;
      }
      if(!need_flatten)
        need_flatten = item[0] == richmath_System_List;
    }

    if(need_flatten)
      newer = Expr{pmath_expr_flatten(newer.release(), pmath_ref(richmath_System_List), 1)};
    
    return newer;
  }

  pmath_debug_print_object("[Warning: cannot merge ", newer.get(), "");
  pmath_debug_print_object(" and ", older.get(), "]\n");
  return newer;
}

Expr StyleImpl::finish_style_merge(StyleOptionName key, Expr value) {
  StyleType type = StyleInformation::get_type(key);
  switch(type) {
    case StyleType::RuleSet:
      return finish_ruleset_merge(key, std::move(value));
      
    case StyleType::AnyFlatList:
      return finish_flatlist_merge(key, std::move(value));
      
    default:
      break;
  }
  return value;
}

Expr StyleImpl::finish_ruleset_merge(StyleOptionName key, Expr value) {
  if(value == richmath_System_Inherited)
    return List();
  
  SharedPtr<EnumStyleConverter> key_converter = StyleInformation::get_enum_converter(key);
  RICHMATH_ASSERT(key_converter.is_valid());
  
  if(value[0] == richmath_System_List) {
    for(size_t i = value.expr_length(); i > 0; --i) {
      Expr rule = value[i];
      
      if(!rule.is_rule())
        continue;
        
      Expr lhs = rule[1];
      Expr rhs = rule[2];
      
      Expr new_rhs = rhs;
      StyleOptionName sub_key = StyleOptionName{key_converter->to_int(lhs)};
      if(sub_key.is_valid()) {
        new_rhs = finish_style_merge(sub_key, rhs);
      }
      else {
        pmath_debug_print_object("[unknown sub-option ", lhs.get(), "]\n");
      }
      
      if(new_rhs != rhs) {
        rule.set(2, std::move(new_rhs));
        value.set(i, std::move(rule));
      }
    }
  }
  
  return value;
}

Expr StyleImpl::finish_flatlist_merge(StyleOptionName key, Expr value) {
  return merge_flatlist_members(value, List());
}

Expr StyleImpl::inherited_tuple_member(Expr inherited, int index) {
  RICHMATH_ASSERT(index >= 1);
  
  if(inherited[0] == richmath_System_List) {
    if((size_t)index <= inherited.expr_length())
      return inherited[index];
  }
  
  if(inherited != richmath_System_Inherited) {
    pmath_debug_print("[Warning: partial redefition of item %d", index);
    pmath_debug_print_object(" of ", inherited.get(), "]\n");
  }
  return Symbol(richmath_System_Inherited);
}

Expr StyleImpl::inherited_ruleset_member(Expr inherited, Expr key) {
  if(inherited[0] == richmath_System_List) {
    size_t len = inherited.expr_length();
    for(size_t i = 1; i <= len; ++i) {
      Expr item = inherited[i];
      if(item.is_rule() && item[1] == key)
        return item[2];
    }
  }
  else if(inherited != richmath_System_Inherited) {
    pmath_debug_print_object("[Warning: partial redefition of rule ", key.get(), "");
    pmath_debug_print_object(" of ", inherited.get(), "]\n");
  }
  
  return Symbol(richmath_System_Inherited);
}

Expr StyleImpl::inherited_margin_leftright(Expr inherited) {
  if(inherited[0] == richmath_System_List) {
    if(inherited.expr_length() == 2)
      return inherited_tuple_member(std::move(inherited), 1);
  }
  
  return List(inherited_tuple_member(inherited, 1), inherited_tuple_member(inherited, 2));
}

Expr StyleImpl::inherited_margin_left(Expr inherited) {
  if(inherited[0] == richmath_System_List) {
    if(inherited.expr_length() == 2)
      return inherited_tuple_member(inherited_tuple_member(std::move(inherited), 1), 1);
  }
  
  return inherited_tuple_member(std::move(inherited), 1);
}

Expr StyleImpl::inherited_margin_right(Expr inherited) {
  if(inherited[0] == richmath_System_List) {
    if(inherited.expr_length() == 2)
      return inherited_tuple_member(inherited_tuple_member(std::move(inherited), 1), 2);
  }
  
  return inherited_tuple_member(std::move(inherited), 2);
}

Expr StyleImpl::inherited_margin_topbottom(Expr inherited) {
  if(inherited[0] == richmath_System_List) {
    if(inherited.expr_length() == 2)
      return inherited_tuple_member(std::move(inherited), 2);
  }
  
  return List(inherited_tuple_member(inherited, 3), inherited_tuple_member(inherited, 4));
}

Expr StyleImpl::inherited_margin_top(Expr inherited) {
  if(inherited[0] == richmath_System_List) {
    if(inherited.expr_length() == 2)
      return inherited_tuple_member(inherited_tuple_member(std::move(inherited), 2), 1);
  }
  
  return inherited_tuple_member(std::move(inherited), 3);
}

Expr StyleImpl::inherited_margin_bottom(Expr inherited) {
  if(inherited[0] == richmath_System_List) {
    if(inherited.expr_length() == 2)
      return inherited_tuple_member(inherited_tuple_member(std::move(inherited), 2), 2);
  }
  
  return inherited_tuple_member(std::move(inherited), 4);
}

Expr StyleImpl::prepare_inherited(StyleOptionName n) const {
  switch (StyleInformation::get_type(n)) {
    case StyleType::Size:    return prepare_inherited_size(n);
    case StyleType::RuleSet: return prepare_inherited_ruleset(n);
    default: break;
  }

  return Symbol(richmath_System_Inherited);
}

Expr StyleImpl::prepare_inherited_size(StyleOptionName n) const {
  StyleOptionName Horizontal = StyleOptionName((int)n + 1);
  StyleOptionName Vertical = StyleOptionName((int)n + 2);
  
  Expr horz;
  bool have_horz = raw_get_expr(Horizontal.to_dynamic(), &horz);
  if(!have_horz)
    horz = Symbol(richmath_System_Inherited);
    
  Expr vert;
  bool have_vert = raw_get_expr(Vertical.to_dynamic(), &vert);
  if(!have_vert)
    vert = Symbol(richmath_System_Inherited);
    
  if(have_horz || have_vert)
    return List(horz, vert);
  
  return Symbol(richmath_System_Inherited);
}

Expr StyleImpl::prepare_inherited_ruleset(StyleOptionName n) const {
  SharedPtr<EnumStyleConverter> key_converter = StyleInformation::get_enum_converter(n);
  
  RICHMATH_ASSERT(key_converter.is_valid());
  
  const Hashtable<Expr, int> &table = key_converter->expr_to_int();
  
  bool all_inherited = true;
  Gather g;
  
  for(auto &entry : table.entries()) {
    Expr dyn;
    if(raw_get_expr(StyleOptionName{entry.value}.to_dynamic(), &dyn)) {
      all_inherited = false;
      Gather::emit(Rule(entry.key, std::move(dyn)));
    }
  }
  
  Expr e = g.end();
  if(all_inherited)
    return Symbol(richmath_System_Inherited);
    
  e.sort();
  return e;
}

void StyleImpl::emit_definition(StyleOptionName n) const {
  RICHMATH_ASSERT(n.is_literal());
  
  Expr e;
  if(raw_get_expr(n.to_dynamic(), &e)) {
    Gather::emit(Rule(StyleInformation::get_name(n), e));
    return;
  }
  
  e = raw_get_pmath(n, prepare_inherited(n));
  if(e == richmath_System_Inherited)
    return;
    
  if(StyleInformation::needs_ruledelayed(e))
    Gather::emit(RuleDelayed(StyleInformation::get_name(n), e));
  else
    Gather::emit(Rule(StyleInformation::get_name(n), e));
}

Expr StyleImpl::raw_get_pmath(StyleOptionName key, Expr inherited) const {
  StyleType type = StyleInformation::get_type(key);
  
  switch(type) {
    case StyleType::None:
      break;
      
    case StyleType::Bool:
      return raw_get_pmath_bool(key, std::move(inherited));
      
    case StyleType::AutoBool:
      return raw_get_pmath_bool_auto(key, std::move(inherited));
      
    case StyleType::Color:
      return raw_get_pmath_color(key, std::move(inherited));
      
    case StyleType::Integer:
      return raw_get_pmath_int(key, std::move(inherited));
      
    case StyleType::Number:
      return raw_get_pmath_float(key, std::move(inherited));
      
    case StyleType::AutoPositive:
      return raw_get_pmath_posnum_auto(key, std::move(inherited));
      
    case StyleType::Length:
      return raw_get_pmath_length(key, std::move(inherited));
      
    case StyleType::Margin:
      return raw_get_pmath_margin(key, std::move(inherited));
      
    case StyleType::Size:
      return raw_get_pmath_size(key, std::move(inherited));
      
    case StyleType::String:
      return raw_get_pmath_string(key, std::move(inherited));
      
    case StyleType::Any:
      return raw_get_pmath_object(key, std::move(inherited));
      
    case StyleType::AnyFlatList:
      return raw_get_pmath_flatlist(key, std::move(inherited));
      
    case StyleType::Enum:
      return raw_get_pmath_enum(key, std::move(inherited));
      
    case StyleType::RuleSet:
      return raw_get_pmath_ruleset(key, std::move(inherited));
  }
  
  return inherited;
}

Expr StyleImpl::raw_get_pmath_bool_auto(StyleOptionName n, Expr inherited) const {
  RICHMATH_ASSERT(is_for_int(n));
  
  int i;
  if(raw_get_int(n, &i)) {
    switch(i) {
      case AutoBoolFalse:
        return Symbol(richmath_System_False);
        
      case AutoBoolTrue:
        return Symbol(richmath_System_True);
        
      case AutoBoolAutomatic:
      default:
        return Symbol(richmath_System_Automatic);
    }
  }
  
  return inherited;
}

Expr StyleImpl::raw_get_pmath_bool(StyleOptionName n, Expr inherited) const {
  RICHMATH_ASSERT(is_for_int(n));
  
  int i;
  if(raw_get_int(n, &i)) {
    if(i)
      return Symbol(richmath_System_True);
      
    return Symbol(richmath_System_False);
  }
  
  return inherited;
}

Expr StyleImpl::raw_get_pmath_color(StyleOptionName n, Expr inherited) const {
  RICHMATH_ASSERT(is_for_color(n));
  
  Color c;
  if(raw_get_color(n, &c))
    return c.to_pmath();
    
  return inherited;
}

Expr StyleImpl::raw_get_pmath_int(StyleOptionName n, Expr inherited) const {
  RICHMATH_ASSERT(is_for_int(n));
  
  int i;
  if(raw_get_int(n, &i))
    return Expr(i);
    
  return inherited;
}

Expr StyleImpl::raw_get_pmath_float(StyleOptionName n, Expr inherited) const {
  RICHMATH_ASSERT(is_for_float(n));
  
  float f;
  if(raw_get_float(n, &f))
    return Number(convert_float_to_nice_double(f));
    
  return inherited;
}

Expr StyleImpl::raw_get_pmath_posnum_auto(StyleOptionName n, Expr inherited) const {
  RICHMATH_ASSERT(is_for_float(n));
  
  float f;
  if(raw_get_float(n, &f)) {
    if(isfinite(f) && f > 0)
      return Number(convert_float_to_nice_double(f));
    
    return Symbol(richmath_System_Automatic);
  }
  
  return inherited;
}

Expr StyleImpl::raw_get_pmath_length(StyleOptionName n, Expr inherited) const {
  RICHMATH_ASSERT(is_for_length(n));
  
  Length dim;
  if(raw_get_length(n, &dim))
    return dim.to_pmath();
    
  return inherited;
}

Expr StyleImpl::raw_get_pmath_margin(StyleOptionName n, Expr inherited) const { // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
  RICHMATH_ASSERT(is_for_length(n));
  
  StyleOptionName Left   = n;
  StyleOptionName Right  = StyleOptionName((int)n + 1);
  StyleOptionName Top    = StyleOptionName((int)n + 2);
  StyleOptionName Bottom = StyleOptionName((int)n + 3);
  
  Length left, right, top, bottom;
  
  if(!raw_get_length(Left,   &left))   left   = SymbolicSize::Invalid;
  if(!raw_get_length(Right,  &right))  right  = SymbolicSize::Invalid;
  if(!raw_get_length(Top,    &top))    top    = SymbolicSize::Invalid;
  if(!raw_get_length(Bottom, &bottom)) bottom = SymbolicSize::Invalid;
  
  if(left.is_valid() || right.is_valid() || top.is_valid() || bottom.is_valid()) {
    Expr l, r, t, b;
    
    if(left.is_valid()) 
      l = left.to_pmath();
    else
      l = inherited_margin_left(inherited);
      
    if(right.is_valid())
      r = right.to_pmath();
    else
      r = inherited_margin_right(inherited);
      
    if(top.is_valid())
      t = top.to_pmath();
    else
      t = inherited_margin_top(inherited);
      
    if(bottom.is_valid())
      b = bottom.to_pmath();
    else
      b = inherited_margin_bottom(std::move(inherited));
    
    if(l == r) {
      if(t == b) {
        if(l == t)
          return l;
        
        return List(l, t);
      }
      
      if(l.is_symbol())
        return List(l, List(t, b));
    }
    else if(t == b && t.is_symbol()) {
      return List(List(l, r), t);
    }
    
    return List(List(l, r), List(t, b));
  }
  
  return inherited;
}

Expr StyleImpl::raw_get_pmath_size(StyleOptionName n, Expr inherited) const { // n + {0,1,2} ~= {Common, Horizontal, Vertical}
  RICHMATH_ASSERT(is_for_length(n));
  
  Expr horz, vert;
  
  StyleOptionName Horizontal = StyleOptionName((int)n + 1);
  StyleOptionName Vertical   = StyleOptionName((int)n + 2);
  
  Length h;
  if(!raw_get_length(Horizontal, &h)) h = SymbolicSize::Invalid;
  
  if(h.is_valid())
    horz = h.to_pmath();
  else
    horz = inherited_tuple_member(inherited, 1);

  Length v;
  if(!raw_get_length(Vertical, &v)) v = SymbolicSize::Invalid;
  
  if(v.is_valid()) 
    vert = v.to_pmath();
  else
    vert = inherited_tuple_member(inherited, 2);

  if(h.is_valid() || v.is_valid())
    return List(horz, vert);
    
  return inherited;
}

Expr StyleImpl::raw_get_pmath_string(StyleOptionName n, Expr inherited) const {
  RICHMATH_ASSERT(is_for_string(n));
  
  String s;
  if(raw_get_string(n, &s))
    return s;
    
  return inherited;
}

Expr StyleImpl::raw_get_pmath_object(StyleOptionName n, Expr inherited) const {
  RICHMATH_ASSERT(is_for_expr(n));
  
  Expr e;
  if(raw_get_expr(n, &e))
    return e;
    
  return inherited;
}

Expr StyleImpl::raw_get_pmath_flatlist(StyleOptionName n, Expr inherited) const {
  RICHMATH_ASSERT(is_for_expr(n));
  
  Expr e;
  if(raw_get_expr(n, &e))
    return e;
    
  return inherited;
}

Expr StyleImpl::raw_get_pmath_enum(StyleOptionName n, Expr inherited) const {
  RICHMATH_ASSERT(is_for_int(n));
  
  int i;
  if(raw_get_int(n, &i)) {
    SharedPtr<EnumStyleConverter> enum_converter = StyleInformation::get_enum_converter(n);
    
    RICHMATH_ASSERT(enum_converter.is_valid());
    
    return enum_converter->to_expr(i);
  }
  
  return inherited;
}

Expr StyleImpl::raw_get_pmath_ruleset(StyleOptionName n, Expr inherited) const {
  SharedPtr<EnumStyleConverter> key_converter = StyleInformation::get_enum_converter(n);
  
  RICHMATH_ASSERT(key_converter.is_valid());
  
  const Hashtable<Expr, int> &table = key_converter->expr_to_int();
  
  bool all_inherited = true;
  Gather g;
  
  for(auto &entry : table.entries()) {
    Expr inherited_value = inherited_ruleset_member(inherited, entry.key);
    Expr value = raw_get_pmath(StyleOptionName{entry.value}, inherited_value);
    
    if(value != richmath_System_Inherited) {
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

//{ class EnumStyleConverter ...

EnumStyleConverter::EnumStyleConverter()
  : Shareable()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  _expr_to_int.default_value = -1;
}

void EnumStyleConverter::add(int val, Expr expr) {
  _int_to_expr.set(val, expr);
  _expr_to_int.set(expr, val);
}

//} ... class EnumStyleConverter

//{ class FlagsStyleConverter ...

FlagsStyleConverter::FlagsStyleConverter()
  : EnumStyleConverter()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  _expr_to_int.default_value = 0;
}

bool FlagsStyleConverter::is_valid_key(int val) {
  for(unsigned uval = (unsigned)val, mask = 1; uval; uval >>= 1, mask <<= 1) {
    if((unsigned)val & mask) {
      if(_int_to_expr.search(mask) == nullptr)
        return false;
    }
  }
  
  return true;
}

bool FlagsStyleConverter::is_valid_expr(Expr expr) {
  if(expr[0] == richmath_System_List) {
    for(auto item : expr.items()) {
      if(_expr_to_int.search(item) == nullptr)
        return false;
    }
    return true;
  }
  
  return _expr_to_int.search(expr) != nullptr;
}

int FlagsStyleConverter::to_int(Expr expr) {
  if(expr[0] == richmath_System_List) {
    unsigned uval = 0;
    for(auto item : expr.items()) {
      uval |= _expr_to_int[item];
    }
    return (int)uval;
  }
  
  return _expr_to_int[expr];
}

Expr FlagsStyleConverter::to_expr(int val) {
  if(auto eptr = _int_to_expr.search(val))
    return *eptr;
  
  if(val == 0)
    return Symbol(richmath_System_None);
  
  Gather g;
  for(unsigned uval = (unsigned)val, mask = 1; uval; uval >>= 1, mask <<= 1) {
    if((unsigned)val & mask) {
      g.emit(_int_to_expr[mask]);
    }
  }
  return g.end();
}

//} ... class FlagsStyleConverter

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
  clear();
  StyleInformation::remove_style();
}

void Style::clear() {
  auto impl = StyleImpl::of(*this);
  Expr old_boxid;
  if(impl.raw_get_expr(InternalRegisteredBoxID, &old_boxid)) {
    int old_ref_id = 0;
    impl.raw_get_int(InternalRegisteredBoxReference, &old_ref_id);
    auto owner_ref = FrontEndReference::unsafe_cast_from_pointer((void*)(intptr_t)old_ref_id);
    auto owner = FrontEndObject::find_cast<StyledObject>(owner_ref);
    
    if(!owner) {
      pmath_debug_print_object("[Style::clear: cannot find owning object for BoxID -> ", old_boxid.get(), "]\n");
    }
    else if(owner->own_style() && owner->own_style().ptr() != this) {
      pmath_debug_print("[?!? Style::clear: box #%d = %p has style %p != %p which claims so with ",
        (int)(intptr_t)FrontEndReference::unsafe_cast_to_pointer(owner->id()),
        owner,
        owner->own_style().ptr(),
        this);
      pmath_debug_print_object("BoxID -> ", old_boxid.get(), "]\n");
      
      owner = nullptr;
    }
    
    if(owner) {
      bool did_remove = box_registry.remove(old_boxid, owner->id());
      if(did_remove) {
        pmath_debug_print("[Style::clear: unregistered box #%d = %p with old ", 
          (int)(intptr_t)FrontEndReference::unsafe_cast_to_pointer(owner->id()), 
          owner);
        pmath_debug_print_object(" BoxID -> ", old_boxid.get(), "]\n");
      } 
      else {
        pmath_debug_print("[Style::clear: did not find box #%d = %p by its old ", 
          (int)(intptr_t)FrontEndReference::unsafe_cast_to_pointer(owner->id()), 
          owner);
        pmath_debug_print_object(" BoxID -> ", old_boxid.get(), "]\n");
      }
      
      owner->has_box_id(false);
    }
  }
  
  int_float_values.clear();
  object_values.clear();
}

void Style::reset(SharedPtr<Style> &style, String base_style_name) {
  if(!style) {
    style = new Style();
    style->set(BaseStyleName, std::move(base_style_name));
    return;
  }
  
  int old_defines_eval_ctx = false;
  style->get(InternalDefinesEvaluationContext, &old_defines_eval_ctx);
  
  int old_has_pending_dynamic = 0;
  style->get(InternalHasPendingDynamic, &old_has_pending_dynamic);
  
  String old_base_style_name;
  style->get(BaseStyleName, &old_base_style_name);
  
  style->clear();
  style->set(BaseStyleName, base_style_name);
  if(!base_style_name.is_null())
    style->set(InternalHasNewBaseStyle, true);
  
  if(old_defines_eval_ctx)
    style->set(InternalDefinesEvaluationContext, true);
  
  if(!old_has_pending_dynamic && old_base_style_name == base_style_name)
    style->remove(InternalHasPendingDynamic);
}

void Style::add_pmath(Expr options, bool amend) {
  if(StyleImpl::of(*this).add_pmath(std::move(options), amend))
    notify_all();
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
  if(expr == richmath_System_Inherited)
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

Expr Style::finish_style_merge(StyleOptionName n, Expr value) {
  return StyleImpl::finish_style_merge(n, std::move(value));
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

bool Style::get(LengthStyleOptionName n, Length *value) const {
  register_observer();
  if(StyleImpl::of(*this).raw_get_length(n, value))
    return true;
  if(StyleImpl::of(*this).raw_get_length(StyleOptionName(n).to_volatile(), value))
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
  RICHMATH_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_set_color(key, value) || any_change;
  any_change = StyleImpl::of(*this).remove_dynamic(key)       || any_change;
  
  if(any_change)
    notify_all();
}

void Style::set(IntStyleOptionName n, int value) {
  StyleOptionName key{n};
  RICHMATH_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_set_int(key, value) || any_change;
  any_change = StyleImpl::of(*this).remove_dynamic(key)     || any_change;
  
  if(any_change)
    notify_all();
}

void Style::set(FloatStyleOptionName n, float value) {
  StyleOptionName key{n};
  RICHMATH_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_set_float(key, value) || any_change;
  any_change = StyleImpl::of(*this).remove_dynamic(key)       || any_change;
  
  if(any_change)
    notify_all();
}

void Style::set(LengthStyleOptionName n, Length value) {
  StyleOptionName key{n};
  RICHMATH_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_set_length(key, value) || any_change;
  any_change = StyleImpl::of(*this).remove_dynamic(key)        || any_change;
  
  if(any_change)
    notify_all();
}

void Style::set(StringStyleOptionName n, String value) {
  StyleOptionName key{n};
  RICHMATH_ASSERT(key.is_literal());
  
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
  RICHMATH_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_set_expr(n, value) || any_change;
  any_change = StyleImpl::of(*this).remove_dynamic(key)    || any_change;
  
  if(any_change)
    notify_all();
}

void Style::remove(ColorStyleOptionName n) {
  StyleOptionName key{n};
  RICHMATH_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_remove_color(key)               || any_change;
  any_change = StyleImpl::of(*this).raw_remove_color(key.to_volatile()) || any_change;
  
  if(any_change)
    notify_all();
}

void Style::remove(IntStyleOptionName n) {
  StyleOptionName key{n};
  RICHMATH_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_remove_int(key)               || any_change;
  any_change = StyleImpl::of(*this).raw_remove_int(key.to_volatile()) || any_change;
  
  if(any_change)
    notify_all();
}

void Style::remove(FloatStyleOptionName n) {
  StyleOptionName key{n};
  RICHMATH_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_remove_float(key)               || any_change;
  any_change = StyleImpl::of(*this).raw_remove_float(key.to_volatile()) || any_change;
  
  if(any_change)
    notify_all();
}

void Style::remove(LengthStyleOptionName n) {
  StyleOptionName key{n};
  RICHMATH_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_remove_length(key)               || any_change;
  any_change = StyleImpl::of(*this).raw_remove_length(key.to_volatile()) || any_change;
  
  if(any_change)
    notify_all();
}

void Style::remove(StringStyleOptionName n) {
  StyleOptionName key{n};
  RICHMATH_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_remove_string(key)               || any_change;
  any_change = StyleImpl::of(*this).raw_remove_string(key.to_volatile()) || any_change;
  
  if(any_change)
    notify_all();
}

void Style::remove(ObjectStyleOptionName n) {
  StyleOptionName key{n};
  RICHMATH_ASSERT(key.is_literal());
  
  bool any_change = false;
  any_change = StyleImpl::of(*this).raw_remove_expr(key)               || any_change;
  any_change = StyleImpl::of(*this).raw_remove_expr(key.to_volatile()) || any_change;
  
  if(any_change)
    notify_all();
}


bool Style::modifies_size(StyleOptionName style_name) {
  switch((int)style_name) {
    case AutoDelete:
    case ContinuousAction:
    case DebugColorizeChanges:
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
    case ShowContents:
    case StripOnInput:
    
    case LanguageCategory:
    case Method:
    case SectionLabel:
    case WindowTitle:
    
    case BoxID:
    case ButtonFunction:
    case ScriptSizeMultipliers:
    case TextShadow:
    case DefaultDuplicateSectionStyle:
    case DefaultNewSectionStyle:
    case DefaultReturnCreatedSectionStyle:
    case GeneratedSectionStyles:
      return false;
  }
  
  if(StyleImpl::is_for_color(style_name))
    return false;
  
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

StyleType Style::get_type(StyleOptionName n) {
  return StyleInformation::get_type(n);
}

Expr Style::get_current_style_value(FrontEndObject *obj, Expr item) {
  return StyleInformation::get_current_style_value(obj, std::move(item));
}

bool Style::put_current_style_value(FrontEndObject *obj, Expr item, Expr rhs) {
  return StyleInformation::put_current_style_value(obj, std::move(item), std::move(rhs));
}

bool Style::set_pmath(StyleOptionName n, Expr obj) {
  if(StyleImpl::of(*this).set_pmath(n, obj)) {
    notify_all();
    return true;
  }
  return false;
}

Expr Style::get_pmath(StyleOptionName key) const {
  // RICHMATH_ASSERT(key.is_literal())
  
  register_observer();
  Expr result = Symbol(richmath_System_Inherited);
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
  impl.emit_definition(AllowScriptLevelChange);
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
  impl.emit_definition(BoxID);
  impl.emit_definition(BoxRotation);
  impl.emit_definition(BoxTransformation);
  impl.emit_definition(ButtonBoxOptions);
  impl.emit_definition(ButtonData);
  impl.emit_definition(ButtonFrame);
  impl.emit_definition(ButtonFunction);
  impl.emit_definition(ButtonSource);
  impl.emit_definition(CapForm);
  impl.emit_definition(CharacterNameStyle);
  impl.emit_definition(ClosingAction);
  //impl.emit_definition(ColorForGraphics);
  impl.emit_definition(CommentStyle);
  impl.emit_definition(ContentPadding);
  impl.emit_definition(ContextMenu);
  impl.emit_definition(ContinuousAction);
  impl.emit_definition(Dashing);
  impl.emit_definition(DefaultDuplicateSectionStyle);
  impl.emit_definition(DefaultNewSectionStyle);
  impl.emit_definition(DefaultReturnCreatedSectionStyle);
  impl.emit_definition(Deinitialization);
  impl.emit_definition(DisplayFunction);
  impl.emit_definition(DockedSections);
  impl.emit_definition(DragDropContextMenu);
  impl.emit_definition(DynamicBoxOptions);
  impl.emit_definition(DynamicLocalBoxOptions);
  impl.emit_definition(DynamicLocalValues);
  impl.emit_definition(Editable);
  impl.emit_definition(Enabled);
  impl.emit_definition(Evaluatable);
  impl.emit_definition(EvaluationContext);
  impl.emit_definition(ExcessOrMissingArgumentStyle);
  impl.emit_definition(FillBoxOptions);
  impl.emit_definition(FillBoxWeight);
  impl.emit_definition(FontColor);
  impl.emit_definition(FontFamilies);
  impl.emit_definition(FontFeatures);
  impl.emit_definition(FontSize);
  impl.emit_definition(FontSlant);
  impl.emit_definition(FontWeight);
  impl.emit_definition(Frame);
  impl.emit_definition(FractionBoxOptions);
  impl.emit_definition(FrameBoxOptions);
  impl.emit_definition(FrameMarginLeft);
  impl.emit_definition(FrameStyle);
  impl.emit_definition(FrameTicks);
  impl.emit_definition(FunctionLocalVariableStyle);
  impl.emit_definition(FunctionNameStyle);
  impl.emit_definition(GeneratedSectionStyles);
  impl.emit_definition(GridBoxColumnSpacing);
  impl.emit_definition(GridBoxOptions);
  impl.emit_definition(GridBoxRowSpacing);
  impl.emit_definition(ImageSizeCommon);
  impl.emit_definition(ImageSizeAction);
  impl.emit_definition(ImplicitOperatorStyle);
  impl.emit_definition(Initialization);
  impl.emit_definition(InlineAutoCompletionStyle);
  impl.emit_definition(InlineSectionEditingStyle);
  impl.emit_definition(InputAliases);
  impl.emit_definition(InputAutoReplacements);
  impl.emit_definition(InputFieldBoxOptions);
  impl.emit_definition(InterpretationFunction);
  impl.emit_definition(JoinForm);
  impl.emit_definition(KeywordSymbolStyle);
  impl.emit_definition(LanguageCategory);
  impl.emit_definition(LineBreakWithin);
  impl.emit_definition(LocalScopeConflictStyle);
  impl.emit_definition(LocalVariableStyle);
  impl.emit_definition(Magnification);
  impl.emit_definition(MatchingBracketHighlightStyle);
  impl.emit_definition(MathFontFamily);
  impl.emit_definition(MenuCommandKey);
  impl.emit_definition(MenuSortingValue);
  impl.emit_definition(Method);
  impl.emit_definition(OccurenceHighlightStyle);
  impl.emit_definition(PaneBoxOptions);
  impl.emit_definition(PanelBoxOptions);
  impl.emit_definition(PatternVariableStyle);
  impl.emit_definition(Placeholder);
  impl.emit_definition(PlotRange);
  impl.emit_definition(PlotRangePaddingLeft);
  impl.emit_definition(PointSize);
  impl.emit_definition(ReturnCreatesNewSection);
  impl.emit_definition(Saveable);
  impl.emit_definition(ScriptLevel);
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
  impl.emit_definition(SectionInsertionPointColor);
  impl.emit_definition(SectionMarginLeft);
  impl.emit_definition(SectionLabel);
  impl.emit_definition(SectionLabelAutoDelete);
  impl.emit_definition(Selectable);
  impl.emit_definition(SetterBoxOptions);
  impl.emit_definition(ShowAutoStyles);
  impl.emit_definition(ShowContents);
  impl.emit_definition(ShowSectionBracket);
  impl.emit_definition(ShowStringCharacters);
  impl.emit_definition(SliderBoxOptions);
  impl.emit_definition(StripOnInput);
  impl.emit_definition(StringStyle);
  impl.emit_definition(StyleDefinitions);
  impl.emit_definition(SurdForm);
  impl.emit_definition(SymbolShadowingStyle);
  impl.emit_definition(SynchronousUpdating);
  impl.emit_definition(SyntaxErrorStyle);
  impl.emit_definition(SyntaxForm);
  impl.emit_definition(TemplateBoxOptions);
  impl.emit_definition(TextShadow);
  impl.emit_definition(Thickness);
  impl.emit_definition(Ticks);
  impl.emit_definition(Tooltip);
  impl.emit_definition(TrackedSymbols);
  impl.emit_definition(UndefinedSymbolStyle);
  impl.emit_definition(UnknownOptionStyle);
  impl.emit_definition(UnsavedVariables);
  impl.emit_definition(Visible);
  impl.emit_definition(WholeSectionGroupOpener);
  impl.emit_definition(WindowFrame);
  impl.emit_definition(WindowTitle);
  
  Expr e;
  if(get(UnknownOptions, &e)) {
    Expr rules = Evaluate(Call(Symbol(richmath_System_DownRules), e));
    
    for(size_t i = 1; i <= rules.expr_length(); ++i) {
      Expr rule = rules[i];
      Expr lhs = rule[1]; // HoldPattern(symbol(x))
      lhs = lhs[1];       //             symbol(x)
      lhs = lhs[1];       //                    x
      
      if(rule[2].is_evaluated())
        rule.set(0, Symbol(richmath_System_Rule));
        
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
      void reload(Expr expr);
      void add(Expr expr);
      bool update_dynamic(SharedPtr<Style> s, StyledObject *parent);
      
      static void add_remove_stylesheet(int delta);
      
    private:
      static Hashset<Expr> currently_loading;
      
      void internal_add(Expr expr);
      void add_section(Expr expr);
      
    private:
      Stylesheet &self;
  };
  
  Hashset<Expr> StylesheetImpl::currently_loading;
}

Stylesheet::Stylesheet() 
: Shareable(),
  _limbo_next(nullptr)
{
  StylesheetImpl::add_remove_stylesheet(+1);
}

Stylesheet::~Stylesheet() {
  users.clear();
  for(auto &e : used_stylesheets.entries())
    e.key->users.remove(id());
  
  used_stylesheets.clear();
  unregister();
  StylesheetImpl::add_remove_stylesheet(-1);
}

Stylesheet::IterBoxReferences Stylesheet::find_registered_box(Expr box_id) {
  return box_registry[box_id];
}

void Stylesheet::update_box_registry(StyledObject *obj) {
  auto s = obj->own_style();
  
  Expr old_box_id = obj->get_own_style(InternalRegisteredBoxID);
  Expr new_box_id = obj->get_own_style(BoxID);
  
  if(old_box_id != new_box_id) {
    if(old_box_id) {
      bool did_remove = box_registry.remove(old_box_id, obj->id());
      if(!did_remove) {
        pmath_debug_print("[update_box_registry: did not find box #%d = %p by its old BoxID", (int)(intptr_t)FrontEndReference::unsafe_cast_to_pointer(obj->id()), obj);
        pmath_debug_print_object(" -> ", old_box_id.get(), "]\n");
      }
    }
    
    if(new_box_id) {
      bool did_insert = box_registry.insert(new_box_id, obj->id());
      if(!did_insert) {
        pmath_debug_print("[update_box_registry: box #%d = %p already known by its new BoxID", (int)(intptr_t)FrontEndReference::unsafe_cast_to_pointer(obj->id()), obj);
        pmath_debug_print_object(" -> ", new_box_id.get(), "]\n");
      }
      obj->has_box_id(true);
    }
    else
      obj->has_box_id(false);
    
    if(new_box_id) {
      StyleImpl::of(*s.ptr()).raw_set_expr(InternalRegisteredBoxID, new_box_id);
      StyleImpl::of(*s.ptr()).raw_set_int(InternalRegisteredBoxReference, (int)(intptr_t)FrontEndReference::unsafe_cast_to_pointer(obj->id()));
    }
    else {
      StyleImpl::of(*s.ptr()).raw_remove(InternalRegisteredBoxID);
      StyleImpl::of(*s.ptr()).raw_remove(InternalRegisteredBoxReference);
    }
    
    if(old_box_id) {
      pmath_debug_print("[update_box_registry: unregistered box #%d = %p for old BoxID", (int)(intptr_t)FrontEndReference::unsafe_cast_to_pointer(obj->id()), obj);
      pmath_debug_print_object(" -> ", old_box_id.get(), "]\n");
    }
    
    if(new_box_id) {
      pmath_debug_print("[update_box_registry: box #%d = %p registered for BoxID", (int)(intptr_t)FrontEndReference::unsafe_cast_to_pointer(obj->id()), obj);
      pmath_debug_print_object(" -> ", new_box_id.get(), "]\n");
    }
  }
}

/*void Stylesheet::unregister_box(StyledObject *obj) {
  if(!obj)
    return;
  
  auto style = obj->own_style();
  if(!style) {
    if(obj->has_box_id()) {
      pmath_debug_print("[no BoxID found for box #%d = %p]\n", (int)(intptr_t)FrontEndReference::unsafe_cast_to_pointer(obj->id()), obj);
    }
    return;
  }
  
  if(Expr box_id = obj->get_own_style(InternalRegisteredBoxID)) {
    bool did_remove = box_registry.remove(box_id, obj->id());
    if(!did_remove) {
      pmath_debug_print("[did not find box #%d = %p by its old BoxID", (int)(intptr_t)FrontEndReference::unsafe_cast_to_pointer(obj->id()), obj);
      pmath_debug_print_object(" -> ", box_id.get(), "]\n");
    }
  }
  else {
    pmath_debug_print("[no BoxID found for box #%d = %p]\n", (int)(intptr_t)FrontEndReference::unsafe_cast_to_pointer(obj->id()), obj);
  }
}*/

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
                        Parse("Get(`1`, Head->HoldComplete)", Application::stylesheet_path_base + expr),
                        Application::button_timeout);
                        
    if(held_boxes.expr_length() == 1 && held_boxes[0] == richmath_System_HoldComplete) {
      stylesheet = new Stylesheet();
      stylesheet->base = Stylesheet::Default->base;
      stylesheet->register_as(expr);
      stylesheet->reload(held_boxes[1]);
      return stylesheet;
    }
    
    return nullptr;
  }
  
  if(expr[0] == richmath_System_Document) {
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

String Stylesheet::path_for_name(Expr name) {
  if(String str = name) {
    if(FileSystem::is_filename_without_directory(name)) {
      return FileSystem::file_name_join(Application::stylesheet_path_base, name);
    }
  }
  
  return {}; 
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
static bool Stylesheet_get_simple(Stylesheet *self, SharedPtr<Style> s, N n, T *value) {
  for(int count = 20; count && s; --count) {
    if(s->get(n, value))
      return true;
      
    s = self->find_parent_style(s);
  }
  
  return false;
}

bool Stylesheet::get(SharedPtr<Style> s, ColorStyleOptionName n, Color *value) {
  return Stylesheet_get_simple(this, s, n, value);
}

bool Stylesheet::get(SharedPtr<Style> s, IntStyleOptionName n, int *value) {
  return Stylesheet_get_simple(this, s, n, value);
}

bool Stylesheet::get(SharedPtr<Style> s, FloatStyleOptionName n, float *value) {
  return Stylesheet_get_simple(this, s, n, value);
}

bool Stylesheet::get(SharedPtr<Style> s, LengthStyleOptionName n, Length *value) {
  return Stylesheet_get_simple(this, s, n, value);
}

bool Stylesheet::get(SharedPtr<Style> s, StringStyleOptionName n, String *value) {
  return Stylesheet_get_simple(this, s, n, value);
}

bool Stylesheet::get(SharedPtr<Style> s, ObjectStyleOptionName n, Expr *value) {
  if(StyleInformation::get_type(n) == StyleType::AnyFlatList) {
    Expr result = get_pmath(s, n);
    if(result != richmath_System_Inherited) {
      *value = std::move(result);
      return true;
    }
    return false;
  }
  return Stylesheet_get_simple(this, s, n, value);
}

Expr Stylesheet::get_pmath(SharedPtr<Style> s, StyleOptionName n) {
  Expr result = Symbol(richmath_System_Inherited);
  
  for(int count = 20; count && s && Style::contains_inherited(result); --count) {
    result = Style::merge_style_values(n, std::move(result), s->get_pmath(n));
    
    s = find_parent_style(s);
  }

  return result;
}

bool Stylesheet::update_dynamic(SharedPtr<Style> s, StyledObject *parent) {
  return StylesheetImpl(*this).update_dynamic(s, parent);
}

//} ... class Stylesheet


//{ class StylesheetImpl ...

void StylesheetImpl::reload(Expr expr) {
  self.styles.clear();
  self.used_stylesheets.clear();
  self.users.clear();
  self._loaded_definition = expr;
  add(expr);
}

void StylesheetImpl::add(Expr expr) {
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

bool StylesheetImpl::update_dynamic(SharedPtr<Style> s, StyledObject *parent) {
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
    
    if(!has_parent_pending_dynamic) {
      Stylesheet::update_box_registry(parent);
      return false;
    }
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
  
  if(dynamic_styles.size() == 0) {
    Stylesheet::update_box_registry(parent);
    return false;
  }
    
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
    if(e.value != richmath_System_DollarAborted && !Dynamic::is_dynamic(e.value)) {
      StyleOptionName key = e.key.to_volatile(); // = e.key.to_literal().to_volatile()
      s_impl.set_pmath(key, e.value);
    }
  }
  
  Stylesheet::update_box_registry(parent);
  
  parent->on_style_changed(resize);
  return true;
}

void StylesheetImpl::internal_add(Expr expr) {
  // TODO: detect stack overflow/infinite recursion
  
  while(expr.is_expr()) {
    if(expr[0] == richmath_System_Document) {
      expr = expr[1];
      continue;
    }
    
    if(expr[0] == richmath_System_SectionGroup) {
      expr = expr[1];
      continue;
    }
    
    if(expr[0] == richmath_System_List) {
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

void StylesheetImpl::add_section(Expr expr) {
  Expr name = expr[1];
  if(name[0] == richmath_System_StyleData) {
    Expr data = name[1];
    if(data.is_string()) {
      Expr options(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
      if(options.is_null())
        return;
      
      Expr data_opts(pmath_options_extract_ex(name.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
      if(data_opts.is_null())
        return;
      
      Expr base_sd(pmath_option_value(richmath_System_StyleData, richmath_System_StyleDefinitions, data_opts.get()));
      
      if(base_sd == richmath_System_Automatic) {
        if(SharedPtr<Style> *style_ptr = self.styles.search(String(data))) {
          (*style_ptr)->add_pmath(options);
          return;
        }
      }
      else if(base_sd[0] == richmath_System_StyleData) {
        if(SharedPtr<Style> *base_style_ptr = self.styles.search(String(base_sd[1]))) {
          SharedPtr<Style> style = new Style();
          style->merge(*base_style_ptr);
          style->add_pmath(options);
          self.styles.set(String(data), style);
          return;
        }
      }
//            else if(base_sd != richmath_System_None)
//              return;
      
      SharedPtr<Style> style = new Style(options);
      self.styles.set(String(data), style);
      
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

void StylesheetImpl::add_remove_stylesheet(int delta) {
  static int total = 0;
  
  if(total == 0) {
    RICHMATH_ASSERT(delta > 0);
    
    // init ...
  }
  
  total += delta;
  RICHMATH_ASSERT(total >= 0);
  
  if(total == 0) {
    box_registry.clear();
  }
}

//} ... class StylesheetImpl

//{ class StyleInformation ...

void StyleInformation::add_style() {
  if(_num_styles++ == 0) {
    _name_to_key.default_value = StyleOptionName{ -1};
    _key_to_type.default_value = StyleType::None;
    
    add_ruleset_head(ButtonBoxOptions,              Symbol( richmath_System_ButtonBoxOptions));
    add_ruleset_head(CharacterNameStyle,            strings::CharacterNameStyle);
    add_ruleset_head(CommentStyle,                  strings::CommentStyle);
    add_ruleset_head(DockedSections,                Symbol( richmath_System_DockedSections));
    add_ruleset_head(DynamicBoxOptions,             Symbol( richmath_System_DynamicBoxOptions));
    add_ruleset_head(DynamicLocalBoxOptions,        Symbol( richmath_System_DynamicLocalBoxOptions));
    add_ruleset_head(ExcessOrMissingArgumentStyle,  strings::ExcessOrMissingArgumentStyle);
    add_ruleset_head(FillBoxOptions,                Symbol( richmath_System_FillBoxOptions));
    add_ruleset_head(FractionBoxOptions,            Symbol( richmath_System_FractionBoxOptions));
    add_ruleset_head(FrameBoxOptions,               Symbol( richmath_System_FrameBoxOptions));
    add_ruleset_head(FunctionLocalVariableStyle,    strings::FunctionLocalVariableStyle);
    add_ruleset_head(FunctionNameStyle,             strings::FunctionNameStyle);
    add_ruleset_head(GridBoxOptions,                Symbol( richmath_System_GridBoxOptions));
    add_ruleset_head(ImplicitOperatorStyle,         strings::ImplicitOperatorStyle);
    add_ruleset_head(InlineAutoCompletionStyle,     strings::InlineAutoCompletionStyle);
    add_ruleset_head(InlineSectionEditingStyle,     strings::InlineSectionEditingStyle);
    add_ruleset_head(InputFieldBoxOptions,          Symbol( richmath_System_InputFieldBoxOptions));
    add_ruleset_head(KeywordSymbolStyle,            strings::KeywordSymbolStyle);
    add_ruleset_head(LocalScopeConflictStyle,       strings::LocalScopeConflictStyle);
    add_ruleset_head(LocalVariableStyle,            strings::LocalVariableStyle);
    add_ruleset_head(MatchingBracketHighlightStyle, strings::MatchingBracketHighlightStyle);
    add_ruleset_head(OccurenceHighlightStyle,       strings::OccurenceHighlightStyle);
    add_ruleset_head(PaneBoxOptions,                Symbol( richmath_System_PaneBoxOptions));
    add_ruleset_head(PanelBoxOptions,               Symbol( richmath_System_PanelBoxOptions));
    add_ruleset_head(PatternVariableStyle,          strings::PatternVariableStyle);
    add_ruleset_head(SetterBoxOptions,              Symbol( richmath_System_SetterBoxOptions));
    add_ruleset_head(SliderBoxOptions,              Symbol( richmath_System_SliderBoxOptions));
    add_ruleset_head(StringStyle,                   strings::StringStyle);
    add_ruleset_head(SymbolShadowingStyle,          strings::SymbolShadowingStyle);
    add_ruleset_head(SyntaxErrorStyle,              strings::SyntaxErrorStyle);
    add_ruleset_head(TemplateBoxOptions,            Symbol( richmath_System_TemplateBoxOptions));
    add_ruleset_head(UndefinedSymbolStyle,          strings::UndefinedSymbolStyle);
    add_ruleset_head(UnknownOptionStyle,            strings::UnknownOptionStyle);
    
    {
      SharedPtr<EnumStyleConverter> converter{new ButtonFrameStyleConverter};
      add_enum(
        ButtonFrame, 
        Symbol(richmath_System_ButtonFrame), 
        converter);
      add_enum(
        ButtonBoxDefaultButtonFrame, 
        List(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_ButtonFrame)),
        converter);
      add_enum(
        SetterBoxDefaultButtonFrame, 
        List(Symbol(richmath_System_SetterBoxOptions), Symbol(richmath_System_ButtonFrame)),
        converter);
    }
    
    {
      SharedPtr<EnumStyleConverter> converter{new ButtonSourceStyleConverter};
      add_enum(
        ButtonSource, 
        Symbol(richmath_System_ButtonSource), 
        converter);
      add_enum(
        ButtonBoxDefaultButtonSource, 
        List(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_ButtonSource)),
        converter);
    }
    
    {
      SharedPtr<EnumStyleConverter> converter{new CapFormStyleConverter};
      add_enum(
        CapForm, 
        Symbol(richmath_System_CapForm), 
        converter);
    }
    
    {
      SharedPtr<EnumStyleConverter> converter{new ImageSizeActionStyleConverter};
      add_enum(
        ImageSizeAction, 
        Symbol(richmath_System_ImageSizeAction), 
        converter);
      add_enum(
        PaneBoxDefaultImageSizeAction, 
        List(Symbol(richmath_System_PaneBoxOptions), Symbol(richmath_System_ImageSizeAction)),
        converter);
    }
    
    add_enum(ClosingAction,     strings::ClosingAction,                     new ClosingActionStyleConverter);
    add_enum(ControlPlacement,  Symbol( richmath_System_ControlPlacement),  new ControlPlacementStyleConverter);
    add_enum(FontSlant,         Symbol( richmath_System_FontSlant),         new FontSlantStyleConverter);
    add_enum(FontWeight,        Symbol( richmath_System_FontWeight),        new FontWeightStyleConverter);
    add_enum(MenuCommandKey,    Symbol( richmath_System_MenuCommandKey),    new MenuCommandKeyStyleConverter);
    add_enum(MenuSortingValue,  Symbol( richmath_System_MenuSortingValue),  new MenuSortingValueStyleConverter);
    add_enum(RemovalConditions, Symbol( richmath_System_RemovalConditions), new RemovalConditionsStyleConverter);
    add_enum(WindowFrame,       Symbol( richmath_System_WindowFrame),       new WindowFrameStyleConverter);
    
    add(StyleType::Color,           Background,                          Symbol( richmath_System_Background));
    add(StyleType::Color,           ColorForGraphics,                    strings::Color);
    add(StyleType::Color,           FontColor,                           Symbol( richmath_System_FontColor));
    add(StyleType::Color,           SectionFrameColor,                   Symbol( richmath_System_SectionFrameColor));
    
    add(StyleType::Color,           CharacterNameSyntaxColor,            List(strings::CharacterNameStyle,            Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           CommentSyntaxColor,                  List(strings::CommentStyle,                  Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           ExcessOrMissingArgumentSyntaxColor,  List(strings::ExcessOrMissingArgumentStyle,  Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           FunctionLocalVariableSyntaxColor,    List(strings::FunctionLocalVariableStyle,    Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           FunctionNameSyntaxColor,             List(strings::FunctionNameStyle,             Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           ImplicitOperatorSyntaxColor,         List(strings::ImplicitOperatorStyle,         Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           KeywordSymbolSyntaxColor,            List(strings::KeywordSymbolStyle,            Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           LocalScopeConflictSyntaxColor,       List(strings::LocalScopeConflictStyle,       Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           LocalVariableSyntaxColor,            List(strings::LocalVariableStyle,            Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           PatternVariableSyntaxColor,          List(strings::PatternVariableStyle,          Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           SectionInsertionPointColor,          strings::SectionInsertionPointColor);
    add(StyleType::Color,           StringSyntaxColor,                   List(strings::StringStyle,                   Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           SymbolShadowingSyntaxColor,          List(strings::SymbolShadowingStyle,          Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           SyntaxErrorColor,                    List(strings::SyntaxErrorStyle,              Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           UndefinedSymbolSyntaxColor,          List(strings::UndefinedSymbolStyle,          Symbol(richmath_System_FontColor)));
    add(StyleType::Color,           UnknownOptionSyntaxColor,            List(strings::UnknownOptionStyle,            Symbol(richmath_System_FontColor)));
    
    add(StyleType::Color,           InlineAutoCompletionBackgroundColor, List(strings::InlineAutoCompletionStyle,      Symbol(richmath_System_Background)));
    add(StyleType::Color,           InlineSectionEditingBackgroundColor, List(strings::InlineSectionEditingStyle,      Symbol(richmath_System_Background)));
    add(StyleType::Color,           MatchingBracketBackgroundColor,      List(strings::MatchingBracketHighlightStyle,  Symbol(richmath_System_Background)));
    add(StyleType::Color,           OccurenceBackgroundColor,            List(strings::OccurenceHighlightStyle,        Symbol(richmath_System_Background)));
    add(StyleType::Color,           FrameBoxDefaultBackground,           List(Symbol(richmath_System_FrameBoxOptions), Symbol(richmath_System_Background)));
    
    add(StyleType::Bool,            AllowScriptLevelChange,           Symbol( richmath_System_AllowScriptLevelChange));
    add(StyleType::AutoBool,        Antialiasing,                     Symbol( richmath_System_Antialiasing));
    add(StyleType::Bool,            AutoDelete,                       Symbol( richmath_System_AutoDelete));
    add(StyleType::Bool,            AutoNumberFormating,              Symbol( richmath_System_AutoNumberFormating));
    add(StyleType::Bool,            AutoSpacing,                      Symbol( richmath_System_AutoSpacing));
    add(StyleType::Bool,            ContentPadding,                   Symbol( richmath_System_ContentPadding));
    add(StyleType::Bool,            ContinuousAction,                 Symbol( richmath_System_ContinuousAction));
    add(StyleType::Bool,            DebugColorizeChanges,             String("DebugColorizeChanges"));
    add(StyleType::Bool,            DebugFollowMouse,                 String("DebugFollowMouse"));
    add(StyleType::Bool,            DebugSelectionBounds,             String("DebugSelectionBounds"));
    add(StyleType::Bool,            Editable,                         Symbol( richmath_System_Editable));
    add(StyleType::AutoBool,        Enabled,                          Symbol( richmath_System_Enabled));
    add(StyleType::Bool,            Evaluatable,                      Symbol( richmath_System_Evaluatable));
    add(StyleType::Bool,            LineBreakWithin,                  Symbol( richmath_System_LineBreakWithin));
    add(StyleType::Bool,            Placeholder,                      Symbol( richmath_System_Placeholder));
    add(StyleType::Bool,            ReturnCreatesNewSection,          Symbol( richmath_System_ReturnCreatesNewSection));
    add(StyleType::Bool,            Saveable,                         Symbol( richmath_System_Saveable));
    add(StyleType::Integer,         ScriptLevel,                      Symbol( richmath_System_ScriptLevel));
    add(StyleType::Bool,            SectionEditDuplicate,             Symbol( richmath_System_SectionEditDuplicate));
    add(StyleType::Bool,            SectionEditDuplicateMakesCopy,    Symbol( richmath_System_SectionEditDuplicateMakesCopy));
    add(StyleType::Bool,            SectionGenerated,                 Symbol( richmath_System_SectionGenerated));
    add(StyleType::Bool,            SectionLabelAutoDelete,           Symbol( richmath_System_SectionLabelAutoDelete));
    add(StyleType::AutoBool,        Selectable,                       Symbol( richmath_System_Selectable));
    add(StyleType::Bool,            ShowAutoStyles,                   Symbol( richmath_System_ShowAutoStyles));
    add(StyleType::Bool,            ShowContents,                     Symbol( richmath_System_ShowContents));
    add(StyleType::AutoBool,        ShowSectionBracket,               Symbol( richmath_System_ShowSectionBracket));
    add(StyleType::Bool,            ShowStringCharacters,             Symbol( richmath_System_ShowStringCharacters));
    add(StyleType::Bool,            StripOnInput,                     Symbol( richmath_System_StripOnInput));
    add(StyleType::Bool,            SurdForm,                         Symbol( richmath_System_SurdForm));
    add(StyleType::AutoBool,        SynchronousUpdating,              Symbol( richmath_System_SynchronousUpdating));
    add(StyleType::Bool,            Visible,                          Symbol( richmath_System_Visible));
    add(StyleType::Bool,            WholeSectionGroupOpener,          Symbol( richmath_System_WholeSectionGroupOpener));
    
    add(StyleType::AutoBool,        ButtonBoxDefaultContentPadding,   List(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_ContentPadding)));
    add(StyleType::AutoBool,        ButtonBoxDefaultEnabled,          List(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_Enabled)));
    
    add(StyleType::AutoBool,        DynamicBoxDefaultSelectable,          List(Symbol(richmath_System_DynamicBoxOptions), Symbol(richmath_System_Selectable)));
    add(StyleType::AutoBool,        DynamicBoxDefaultSynchronousUpdating, List(Symbol(richmath_System_DynamicBoxOptions), Symbol(richmath_System_SynchronousUpdating)));
    
    add(StyleType::Bool,            FractionBoxDefaultAllowScriptLevelChange, List(Symbol(richmath_System_FractionBoxOptions), Symbol( richmath_System_AllowScriptLevelChange)));
    
    add(StyleType::AutoBool,        FrameBoxDefaultContentPadding,    List(Symbol(richmath_System_FrameBoxOptions), Symbol(richmath_System_ContentPadding)));
    
    add(StyleType::Bool,            GridBoxDefaultAllowScriptLevelChange, List(Symbol(richmath_System_GridBoxOptions), Symbol( richmath_System_AllowScriptLevelChange)));

    add(StyleType::Bool,            InputFieldBoxDefaultContinuousAction, List(Symbol(richmath_System_InputFieldBoxOptions), Symbol( richmath_System_ContinuousAction)));
    add(StyleType::Bool,            InputFieldBoxDefaultEnabled,          List(Symbol(richmath_System_InputFieldBoxOptions), Symbol( richmath_System_Enabled)));
    
    add(StyleType::Bool,            PaneBoxDefaultLineBreakWithin,        List(Symbol(richmath_System_PaneBoxOptions), Symbol( richmath_System_LineBreakWithin)));
    
    add(StyleType::AutoBool,        PanelBoxDefaultContentPadding,    List(Symbol(richmath_System_PanelBoxOptions), Symbol(richmath_System_ContentPadding)));
    add(StyleType::Bool,            PanelBoxDefaultEnabled,           List(Symbol(richmath_System_PanelBoxOptions), Symbol(richmath_System_Enabled)));
    
    add(StyleType::AutoBool,        SetterBoxDefaultContentPadding,   List(Symbol(richmath_System_SetterBoxOptions), Symbol(richmath_System_ContentPadding)));
    add(StyleType::Bool,            SetterBoxDefaultEnabled,          List(Symbol(richmath_System_SetterBoxOptions), Symbol(richmath_System_Enabled)));
    
    add(StyleType::Bool,            ContinuousAction,                 List(Symbol(richmath_System_SliderBoxOptions), Symbol(richmath_System_ContinuousAction)));
    add(StyleType::AutoBool,        SliderBoxDefaultEnabled,          List(Symbol(richmath_System_SliderBoxOptions), Symbol(richmath_System_Enabled)));
    
    add(StyleType::AutoPositive,    AspectRatio,                      Symbol( richmath_System_AspectRatio));
    add(StyleType::Number,          FillBoxWeight,                    Symbol( richmath_System_FillBoxWeight));
    add(StyleType::Length,          FontSize,                         Symbol( richmath_System_FontSize));
    add(StyleType::Number,          GridBoxColumnSpacing,             Symbol( richmath_System_GridBoxColumnSpacing));
    add(StyleType::Number,          GridBoxRowSpacing,                Symbol( richmath_System_GridBoxRowSpacing));
    add(StyleType::Number,          Magnification,                    Symbol( richmath_System_Magnification));
    
    add(StyleType::Number,          InlineAutoCompletionHighlightOpacity, List(strings::InlineAutoCompletionStyle,     Symbol(richmath_System_Opacity)));
    add(StyleType::Number,          InlineSectionEditingHighlightOpacity, List(strings::InlineSectionEditingStyle,     Symbol(richmath_System_Opacity)));
    add(StyleType::Number,          MatchingBracketHighlightOpacity,      List(strings::MatchingBracketHighlightStyle, Symbol(richmath_System_Opacity)));
    add(StyleType::Number,          OccurenceHighlightOpacity,            List(strings::OccurenceHighlightStyle,       Symbol(richmath_System_Opacity)));
    
    add(StyleType::Size,            ImageSizeCommon,                  Symbol( richmath_System_ImageSize));
    // ImageSizeHorizontal
    // ImageSizeVertical
    
    add(StyleType::Size,            PaneBoxDefaultImageSizeCommon,    List(Symbol(richmath_System_PaneBoxOptions), Symbol(richmath_System_ImageSize)));
    // PaneBoxDefaultImageSizeHorizontal
    // PaneBoxDefaultImageSizeVertical
    
    add(StyleType::Margin,          FrameMarginLeft,                  Symbol( richmath_System_FrameMargins));
    // SectionFrameMarginRight
    // SectionFrameMarginTop
    // SectionFrameMarginBottom
    add(StyleType::Margin,          FrameBoxDefaultFrameMarginLeft,   List(Symbol(richmath_System_FrameBoxOptions), Symbol( richmath_System_FrameMargins)));
    // FrameBoxDefaultSectionFrameMarginRight
    // FrameBoxDefaultSectionFrameMarginTop
    // FrameBoxDefaultSectionFrameMarginBottom
    add(StyleType::Margin,          PlotRangePaddingLeft,             Symbol( richmath_System_PlotRangePadding));
    // PlotRangePaddingRight
    // PlotRangePaddingTop
    // PlotRangePaddingBottom
    add(StyleType::Margin,          SectionMarginLeft,                Symbol( richmath_System_SectionMargins));
    // SectionMarginRight
    // SectionMarginTop
    // SectionMarginBottom
    add(StyleType::Margin,          SectionFrameLeft,                 Symbol( richmath_System_SectionFrame));
    // SectionFrameRight
    // SectionFrameTop
    // SectionFrameBottom
    add(StyleType::Margin,          SectionFrameMarginLeft,           Symbol( richmath_System_SectionFrameMargins));
    // SectionFrameMarginRight
    // SectionFrameMarginTop
    // SectionFrameMarginBottom
    add(StyleType::Margin,          SectionFrameLabelMarginLeft,      Symbol( richmath_System_SectionFrameLabelMargins));
    // SectionFrameLabelMarginRight
    // SectionFrameLabelMarginTop
    // SectionFrameLabelMarginBottom
    add(StyleType::Number,          SectionGroupPrecedence,           Symbol( richmath_System_SectionGroupPrecedence));
    
    add(StyleType::Length,          PointSize,                        Symbol( richmath_System_PointSize));
    add(StyleType::Length,          Thickness,                        Symbol( richmath_System_Thickness));
    
    add(StyleType::Number,          FillBoxDefaultFillBoxWeight,      List(Symbol(richmath_System_FillBoxOptions), Symbol( richmath_System_FillBoxWeight)));
    add(StyleType::Bool,            FillBoxDefaultStripOnInput,       List(Symbol(richmath_System_FillBoxOptions), Symbol( richmath_System_StripOnInput)));
    
    add(StyleType::String,          BaseStyleName,                    Symbol( richmath_System_BaseStyle));
    add(StyleType::String,          Method,                           Symbol( richmath_System_Method));
    add(StyleType::String,          LanguageCategory,                 Symbol( richmath_System_LanguageCategory));
    add(StyleType::String,          SectionLabel,                     Symbol( richmath_System_SectionLabel));
    add(StyleType::String,          WindowTitle,                      Symbol( richmath_System_WindowTitle));
    
    add(StyleType::String,          ButtonBoxDefaultMethod,           List(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_Method)));
    
    add(StyleType::Any,             Appearance,                       Symbol( richmath_System_Appearance));
    add(StyleType::Any,             Axes,                             Symbol( richmath_System_Axes));
    add(StyleType::Any,             AxesOrigin,                       Symbol( richmath_System_AxesOrigin));
    add(StyleType::Any,             BaselinePosition,                 Symbol( richmath_System_BaselinePosition));
    add(StyleType::Any,             BorderRadius,                     Symbol( richmath_System_BorderRadius));
    add(StyleType::Any,             BoxID,                            Symbol( richmath_System_BoxID));
    add(StyleType::Any,             BoxRotation,                      Symbol( richmath_System_BoxRotation));
    add(StyleType::Any,             BoxTransformation,                Symbol( richmath_System_BoxTransformation));
    add(StyleType::Any,             ButtonData,                       Symbol( richmath_System_ButtonData));
    add(StyleType::Any,             ButtonFunction,                   Symbol( richmath_System_ButtonFunction));
    add(StyleType::Any,             Dashing,                          Symbol( richmath_System_Dashing));
    add(StyleType::Any,             DefaultDuplicateSectionStyle,     Symbol( richmath_System_DefaultDuplicateSectionStyle));
    add(StyleType::Any,             DefaultNewSectionStyle,           Symbol( richmath_System_DefaultNewSectionStyle));
    add(StyleType::Any,             DefaultReturnCreatedSectionStyle, Symbol( richmath_System_DefaultReturnCreatedSectionStyle));
    add(StyleType::Any,             Deinitialization,                 Symbol( richmath_System_Deinitialization));
    add(StyleType::Any,             DisplayFunction,                  Symbol( richmath_System_DisplayFunction));
    add(StyleType::Any,             DynamicLocalValues,               Symbol( richmath_System_DynamicLocalValues));
    add(StyleType::Any,             EvaluationContext,                Symbol( richmath_System_EvaluationContext));
    add(StyleType::Any,             FontFeatures,                     Symbol( richmath_System_FontFeatures));
    add(StyleType::Any,             Frame,                            Symbol( richmath_System_Frame));
    add(StyleType::Any,             FrameStyle,                       Symbol( richmath_System_FrameStyle));
    add(StyleType::Any,             FrameTicks,                       Symbol( richmath_System_FrameTicks));
    add(StyleType::Any,             GeneratedSectionStyles,           Symbol( richmath_System_GeneratedSectionStyles));
    add(StyleType::Any,             Initialization,                   Symbol( richmath_System_Initialization));
    add(StyleType::Any,             InterpretationFunction,           Symbol( richmath_System_InterpretationFunction));
    add(StyleType::Any,             JoinForm,                         Symbol( richmath_System_JoinForm));
    add(StyleType::Any,             MathFontFamily,                   Symbol( richmath_System_MathFontFamily));
    add(StyleType::Any,             PlotRange,                        Symbol( richmath_System_PlotRange));
    add(StyleType::Any,             ScriptSizeMultipliers,            Symbol( richmath_System_ScriptSizeMultipliers));
    add(StyleType::Any,             SectionDingbat,                   Symbol( richmath_System_SectionDingbat));
    add(StyleType::Any,             SectionEvaluationFunction,        Symbol( richmath_System_SectionEvaluationFunction));
    add(StyleType::Any,             StyleDefinitions,                 Symbol( richmath_System_StyleDefinitions));
    add(StyleType::Any,             SyntaxForm,                       Symbol( richmath_System_SyntaxForm));
    add(StyleType::Any,             TextShadow,                       Symbol( richmath_System_TextShadow));
    add(StyleType::Any,             Ticks,                            Symbol( richmath_System_Ticks));
    add(StyleType::Any,             Tooltip,                          Symbol( richmath_System_Tooltip));
    add(StyleType::Any,             TrackedSymbols,                   Symbol( richmath_System_TrackedSymbols));
    add(StyleType::Any,             UnsavedVariables,                 Symbol( richmath_System_UnsavedVariables));
    
    add(StyleType::AnyFlatList, ContextMenu,               Symbol( richmath_System_ContextMenu));
    add(StyleType::AnyFlatList, DragDropContextMenu,       strings::DragDropContextMenu);
    
    add(StyleType::AnyFlatList, DockedSectionsTop,         List(Symbol(richmath_System_DockedSections), String("Top")));
    add(StyleType::AnyFlatList, DockedSectionsTopGlass,    List(Symbol(richmath_System_DockedSections), String("TopGlass")));
    add(StyleType::AnyFlatList, DockedSectionsBottom,      List(Symbol(richmath_System_DockedSections), String("Bottom")));
    add(StyleType::AnyFlatList, DockedSectionsBottomGlass, List(Symbol(richmath_System_DockedSections), String("BottomGlass")));
    
    add(StyleType::AnyFlatList, FontFamilies,              Symbol( richmath_System_FontFamily));
    add(StyleType::AnyFlatList, InputAliases,              Symbol( richmath_System_InputAliases));
    add(StyleType::AnyFlatList, InputAutoReplacements,     Symbol( richmath_System_InputAutoReplacements));
    
    add(StyleType::Any, ButtonBoxDefaultAppearance,       List(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_Appearance)));
    add(StyleType::Any, ButtonBoxDefaultBaselinePosition, List(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_BaselinePosition)));
    add(StyleType::Any, ButtonBoxDefaultButtonData,       List(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_ButtonData)));
    add(StyleType::Any, ButtonBoxDefaultButtonFunction,   List(Symbol(richmath_System_ButtonBoxOptions), Symbol(richmath_System_ButtonFunction)));
    
    add(StyleType::Any, DynamicBoxDefaultDeinitialization, List(Symbol(richmath_System_DynamicBoxOptions), Symbol(richmath_System_Deinitialization)));
    add(StyleType::Any, DynamicBoxDefaultInitialization,   List(Symbol(richmath_System_DynamicBoxOptions), Symbol(richmath_System_Initialization)));
    add(StyleType::Any, DynamicBoxDefaultTrackedSymbols,   List(Symbol(richmath_System_DynamicBoxOptions), Symbol(richmath_System_TrackedSymbols)));
    
    add(StyleType::Any, DynamicLocalBoxDefaultDeinitialization,   List(Symbol(richmath_System_DynamicLocalBoxOptions), Symbol(richmath_System_Deinitialization)));
    add(StyleType::Any, DynamicLocalBoxDefaultDynamicLocalValues, List(Symbol(richmath_System_DynamicLocalBoxOptions), Symbol(richmath_System_DynamicLocalValues)));
    add(StyleType::Any, DynamicLocalBoxDefaultInitialization,     List(Symbol(richmath_System_DynamicLocalBoxOptions), Symbol(richmath_System_Initialization)));
    add(StyleType::Any, DynamicLocalBoxDefaultUnsavedVariables,   List(Symbol(richmath_System_DynamicLocalBoxOptions), Symbol(richmath_System_UnsavedVariables)));
    
    add(StyleType::Any, FrameBoxDefaultBaselinePosition,  List(Symbol(richmath_System_FrameBoxOptions), Symbol( richmath_System_BaselinePosition)));
    add(StyleType::Any, FrameBoxDefaultBorderRadius,      List(Symbol(richmath_System_FrameBoxOptions), Symbol( richmath_System_BorderRadius)));
    add(StyleType::Any, FrameBoxDefaultFrameStyle,        List(Symbol(richmath_System_FrameBoxOptions), Symbol( richmath_System_FrameStyle)));
    
    add(StyleType::Any, InputFieldBoxDefaultAppearance,       List(Symbol(richmath_System_InputFieldBoxOptions), Symbol(richmath_System_Appearance)));
    add(StyleType::Any, InputFieldBoxDefaultBaselinePosition, List(Symbol(richmath_System_InputFieldBoxOptions), Symbol(richmath_System_BaselinePosition)));
    
    add(StyleType::Any, PaneBoxDefaultBaselinePosition,  List(Symbol(richmath_System_PaneBoxOptions), Symbol(richmath_System_BaselinePosition)));
    
    add(StyleType::Any, PanelBoxDefaultAppearance,       List(Symbol(richmath_System_PanelBoxOptions), Symbol(richmath_System_Appearance)));
    add(StyleType::Any, PanelBoxDefaultBaselinePosition, List(Symbol(richmath_System_PanelBoxOptions), Symbol(richmath_System_BaselinePosition)));
    
    add(StyleType::Any, SetterBoxDefaultBaselinePosition, List(Symbol(richmath_System_SetterBoxOptions), Symbol(richmath_System_BaselinePosition)));
    
    add(StyleType::Any, SliderBoxDefaultAppearance,      List(Symbol(richmath_System_SliderBoxOptions), Symbol(richmath_System_Appearance)));
    
    add(StyleType::Any, TemplateBoxDefaultDisplayFunction,        List(Symbol(richmath_System_TemplateBoxOptions), Symbol(richmath_System_DisplayFunction)));
    add(StyleType::Any, TemplateBoxDefaultInterpretationFunction, List(Symbol(richmath_System_TemplateBoxOptions), Symbol(richmath_System_InterpretationFunction)));
    add(StyleType::Any, TemplateBoxDefaultSyntaxForm,             List(Symbol(richmath_System_TemplateBoxOptions), Symbol(richmath_System_SyntaxForm)));
    add(StyleType::Any, TemplateBoxDefaultTooltip,                List(Symbol(richmath_System_TemplateBoxOptions), Symbol(richmath_System_Tooltip)));
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
  return literal_key == ClosingAction             ||
         literal_key == DockedSectionsTop         ||
         literal_key == DockedSectionsTopGlass    ||
         literal_key == DockedSectionsBottom      ||
         literal_key == DockedSectionsBottomGlass ||
         literal_key == Magnification             ||
         literal_key == StyleDefinitions          ||
         literal_key == Visible                   ||
         literal_key == WindowFrame               ||
         literal_key == WindowTitle;
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
  RICHMATH_ASSERT(type != StyleType::Enum);
  
  _key_to_type.set(key, type);
  if(type == StyleType::Size) { // {horz, vert} = {key+1, key+2}
    StyleOptionName Horizontal = StyleOptionName((int)key + 1);
    StyleOptionName Vertical   = StyleOptionName((int)key + 2);
    _key_to_type.set(Horizontal, StyleType::Length);
    _key_to_type.set(Vertical,   StyleType::Length);
  }
//  else if(type == StyleType::Margin) { // {left, right, top, bottom} = {key, key+1, key+2, key+3}
//    //StyleOptionName Left   = StyleOptionName((int)key + 1);
//    StyleOptionName Right  = StyleOptionName((int)key + 1);
//    StyleOptionName Top    = StyleOptionName((int)key + 2);
//    StyleOptionName Bottom = StyleOptionName((int)key + 3);
//    //_key_to_type.set(Left,   StyleType::Length);
//    _key_to_type.set(Right,  StyleType::Length);
//    _key_to_type.set(Top,    StyleType::Length);
//    _key_to_type.set(Bottom, StyleType::Length);
//  }

  _key_to_name.set(key, name);
  _name_to_key.set(name, key);
  
  add_to_ruleset(key, name);
  CurrentValue::register_provider(name, get_current_style_value, put_current_style_value);
}

void StyleInformation::add_enum(IntStyleOptionName key, const Expr &name, SharedPtr<EnumStyleConverter> enum_converter) {
  _key_to_enum_converter.set(key, enum_converter);
  _key_to_type.set(          key, StyleType::Enum);
  _key_to_name.set(          key, name);
  _name_to_key.set(          name, key);
  
  CurrentValue::register_provider(name, get_current_style_value, put_current_style_value);
  
  add_to_ruleset(key, name);
}

void StyleInformation::add_to_ruleset(StyleOptionName key, const Expr &name) {
  if(name[0] == richmath_System_List && name.expr_length() == 2) {
    Expr super_name = name[1];
    Expr sub_name   = name[2];
    
    StyleOptionName super_key = _name_to_key[super_name];
    if(_key_to_type[super_key] != StyleType::RuleSet) {
      pmath_debug_print_object("[not a StyleType::RuleSet: ", super_name.get(), "]\n");
      return;
    }
    
    SharedPtr<EnumStyleConverter> sec = _key_to_enum_converter[super_key];
    auto sur = dynamic_cast<SubRuleConverter*>(sec.ptr());
    if(!sur) {
      pmath_debug_print_object("[invalid EnumStyleConverter: ", super_name.get(), "]\n");
      return;
    }
    
    sur->add(key, sub_name);
  }
}

void StyleInformation::add_ruleset_head(StyleOptionName key, const Expr &symbol) {
  _key_to_enum_converter.set(key, new SubRuleConverter);
  _key_to_type.set(          key, StyleType::RuleSet);
  _key_to_name.set(          key, symbol);
  _name_to_key.set(          symbol, key);
  
  CurrentValue::register_provider(symbol, get_current_style_value, put_current_style_value);
}

Expr StyleInformation::get_current_style_value(FrontEndObject *obj, Expr item) {
  auto styled_obj = dynamic_cast<StyledObject*>(obj);
  if(!styled_obj)
    return Symbol(richmath_System_DollarFailed);

  if(item[0] == richmath_System_List && item.expr_length() == 1)
    item = item[1];
    
  StyleOptionName key = Style::get_key(item);
  if(key.is_valid()) 
    return styled_obj->get_pmath_style(key);
  
  return Symbol(richmath_System_DollarFailed);
}

bool StyleInformation::put_current_style_value(FrontEndObject *obj, Expr item, Expr rhs) {
  auto styled_obj = dynamic_cast<ActiveStyledObject*>(obj);
  if(!styled_obj)
    return false;
  
  if(item[0] == richmath_System_List && item.expr_length() == 1)
    item = item[1];
    
  StyleOptionName key = Style::get_key(item);
  if(!key.is_valid())
    return false;
  
  Expr opts = styled_obj->allowed_options();
  if(item[0] == richmath_System_List) {
    Expr rhs = opts.lookup(item[1], Expr{PMATH_UNDEFINED});
    if(rhs == PMATH_UNDEFINED)
      return false;
    
//    if(rhs[0] == richmath_System_List) {
//      // TODO: check further items ...
//    }
  }
  else if(opts.lookup(std::move(item), Expr{PMATH_UNDEFINED}) == PMATH_UNDEFINED)
    return false;
  
  if(!styled_obj->style) {
    if(rhs == richmath_System_Inherited)
      return true;
    
    styled_obj->style = new Style();
  }
  
  bool any_change = false;
  if(StyleInformation::get_type(key) == StyleType::AnyFlatList) {
    // Reset the style so that calling CurrentValue(...):= {..., Inherited, ...} multiple times
    // will not repeatedly fill in the previous value.
    if(rhs != richmath_System_Inherited) {
      any_change = styled_obj->style->set_pmath(key, Symbol(richmath_System_Inherited)) || any_change;
    }
  }
  any_change = styled_obj->style->set_pmath(key, std::move(rhs)) || any_change;
  
  if(any_change)
    styled_obj->on_style_changed(Style::modifies_size(key));
  
  return true;
}

bool StyleInformation::is_list_with_inherited(Expr expr) {
  if(expr[0] != richmath_System_List)
    return false;
  
  for(auto e : expr.items())
    if(e == richmath_System_Inherited)
      return true;
  
  return false;
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
  if(head == richmath_System_Dynamic || head == richmath_System_Function)
    return false;
    
  if( head == richmath_System_List ||
      head == richmath_System_Range ||
      head == richmath_System_NCache ||
      head == richmath_System_PureArgument ||
      head == richmath_System_Rule ||
      head == richmath_System_RuleDelayed ||
      head == richmath_System_GrayLevel ||
      head == richmath_System_Hue ||
      head == richmath_System_RGBColor)
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

ButtonFrameStyleConverter::ButtonFrameStyleConverter() : EnumStyleConverter() {
  _int_to_expr.default_value = Symbol(richmath_System_Automatic);
  _expr_to_int.default_value = -1;//ContainerType::PushButton;
  
  add(ContainerType::None,                 Symbol(richmath_System_None));
  add(ContainerType::FramelessButton,      strings::Frameless);
  add(ContainerType::GenericButton,        String("Generic"));
  add(ContainerType::PushButton,           String("DialogBox"));
  add(ContainerType::DefaultPushButton,    String("Defaulted"));
  add(ContainerType::PaletteButton,        strings::Palette);
  add(ContainerType::AddressBandGoButton,  String("AddressBandGo"));
  add(ContainerType::ListViewItemSelected, String("ListViewItemSelected"));
  add(ContainerType::ListViewItem,         String("ListViewItem"));
  
  add(ContainerType::OpenerTriangleClosed, String("OpenerTriangleClosed"));
  add(ContainerType::OpenerTriangleOpened, String("OpenerTriangleOpened"));
  
  add(ContainerType::NavigationBack,       String("NavigationBack"));
  add(ContainerType::NavigationForward,    String("NavigationForward"));
  
  add(ContainerType::TabHeadAbuttingRight,     String("TabHeadAbuttingRight"));
  add(ContainerType::TabHeadAbuttingLeftRight, String("TabHeadAbuttingLeftRight"));
  add(ContainerType::TabHeadAbuttingLeft,      String("TabHeadAbuttingLeft"));
  add(ContainerType::TabHead,                  strings::TabHead);
}

ButtonSourceStyleConverter::ButtonSourceStyleConverter() : EnumStyleConverter() {
  _int_to_expr.default_value = Expr();
  _expr_to_int.default_value = -1;
  
  add(ButtonSourceAutomatic,      Symbol(richmath_System_Automatic));
  add(ButtonSourceButtonBox,      Symbol(richmath_System_ButtonBox));
  add(ButtonSourceButtonContents, Symbol(richmath_System_ButtonContents));
  add(ButtonSourceButtonData,     Symbol(richmath_System_ButtonData));
  add(ButtonSourceFrontEndObject, Symbol(richmath_System_FrontEndObject));
}

CapFormStyleConverter::CapFormStyleConverter() : EnumStyleConverter() {
  _int_to_expr.default_value = Expr();
  _expr_to_int.default_value = -1;
  
  add(CapFormNone,      Symbol(richmath_System_None));
  add(CapFormButt,      strings::Butt);
  add(CapFormRound,     strings::Round);
  add(CapFormSquare,    strings::Square);
}

ClosingActionStyleConverter::ClosingActionStyleConverter() : EnumStyleConverter() {
  _int_to_expr.default_value = Expr();
  _expr_to_int.default_value = -1;
  
  add(ClosingActionDelete,  strings::Delete);
  add(ClosingActionHide,    strings::Hide);
}

ControlPlacementStyleConverter::ControlPlacementStyleConverter(): EnumStyleConverter() {
  add(ControlPlacementKindBottom, Symbol(richmath_System_Bottom));
  add(ControlPlacementKindLeft,   Symbol(richmath_System_Left));
  add(ControlPlacementKindRight,  Symbol(richmath_System_Right));
  add(ControlPlacementKindTop,    Symbol(richmath_System_Top));
}

FontSlantStyleConverter::FontSlantStyleConverter() : EnumStyleConverter() {
  _int_to_expr.default_value = Expr();
  _expr_to_int.default_value = -1;
  
  add(FontSlantPlain,  Symbol(richmath_System_Plain));
  add(FontSlantItalic, Symbol(richmath_System_Italic));
}

FontWeightStyleConverter::FontWeightStyleConverter() : EnumStyleConverter() {
  _int_to_expr.default_value = Expr();
  _expr_to_int.default_value = -1;
  
  add(FontWeightPlain, Symbol(richmath_System_Plain));
  add(FontWeightBold,  Symbol(richmath_System_Bold));
}

ImageSizeActionStyleConverter::ImageSizeActionStyleConverter() : EnumStyleConverter() {
  _int_to_expr.default_value = Expr();
  _expr_to_int.default_value = -1;
  
  add(ImageSizeActionClip,        String("Clip"));
  add(ImageSizeActionShrinkToFit, String("ShrinkToFit"));
  add(ImageSizeActionResizeToFit, String("ResizeToFit"));
}

MenuCommandKeyStyleConverter::MenuCommandKeyStyleConverter() : EnumStyleConverter() {
  _int_to_expr.default_value = Expr();
  _expr_to_int.default_value = -1;
  
  add(0,   Symbol(richmath_System_None));
  add('1', String("1"));
  add('2', String("2"));
  add('3', String("3"));
  add('4', String("4"));
  add('5', String("5"));
  add('6', String("6"));
  add('7', String("7"));
  add('8', String("8"));
  add('9', String("9"));
}

MenuSortingValueStyleConverter::MenuSortingValueStyleConverter() : EnumStyleConverter() {
  _int_to_expr.default_value = Expr();
  _expr_to_int.default_value = -1;
  
  add(0, Symbol(richmath_System_None));
}

bool MenuSortingValueStyleConverter::is_valid_key(int val) {
  return val >= 0;
}

bool MenuSortingValueStyleConverter::is_valid_expr(Expr expr) {
  if(expr.is_int32()) {
    int value = PMATH_AS_INT32(expr.get());
    return value >= 0;
  }
  return expr == richmath_System_None;
}
    
int MenuSortingValueStyleConverter::to_int(Expr expr) {
  if(expr.is_int32()) {
    int value = PMATH_AS_INT32(expr.get());
    if(value >= 0)
      return value;
  }
  return EnumStyleConverter::to_int(std::move(expr));
}

Expr MenuSortingValueStyleConverter::to_expr(int val) {
  if(val > 0)
    return Expr(val);
  
  return EnumStyleConverter::to_expr(val);
}

RemovalConditionsStyleConverter::RemovalConditionsStyleConverter(): FlagsStyleConverter() {
  add(RemovalConditionFlagSelectionExit,          String("SelectionExit"));
  add(RemovalConditionFlagMouseExit,              String("MouseExit"));
  add(RemovalConditionFlagMouseClickOutside,      String("MouseClickOutside"));
  add(RemovalConditionFlagMouseClickOutsidePopup, String("MouseClickOutsidePopup"));
  add(RemovalConditionFlagParentChanged,          String("ParentChanged"));
}

WindowFrameStyleConverter::WindowFrameStyleConverter() : EnumStyleConverter() {
  _int_to_expr.default_value = Expr();
  _expr_to_int.default_value = -1;
  
  add(WindowFrameNone,        Symbol(richmath_System_None));
  add(WindowFrameNormal,      strings::Normal);
  add(WindowFrameDialog,      String("Dialog"));
  add(WindowFramePalette,     strings::Palette);
  add(WindowFrameThin,        String("ThinFrame"));
  add(WindowFrameThinCallout, String("ThinFrameCallout"));
}
