#ifndef RICHMATH__BOXES__PANELBOX_H__INCLUDED
#define RICHMATH__BOXES__PANELBOX_H__INCLUDED

#include <boxes/containerwidgetbox.h>


namespace richmath {
  class PanelBox: public ContainerWidgetBox {
    public:
      explicit PanelBox(MathSequence *content = nullptr);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual bool expand(const BoxSize &size) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual Box *mouse_sensitive() override { return Box::mouse_sensitive(); }
      
    protected:
      virtual void resize_default_baseline(Context *context) override;
  };
}

#endif // RICHMATH__BOXES__PANELBOX_H__INCLUDED
