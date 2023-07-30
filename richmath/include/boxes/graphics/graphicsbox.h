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
  
  enum AxisIndex {
    AxisIndexX = 0,
    AxisIndexY,
    AxisIndexLeft,
    AxisIndexRight,
    AxisIndexBottom,
    AxisIndexTop
  };
  
  class GraphicsBox final : public Box {
      using base = Box;
      class Impl;
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
      virtual bool request_repaint(const RectangleF &rect) override;
      
      virtual bool expand(Context &context, const BoxSize &size) override;
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      
      virtual void reset_style() override;
      
      virtual Box *remove(int *index) override { return this; }
      
      virtual Expr to_pmath_symbol() override;
      
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual bool selectable(int i = -1) override;
        
      virtual VolatileSelection normalize_selection(int start, int end) override;
      
      int calc_mouse_over_part(float x, float y);
      void transform_inner_to_outer(cairo_matrix_t *mat);
      
      virtual Box *mouse_sensitive() override;
      virtual void on_mouse_enter() override;
      virtual void on_mouse_exit() override;
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_move(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
    
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::GraphicsBox; }

    protected:
      enum {
        UserHasChangedSizeBit = base::NumFlagsBits,
        IsCurrentlyResizingBit,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
    private:
      bool user_has_changed_size() {         return get_flag(UserHasChangedSizeBit); }
      void user_has_changed_size(bool value) {   change_flag(UserHasChangedSizeBit, value); }
      bool is_currently_resizing() {       return get_flag(IsCurrentlyResizingBit); }
      void is_currently_resizing(bool value) { change_flag(IsCurrentlyResizingBit, value); }
    
    private:
      int   mouse_over_part; // GraphicsPartXXX
      float mouse_down_x;
      float mouse_down_y;
      
      float margin_left;
      float margin_right;
      float margin_top;
      float margin_bottom;
      
      float em;
      AxisTicks *ticks[6]; // indexd by enum AxisIndex
      
      SharedPtr<Buffer>         cached_bitmap;
      GraphicsElementCollection elements;
      Expr                      error_boxes_expr;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__GRAPHICSBOX_H__INCLUDED
