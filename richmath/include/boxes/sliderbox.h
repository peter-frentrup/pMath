#ifndef RICHMATH__BOXES__SLIDERBOX_H__INCLUDED
#define RICHMATH__BOXES__SLIDERBOX_H__INCLUDED

#include <boxes/emptywidgetbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class SliderBox final : public EmptyWidgetBox {
      using base = EmptyWidgetBox;
      class Impl;
    protected:
      virtual ~SliderBox();
    public:
      explicit SliderBox();
      
      // Box::try_create<SliderBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Box *item(int i) override { return nullptr; }
      virtual int count() override { return 0; }
      virtual int length() override { return 0; }
      
      virtual ControlState calc_state(Context &context) override;
      
      virtual bool expand(Context &context, const BoxSize &size) override;
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      virtual Box *remove(int *index) override { return this; }
      
      virtual Expr to_pmath_symbol() override;
      
      virtual void reset_style() override;
      
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      virtual VolatileSelection dynamic_to_literal(int start, int end) override;
      
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual void on_mouse_exit() override;
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_move(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
      virtual void on_mouse_cancel() override;
      
      const Interval<double> &range_interval() const { return _range_interval; }
      double                  range_step()     const { return _range_step; }
      double                  range_value()    const { return _range_value; }
      
      void range_value(double new_val);
      
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::SliderBox; }

    private:
      Interval<double> _range_interval;
      double           _range_step;
      double           _range_value;
      double           _animation_start_value;
      double           _animation_start_time;
      Expr             _range_expr;
      Dynamic          _dynamic;
      
      float _thumb_width;
      float _channel_width;
      
      enum {
        HaveDrawnBit = base::NumFlagsBits,
        MouseOverThumbBit,
        UseDoubleValuesBit,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool have_drawn() {              return get_flag(HaveDrawnBit); }
      void have_drawn(bool value) {        change_flag(HaveDrawnBit, value); }
      bool mouse_over_thumb() {        return get_flag(MouseOverThumbBit); }
      void mouse_over_thumb(bool value) {  change_flag(MouseOverThumbBit, value); }
      bool use_double_values() {       return get_flag(UseDoubleValuesBit); }
      void use_double_values(bool value) { change_flag(UseDoubleValuesBit, value); }
  };
}

#endif // RICHMATH__BOXES__SLIDERBOX_H__INCLUDED
