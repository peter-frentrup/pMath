#ifndef __BOXES__BUTTONBOX_H__
#define __BOXES__BUTTONBOX_H__

#include <boxes/containerwidgetbox.h>

namespace richmath{
  class ButtonBox: public ContainerWidgetBox {
    public:
      static ButtonBox *create(Expr expr, int opts);
      
      virtual bool expand(const BoxSize &size);
      virtual void resize(Context *context);
      
      virtual Expr to_pmath_symbol(){ return Symbol(PMATH_SYMBOL_BUTTONBOX); }
      virtual Expr to_pmath(bool parseable);
      
      virtual void on_mouse_down(MouseEvent &event);
      virtual void on_mouse_move(MouseEvent &event);
      virtual void on_mouse_up(MouseEvent &event);
      
      virtual void click();
    
    protected:
      explicit ButtonBox(MathSequence *content = 0);
  };
}

#endif // __BOXES__BUTTONBOX_H__
