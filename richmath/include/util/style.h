#ifndef __UTIL__STYLE_H__
#define __UTIL__STYLE_H__

#include <util/hashtable.h>
#include <util/pmath-extra.h>
#include <util/sharedptr.h>


namespace richmath {
  class Box;
  
  Expr color_to_pmath(int color);
  int pmath_to_color(Expr obj); // -2 on error, -1=None
  
  enum IntStyleOptionName {
    Background = 0,
    FontColor,
    SectionFrameColor,
    
    Antialiasing, // 0=off, 1=on, 2=default
    
    FontSlant,
    FontWeight,
    
    AutoDelete,
    AutoNumberFormating,
    AutoSpacing,
    ContinuousAction,
    Editable,
    Evaluatable,
    InternalHasModifiedWindowOption,
    InternalHasPendingDynamic,
    InternalUsesCurrentValueOfMouseOver,
    LineBreakWithin,
    Placeholder,
    ReturnCreatesNewSection,
    SectionEditDuplicate,
    SectionEditDuplicateMakesCopy,
    SectionGenerated,
    SectionLabelAutoDelete,
    Selectable,
    ShowAutoStyles,
    ShowSectionBracket,
    ShowStringCharacters,
    StripOnInput,
    Visible,
    
    ButtonFrame, // -1 = Automatic,  other: ContainerType value
    WindowFrame  // WindowFrameType
  };
  
  enum {
    FontWeightPlain = 0,
    FontWeightBold  = 100
  };
  
  typedef enum {
    WindowFrameNormal  = 0,
    WindowFramePalette = 1,
    WindowFrameDialog  = 2
  } WindowFrameType;
  
  static const float ImageSizeAutomatic = -1.0f;
  
  enum FloatStyleOptionName {
    FontSize = 10000, // greater than any IntStyleOptionName value
    
    AspectRatio,
    
    GridBoxColumnSpacing,
    GridBoxRowSpacing,
    
    ImageSizeCommon,
    ImageSizeHorizontal, // > 0 or ImageSizeAutomatic or ImageSizeAll
    ImageSizeVertical,   // > 0 or ImageSizeAutomatic
    
    SectionMarginLeft,
    SectionMarginRight,
    SectionMarginTop,
    SectionMarginBottom,
    
    SectionFrameLeft,
    SectionFrameRight,
    SectionFrameTop,
    SectionFrameBottom,
    
    SectionFrameMarginLeft,
    SectionFrameMarginRight,
    SectionFrameMarginTop,
    SectionFrameMarginBottom,
    
    SectionGroupPrecedence
  };
  
  enum StringStyleOptionName {
    BaseStyleName = 20000, // greater than any FloatStyleOptionName value
    Method,
    
    LanguageCategory,
    SectionLabel,
    
    WindowTitle
  };
  
  enum ObjectStyleOptionName {
    ButtonFunction = 30000, // greater than any StringStyleOptionName value
    ScriptSizeMultipliers,
    TextShadow,
    FontFamilies,
    UnknownOptions,
    
    BoxRotation,
    BoxTransformation,
    PlotRange,
    BorderRadius,
    
    DefaultDuplicateSectionStyle,
    DefaultNewSectionStyle,
    DefaultReturnCreatedSectionStyle,
    
    DockedSections,
    DockedSectionsTop,
    DockedSectionsTopGlass,
    DockedSectionsBottom,
    DockedSectionsBottomGlass,
    
    StyleDefinitions,
    GeneratedSectionStyles
  };
  
  const int DynamicOffset = 1000000;
  
  enum StyleType {
    StyleTypeNone,
    
    StyleTypeBool,
    StyleTypeBoolAuto,
    StyleTypeColor,
    StyleTypeNumber,
    StyleTypeMargin,
    StyleTypeSize,
    StyleTypeString,
    StyleTypeAny,
    
    StyleTypeEnum,
    StyleTypeRuleSet
  };
  
  typedef union {
    int   int_value;
    float float_value;
  } IntFloatUnion;
  
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
      
      const Hashtable<Expr, int> &expr_to_int(){ return _expr_to_int; }
      
    protected:
      void add(int val, Expr expr);
      
