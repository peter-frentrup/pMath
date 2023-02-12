#ifndef RICHMATH__BOXES__CONTAINERWIDGETBOX_H__INCLUDED
#define RICHMATH__BOXES__CONTAINERWIDGETBOX_H__INCLUDED

#include <boxes/stylebox.h>
#include <gui/control-painter.h>


namespace richmath {
  class ContainerWidgetBox: public AbstractStyleBox, public ControlContext {
      using base = AbstractStyleBox;
    public:
      explicit ContainerWidgetBox(ContainerType _type, AbstractSequence *content = nullptr);
      
      virtual MathSequence *as_inline_span() override { return nullptr; }
      
      virtual ControlState calc_state(Context &context);
      ControlState latest_state() { return old_state; }
      
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
    
      enum {
        MouseInsideBit = base::NumFlagsBits,
        MouseLeftDownBit,
        MouseMiddleDownBit,
        MouseRightDownBit,
        SelectionInsideBit,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool mouse_inside() {            return get_flag(MouseInsideBit); }
      void mouse_inside(bool value) {      change_flag(MouseInsideBit, value); }
      bool mouse_left_down() {         return get_flag(MouseLeftDownBit); }
      void mouse_left_down(bool value) {   change_flag(MouseLeftDownBit, value); }
      bool mouse_middle_down() {       return get_flag(MouseMiddleDownBit); }
      void mouse_middle_down(bool value) { change_flag(MouseMiddleDownBit, value); }
      bool mouse_right_down() {        return get_flag(MouseRightDownBit); }
      void mouse_right_down(bool value) {  change_flag(MouseRightDownBit, value); }
      bool selection_inside() {        return get_flag(SelectionInsideBit); }
      void selection_inside(bool value) {  change_flag(SelectionInsideBit, value); }
    
    protected:
      SharedPtr<BoxAnimation> animation;
      ContainerType type;
      ControlState  old_state;
      uint16_t      _unused_u16;
  };
}

#endif // RICHMATH__BOXES__CONTAINERWIDGETBOX_H__INCLUDED
