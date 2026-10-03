#ifndef RICHMATH__GUI__FLTK__MFLTK_WIDGET_H__INCLUDED
#define RICHMATH__GUI__FLTK__MFLTK_WIDGET_H__INCLUDED

#ifndef RICHMATH_USE_FLTK_GUI
#  error This header is FLTK specific.
#endif

#include <gui/fltk/basic-fltk-widget.h>

#include <gui/native-widget.h>

class Fl_Scrollbar;

namespace richmath {
  // Must call init() immediately after the construction of a derived object!
  class MathFltkWidget: public NativeWidget, public BasicFltkWidget {
      class Impl;
    protected:
      virtual void after_construction() override;
      
    public:
      explicit MathFltkWidget(Document *doc);
      virtual ~MathFltkWidget();
    
      virtual Vector2F window_size() override;
      virtual Vector2F page_size() override { return window_size(); }
      virtual Vector2F monitor_size() override;
      
      virtual bool is_scrollable() override { return true; }
      virtual bool autohide_vertical_scrollbar() override { return _autohide_vertical_scrollbar; }
      virtual Point scroll_pos() override;
      virtual bool scroll_to(Point pos) override;
      
      virtual void show_tooltip(Box *source, Expr boxes) override;
      virtual void hide_tooltip() override;
      virtual Document *try_create_popup_window(const SelectionReference &anchor) override;
      
      virtual bool is_scaleable() override { return true; }
      
      virtual double message_time() override;
      virtual double double_click_time() override;
      virtual Vector2F double_click_dist() override;
      virtual void do_drag_drop(const VolatileSelection &src, MouseEvent &event) override;
      
      virtual void bring_to_front() override;
      virtual void close() override {}
      virtual void invalidate() override;
      virtual void invalidate_options() override;
      virtual void invalidate_rect(const RectangleF &rect) override;
      virtual void force_redraw() override;
      
      virtual void set_cursor(CursorType type) override;
      
      virtual void running_state_changed() override {}
      
      virtual bool is_mouse_down() override;
      
      virtual void beep() override;
      
      virtual bool register_timed_event(SharedPtr<TimedEvent> event) override;
      
      virtual String directory() override { return String(); }
      virtual void directory(String new_directory) override {}
      
      virtual String filename() override { return String(); }
      virtual void filename(String new_filename) override {}
      
      virtual String full_filename() override { return String(); }
      virtual void full_filename(String new_full_filename) override {}
      
      virtual void on_saved() override {}
      
      virtual bool is_focused_widget() override;
      virtual bool is_using_dark_mode() override { return false; }
      
    protected:
      virtual Fl_Scrollbar *vscroll() { return nullptr; }
      virtual Fl_Scrollbar *hscroll() { return nullptr; }
      
      virtual void paint_background(Canvas &canvas);
      virtual void paint_canvas(Canvas &canvas, bool resize_only);
      
      virtual void do_set_selected_document() {}
      
    protected:
      bool _autohide_vertical_scrollbar;
      
      CursorType cursor;
      int last_width;
  };
}

#endif // RICHMATH__GUI__FLTK__MFLTK_WIDGET_H__INCLUDED
