#ifndef RICHMATH__GUI__WIN32__WIN32_SCROLLBAR_OVERLAY_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_SCROLLBAR_OVERLAY_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <util/array.h>
#include <gui/win32/basic-win32-widget.h>


namespace richmath {
  enum class IndicatorLane: uint8_t {
    Middle,
    All
  };
  
  struct Indicator {
    float     position;
    unsigned  color: 24;
    unsigned  lane: 8;
  };
  
  class Win32ScrollBarOverlay: public BasicWin32Widget {
      friend class Win32ScrollBarOverlayImpl;
    public:
      Win32ScrollBarOverlay(HWND *parent_ptr, HWND *scrollbar_owner_ptr);
      
      void set_scale(float _scale);
      void update();
      void clear();
      void add(float position, unsigned color, IndicatorLane lane);
      
      void handle_scrollbar_owner_callback(UINT message, WPARAM wParam, LPARAM lParam);
      
    protected:
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override;
      
    private:
      HWND            *scrollbar_owner_ptr;
      Array<Indicator> indicators;
      float            scale;
  };
}

#endif // RICHMATH__GUI__WIN32__WIN32_SCROLLBAR_OVERLAY_H__INCLUDED
