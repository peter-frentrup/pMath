#ifndef RICHMATH__BOXES__EMPTYWIDGETBOX_H__INCLUDED
#define RICHMATH__BOXES__EMPTYWIDGETBOX_H__INCLUDED

#include <boxes/box.h>
#include <gui/control-painter.h>


namespace richmath {
  class EmptyWidgetBox: public Box, public ControlContext {
      using base = Box;
    public:
      virtual Box *item(int i) override { return 0; }
      virtual int count() override { return 0; }
      virtual int length() override { return 0; }
      
      virtual ControlState calc_state(Context &context);
      
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      virtual Box *remove(int *index) override { return this; }
      
      virtual void dynamic_updated() override;
      
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual Box *mouse_sensitive() override { return this; }
      virtual void on_mouse_enter() override;
      virtual void on_mouse_exit() override;
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_move(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
      virtual void on_mouse_cancel() override;
      
      virtual void click();
      
      virtual bool is_foreground_window() override;
      virtual bool is_focused_widget() override;
      virtual bool is_using_dark_mode() override;
      virtual int dpi() override;
    
    protected:
      explicit EmptyWidgetBox(ContainerType _type);
      
    protected:
      SharedPtr<BoxAnimation> animation;
      ContainerType type;
      ContainerType old_type;
      ControlState  old_state;
      
      bool mouse_inside : 1;
      bool mouse_left_down : 1;
      bool mouse_middle_down : 1;
      bool mouse_right_down : 1;
      bool must_update : 1;
  };
}

#endif // RICHMATH__BOXES__EMPTYWIDGETBOX_H__INCLUDED
