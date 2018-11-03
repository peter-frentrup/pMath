#ifndef RICHMATH__GUI__GTK__MGTK_DOCUMENT_WINDOW_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_DOCUMENT_WINDOW_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <gui/gtk/basic-gtk-widget.h>
#include <gui/gtk/mgtk-icons.h>
#include <gui/gtk/mgtk-widget.h>

#include <eval/observable.h>


namespace richmath {
  class MathGtkDock;
  class MathGtkWorkingArea;
  
  class MathGtkDocumentWindow: public BasicGtkWidget {
    public:
      class DocumentPosition {
        public:
          DocumentPosition() {}
          DocumentPosition(int _id, int _x, int _y)
            : id(_id), x(_x), y(_y)
          {
          }
          
          int id;
          int x;
          int y;
      };
      
    protected:
      virtual void after_construction() override;
      
    public:
      MathGtkDocumentWindow();
      virtual ~MathGtkDocumentWindow();
      
      void invalidate_options();
      void reset_title(){ title(_title); }
      
      bool            is_palette() {   return _window_frame == WindowFramePalette; }
      WindowFrameType window_frame() { return _window_frame; }
      
      void run_menucommand(Expr cmd);
      
      void adjustment_changed(GtkAdjustment *adjustment);
      
      Document *top() {          return ((MathGtkWidget*)_top_area)->document();     }
      Document *document() {     return ((MathGtkWidget*)_working_area)->document(); }
      Document *bottom() {       return ((MathGtkWidget*)_bottom_area)->document();  }
      
      MathGtkWidget *top_area() {     return (MathGtkWidget*)_top_area;     }
      MathGtkWidget *working_area() { return (MathGtkWidget*)_working_area; }
      MathGtkWidget *bottom_area() {  return (MathGtkWidget*)_bottom_area;  }
      
      // all windows are arranged in a ring buffer:
      static MathGtkDocumentWindow *first_window();
      MathGtkDocumentWindow *prev_window() { return _prev_window; }
      MathGtkDocumentWindow *next_window() { return _next_window; }
      
      virtual void bring_to_front();
      virtual void close();
      
      void reset_window_frame(){ window_frame(_window_frame); }
      void set_gravity();
      void set_initial_rect(int x, int y, int w, int h);
      
      void get_window_margins(int *left, int *right, int *top, int *bottom);
      void get_outer_rect(GdkRectangle *rect);
      const GdkRectangle &previous_rect() { return _previous_rect; }
      static bool test_rects_touch(const GdkRectangle &rect1, const GdkRectangle &rect2, int *maxdx, int *maxdy, GdkWindowEdge *edge);
      
      void move_palettes();
      
      String filename(){ return _filename; }
      void filename(String new_filename);
      
      virtual void on_editing();
      virtual void on_saved();

    protected:
      void title(       String          text);
      void window_frame(WindowFrameType type);
      
      virtual bool on_configure(GdkEvent *e);
      virtual bool on_focus_in(GdkEvent *e);
      virtual bool on_focus_out(GdkEvent *e);
      virtual bool on_scroll(GdkEvent *e);
      
    private:
      String                  _title;
      WindowFrameType         _window_frame;
      ObservableValue<String> _filename;
      
      Array<DocumentPosition> _snapped_documents; // [0] = self
      
      GdkRectangle _previous_rect;
      
      MathGtkDock        *_top_area;
      MathGtkWorkingArea *_working_area;
      MathGtkDock        *_bottom_area;
      
      MathGtkDocumentWindow *_prev_window;
      MathGtkDocumentWindow *_next_window;
      
      MathGtkIcons icons;
      
      GtkWidget *_menu_bar;
      GtkAdjustment *_hadjustment;
      GtkAdjustment *_vadjustment;
      GtkWidget *_hscrollbar;
      GtkWidget *_vscrollbar;
      GtkWidget *_table;
      
      bool _has_unsaved_changes;
  };
}

#endif // RICHMATH__GUI__GTK__MGTK_DOCUMENT_WINDOW_H__INCLUDED
