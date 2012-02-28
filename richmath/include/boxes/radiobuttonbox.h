#ifndef __BOXES__RADIOBUTTONBOX_H__
#define __BOXES__RADIOBUTTONBOX_H__

#include <boxes/emptywidgetbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class RadioButtonBox: public EmptyWidgetBox {
    public:
      RadioButtonBox();
      
      // Box::try_create<RadioButtonBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, int opts);
      
      virtual void paint(Context *context);
      
      virtual Expr to_pmath_symbol() { return Symbol(PMATH_SYMBOL_RADIOBUTTONBOX); }
      virtual Expr to_pmath(int flags);
      
      virtual void dynamic_finished(Expr info, Expr result);
      virtual Box *dynamic_to_literal(int *start, int *end);
      
      virtual void on_mouse_up(MouseEvent &event);
      
    protected:
      ContainerType calc_type(Expr result);
      
      Dynamic dynamic;
      Expr    value;
      bool    first_paint;
  };
}

#endif // __BOXES__RADIOBUTTONBOX_H__
