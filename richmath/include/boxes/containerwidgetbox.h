#ifndef RICHMATH__BOXES__CONTAINERWIDGETBOX_H__INCLUDED
#define RICHMATH__BOXES__CONTAINERWIDGETBOX_H__INCLUDED

#include <boxes/stylebox.h>
#include <gui/control-painter.h>


namespace richmath {
  class ContainerWidgetBox: public AbstractStyleBox, public ControlContext {
      using base = AbstractStyleBox;
    public:
      explicit ContainerWidgetBox(ContainerType _type, MathSequence *content = nullptr);
      
      virtual ControlState calc_state(Context &context);
      
      virtual void paint(Context &context) override;
      
      virtual void reset_style() override;
      
      virtual Box *mouse_sensitive() override { return this; }
      virtual void on_mouse_enter() override;
      virtual void on_mouse_exit() override;
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_move(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
      virtual void on_mouse_cancel() override;
      
      virtual void on_enter() override;
      virtual void on_exit() override;
      
      virtual bool is_foreground_window() override;
      virtual bool is_focused_widget() override;
      virtual bool is_using_dark_mode() override;
      virtual int dpi() override;
    
    protected:
      virtual void resize_default_baseline(Context &context) override;
      
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

#endif // RICHMATH__BOXES__CONTAINERWIDGETBOX_H__INCLUDED
