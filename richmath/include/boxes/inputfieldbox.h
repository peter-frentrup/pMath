#ifndef __BOXES__INPUTFIELDBOX_H__
#define __BOXES__INPUTFIELDBOX_H__

#include <boxes/containerwidgetbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class InputFieldBox: public ContainerWidgetBox {
    public:
      InputFieldBox(MathSequence *content = 0);
      
      // Box::try_create<InputFieldBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual ControlState calc_state(Context *context) override;
      
      virtual bool expand(const BoxSize &size) override;
      virtual void resize(Context *context) override;
      virtual void paint_content(Context *context) override;
      
      virtual void reset_style() override;
      
      virtual void scroll_to(float x, float y, float w, float h) override;
      virtual void scroll_to(Canvas *canvas, Box *child, int start, int end) override;
      
      virtual Box *remove(int *index) override;
      
      virtual Expr to_pmath_symbol() override { return Symbol(PMATH_SYMBOL_INPUTFIELDBOX); }
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      virtual Box *dynamic_to_literal(int *start, int *end) override;
      
      virtual void invalidate() override;
      
      virtual bool remove_inserts_placeholder() override { return false; }
      
      virtual bool exitable() override;
      virtual bool selectable(int i = -1) override;
      
      virtual bool edit_selection(Context *context) override { return true; }
      
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_move(MouseEvent &event) override;
      
      virtual void on_enter() override;
      virtual void on_exit() override;
      virtual void on_finish_editing() override;
      
      virtual void on_key_down(SpecialKeyEvent &event) override;
      virtual void on_key_press(uint32_t unichar) override;
      
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
