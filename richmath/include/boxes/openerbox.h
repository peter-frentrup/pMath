#ifndef RICHMATH__BOXES__OPENERBOX_H__INCLUDED
#define RICHMATH__BOXES__OPENERBOX_H__INCLUDED

#include <boxes/emptywidgetbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class OpenerBox final : public EmptyWidgetBox {
      using base = EmptyWidgetBox;
      class Impl;
    public:
      OpenerBox();
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void paint(Context &context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void reset_style() override;
      
      virtual void dynamic_finished(Expr info, Expr result) override;
      virtual VolatileSelection dynamic_to_literal(int start, int end) override;
      
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
      virtual void on_mouse_cancel() override;
      virtual void click() override;
      
    private:
      Dynamic dynamic;
      Expr    mouse_down_value;
      bool    is_initialized;
  };
}

#endif // RICHMATH__BOXES__OPENERBOX_H__INCLUDED