    protected:
      Hashtable<int, Expr, cast_hash> _int_to_expr;
      Hashtable<Expr, int>            _expr_to_int;
  };
  
  class Style: public Shareable {
    public:
      Style();
      Style(Expr options);
      virtual ~Style();
      
      void clear();
      void add_pmath(Expr options);
      
      void merge(SharedPtr<Style> other);
      
      bool get(IntStyleOptionName    n, int    *value);
      bool get(FloatStyleOptionName  n, float  *value);
      bool get(StringStyleOptionName n, String *value);
      bool get(ObjectStyleOptionName n, Expr   *value);
      
      bool get_dynamic(IntStyleOptionName    n, Expr *value) { return get_dynamic((int)n, value); }
      bool get_dynamic(FloatStyleOptionName  n, Expr *value) { return get_dynamic((int)n, value); }
      bool get_dynamic(StringStyleOptionName n, Expr *value) { return get_dynamic((int)n, value); }
      bool get_dynamic(ObjectStyleOptionName n, Expr *value) { return get_dynamic((int)n, value); }
      
      void set(IntStyleOptionName    n, int    value);
      void set(FloatStyleOptionName  n, float  value);
      void set(StringStyleOptionName n, String value);
      void set(ObjectStyleOptionName n, Expr   value);
      
      void set_dynamic(IntStyleOptionName    n, Expr value) { set_dynamic((int)n, value); }
      void set_dynamic(FloatStyleOptionName  n, Expr value) { set_dynamic((int)n, value); }
      void set_dynamic(StringStyleOptionName n, Expr value) { set_dynamic((int)n, value); }
      void set_dynamic(ObjectStyleOptionName n, Expr value) { set_dynamic((int)n, value); }
      
      void remove(IntStyleOptionName    n);
      void remove(FloatStyleOptionName  n);
      void remove(StringStyleOptionName n);
      void remove(ObjectStyleOptionName n);
      
      void remove_dynamic(IntStyleOptionName    n) { remove_dynamic((int)n); }
      void remove_dynamic(FloatStyleOptionName  n) { remove_dynamic((int)n); }
      void remove_dynamic(StringStyleOptionName n) { remove_dynamic((int)n); }
      void remove_dynamic(ObjectStyleOptionName n) { remove_dynamic((int)n); }
      
      unsigned int count();
      
      static bool modifies_size(IntStyleOptionName    style_name) { return modifies_size((int)style_name); }
      static bool modifies_size(FloatStyleOptionName  style_name) { return modifies_size((int)style_name); }
      static bool modifies_size(StringStyleOptionName style_name) { return modifies_size((int)style_name); }
      static bool modifies_size(ObjectStyleOptionName style_name) { return modifies_size((int)style_name); }
      
      static Expr get_name(IntStyleOptionName    n) { return get_name((int)n); }
      static Expr get_name(FloatStyleOptionName  n) { return get_name((int)n); }
      static Expr get_name(StringStyleOptionName n) { return get_name((int)n); }
      static Expr get_name(ObjectStyleOptionName n) { return get_name((int)n); }
      
      static enum StyleType get_type(IntStyleOptionName    n) { return get_type((int)n); }
      static enum StyleType get_type(FloatStyleOptionName  n) { return get_type((int)n); }
      static enum StyleType get_type(StringStyleOptionName n) { return get_type((int)n); }
      static enum StyleType get_type(ObjectStyleOptionName n) { return get_type((int)n); }
      
      static SharedPtr<StyleEnumConverter> get_enum_values(IntStyleOptionName n);
      static SharedPtr<StyleEnumConverter> get_sub_rules(ObjectStyleOptionName n);
      
      void set_pmath(IntStyleOptionName    n, Expr value) { set_pmath((int)n, value); }
      void set_pmath(FloatStyleOptionName  n, Expr value) { set_pmath((int)n, value); }
      void set_pmath(StringStyleOptionName n, Expr value) { set_pmath((int)n, value); }
      void set_pmath(ObjectStyleOptionName n, Expr value) { set_pmath((int)n, value); }
      
      Expr get_pmath(IntStyleOptionName    n) { return get_pmath((int)n); }
      Expr get_pmath(FloatStyleOptionName  n) { return get_pmath((int)n); }
      Expr get_pmath(StringStyleOptionName n) { return get_pmath((int)n); }
      Expr get_pmath(ObjectStyleOptionName n) { return get_pmath((int)n); }
      
      bool update_dynamic(Box *parent);
      
      void emit_pmath(IntStyleOptionName    n) { return emit_pmath((int)n); }
      void emit_pmath(FloatStyleOptionName  n) { return emit_pmath((int)n); }
      void emit_pmath(StringStyleOptionName n) { return emit_pmath((int)n); }
      void emit_pmath(ObjectStyleOptionName n) { return emit_pmath((int)n); }
      
      void emit_to_pmath(bool with_inherited = false);
      
    protected:
      static bool modifies_size(int style_name);
      
      static Expr           get_name(int n);
      static enum StyleType get_type(int n);
      
      void set_pmath(          int key,                 Expr value);
      void set_pmath_bool_auto(IntStyleOptionName    n, Expr obj);
      void set_pmath_bool(     IntStyleOptionName    n, Expr obj);
      void set_pmath_color(    IntStyleOptionName    n, Expr obj);
      void set_pmath_float(    FloatStyleOptionName  n, Expr obj);
      void set_pmath_margin(   FloatStyleOptionName  n, Expr obj); // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
      void set_pmath_size(     FloatStyleOptionName  n, Expr obj); // n + {0,1,2} ~= {Common, Horizontal, Vertical}
      void set_pmath_string(   StringStyleOptionName n, Expr obj);
      void set_pmath_object(   ObjectStyleOptionName n, Expr obj);
      void set_pmath_enum(     IntStyleOptionName    n, Expr obj);
      void set_pmath_ruleset(  ObjectStyleOptionName n, Expr obj);
      
      Expr get_pmath(          int                   n);
      Expr get_pmath_bool_auto(IntStyleOptionName    n);
      Expr get_pmath_bool(     IntStyleOptionName    n);
      Expr get_pmath_color(    IntStyleOptionName    n);
      Expr get_pmath_float(    FloatStyleOptionName  n);
      Expr get_pmath_margin(   FloatStyleOptionName  n); // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
      Expr get_pmath_size(     FloatStyleOptionName  n); // n + {0,1,2} ~= {Common, Horizontal, Vertical}
      Expr get_pmath_string(   StringStyleOptionName n);
      Expr get_pmath_object(   ObjectStyleOptionName n);
      Expr get_pmath_enum(     IntStyleOptionName    n);
      Expr get_pmath_ruleset(  ObjectStyleOptionName n);
      
      void emit_pmath(int n);
      
      bool get_dynamic(int n, Expr *value) {
        return get((ObjectStyleOptionName)(n + DynamicOffset), value);
      }
      
      void set_dynamic(int n, Expr value) {
        remove((ObjectStyleOptionName)(n + DynamicOffset));
        set(   (ObjectStyleOptionName)(n + DynamicOffset), value);
        
        set(InternalHasPendingDynamic, true);
      }
      
      void remove_dynamic(int n) {
        remove((ObjectStyleOptionName)(n + DynamicOffset));
      }
      
    private:
      Hashtable<int, IntFloatUnion, cast_hash> int_float_values;
      Hashtable<int, Expr,          cast_hash> object_values;
  };
  
  class Stylesheet: public Shareable {
    public:
      Hashtable<String, SharedPtr<Style> > styles;
      
      SharedPtr<Style> base;
      
      // each get() ignores base:
      bool get(SharedPtr<Style> s, IntStyleOptionName    n, int    *value);
      bool get(SharedPtr<Style> s, FloatStyleOptionName  n, float  *value);
      bool get(SharedPtr<Style> s, StringStyleOptionName n, String *value);
      bool get(SharedPtr<Style> s, ObjectStyleOptionName n, Expr   *value);
      
      int    get_with_base(SharedPtr<Style> s, IntStyleOptionName    n);
      float  get_with_base(SharedPtr<Style> s, FloatStyleOptionName  n);
      String get_with_base(SharedPtr<Style> s, StringStyleOptionName n);
      Expr   get_with_base(SharedPtr<Style> s, ObjectStyleOptionName n);
      
    public:
      static SharedPtr<Stylesheet> Default;
  };
};

#endif // __UTIL__STYLE_H__
