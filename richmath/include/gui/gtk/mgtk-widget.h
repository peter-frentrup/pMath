#ifndef __GUI__GTK__RMGTK_WIDGET_H__
#define __GUI__GTK__RMGTK_WIDGET_H__

#ifndef RICHMATH_USE_GTK_GUI
  #error this header is gtk specific
#endif

#include <gui/document.h>
#include <gui/native-widget.h>
#include <gui/gtk/basic-gtk-widget.h>
#include <gui/gtk/mgtk-cursors.h>

namespace richmath{
  // Must call init() immediately init after the construction of a derived object!
  class MathGtkWidget: public NativeWidget, public BasicGtkWidget {
    protected:
      virtual void after_construction();
    
    public:
      MathGtkWidget(Document *doc);
      virtual ~MathGtkWidget();
      
      virtual void window_size(float *w, float *h);
      virtual void page_size(float *w, float *h){
        window_size(w, h); 
      }
      
      virtual bool is_scrollable(){ return true; }
      virtual bool autohide_vertical_scrollbar(){ return _autohide_vertical_scrollbar; }
      virtual void scroll_pos(float *x, float *y);
      virtual void scroll_to(float x, float y);
      
      virtual void show_tooltip(Expr boxes);
      virtual void hide_tooltip();
      
      virtual bool is_scaleable(){ return true; }
      
      virtual double message_time();
      virtual double double_click_time();
      virtual void double_click_dist(float *dx, float *dy);
      virtual void do_drag_drop(Box *src, int start, int end);
      virtual bool cursor_position(float *x, float *y);
      
      virtual void close(){}
      virtual void invalidate();
      virtual void force_redraw();
      
      virtual void set_cursor(CursorType type);
      
      virtual void running_state_changed();
      
      virtual bool is_mouse_down();
      
      virtual void beep();
    
      virtual bool register_timed_event(SharedPtr<TimedEvent> event);
    
      GtkAdjustment *hadjustment(){ return _hadjustment; }
      GtkAdjustment *vadjustment(){ return _vadjustment; }
      void hadjustment(GtkAdjustment *ha);
      void vadjustment(GtkAdjustment *va);
      
    public:
      bool _autohide_vertical_scrollbar;
      
    private:
      CursorType cursor;
      bool mouse_moving;
      
      bool is_painting;
      bool is_blinking;
      
      int old_width;
      
      MathGtkCursors cursors;
      
      GtkAdjustment *_hadjustment;
      GtkAdjustment *_vadjustment;
    
    protected:
      virtual void paint_background(Canvas *canvas);
      virtual void paint_canvas(Canvas *canvas, bool resize_only);
      
      virtual bool on_expose(GdkEvent *e);
      virtual bool on_focus_in(GdkEvent *e);
      virtual bool on_key_press(GdkEvent *e);
      virtual bool on_key_release(GdkEvent *e);
      virtual bool on_button_press(GdkEvent *e);
      virtual bool on_button_release(GdkEvent *e);
      virtual bool on_motion_notify(GdkEvent *e);
      virtual bool on_leave_notify(GdkEvent *e);
      
      static gboolean blink_caret(gpointer id_as_ptr);
  };
}

#endif // __GUI__GTK__RMGTK_WIDGET_H__
