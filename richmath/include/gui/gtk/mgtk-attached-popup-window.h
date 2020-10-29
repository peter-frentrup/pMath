#ifndef RICHMATH__GUI__GTK__MGTK_ATTACHED_POPUP_WINDOW_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_ATTACHED_POPUP_WINDOW_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <gui/gtk/mgtk-widget.h>

namespace richmath {
  class MathGtkPopupContentArea;
  
  class MathGtkAttachedPopupWindow final : public BasicGtkWidget, public ControlContext {
      using base = BasicGtkWidget;
      class Impl;
    public:
      MathGtkAttachedPopupWindow(Document *owner, Box *anchor);
      
      void anchor_location_changed();
      
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
      
      bool on_unmap(GdkEvent *e);
      bool on_delete(GdkEvent *e);
      bool on_window_state(GdkEvent *e);
      
    private:
      GtkAdjustment           *_hadjustment;
      GtkAdjustment           *_vadjustment;
      GtkWidget               *_hscrollbar;
      GtkWidget               *_vscrollbar;
      GtkWidget               *_table;
      MathGtkPopupContentArea *_content_area;
      ObservableValue<bool>    _active;
  };
}

#endif // RICHMATH__GUI__GTK__MGTK_ATTACHED_POPUP_WINDOW_H__INCLUDED