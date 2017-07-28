#ifndef __BOXES__BUTTONBOX_H__
#define __BOXES__BUTTONBOX_H__

#include <boxes/containerwidgetbox.h>


namespace richmath {
  class ButtonBox: public ContainerWidgetBox {
    public:
      explicit ButtonBox(MathSequence *content = 0);
      
      // Box::try_create<ButtonBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, int opts) override;
      
      virtual bool expand(const BoxSize &size) override;
      virtual void resize(Context *context) override;
      
      virtual Expr to_pmath_symbol() override { return Symbol(PMATH_SYMBOL_BUTTONBOX); }
      virtual Expr to_pmath(BoxFlags flags) override;
      
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_move(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
      
      virtual void click();
  };
}

#endif // __BOXES__BUTTONBOX_H__
