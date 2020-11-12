#ifndef RICHMATH__UTIL__STYLED_OBJECT_H__INCLUDED
#define RICHMATH__UTIL__STYLED_OBJECT_H__INCLUDED


#include <util/frontendobject.h>
#include <util/style.h>


namespace richmath {
  class StyledObject : public FrontEndObject {
      class Impl;
    public:
      virtual StyledObject *style_parent() = 0;
      virtual SharedPtr<Style> own_style() { return nullptr; }
      
      virtual SharedPtr<Stylesheet> stylesheet();
      
      virtual bool changes_children_style() { return false; }
      
      bool enabled();
      
      Color  get_style(ColorStyleOptionName  n, Color  result = Color::None);
      int    get_style(IntStyleOptionName    n, int    result = 0);
      float  get_style(FloatStyleOptionName  n, float  result = 0.0);
      String get_style(StringStyleOptionName n, String result);
      Expr   get_style(ObjectStyleOptionName n, Expr   result);
      String get_style(StringStyleOptionName n);
      Expr   get_style(ObjectStyleOptionName n);
      Expr   get_pmath_style(StyleOptionName n);
      
      // ignore parents (except for search via get_default_key)
      Color  get_own_style(ColorStyleOptionName  n, Color  fallback_result = Color::None);
      int    get_own_style(IntStyleOptionName    n, int    fallback_result = 0);
      float  get_own_style(FloatStyleOptionName  n, float  fallback_result = 0.0);
      String get_own_style(StringStyleOptionName n, String fallback_result);
      Expr   get_own_style(ObjectStyleOptionName n, Expr   fallback_result);
      String get_own_style(StringStyleOptionName n);
      Expr   get_own_style(ObjectStyleOptionName n);
      
      StyleOptionName get_default_key(StyleOptionName n) { return StyleOptionName { (int)get_default_styles_offset() + (int)n }; }
      ColorStyleOptionName  get_default_key(ColorStyleOptionName n) {  return (ColorStyleOptionName) get_default_key(StyleOptionName{n}); }
      IntStyleOptionName    get_default_key(IntStyleOptionName n) {    return (IntStyleOptionName)   get_default_key(StyleOptionName{n}); }
      FloatStyleOptionName  get_default_key(FloatStyleOptionName n) {  return (FloatStyleOptionName) get_default_key(StyleOptionName{n}); }
      StringStyleOptionName get_default_key(StringStyleOptionName n) { return (StringStyleOptionName)get_default_key(StyleOptionName{n}); }
      ObjectStyleOptionName get_default_key(ObjectStyleOptionName n) { return (ObjectStyleOptionName)get_default_key(StyleOptionName{n}); }
      
      virtual void reset_style();
      
    protected:
      virtual DefaultStyleOptionOffsets get_default_styles_offset() { return DefaultStyleOptionOffsets::None; }
  };
  
  class ActiveStyledObject : public StyledObject {
    public:
      SharedPtr<Style> style;
      
    public:
      virtual SharedPtr<Style> own_style() final override { return style; };
      
      virtual Expr allowed_options() = 0;
      
      /// A style option changed.
      virtual void invalidate_options() {}
  };
}

#endif // RICHMATH__UTIL__STYLED_OBJECT_H__INCLUDED
