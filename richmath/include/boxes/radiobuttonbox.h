#ifndef __BOXES__RADIOBUTTONBOX_H__
#define __BOXES__RADIOBUTTONBOX_H__

#include <boxes/emptywidgetbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class RadioButtonBox: public EmptyWidgetBox {
    public:
      RadioButtonBox();
      
      // Box::try_create<RadioButtonBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void paint(Context *context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void dynamic_finished(Expr info, Expr result) override;
      virtual Box *dynamic_to_literal(int *start, int *end) override;
      
      virtual void click() override;
      
    protected:
      ContainerType calc_type(Expr result);
      
      Dynamic dynamic;
      Expr    value;
      bool    first_paint;
  };
}

#endif // __BOXES__RADIOBUTTONBOX_H__
