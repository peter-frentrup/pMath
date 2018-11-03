#ifndef RICHMATH__GUI__GTK__MGTK_TOOLTIP_WINDOW_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_TOOLTIP_WINDOW_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <gui/gtk/mgtk-widget.h>


namespace richmath {
  class MathGtkTooltipWindow: public MathGtkWidget {
    public:
      virtual ~MathGtkTooltipWindow();
      
      static void move_global_tooltip();
      static void show_global_tooltip(Expr boxes, SharedPtr<Stylesheet> stylesheet);
      static void hide_global_tooltip();
      static void delete_global_tooltip();
      
      virtual void page_size(float *w, float *h) override;
      
      virtual bool is_scrollable() override { return false; }
      
    protected:
      Expr  _content_expr;
      
      int best_width;
      int best_height;
      
    protected:
      MathGtkTooltipWindow();
      virtual void after_construction() override;
      
      void resize(bool just_move);
      virtual void paint_canvas(Canvas *canvas, bool resize_only) override;
  };
}

#endif // RICHMATH__GUI__GTK__MGTK_TOOLTIP_WINDOW_H__INCLUDED
