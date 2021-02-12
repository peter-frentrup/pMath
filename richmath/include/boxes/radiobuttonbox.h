#ifndef RICHMATH__BOXES__RADIOBUTTONBOX_H__INCLUDED
#define RICHMATH__BOXES__RADIOBUTTONBOX_H__INCLUDED

#include <boxes/emptywidgetbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class RadioButtonBox final : public EmptyWidgetBox {
      class Impl;
      using base = EmptyWidgetBox;
    public:
      RadioButtonBox();
      
      // Box::try_create<RadioButtonBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void paint(Context &context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void reset_style() override;

      virtual void dynamic_finished(Expr info, Expr result) override;
      virtual VolatileSelection dynamic_to_literal(int start, int end) override;
      
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void click() override;
      
    private:
      Dynamic dynamic;
      Expr    value;
      bool    first_paint;
      bool    is_initialized;
  };
}

#endif // RICHMATH__BOXES__RADIOBUTTONBOX_H__INCLUDED
