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
    InternalHavePendingDynamic,
    InternalUsesCurrentValueOfMouseOver,
    LineBreakWithin,
    Placeholder,
    SectionGenerated,
    SectionLabelAutoDelete,
    Selectable,
    ShowAutoStyles,
    ShowSectionBracket,
    ShowStringCharacters,
    StripOnInput,
    
    ButtonFrame // -1 = Automatic,  other: ContainerType value
  };
  
  static const int FontWeightPlain = 0;
  static const int FontWeightBold  = 100;
  
  static const float ImageSizeAutomatic = -1.0f;
  
  enum FloatStyleOptionName {
    FontSize = 10000, // greater than any IntStyleOptionName value
    
    AspectRatio,
    
    GridBoxColumnSpacing,
    GridBoxRowSpacing,
    
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
    FontFamily,
    SectionLabel,
    Method,
    WindowTitle
  };
  
  enum ObjectStyleOptionName {
    ButtonFunction = 30000, // greater than any StringStyleOptionName value
    ScriptSizeMultipliers,
    TextShadow,
    UnknownOptions,
    
    BoxRotation,
    BoxTransformation,
    PlotRange
  };
  
  const int DynamicOffset = 1000000;
  
  typedef union {
    int   int_value;
    float float_value;
  } IntFloatUnion;
  
  class Style: public Shareable {
    public:
      Style();
      Style(Expr options);
      
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
      
      void set_pmath_bool_auto(IntStyleOptionName n, Expr obj); // 0/1=true/false, 2=auto
      void set_pmath_bool(IntStyleOptionName      n, Expr obj);
      void set_pmath_color(IntStyleOptionName     n, Expr obj);
      void set_pmath_float(FloatStyleOptionName   n, Expr obj);
      void set_pmath_margin(FloatStyleOptionName  n, Expr obj); // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
      void set_pmath_size(  FloatStyleOptionName  n, Expr obj); // n + {0,1} ~= {Horizontal, Vertical}
      void set_pmath_string(StringStyleOptionName n, Expr obj);
      void set_pmath_object(ObjectStyleOptionName n, Expr obj);
      
      unsigned int count();
      
      static bool modifies_size(IntStyleOptionName    style_name) { return modifies_size((int)style_name); }
      static bool modifies_size(FloatStyleOptionName  style_name) { return modifies_size((int)style_name); }
      static bool modifies_size(StringStyleOptionName style_name) { return modifies_size((int)style_name); }
      static bool modifies_size(ObjectStyleOptionName style_name) { return modifies_size((int)style_name); }
      
      static Expr get_symbol(IntStyleOptionName    n) { return get_symbol((int)n); }
      static Expr get_symbol(FloatStyleOptionName  n) { return get_symbol((int)n); }
      static Expr get_symbol(StringStyleOptionName n) { return get_symbol((int)n); }
      static Expr get_symbol(ObjectStyleOptionName n) { return get_symbol((int)n); }
      
      bool update_dynamic(Box *parent);
      
      void emit_to_pmath(bool for_sections = true, bool with_inherited = false);
      
    protected:
      static bool modifies_size(int style_name);
      
      static Expr get_symbol(int n);
      
      bool get_dynamic(int n, Expr *value) {
        return get((ObjectStyleOptionName)(n + DynamicOffset), value);
      }
      
      void set_dynamic(int n, Expr value) {
        remove((ObjectStyleOptionName)(n + DynamicOffset));
        set((ObjectStyleOptionName)(n + DynamicOffset), value);
        
        set(InternalHavePendingDynamic, true);
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
