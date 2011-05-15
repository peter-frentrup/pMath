#ifndef __RICHMATH__BOXES__SLIDERBOX_H__
#define __RICHMATH__BOXES__SLIDERBOX_H__

#include <boxes/box.h>
#include <eval/dynamic.h>
#include <gui/control-painter.h>

namespace richmath{
  class SliderBox: public Box {
    public:
      virtual ~SliderBox();
      
      static SliderBox *create(Expr expr);
      
      virtual Box *item(int i){ return 0; }
      virtual int count(){ return 0; }
      virtual int length(){ return 0; }
      
      //virtual bool expand(const BoxSize &size);
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      virtual Box *remove(int *index){ return this; }
      
      virtual Expr to_pmath_symbol(){ return Symbol(PMATH_SYMBOL_SLIDERBOX); }
      virtual Expr to_pmath(bool parseable);
      
      virtual void dynamic_updated();
      virtual void dynamic_finished(Expr info, Expr result);
      
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start);

      virtual Box *mouse_sensitive(){ return this; }
      virtual void on_mouse_enter();
      virtual void on_mouse_exit();
      virtual void on_mouse_down(MouseEvent &event);
      virtual void on_mouse_move(MouseEvent &event);
      virtual void on_mouse_up(  MouseEvent &event);
      
    protected:
      explicit SliderBox();
      
      float calc_thumb_pos(double val);
      double mouse_to_val(double mouse_x);
      void assign_dynamic_value(double d);
    
    protected:
      double range_min;
      double range_max;
      double range_step;
      double range_value;
      Expr range;
      Dynamic dynamic;
      
      SharedPtr<BoxAnimation> animation;
      ControlState old_thumb_state;
      ControlState new_thumb_state;
      float thumb_width;
      float channel_width;
      bool must_update;
      bool have_drawn;
      bool mouse_down;
      bool use_double_values;
  };
}

#endif // __RICHMATH__BOXES__SLIDERBOX_H__
