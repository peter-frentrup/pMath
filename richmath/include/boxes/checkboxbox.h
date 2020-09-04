#ifndef RICHMATH__BOXES__CHECKBOXBOX_H__INCLUDED
#define RICHMATH__BOXES__CHECKBOXBOX_H__INCLUDED

#include <boxes/emptywidgetbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class CheckboxBox: public EmptyWidgetBox {
      using base = EmptyWidgetBox;
      class Impl;
    public:
      CheckboxBox();
      
      // Box::try_create<CheckboxBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void paint(Context &context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void reset_style() override;
      
      virtual void dynamic_finished(Expr info, Expr result) override;
      virtual Box *dynamic_to_literal(int *start, int *end) override;
      
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
      virtual void on_mouse_cancel() override;
      virtual void click() override;
      
    protected:
      ContainerType calc_type(Expr result);
      Expr to_literal();
      
    protected:
      Dynamic dynamic;
      Expr    values;
      Expr    mouse_down_value;
  };
}

#endif // RICHMATH__BOXES__CHECKBOXBOX_H__INCLUDED
