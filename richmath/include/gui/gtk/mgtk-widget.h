#ifndef RICHMATH__GUI__GTK__RMGTK_WIDGET_H__INCLUDED
#define RICHMATH__GUI__GTK__RMGTK_WIDGET_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <gui/document.h>
#include <gui/native-widget.h>
#include <gui/gtk/basic-gtk-widget.h>
#include <gui/gtk/mgtk-cursors.h>


namespace richmath {
  // Must call init() immediately init after the construction of a derived object!
  class MathGtkWidget: public NativeWidget, public BasicGtkWidget {
    protected:
      virtual void after_construction() override;
      
    public:
      MathGtkWidget(Document *doc);
      virtual ~MathGtkWidget();
      
      virtual Vector2F window_size() override;
      virtual Vector2F page_size() override { return window_size(); }
      
      virtual bool is_scrollable() override { return true; }
      virtual bool autohide_vertical_scrollbar() override { return _autohide_vertical_scrollbar; }
      virtual Point scroll_pos() override;
      virtual void scroll_to(Point pos) override;
      
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
      
      virtual void running_state_changed() override;
      
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
      
      virtual bool is_focused_widget() override { return _focused; }
      virtual bool is_using_dark_mode() override { return has_dark_background(); }
      
      bool has_dark_background() { return _has_dark_background; }
      
      GtkAdjustment *hadjustment() { return _hadjustment; }
      GtkAdjustment *vadjustment() { return _vadjustment; }
      void hadjustment(GtkAdjustment *ha);
      void vadjustment(GtkAdjustment *va);
      
      GtkIMContext *im_context() { return _im_context; }
      
      void do_drop_data(GdkDragContext *context, GdkDragAction action, GdkAtom target, guint time);
      
    public:
      bool _autohide_vertical_scrollbar;
      
    protected:
      int _mouse_down_button;
      
    private:
      CursorType cursor;
      bool mouse_moving : 1;
      bool is_blinking : 1;
      bool ignore_key_release : 1;
      bool _has_dark_background : 1;
      ObservableValue<bool> _focused;
      
      ObservableValue<int> old_width;
      
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
      
      virtual void paint_background(Canvas &canvas);
      virtual void paint_canvas(Canvas &canvas, bool resize_only);
      virtual void on_changed_dark_mode();
      virtual void handle_mouse_move(MouseEvent &event);
      
      virtual bool on_map(GdkEvent *e);
      virtual bool on_unmap(GdkEvent *e);
      virtual bool on_draw(cairo_t *cr);
      virtual bool on_focus_in(GdkEvent *e);
      virtual bool on_focus_out(GdkEvent *e);
      virtual bool on_key_press(GdkEvent *e);
      virtual bool on_key_release(GdkEvent *e);
      virtual bool on_button_press(GdkEvent *e);
      virtual bool on_button_release(GdkEvent *e);
      virtual bool on_motion_notify(GdkEvent *e);
      virtual bool on_leave_notify(GdkEvent *e);
      virtual bool on_scroll(GdkEvent *e);
      
      GtkMenu *create_popup_menu(VolatileSelection src, ObjectStyleOptionName style_name = ContextMenu);
      
      virtual void do_set_current_document() {}
      static gboolean blink_caret(gpointer id_as_ptr);
      
    private:
      bool on_expose(GdkEvent *e);
  };
}

#endif // RICHMATH__GUI__GTK__RMGTK_WIDGET_H__INCLUDED
