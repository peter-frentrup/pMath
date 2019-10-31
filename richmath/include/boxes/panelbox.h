#ifndef RICHMATH__BOXES__PANELBOX_H__INCLUDED
#define RICHMATH__BOXES__PANELBOX_H__INCLUDED

#include <boxes/containerwidgetbox.h>


namespace richmath {
  class PanelBox: public ContainerWidgetBox {
      using base = ContainerWidgetBox;
    public:
      explicit PanelBox(MathSequence *content = nullptr);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual ControlState calc_state(Context *context) override;
      
      virtual bool expand(const BoxSize &size) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void reset_style() override;
      
      virtual Box *mouse_sensitive() override { return Box::mouse_sensitive(); }
      virtual void on_enter() override;
      virtual void on_exit() override;
      
    protected:
      virtual void resize_default_baseline(Context *context) override;

      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::PanelBox; }
  };
}

#endif // RICHMATH__BOXES__PANELBOX_H__INCLUDED
