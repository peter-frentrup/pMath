#ifndef RICHMATH__BOXES__BUTTONBOX_H__INCLUDED
#define RICHMATH__BOXES__BUTTONBOX_H__INCLUDED

#include <boxes/containerwidgetbox.h>


namespace richmath {
  class AbstractButtonBox: public ContainerWidgetBox {
      using base = ContainerWidgetBox;
    protected:
      explicit AbstractButtonBox(MathSequence *content = nullptr, ContainerType _type = PushButton);
    
      virtual void resize_default_baseline(Context *context) override;
      
      virtual ContainerType default_container_type() { return PushButton; }
      
    public:
      virtual bool expand(const BoxSize &size) override;
      
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_move(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
      
      virtual void click() = 0;
  };
  
  class ButtonBox: public AbstractButtonBox {
      using base = AbstractButtonBox;
    public:
      explicit ButtonBox(MathSequence *content = nullptr);
      
      // Box::try_create<ButtonBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void reset_style() override;
      
      virtual void click() override;
    
    protected:
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::ButtonBox; }
  };
}

#endif // RICHMATH__BOXES__BUTTONBOX_H__INCLUDED
