#ifndef RICHMATH__GUI__GTK__MGTK_ATTACHED_POPUP_WINDOW_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_ATTACHED_POPUP_WINDOW_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <gui/gtk/mgtk-widget.h>

namespace richmath {
  // TODO: a MathGtkWidget has no scroll bars, better inherit from BasicGtkWidget and encapsulate a MathGtkWidget?
  class MathGtkAttachedPopupWindow: public MathGtkWidget {
      using base = MathGtkWidget;
      class Impl;
    public:
      MathGtkAttachedPopupWindow(Document *owner, Box *anchor);
      
      void anchor_location_changed();
      
      virtual void close() override;
      virtual void invalidate_options() override;
      
      virtual bool is_foreground_window() override { return _active; };
      virtual bool is_focused_widget() override { return _active; };
      virtual bool is_using_dark_mode() override;
      virtual int dpi() override;
      
    protected:
      virtual ~MathGtkAttachedPopupWindow();
      virtual void after_construction() override;
      
      virtual void paint_canvas(Canvas &canvas, bool resize_only) override;
      
      bool on_delete(GdkEvent *e);
      bool on_window_state(GdkEvent *e);
      
    private:
      ObservableValue<bool> _active;
      int _best_width;
      int _best_height;
  };
}

#endif // RICHMATH__GUI__GTK__MGTK_ATTACHED_POPUP_WINDOW_H__INCLUDED
