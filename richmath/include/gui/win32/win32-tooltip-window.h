#ifndef __GUI__WIN32_TOOLTIP_WINDOW_H__
#define __GUI__WIN32_TOOLTIP_WINDOW_H__

#ifndef RICHMATH_USE_WIN32_GUI
  #error this header is win32 specific
#endif

#include <gui/win32/win32-widget.h>


namespace richmath{
  class Win32TooltipWindow: public Win32Widget{
    public:
      virtual ~Win32TooltipWindow();
      
      static void move_global_tooltip();
      static void show_global_tooltip(Expr boxes);
      static void hide_global_tooltip();
      static void delete_global_tooltip();
      
      virtual void page_size(float *w, float *h);
      
      virtual bool is_scrollable(){ return false; }
      
    protected:
      Expr  _content_expr;
      bool  _align_left;
      
      int best_width;
      int best_height;
      
    protected:
      Win32TooltipWindow();
      virtual void after_construction();
      
      void resize(bool just_move);
      virtual void paint_canvas(Canvas *canvas, bool resize_only);
      
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam);
  };
}

#endif // __GUI__WIN32_TOOLTIP_WINDOW_H__
