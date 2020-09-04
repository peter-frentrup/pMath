#ifndef RICHMATH__BOXES__STYLEBOX_H__INCLUDED
#define RICHMATH__BOXES__STYLEBOX_H__INCLUDED

#include <boxes/ownerbox.h>

namespace richmath {
  class AbstractStyleBox: public ExpandableOwnerBox {
      using base = ExpandableOwnerBox;
    public:
      explicit AbstractStyleBox(MathSequence *content = nullptr);
      
      virtual void paint(Context &context) override;
      
      virtual void colorize_scope(SyntaxState &state) override;
      
      virtual VolatileSelection mouse_selection(float x, float y, bool *was_inside_start) override;
        
    protected:
      virtual void resize_default_baseline(Context &context) override;
      void paint_or_resize_no_baseline(Context &context, bool paint);
  };
  
  class StyleBox: public AbstractStyleBox {
      using base = AbstractStyleBox;
    public:
      explicit StyleBox(MathSequence *content = nullptr);
      
      // Box::try_create<StyleBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual bool changes_children_style() override { return true; }
  };
  
  class TagBox: public AbstractStyleBox {
      using base = AbstractStyleBox;
    public:
      explicit TagBox(MathSequence *content = nullptr);
      TagBox(MathSequence *content, Expr _tag);
      
      // Box::try_create<TagBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
    
    protected:
      virtual void resize_default_baseline(Context &context) override;
      
    public:
      Expr tag;
  };
}

#endif // RICHMATH__BOXES__STYLEBOX_H__INCLUDED
