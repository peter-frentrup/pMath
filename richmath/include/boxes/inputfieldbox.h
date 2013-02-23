#ifndef __BOXES__INPUTFIELDBOX_H__
#define __BOXES__INPUTFIELDBOX_H__

#include <boxes/containerwidgetbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class InputFieldBox: public ContainerWidgetBox {
    public:
      InputFieldBox(MathSequence *content = 0);
      
      // Box::try_create<InputFieldBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, int opts);
      
      virtual ControlState calc_state(Context *context);
      
      virtual bool expand(const BoxSize &size);
      virtual void resize(Context *context);
      virtual void paint_content(Context *context);
      
      virtual void reset_style();
      
      virtual void scroll_to(float x, float y, float w, float h);
      virtual void scroll_to(Canvas *canvas, Box *child, int start, int end);
      
      virtual Box *remove(int *index);
      
      virtual Expr to_pmath_symbol() { return Symbol(PMATH_SYMBOL_INPUTFIELDBOX); }
      virtual Expr to_pmath(int flags);
      
      virtual void dynamic_updated();
      virtual void dynamic_finished(Expr info, Expr result);
      virtual Box *dynamic_to_literal(int *start, int *end);
      
      virtual void invalidate();
      
      virtual bool remove_inserts_placeholder() { return false; }
      
      virtual bool exitable();
      virtual bool selectable(int i = -1);
      
      virtual bool edit_selection(Context *context) { return true; }
      
      virtual void on_mouse_down(MouseEvent &event);
      virtual void on_mouse_move(MouseEvent &event);
      
      virtual void on_enter();
      virtual void on_exit();
      virtual void on_finish_editing();
      
      virtual void on_key_down(SpecialKeyEvent &event);
      virtual void on_key_press(uint32_t unichar);
      
      bool assign_dynamic();
      
    protected:
      bool must_update;
      bool invalidated;
      bool transparent;
      double last_click_time;
      float last_click_global_x;
      float last_click_global_y;
      float frame_x;
      
    public:
      Dynamic dynamic;
      Expr input_type;
  };
}

#endif // __BOXES__INPUTFIELDBOX_H__
