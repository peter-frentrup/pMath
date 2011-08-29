#ifndef __GUI__GTK__RMGTK_WIDGET_H__
#define __GUI__GTK__RMGTK_WIDGET_H__

#ifndef RICHMATH_USE_GTK_GUI
#error this header is gtk specific
#endif

#include <gui/document.h>
#include <gui/native-widget.h>
#include <gui/gtk/basic-gtk-widget.h>
#include <gui/gtk/mgtk-cursors.h>


namespace richmath {
  // Must call init() immediately init after the construction of a derived object!
  class MathGtkWidget: public NativeWidget, public BasicGtkWidget {
    protected:
      virtual void after_construction();
      
    public:
      MathGtkWidget(Document *doc);
      virtual ~MathGtkWidget();
      
      virtual void window_size(float *w, float *h);
      virtual void page_size(float *w, float *h) {
        window_size(w, h);
      }
      
      virtual bool is_scrollable() { return true; }
      virtual bool autohide_vertical_scrollbar() { return _autohide_vertical_scrollbar; }
      virtual void scroll_pos(float *x, float *y);
      virtual void scroll_to(float x, float y);
      
      virtual void show_tooltip(Expr boxes);
      virtual void hide_tooltip();
      
      virtual bool is_scaleable() { return true; }
      
      virtual double message_time();
      virtual double double_click_time();
      virtual void double_click_dist(float *dx, float *dy);
      virtual void do_drag_drop(Box *src, int start, int end);
      virtual bool cursor_position(float *x, float *y);
      
      virtual void bring_to_front();
      virtual void close() {}
      virtual void invalidate();
      virtual void invalidate_options();
      virtual void invalidate_rect(float x, float y, float w, float h);
      virtual void force_redraw();
      
      virtual void set_cursor(CursorType type);
      
      virtual void running_state_changed();
      
      virtual bool is_mouse_down();
      
      virtual void beep();
      
      virtual bool register_timed_event(SharedPtr<TimedEvent> event);
      
      GtkAdjustment *hadjustment() { return _hadjustment; }
      GtkAdjustment *vadjustment() { return _vadjustment; }
      void hadjustment(GtkAdjustment *ha);
      void vadjustment(GtkAdjustment *va);
      
      GtkIMContext *im_context() { return _im_context; }
      
    public:
      bool _autohide_vertical_scrollbar;
      
    protected:
      int _mouse_down_button;
      
    private:
      CursorType cursor;
      bool mouse_moving;
      
      bool is_painting;
      bool is_blinking;
      bool ignore_key_release;
      
      int old_width;
      
      MathGtkCursors cursors;
      
      GtkAdjustment *_hadjustment;
      GtkAdjustment *_vadjustment;
      GtkIMContext  *_im_context;
      SelectionReference _im_context_pos;
      
    private:
      static void im_commit_callback(
        GtkIMContext  *context,
        const char    *str,
        MathGtkWidget *self
      ) {
        self->on_im_commit(str);
      }
      
      static void im_preedit_changed_callback(
        GtkIMContext  *context,
        MathGtkWidget *self
      ) {
        self->on_im_preedit_changed();
      }
      
      static void drag_data_get_callback(
        GtkWidget        *widget,
        GdkDragContext   *drag_context,
        GtkSelectionData *data,
        guint             info,
        guint             time,
        MathGtkWidget    *self
      ) {
        self->on_drag_data_get(drag_context, data, info, time);
      }
      
      static void drag_data_delete_callback(
        GtkWidget      *widget,
        GdkDragContext *drag_context,
        MathGtkWidget  *self
      ) {
        self->on_drag_data_delete(drag_context);
      }
      
      static void drag_data_received_callback(
        GtkWidget        *widget,
        GdkDragContext   *drag_context,
        gint              x,
        gint              y,
        GtkSelectionData *data,
        guint             info,
        guint             time,
        MathGtkWidget    *self
      ) {
        return self->on_drag_data_received(drag_context, x, y, data, info, time);
      }
      
      static void drag_end_callback(
        GtkWidget      *widget,
        GdkDragContext *drag_context,
        MathGtkWidget  *self
      ) {
        self->on_drag_end(drag_context);
      }
      
      static gboolean drag_motion_callback(
        GtkWidget      *widget,
        GdkDragContext *drag_context,
        gint            x,
        gint            y,
        guint           time,
        MathGtkWidget  *self
      ) {
        return self->on_drag_motion(drag_context, x, y, time);
      }
      
      static gboolean drag_drop_callback(
        GtkWidget      *widget,
        GdkDragContext *drag_context,
        gint            x,
        gint            y,
        guint           time,
        MathGtkWidget  *self
      ) {
        return self->on_drag_drop(drag_context, x, y, time);
      }
      
    protected:
      virtual void update_im_cursor_location();
      
      virtual void on_im_commit(const char *str);
      virtual void on_im_preedit_changed();
      
      virtual void on_drag_data_get(GdkDragContext *context, GtkSelectionData *data, guint info, guint time);
      virtual void on_drag_data_delete(GdkDragContext *context);
      virtual void on_drag_data_received(GdkDragContext *context, int x, int y, GtkSelectionData *data, guint info, guint time);
      virtual void on_drag_end(GdkDragContext *context);
      virtual bool on_drag_motion(GdkDragContext *context, int x, int y, guint time);
      virtual bool on_drag_drop(GdkDragContext *context, int x, int y, guint time);
      
      virtual void paint_background(Canvas *canvas);
      virtual void paint_canvas(Canvas *canvas, bool resize_only);
      virtual void handle_mouse_move(MouseEvent &event);
      
      virtual bool on_map(GdkEvent *e);
      virtual bool on_unmap(GdkEvent *e);
      virtual bool on_expose(GdkEvent *e);
      virtual bool on_focus_in(GdkEvent *e);
      virtual bool on_focus_out(GdkEvent *e);
      virtual bool on_key_press(GdkEvent *e);
      virtual bool on_key_release(GdkEvent *e);
      virtual bool on_button_press(GdkEvent *e);
      virtual bool on_button_release(GdkEvent *e);
      virtual bool on_motion_notify(GdkEvent *e);
      virtual bool on_leave_notify(GdkEvent *e);
      virtual bool on_scroll(GdkEvent *e);
      
      static gboolean blink_caret(gpointer id_as_ptr);
  };
}

#endif // __GUI__GTK__RMGTK_WIDGET_H__
