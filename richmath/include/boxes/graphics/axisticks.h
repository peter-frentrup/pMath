#ifndef __BOXES__GRAPHICS__AXISTICKS_H__
#define __BOXES__GRAPHICS__AXISTICKS_H__

#include <boxes/box.h>
#include <util/array.h>


namespace richmath {
  class AxisTicks: public Box {
    public:
      AxisTicks();
      virtual ~AxisTicks();
      
      virtual bool try_load_from_object(Expr object, int options); // BoxOptionXXX
      void             load_from_object(Expr object, int options); // BoxOptionXXX
      
      virtual Box *item(int i) { return _labels[i];       }
      virtual int count() {      return _labels.length(); }
      
      AbstractSequence *label(   int i) { return _labels[   i]; }
      double           &position(int i) { return _positions[i]; }
      float             max_rel_tick(){   return _max_rel_tick; }
      
      bool is_visible(double t){ return start_position <= t && t <= end_position; }
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      
      void calc_bounds(float *x1, float *y1, float *x2, float *y2);
      BoxSize all_labels_extents();
      
      virtual Box *remove(int *index);
      
      virtual Expr to_pmath_symbol() { return Symbol(PMATH_SYMBOL_LIST); }
      virtual Expr to_pmath(int flags); // BoxFlagXXX
      
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start);
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix);
        
      virtual bool selectable(int i = -1) { return false; }
      
      void get_tick_position(
        double  t,
        float  *x,
        float  *y);
        
    protected:
      void set_count(int new_count);
      
      void draw_tick(Canvas *canvas, float x, float y, float length);
      
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
      bool axis_hidden;
  };
}

#endif // __BOXES__GRAPHICS__AXISTICKS_H__
