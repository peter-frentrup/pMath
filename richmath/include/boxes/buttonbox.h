#ifndef RICHMATH__BOXES__BUTTONBOX_H__INCLUDED
#define RICHMATH__BOXES__BUTTONBOX_H__INCLUDED

#include <boxes/containerwidgetbox.h>


namespace richmath {
  class ButtonBox: public ContainerWidgetBox {
    public:
      explicit ButtonBox(MathSequence *content = 0);
      
      // Box::try_create<ButtonBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual bool expand(const BoxSize &size) override;
      virtual void resize(Context *context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_move(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
      
      virtual void click();
  };
}

#endif // RICHMATH__BOXES__BUTTONBOX_H__INCLUDED
