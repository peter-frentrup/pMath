#ifndef __BOXES__INPUTBOX_H__
#define __BOXES__INPUTBOX_H__

#include <boxes/containerwidgetbox.h>

namespace richmath{
  class InputBox: public ContainerWidgetBox {
    public:
      InputBox(MathSequence *content = 0);
      
      virtual ControlState calc_state(Context *context);
      
      virtual bool expand(const BoxSize &size);
      virtual void resize(Context *context);
      virtual void paint_content(Context *context);
      
      virtual void scroll_to(float x, float y, float w, float h);
      
      virtual Box *remove(int *index);
      
      virtual Expr to_pmath(bool parseable);
      
      virtual bool remove_inserts_placeholder(){ return false; }
      
      virtual bool exitable();
      virtual bool selectable(int i = -1);
      
      virtual bool edit_selection(Context *context){ return true; }
      
      virtual void on_mouse_down(MouseEvent &event);
      virtual void on_mouse_move(MouseEvent &event);
      
      virtual void on_enter();
      virtual void on_exit();
      
      virtual void on_key_down(SpecialKeyEvent &event);
      virtual void on_key_press(uint32_t unichar);
      
    protected:
      bool transparent;
      bool autoscroll;
      long last_click_time;
      float last_click_global_x;
      float last_click_global_y;
      
      float frame_x;
  };
}

#endif // __BOXES__INPUTBOX_H__
