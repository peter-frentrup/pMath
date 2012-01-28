#ifndef __BOXES__GRAPHICS__GRAPHICSBOX_H__
#define __BOXES__GRAPHICS__GRAPHICSBOX_H__

#include <boxes/box.h>
#include <boxes/graphics/graphicselement.h>


namespace richmath {
  const int GraphicsPartNone            = -1;
  const int GraphicsPartBackground      = -2;
  const int GraphicsPartSizeRight       = -3;
  const int GraphicsPartSizeBottom      = -4;
  const int GraphicsPartSizeBottomRight = -5;
  
  class AxisTicks;
  
  class GraphicsBox: public Box {
    public:
      virtual ~GraphicsBox();
      
      static GraphicsBox *create(Expr expr, int opts);
      
      virtual Box *item(int i);
      virtual int count();
      
      virtual bool expand(const BoxSize &size);
      virtual void resize(Context *context);
      virtual void paint(Context *context);
    
      virtual Box *remove(int *index) { return this; }
      
      virtual Expr to_pmath_symbol(){ return Symbol(PMATH_SYMBOL_GRAPHICSBOX); }
      virtual Expr to_pmath(int flags); // BoxFlagXXX
      
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start);
        
      virtual bool selectable(int i = -1);
        
      virtual Box *normalize_selection(int *start, int *end);
      
      int calc_mouse_over_part(float x, float y);
      void transform_inner_to_outer(cairo_matrix_t *mat);
      
      virtual Box *mouse_sensitive();
      virtual void on_mouse_enter();
      virtual void on_mouse_exit();
      virtual void on_mouse_down(MouseEvent &event);
      virtual void on_mouse_move(MouseEvent &event);
      virtual void on_mouse_up(MouseEvent &event);
      
    protected:
      int   mouse_over_part; // GraphicsPartXXX
      float mouse_down_x;
      float mouse_down_y;
      
      float margin_left;
      float margin_right;
      float margin_top;
      float margin_bottom;
      
      float em;
      
      AxisTicks *x_axis_ticks;
      AxisTicks *y_axis_ticks;
      
      GraphicsElementCollection elements;
      Expr error_boxes_expr;
    
    protected:
      GraphicsBox();
      
      void calculate_size(const float *optional_expand_width = 0);
      void resize_axes(Context *context);
  };
}

#endif // __BOXES__GRAPHICS__GRAPHICSBOX_H__
