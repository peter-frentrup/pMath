#ifndef RICHMATH__GUI__GTK__MGTK_DOCUMENT_WINDOW_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_DOCUMENT_WINDOW_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <gui/common-document-windows.h>
#include <gui/gtk/basic-gtk-widget.h>
#include <gui/gtk/mgtk-icons.h>
#include <gui/gtk/mgtk-widget.h>

#include <eval/observable.h>


namespace richmath {
  class MathGtkDock;
  class MathGtkWorkingArea;
  
  class MathGtkDocumentWindow: public CommonDocumentWindow, public BasicGtkWidget, public ControlContext {
      class Impl;
    public:
      class DocumentPosition {
        public:
          DocumentPosition() {}
          DocumentPosition(FrontEndReference _id, int _x, int _y)
            : id(_id), x(_x), y(_y)
          {
          }
          
          FrontEndReference id;
          int x;
          int y;
      };
      
    protected:
      virtual void after_construction() override;
      
    public:
      MathGtkDocumentWindow();
      virtual ~MathGtkDocumentWindow();
      
      void update_dark_mode();
      void invalidate_options();
      
      bool            is_palette() {   return _window_frame == WindowFramePalette; }
      WindowFrameType window_frame() { return _window_frame; }
      
      void run_menucommand(Expr cmd);
      
      Document *top() {          return ((MathGtkWidget*)_top_area)->document();     }
      Document *document() {     return ((MathGtkWidget*)_working_area)->document(); }
      Document *bottom() {       return ((MathGtkWidget*)_bottom_area)->document();  }
      
      MathGtkWidget *top_area() {     return (MathGtkWidget*)_top_area;     }
      MathGtkWidget *working_area() { return (MathGtkWidget*)_working_area; }
      MathGtkWidget *bottom_area() {  return (MathGtkWidget*)_bottom_area;  }
      
      virtual void bring_to_front();
      virtual void close();
      
      virtual bool is_foreground_window() override { return _active; };
      virtual bool is_focused_widget() override { return _active; };
      virtual bool is_using_dark_mode() override { return _use_dark_mode; }
      virtual int dpi() override;
      
      virtual void reset_title() override;
      
      void reset_window_frame(){ window_frame(_window_frame); }
      void set_gravity();
      void set_initial_rect(int x, int y, int w, int h);
      
      void get_window_margins(int *left, int *right, int *top, int *bottom);
      void get_outer_rect(GdkRectangle *rect);
      const GdkRectangle &previous_rect() { return _previous_rect; }
      static bool test_rects_touch(const GdkRectangle &rect1, const GdkRectangle &rect2, int *maxdx, int *maxdy, GdkWindowEdge *edge);
      
      void move_palettes();
      
      virtual void on_idle_after_edit(MathGtkWidget *sender) { CommonDocumentWindow::on_idle_after_edit(); }
      
    protected:
      virtual void finish_apply_title(String displayed_title) override;
      void window_frame(WindowFrameType type);
      
      virtual bool on_configure(GdkEvent *e);
      virtual bool on_delete(GdkEvent *e);
      virtual bool on_focus_in(GdkEvent *e);
      virtual bool on_focus_out(GdkEvent *e);
      virtual bool on_scroll(GdkEvent *e);
      virtual bool on_window_state(GdkEvent *e);
      
    private:
      Array<DocumentPosition> _snapped_documents; // [0] = self
      
      GdkRectangle _previous_rect;
      
      MathGtkDock        *_top_area;
      MathGtkWorkingArea *_working_area;
      MathGtkDock        *_bottom_area;
      
      MathGtkIcons icons;
      
      GtkWidget *_menu_bar;
      GtkAdjustment *_hadjustment;
      GtkAdjustment *_vadjustment;
      GtkWidget *_hscrollbar;
      GtkWidget *_vscrollbar;
      GtkWidget *_table;
      
#if GTK_MAJOR_VERSION >= 3
      GtkStyleProvider *_style_provider;
      GtkWidget *_menu_bar_pin;
#endif

      WindowFrameType _window_frame;
      ObservableValue<bool> _active;
      bool _use_dark_mode : 1;
  };
}

#endif // RICHMATH__GUI__GTK__MGTK_DOCUMENT_WINDOW_H__INCLUDED
