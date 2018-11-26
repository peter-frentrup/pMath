#ifndef __RICHMATHRICHMATH__BOXES__SETTERBOX_H__INCLUDED
#define __RICHMATHRICHMATH__BOXES__SETTERBOX_H__INCLUDED

#include <boxes/containerwidgetbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class SetterBox: public ContainerWidgetBox {
    public:
      explicit SetterBox(MathSequence *content = 0);
      
      // Box::try_create<SetterBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual ControlState calc_state(Context *context) override;
      
      virtual bool expand(const BoxSize &size) override;
      virtual void resize(Context *context) override;
      virtual void paint(Context *context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
      
      virtual void click();
      
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      virtual Box *dynamic_to_literal(int *start, int *end) override;
    
    protected:
      Dynamic dynamic;
      Expr    value;
      
      bool must_update;
      bool is_down;
  };
};

#endif // __RICHMATHRICHMATH__BOXES__SETTERBOX_H__INCLUDED
