#ifndef RICHMATH__GUI__GTK__MGTK_ATTACHED_POPUP_WINDOW_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_ATTACHED_POPUP_WINDOW_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <graphics/callout-triangle.h>
#include <gui/gtk/mgtk-widget.h>

namespace richmath {
  class MathGtkPopupContentArea;
  
  class MathGtkAttachedPopupWindow final : public BasicGtkWidget, public ControlContext {
      using base = BasicGtkWidget;
      class Impl;
    public:
      MathGtkAttachedPopupWindow(Document *owner, const SelectionReference &anchor);
      
      void invalidate_options();
      void invalidate_source_location();
      
      MathGtkWidget *content_area() { return (MathGtkWidget*)_content_area; }
      Document      *content() { return content_area()->document(); }
      
      void close();
      
      virtual bool is_foreground_window() override { return _active; };
      virtual bool is_focused_widget() override { return _active; };
      virtual bool is_using_dark_mode() override;
      virtual int dpi() override;
      
    protected:
      virtual ~MathGtkAttachedPopupWindow();
      virtual void after_construction() override;
      
      bool on_configure(GdkEvent *e);
      bool on_delete(GdkEvent *e);
      bool on_draw(cairo_t *cr);
      bool on_map(GdkEvent *e);
      bool on_unmap(GdkEvent *e);
      bool on_window_state(GdkEvent *e);
    
    private:
      bool on_expose(GdkEvent *e); // GTK-2 only
      
    private:
      GtkAdjustment           *_hadjustment;
      GtkAdjustment           *_vadjustment;
      GtkWidget               *_hscrollbar;
      GtkWidget               *_vscrollbar;
      GtkWidget               *_content_alignment;
      MathGtkPopupContentArea *_content_area;
      ContainerType            _appearance;
      ObservableValue<bool>    _active;
      CalloutTriangle          _callout_triangle;
  };
}

#endif // RICHMATH__GUI__GTK__MGTK_ATTACHED_POPUP_WINDOW_H__INCLUDED
