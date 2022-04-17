#ifndef RICHMATH__GUI__WIN32__WIN32_ATTACHED_POPUP_WINDOW_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_ATTACHED_POPUP_WINDOW_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/win32/win32-widget.h>

namespace richmath {
  class Win32AttachedPopupWindow: public Win32Widget {
      using base = Win32Widget;
      class Impl;
    public:
      Win32AttachedPopupWindow(Document *owner, const SelectionReference &anchor);
      
      virtual void close() override;
      virtual void invalidate_options() override;
      virtual void invalidate_source_location() override;
      
      virtual bool is_foreground_window() override { return _active; }
      virtual bool is_using_dark_mode() override;
      virtual int dpi() override;
      
    protected:
      virtual ~Win32AttachedPopupWindow();
      virtual void after_construction() override;
      
      virtual void paint_background(Canvas &canvas) override;
      virtual void paint_canvas(Canvas &canvas, bool resize_only) override;
      virtual void on_close() override;
      virtual void do_set_current_document() override;
      
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override;
    
    private:
      ObservableValue<bool> _active; // alternatively, we could inherit from BasicWin32Window
      Vector2F _best_size;
  };
}

#endif // RICHMATH__GUI__WIN32__WIN32_ATTACHED_POPUP_WINDOW_H__INCLUDED
