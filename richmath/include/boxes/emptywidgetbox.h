#ifndef __BOXES__EMPTYWIDGETBOX_H__
#define __BOXES__EMPTYWIDGETBOX_H__

#include <boxes/box.h>
#include <gui/control-painter.h>


namespace richmath {
  class EmptyWidgetBox: public Box {
    public:
      virtual Box *item(int i) { return 0; }
      virtual int count() { return 0; }
      virtual int length() { return 0; }
      
      virtual ControlState calc_state(Context *context);
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      virtual Box *remove(int *index) { return this; }
      
      void dynamic_updated();
      
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start);
        
      virtual Box *mouse_sensitive() { return this; }
      virtual void on_mouse_enter();
      virtual void on_mouse_exit();
      virtual void on_mouse_down(MouseEvent &event);
      virtual void on_mouse_move(MouseEvent &event);
      virtual void on_mouse_up(MouseEvent &event);
      virtual void on_mouse_cancel();
      
    protected:
      explicit EmptyWidgetBox(ContainerType _type);
      
    protected:
      SharedPtr<BoxAnimation> animation;
      ContainerType type;
      ContainerType old_type;
      ControlState  old_state;
      
      bool mouse_inside;
      bool mouse_left_down;
      bool mouse_middle_down;
      bool mouse_right_down;
      bool must_update;
  };
}

#endif // __BOXES__EMPTYWIDGETBOX_H__
