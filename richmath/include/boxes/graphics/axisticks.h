#ifndef RICHMATH__BOXES__GRAPHICS__AXISTICKS_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__AXISTICKS_H__INCLUDED

#include <boxes/abstractsequence.h>
#include <util/array.h>


namespace richmath {
  class AxisTicks final : public Box {
      using base = Box;
    protected:
      virtual ~AxisTicks();
    public:
      AxisTicks();
      
      virtual bool try_load_from_object(Expr object, BoxInputFlags options) override;
      void             load_from_object(Expr object, BoxInputFlags options);
      
      virtual Box *item(int i) override { return _labels[i];       }
      virtual int count() override {      return _labels.length(); }
      
      AbstractSequence *label(   int i) { return _labels[   i]; }
      double           &position(int i) { return _positions[i]; }
      float             max_rel_tick(){   return _max_rel_tick; }
      
      bool is_visible(double t);
      
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      
      void calc_bounds(float *x1, float *y1, float *x2, float *y2);
      BoxSize all_labels_extents();
      
      virtual Box *remove(int *index) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
        
      virtual bool selectable(int i = -1) override { return false; }
      
      void get_tick_position(
        double  t,
        float  *x,
        float  *y);
        
      bool axis_hidden() {         return get_flag(AxisHiddenBit); }
      void axis_hidden(bool value) {   change_flag(AxisHiddenBit, value); }
    
    protected:
      void set_count(int new_count);
      
      void draw_tick(Canvas &canvas, float x, float y, float length);
      
      void get_label_center(
        double  t,
        float   label_width,
        float   label_height,
        float  *x,
        float  *y);
        
      void get_label_position(int i, float *x, float *y);
      
      static double get_square_rect_radius(
        float width,
        float height,
        float dx,
        float dy);
    
    protected:
      enum {
        AxisHiddenBit = base::NumFlagsBits,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
    private:
      Array<AbstractSequence*>  _labels;
      Array<double>             _positions;
      Array<float>              _rel_tick_pos;
      Array<float>              _rel_tick_neg;
      float                     _max_rel_tick;
      
    public:
      float start_x;
      float start_y;
      float end_x;
      float end_y;
      float label_direction_x;
      float label_direction_y;
      float label_center_distance_min;
      float tick_length_factor;
      float extra_offset;
      double start_position;
      double end_position;
      double ignore_label_position;
      Expr _expr;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__AXISTICKS_H__INCLUDED
