#ifndef __BOXES__CONTAINERWIDGETBOX_H__
#define __BOXES__CONTAINERWIDGETBOX_H__

#include <boxes/stylebox.h>
#include <gui/control-painter.h>


namespace richmath {
  class ContainerWidgetBox: public AbstractStyleBox {
    public:
      explicit ContainerWidgetBox(ContainerType _type, MathSequence *content = 0);
      
      virtual ControlState calc_state(Context *context);
      
      virtual void resize(Context *context) override;
      virtual void paint(Context *context) override;
      
      virtual Box *mouse_sensitive() override { return this; }
      virtual void on_mouse_enter() override;
      virtual void on_mouse_exit() override;
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_move(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
      virtual void on_mouse_cancel() override;
      
      virtual void on_enter() override;
      virtual void on_exit() override;
      
    protected:
      SharedPtr<BoxAnimation> animation;
      ContainerType type;
      ControlState  old_state;
      
      bool mouse_inside;
      bool mouse_left_down;
      bool mouse_middle_down;
      bool mouse_right_down;
      bool selection_inside;
  };
}

#endif // __BOXES__CONTAINERWIDGETBOX_H__
