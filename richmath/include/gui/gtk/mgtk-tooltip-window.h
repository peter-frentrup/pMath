#ifndef __GUI__GTK__MGTK_TOOLTIP_WINDOW_H__
#define __GUI__GTK__MGTK_TOOLTIP_WINDOW_H__

#include <gui/gtk/mgtk-widget.h>

namespace richmath{
  class MathGtkTooltipWindow: public MathGtkWidget {
    public:
      virtual ~MathGtkTooltipWindow();
      
      static void move_global_tooltip();
      static void show_global_tooltip(Expr boxes);
      static void hide_global_tooltip();
      static void delete_global_tooltip();
      
      virtual void page_size(float *w, float *h);
      
      virtual bool is_scrollable(){ return false; }
      
    protected:
      Expr  _content_expr;
      
      int best_width;
      int best_height;
      
    protected:
      MathGtkTooltipWindow();
      virtual void after_construction();
      
      void resize(bool just_move);
      virtual void paint_canvas(Canvas *canvas, bool resize_only);
  };
}

#endif // __GUI__GTK__MGTK_TOOLTIP_WINDOW_H__
