#ifndef RICHMATH__GUI__WIN32_TOOLTIP_WINDOW_H__INCLUDED
#define RICHMATH__GUI__WIN32_TOOLTIP_WINDOW_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/win32/win32-widget.h>


namespace richmath {
  class Win32TooltipWindow: public Win32Widget {
    protected:
      virtual ~Win32TooltipWindow();
      
    public:
      static void move_global_tooltip();
      static void show_global_tooltip(Box *source, Expr boxes, SharedPtr<Stylesheet> stylesheet);
      static void hide_global_tooltip();
      static void delete_global_tooltip();
      
      virtual Vector2F page_size() override;
      
      virtual bool is_scrollable() override { return false; }
      
      virtual bool is_foreground_window() override { return true; }
      virtual bool is_focused_widget() override { return false; }
      virtual bool is_using_dark_mode() override;
      virtual int dpi() override;
      
    protected:
      Expr  _content_expr;
      
      int best_width;
      int best_height;
      
    protected:
      Win32TooltipWindow();
      virtual void after_construction() override;
      
      void resize(bool just_move);
      virtual void paint_canvas(Canvas &canvas, bool resize_only) override;
      
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override;
      
    private:
      void init_tooltip_class();
  };
}

#endif // RICHMATH__GUI__WIN32_TOOLTIP_WINDOW_H__INCLUDED
