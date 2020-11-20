#ifndef __RICHMATHRICHMATH__BOXES__SLIDERBOX_H__INCLUDED
#define __RICHMATHRICHMATH__BOXES__SLIDERBOX_H__INCLUDED

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
      
      virtual bool expand(const BoxSize &size) override;
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      virtual Box *remove(int *index) override { return this; }
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
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
      
    protected:
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::SliderBox; }

    private:
      double range_min;
      double range_max;
      double range_step;
      double range_value;
      Expr range;
      Dynamic dynamic;
      
      float thumb_width;
      float channel_width;
      bool have_drawn;
      bool mouse_over_thumb;
      bool use_double_values;
  };
}

#endif // __RICHMATHRICHMATH__BOXES__SLIDERBOX_H__INCLUDED
