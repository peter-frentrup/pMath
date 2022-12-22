#ifndef RICHMATH__BOXES__STYLEBOX_H__INCLUDED
#define RICHMATH__BOXES__STYLEBOX_H__INCLUDED

#include <boxes/ownerbox.h>

namespace richmath {
  class AbstractStyleBox: public ExpandableOwnerBox {
      using base = ExpandableOwnerBox;
    public:
      explicit AbstractStyleBox(AbstractSequence *content);
      
      virtual MathSequence *as_inline_span() override;
      
      virtual void paint(Context &context) override;
      virtual void begin_paint_inline_span(Context &context, BasicHeterogeneousStack &context_stack, DisplayStage stage) override;
      virtual void end_paint_inline_span(Context &context, BasicHeterogeneousStack &context_stack, DisplayStage stage) override;
      
      virtual void colorize_scope(SyntaxState &state) override;
      
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
      virtual void after_inline_span_mouse_selection(Box *top, VolatileSelection &sel, bool &was_inside_start) override;
      
    protected:
      virtual void resize_default_baseline(Context &context) override;
      void paint_or_resize_no_baseline(Context &context, bool paint);
  };
  
  class StyleBox final : public AbstractStyleBox {
      using base = AbstractStyleBox;
    public:
      explicit StyleBox(AbstractSequence *content);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr to_pmath_symbol() override;
      
      virtual bool changes_children_style() override { return true; }
    
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
  };
  
  class TagBox final : public AbstractStyleBox {
      using base = AbstractStyleBox;
    public:
      explicit TagBox(AbstractSequence *content);
      explicit TagBox(AbstractSequence *content, Expr _tag);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr to_pmath_symbol() override;
    
      virtual void reset_style() override;
    
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
    
    private:
      Expr tag;
  };
}

#endif // RICHMATH__BOXES__STYLEBOX_H__INCLUDED
