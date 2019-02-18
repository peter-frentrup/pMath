#ifndef __RICHMATH__BOXES__SETTERBOX_H__INCLUDED
#define __RICHMATH__BOXES__SETTERBOX_H__INCLUDED

#include <boxes/buttonbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class SetterBox: public AbstractButtonBox {
    public:
      explicit SetterBox(MathSequence *content = nullptr);
      
      // Box::try_create<SetterBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual ControlState calc_state(Context *context) override;
      
      virtual void paint(Context *context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void reset_style() override;
      
      virtual void click() override;
      
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      virtual Box *dynamic_to_literal(int *start, int *end) override;
    
    protected:
      virtual ContainerType default_container_type() { return PaletteButton; }
      
    protected:
      Dynamic dynamic;
      Expr    value;
      
      bool must_update;
      bool is_down;
  };
};

#endif // __RICHMATH__BOXES__SETTERBOX_H__INCLUDED
