#ifndef __BOXES__CHECKBOXBOX_H__
#define __BOXES__CHECKBOXBOX_H__

#include <boxes/emptywidgetbox.h>
#include <eval/dynamic.h>

namespace richmath{
  class CheckboxBox: public EmptyWidgetBox{
    public:
      static CheckboxBox *create(Expr expr);
      
      virtual void paint(Context *context);
      
      virtual Expr to_pmath_symbol(){ return Symbol(PMATH_SYMBOL_CHECKBOXBOX); }
      virtual Expr to_pmath(int flags);
      
      virtual void dynamic_finished(Expr info, Expr result);
      virtual Box *dynamic_to_literal(int *start, int *end);
      
      virtual void on_mouse_up(MouseEvent &event);
      
    protected:
      CheckboxBox();
      
      ContainerType calc_type(Expr result);
    
    protected:
      Dynamic dynamic;
      Expr    values;
  };
}

#endif // __BOXES__CHECKBOXBOX_H__
