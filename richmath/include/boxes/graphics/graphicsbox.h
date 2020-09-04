#ifndef RICHMATH__BOXES__GRAPHICS__GRAPHICSBOX_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__GRAPHICSBOX_H__INCLUDED

#include <boxes/box.h>
#include <boxes/graphics/graphicselement.h>

#include <graphics/buffer.h>

namespace richmath {
  const int GraphicsPartNone            = -1;
  const int GraphicsPartBackground      = -2;
  const int GraphicsPartSizeRight       = -3;
  const int GraphicsPartSizeBottom      = -4;
  const int GraphicsPartSizeBottomRight = -5;
  
  class AxisTicks;
  
  enum AxisIndex{
    AxisIndexX = 0,
    AxisIndexY,
    AxisIndexLeft,
    AxisIndexRight,
    AxisIndexBottom,
    AxisIndexTop
  };
  
  class GraphicsBox: public Box {
      class Impl;
    protected:
      virtual ~GraphicsBox();
    public:
      GraphicsBox();
      
      // Box::try_create<GraphicsBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      void reset_user_options();
      void set_user_default_options(Expr rules);
      Expr get_user_options();
      
      virtual Box *item(int i) override;
      virtual int count() override;
      
      virtual void invalidate() override;
      virtual bool request_repaint(float x, float y, float w, float h) override;
      
      virtual bool expand(const BoxSize &size) override;
      virtual void resize(Context *context) override;
      virtual void paint(Context *context) override;
      
      virtual void reset_style() override;
      
      virtual Box *remove(int *index) override { return this; }
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual VolatileSelection mouse_selection(float x, float y, bool *was_inside_start) override;
        
      virtual bool selectable(int i = -1) override;
        
      virtual Box *normalize_selection(int *start, int *end) override;
      
      int calc_mouse_over_part(float x, float y);
      void transform_inner_to_outer(cairo_matrix_t *mat);
      
      virtual Box *mouse_sensitive() override;
      virtual void on_mouse_enter() override;
      virtual void on_mouse_exit() override;
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_move(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
      
    protected:
      int   mouse_over_part; // GraphicsPartXXX
      float mouse_down_x;
      float mouse_down_y;
      
      float margin_left;
      float margin_right;
      float margin_top;
      float margin_bottom;
      
      float em;
      AxisTicks *ticks[6]; // indexd by enum AxisIndex
      
      GraphicsElementCollection elements;
      Expr                      error_boxes_expr;
      SharedPtr<Buffer>         cached_bitmap;
      
      bool user_has_changed_size;
      bool is_currently_resizing;
      
    protected:
      void calculate_size(const float *optional_expand_width = 0);
      
      void try_get_axes_origin(const GraphicsBounds &bounds, double *ox, double *oy);
      void calculate_axes_origin(const GraphicsBounds &bounds, double *ox, double *oy);
      
      GraphicsBounds calculate_plotrange();
      bool have_frame(bool *left, bool *right, bool *bottom, bool *top);
      bool have_axes(bool *x, bool *y);
      
      Expr generate_default_ticks(double min, double max, bool with_labels);
      Expr generate_ticks(const GraphicsBounds &bounds, enum AxisIndex part);
      Expr get_ticks(const GraphicsBounds &bounds, enum AxisIndex part);
      
      bool set_axis_ends(enum AxisIndex part, const GraphicsBounds &bounds); // true if ends changed
      
      void resize_axes(Context *context);
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__GRAPHICSBOX_H__INCLUDED
