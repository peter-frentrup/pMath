#ifndef __GUI__WIN32__WIN32_SCROLLBAR_OVERLAY_H__
#define __GUI__WIN32__WIN32_SCROLLBAR_OVERLAY_H__

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <util/array.h>
#include <gui/win32/basic-win32-widget.h>


namespace richmath {
  struct Indicator {
    float    position;
    uint32_t color;
  };
  
  class Win32ScrollBarOverlay: public BasicWin32Widget {
      friend class Win32ScrollBarOverlayImpl;
    public:
      Win32ScrollBarOverlay(HWND *parent_ptr, HWND *scrollbar_owner_ptr);
      
      void set_scale(float _scale);
      void update();
      void clear();
      void add(float position, int style);
      
      void handle_scrollbar_owner_callback(UINT message, WPARAM wParam, LPARAM lParam);
      
    protected:
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override;
      
    private:
      HWND            *scrollbar_owner_ptr;
      Array<Indicator> indicators;
      float            scale;
  };
}

#endif // __GUI__WIN32__WIN32_SCROLLBAR_OVERLAY_H__
