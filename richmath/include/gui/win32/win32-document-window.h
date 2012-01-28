#ifndef __GUI__WIN32__WIN32_DOCUMENT_WINDOW_H__
#define __GUI__WIN32__WIN32_DOCUMENT_WINDOW_H__

#ifndef RICHMATH_USE_WIN32_GUI
#error this header is win32 specific
#endif

#include <gui/win32/basic-win32-window.h>
#include <gui/win32/win32-widget.h>


namespace richmath {
  class Win32Dock;
  class Win32GlassDock;
  class Win32Menubar;
  class Win32WorkingArea;
  
  // Must call init() immediately after the construction of a derived object!
  class Win32DocumentWindow: public BasicWin32Window {
    protected:
      virtual void after_construction();
      
    public:
      Win32DocumentWindow(
        Document *doc,
        DWORD style_ex,
        DWORD style,
        int x,
        int y,
        int width,
        int height);
      virtual ~Win32DocumentWindow();
      
      Document *top_glass() {    return ((Win32Widget*)_top_glass_area)->document();    }
      Document *top() {          return ((Win32Widget*)_top_area)->document();          }
      Document *document() {     return ((Win32Widget*)_working_area)->document();      }
      Document *bottom() {       return ((Win32Widget*)_bottom_area)->document();       }
      Document *bottom_glass() { return ((Win32Widget*)_bottom_glass_area)->document(); }
      
      bool is_all_glass();
      
      void rearrange();
      void invalidate_options();
      void reset_title(){ title(_title); }
      
      bool is_palette() { return _is_palette; }
      void is_palette(bool value);
      
      virtual bool is_closed();
      
      Win32Widget *top_glass_area() {    return (Win32Widget*)_top_glass_area;    }
      Win32Widget *top_area() {          return (Win32Widget*)_top_area;          }
      Win32Widget *working_area() {      return (Win32Widget*)_working_area;      }
      Win32Widget *bottom_area() {       return (Win32Widget*)_bottom_area;       }
      Win32Widget *bottom_glass_area() { return (Win32Widget*)_bottom_glass_area; }
      
    private:
      Win32GlassDock   *_top_glass_area;
      Win32Dock        *_top_area;
      Win32WorkingArea *_working_area;
      Win32Dock        *_bottom_area;
      Win32GlassDock   *_bottom_glass_area;
      
      Win32Menubar *menubar;
      bool creation;
      
      bool _is_palette;
      
      String _title;
      
    protected:
      virtual void on_theme_changed();
      
      void title(String text);
      
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam);
  };
}

#endif // __GUI__WIN32__WIN32_DOCUMENT_WINDOW_H__
