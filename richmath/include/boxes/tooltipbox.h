#ifndef RICHMATH__BOXES__TOOLTIPBOX_H__INCLUDED
#define RICHMATH__BOXES__TOOLTIPBOX_H__INCLUDED

#include <boxes/stylebox.h>


namespace richmath {
  class TooltipBox: public ExpandableAbstractStyleBox {
    public:
      TooltipBox();
      
      // Box::try_create<TooltipBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void on_mouse_enter() override;
      virtual void on_mouse_exit() override;
      
    protected:
      Expr tooltip_boxes;
  };
}

#endif // RICHMATH__BOXES__TOOLTIPBOX_H__INCLUDED
