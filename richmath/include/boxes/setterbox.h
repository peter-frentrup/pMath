#ifndef __RICHMATH__BOXES__SETTERBOX_H__
#define __RICHMATH__BOXES__SETTERBOX_H__

#include <boxes/containerwidgetbox.h>
#include <eval/dynamic.h>

namespace richmath{
  class SetterBox: public ContainerWidgetBox {
    public:
      static SetterBox *create(Expr expr, int opts);
      
      virtual ControlState calc_state(Context *context);
      
      virtual bool expand(const BoxSize &size);
      virtual void paint(Context *context);
      
      virtual Expr to_pmath_symbol(){ return Symbol(PMATH_SYMBOL_SETTERBOX); }
      virtual Expr to_pmath(bool parseable);
      
      virtual void on_mouse_down(MouseEvent &event);
      virtual void on_mouse_up(MouseEvent &event);
      
      virtual void click();
      
      virtual void dynamic_updated();
      virtual void dynamic_finished(Expr info, Expr result);
    
    protected:
      explicit SetterBox(MathSequence *content = 0);
      
    protected:
      Dynamic dynamic;
      Expr    value;
      
      bool must_update;
      bool is_down;
  };
};

#endif // __RICHMATH__BOXES__SETTERBOX_H__
