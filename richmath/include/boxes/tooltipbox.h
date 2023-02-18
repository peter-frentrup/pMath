#ifndef RICHMATH__BOXES__TOOLTIPBOX_H__INCLUDED
#define RICHMATH__BOXES__TOOLTIPBOX_H__INCLUDED

#include <boxes/stylebox.h>


namespace richmath {
  class TooltipBox final : public AbstractStyleBox {
      using base = AbstractStyleBox;
    public:
      explicit TooltipBox(AbstractSequence *content);
      
      // Box::try_create<TooltipBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr to_pmath_symbol() override;
      
      virtual void on_mouse_enter() override;
      virtual void on_mouse_exit() override;
      
      Expr tooltip_boxes() const { return _tooltip_boxes; }
      
    protected:
      Expr _tooltip_boxes;
      
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
  };
}

#endif // RICHMATH__BOXES__TOOLTIPBOX_H__INCLUDED
