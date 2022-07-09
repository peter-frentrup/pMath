#ifndef RICHMATH__BOXES__INPUTFIELDBOX_H__INCLUDED
#define RICHMATH__BOXES__INPUTFIELDBOX_H__INCLUDED

#include <boxes/containerwidgetbox.h>
#include <eval/dynamic.h>


namespace richmath {
  enum class InputFieldType : uint16_t {
    Expression,
    HeldExpression,
    RawBoxes,
    String,
    Number,
  };
  
  class InputFieldBox final : public ContainerWidgetBox {
      using base = ContainerWidgetBox;
      class Impl;
    public:
      InputFieldBox(AbstractSequence *content = nullptr);
      
      // Box::try_create<InputFieldBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual ControlState calc_state(Context &context) override;
      
      virtual bool expand(const BoxSize &size) override;
      virtual void paint_content(Context &context) override;
      
      virtual void reset_style() override;
      
      virtual bool scroll_to(const RectangleF &rect) override;
      virtual bool scroll_to(Canvas &canvas, const VolatileSelection &child_sel) override;
      
      virtual Box *remove(int *index) override;
      
      virtual Expr to_pmath_symbol() override;
      
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      virtual VolatileSelection dynamic_to_literal(int start, int end) override;
      
      virtual void invalidate() override;
      
      virtual bool remove_inserts_placeholder() override { return false; }
      
      virtual bool exitable() override { return false; }
      virtual bool selection_exitable(bool vertical) override { return false; }
      virtual bool selectable(int i = -1) override;
      
      virtual bool edit_selection(SelectionReference &selection, EditAction action) override { return true; }
      
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_move(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
      
      virtual void on_enter() override;
      virtual void on_exit() override;
      virtual void on_finish_editing() override;
      void continue_assign_dynamic();
      
      virtual void on_key_down(SpecialKeyEvent &event) override;
      virtual void on_key_press(uint32_t unichar) override;
      
      InputFieldType input_type() { return (InputFieldType)_unused_u16; }
      
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      virtual void resize_default_baseline(Context &context) override;

      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::InputFieldBox; }
      
    protected:
      enum {
        InvalidatedBit = base::NumFlagsBits,
        MustUpdateBit,
        DidContinuousUpdatesBit,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool invalidated() {                  return get_flag(InvalidatedBit); }
      void invalidated(bool value) {            change_flag(InvalidatedBit, value); }
      bool must_update() {                  return get_flag(MustUpdateBit); }
      void must_update(bool value) {            change_flag(MustUpdateBit, value); }
      bool did_continuous_updates() {       return get_flag(DidContinuousUpdatesBit); }
      void did_continuous_updates(bool value) { change_flag(DidContinuousUpdatesBit, value); }
      
      void input_type(InputFieldType value) { _unused_u16 = (uint16_t)value; }
      
    protected:
      float frame_x;
      SharedPtr<TimedEvent> _continue_assign_dynamic_event;
      Expr _assigned_result;
      
    public:
      Dynamic dynamic;
  };
}

#endif // RICHMATH__BOXES__INPUTFIELDBOX_H__INCLUDED
