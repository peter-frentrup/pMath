#ifndef __UTIL__STYLE_H__
#define __UTIL__STYLE_H__

#include <util/hashtable.h>
#include <util/pmath-extra.h>
#include <util/sharedptr.h>

namespace richmath{
  pmath_t color_to_pmath(int color);
  pmath_t double_to_pmath(double value);
  int pmath_to_color(Expr obj); // -2 on error, -1=None
  
  enum IntStyleOptionName{
    Background = 0,
    FontColor,
    SectionFrameColor,
    
    Antialiasing, // 0=off, 1=on, 2=default
    
    FontSlant,
    FontWeight,
    
    AutoDelete,
    AutoNumberFormating,
    AutoSpacing,
    Editable,
    LineBreakWithin,
    Placeholder,
    SectionGenerated,
    SectionLabelAutoDelete,
    Selectable,
    ShowAutoStyles,
    ShowSectionBracket,
    ShowStringCharacters,
    
    ButtonFrame, // -1 = Automatic,  other: ContainerType value
    
    ContentType
  };
  
  static const int FontWeightPlain = 0;
  static const int FontWeightBold  = 100;
  
  static const int ContentTypeBoxData = 0;
  static const int ContentTypeString  = 1;
  
  enum FloatStyleOptionName{
    FontSize = 10000, // greather than any IntStyleOptionName value
    
    GridBoxColumnSpacing,
    GridBoxRowSpacing,
    
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
  
  enum StringStyleOptionName{
    BaseStyleName = 20000, // greather than any FloatStyleOptionName value
    FontFamily,
    SectionLabel,
    Method
  };
  
  enum ObjectStyleOptionName{
    ButtonFunction = 30000 ,// greather than any StringStyleOptionName value
    ScriptSizeMultipliers,
    TextShadow
  };
  
  typedef union{
    int   int_value;
    float float_value;
  }IntFloatUnion;
  
  class Style: public Shareable {
    public:
      Style();
      Style(Expr options);
      
      void add_pmath(Expr options);
      
      void merge(SharedPtr<Style> other);
      
      bool get(IntStyleOptionName    n, int    *value);
      bool get(FloatStyleOptionName  n, float  *value);
      bool get(StringStyleOptionName n, String *value);
      bool get(ObjectStyleOptionName n, Expr   *value);
      
      bool get_dynamic(int n, Expr *value);
      
      void set(IntStyleOptionName    n, int    value);
      void set(FloatStyleOptionName  n, float  value);
      void set(StringStyleOptionName n, String value);
      void set(ObjectStyleOptionName n, Expr   value);
      
      void set_dynamic(int n, Expr value);
      
      void remove(IntStyleOptionName    n);
      void remove(FloatStyleOptionName  n);
      void remove(StringStyleOptionName n);
      void remove(ObjectStyleOptionName n);
      
      void set_pmath_bool_auto(IntStyleOptionName n, Expr obj); // 0/1=true/false, 2=auto
      void set_pmath_bool(IntStyleOptionName      n, Expr obj);
      void set_pmath_color(IntStyleOptionName     n, Expr obj);
      void set_pmath_float(FloatStyleOptionName   n, Expr obj);
      void set_pmath_margin(FloatStyleOptionName  n, Expr obj); // n + {0,1,2,3} ~= {Left, Right, Top, Bottom}
      void set_pmath_string(StringStyleOptionName n, Expr obj);
      
      unsigned int count();
      
      void emit_to_pmath(bool for_sections = true, bool with_inherited = false);
      
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
