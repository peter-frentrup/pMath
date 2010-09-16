#ifndef __BOXES__CONTAINERWIDGETBOX_H__
#define __BOXES__CONTAINERWIDGETBOX_H__

#include <boxes/stylebox.h>
#include <gui/control-painter.h>

namespace richmath{
  class ContainerWidgetBox: public AbstractStyleBox {
    public:
      explicit ContainerWidgetBox(ContainerType _type, MathSequence *content = 0);
      
      virtual ControlState calc_state(Context *context);
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      
      virtual Box *mouse_sensitive(){ return this; }
      virtual void on_mouse_enter();
      virtual void on_mouse_exit();
      virtual void on_mouse_down(MouseEvent &event);
      virtual void on_mouse_move(MouseEvent &event);
      virtual void on_mouse_up(  MouseEvent &event);
      virtual void on_mouse_cancel();
      
      virtual void on_enter();
      virtual void on_exit();
    
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
