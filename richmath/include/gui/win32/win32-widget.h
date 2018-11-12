#ifndef RICHMATH__GUI__WIN32__WIN32_WIDGET_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_WIDGET_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/document.h>
#include <gui/native-widget.h>
#include <gui/win32/basic-win32-widget.h>
#include <gui/win32/stylus/stylusutil.h>


namespace richmath {
  // Must call init() immediately after the construction of a derived object!
  class Win32Widget: public NativeWidget, public BasicWin32Widget {
    protected:
      virtual void after_construction() override;
      
    public:
      Win32Widget(
        Document *doc,
        DWORD style_ex,
        DWORD style,
        int x,
        int y,
        int width,
        int height,
        HWND *parent);
      virtual ~Win32Widget();
      
      virtual void window_size(float *w, float *h) override;
      virtual void page_size(float *w, float *h) override {
        window_size(w, h);
      }
      
      virtual bool is_scrollable() override { return true; }
      virtual bool autohide_vertical_scrollbar() override { return _autohide_vertical_scrollbar; }
      virtual void scroll_pos(float *x, float *y) override;
      virtual void scroll_to(float x, float y) override;
      
      virtual void show_tooltip(Expr boxes) override;
      virtual void hide_tooltip() override;
      
      virtual bool is_scaleable() override { return true; }
      
      virtual double message_time() override;
      virtual double double_click_time() override;
      virtual void double_click_dist(float *dx, float *dy) override;
      virtual void do_drag_drop(Box *src, int start, int end) override;
      virtual bool cursor_position(float *x, float *y) override;
      
      virtual void bring_to_front() override;
      virtual void close() override {}
      virtual void invalidate() override;
      virtual void invalidate_options() override;
      virtual void invalidate_rect(float x, float y, float w, float h) override;
      virtual void force_redraw() override;
      
      virtual void set_cursor(CursorType type) override;
      
      virtual void running_state_changed() override;
      
      virtual bool is_mouse_down() override;
      
      virtual void beep() override;
      
      virtual bool register_timed_event(SharedPtr<TimedEvent> event) override;
      
      virtual String filename() override { return String(); }
      virtual void filename(String new_filename) override {}
      
      virtual void on_saved() override {}
      
    public:
      STDMETHODIMP DragEnter(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect) override;
      STDMETHODIMP DragLeave(void) override;
      
      STDMETHODIMP DataInterest(RealTimeStylusDataInterest* pEventInterest) override;
      STDMETHODIMP StylusDown(IRealTimeStylus*, const StylusInfo*, ULONG, LONG*, LONG**) override;
      STDMETHODIMP StylusUp(IRealTimeStylus*, const StylusInfo*, ULONG, LONG*, LONG**) override;
      
    public:
      bool _autohide_vertical_scrollbar;
      cairo_format_t _image_format;
      
    private:
      CursorType cursor;
      bool mouse_moving;
      
      bool is_painting;
      
      bool scrolling;
      bool already_scrolled;
      MouseEvent mouse_down_event; // coordinates in pixels, relative to widget top/left (no scrolling adjustment)
      
      int _width;
      int _height;
      
      float gesture_zoom_factor;
      
      Hashtable<SharedPtr<TimedEvent>, Void> animations;
      bool animation_running;
      bool is_dragging;
      bool is_drop_over;
      
      ComBase<IRealTimeStylus> stylus;
      
    protected:
      virtual void paint_background(Canvas *canvas);
      virtual void paint_canvas(Canvas *canvas, bool resize_only);
      
      virtual void on_paint(HDC dc, bool from_wmpaint);
      virtual void on_hscroll(WORD kind, WORD thumbPos);
      virtual void on_vscroll(WORD kind, WORD thumbPos);
      virtual void on_mousedown(MouseEvent &event);
      virtual void on_mouseup(MouseEvent &event);
      virtual void on_mousemove(MouseEvent &event);
      virtual void on_keydown(DWORD virtkey, bool ctrl, bool alt, bool shift);
      virtual void on_popupmenu(POINT screen_pt);
      
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override;
      
      virtual bool is_data_droppable(IDataObject *data_object) override;
      virtual DWORD drop_effect(DWORD key_state, POINTL ptl, DWORD allowed_effects) override;
      virtual void do_drop_data(IDataObject *data_object, DWORD effect) override;
      virtual void position_drop_cursor(POINTL ptl) override;
  };
  
  SpecialKey win32_virtual_to_special_key(DWORD vkey);
}

#endif // RICHMATH__GUI__WIN32__WIN32_WIDGET_H__INCLUDED
