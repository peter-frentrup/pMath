#ifndef __BOXES__TOOLTIPBOX_H__
#define __BOXES__TOOLTIPBOX_H__

#include <boxes/stylebox.h>


namespace richmath {
  class TooltipBox: public ExpandableAbstractStyleBox {
    public:
      static TooltipBox *create(Expr expr, int opts);
      
      virtual Expr to_pmath_symbol() { return Symbol(PMATH_SYMBOL_TOOLTIPBOX); }
      virtual Expr to_pmath(int flags);
      
      virtual void on_mouse_enter();
      virtual void on_mouse_exit();
      
    protected:
      TooltipBox();
      
    protected:
      Expr tooltip_boxes;
  };
}

#endif // __BOXES__TOOLTIPBOX_H__
