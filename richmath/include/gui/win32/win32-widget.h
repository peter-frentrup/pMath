#ifndef __GUI__WIN32__WIN32_WIDGET_H__
#define __GUI__WIN32__WIN32_WIDGET_H__

#include <gui/document.h>
#include <gui/native-widget.h>
#include <gui/win32/basic-win32-widget.h>

namespace richmath{
  // Must call immediately init after the construction of a derived object!
  class Win32Widget: public NativeWidget, public BasicWin32Widget{
    protected:
      virtual void after_construction();
      
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
      
      virtual void window_size(float *w, float *h);
      virtual void page_size(float *w, float *h){
        window_size(w, h); 
      }
      
      virtual bool is_scrollable(){ return true; }
      virtual bool autohide_vertical_scrollbar(){ return _autohide_vertical_scrollbar; }
      virtual void scroll_pos(float *x, float *y);
      virtual void scroll_to(float x, float y);
      
      virtual bool is_scaleable(){ return true; }
      
      virtual long message_time();
      virtual long double_click_time();
      virtual void double_click_dist(float *dx, float *dy);
      
      virtual void invalidate();
      virtual void set_cursor(CursorType type);
      
      virtual void running_state_changed();
      
      virtual void beep();
    
      virtual bool register_timed_event(SharedPtr<TimedEvent> event);
      
    public:
      bool _autohide_vertical_scrollbar;
      cairo_format_t _image_format;
      
    private:
      CursorType cursor;
      bool mouse_moving;
      
      bool is_painting;
      
      bool scrolling;
      bool already_scrolled;
      MouseEvent mouse_down_event; // coordinates in pixels, relative to widget top/left (no scrolling)
//      float scroll_start_x;
//      float scroll_start_y;
      
      int _width;
      int _height;
      
      Hashtable<SharedPtr<TimedEvent>,Void> animations;
      bool animation_running;
      
    protected:
      virtual void paint_background(Canvas *canvas);
      virtual void paint_canvas(Canvas *canvas, bool resize_only);
      
      virtual void on_paint(HDC dc, bool from_wmpaint);
      virtual void on_hscroll(WORD kind);
      virtual void on_vscroll(WORD kind);
      virtual void on_mousedown(MouseEvent &event);
      virtual void on_mouseup(MouseEvent &event);
      virtual void on_mousemove(MouseEvent &event);
      virtual void on_keydown(DWORD virtkey, bool ctrl, bool alt, bool shift);
      
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam);
  };
  
  SpecialKey win32_virtual_to_special_key(DWORD vkey);
  String win32_command_id_to_command_string(DWORD id);
}

#endif // __GUI__WIN32__WIN32_WIDGET_H__
