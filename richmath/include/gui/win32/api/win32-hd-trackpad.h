#ifndef RICHMATH__GUI__WIN32__API__HIGH_DEFINITION_TRACKPAD_H__INCLUDED
#define RICHMATH__GUI__WIN32__API__HIGH_DEFINITION_TRACKPAD_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <windows.h>

class IDirectManipulationViewportEventHandler;

namespace richmath {
  class HiDefTrackpadHandlerImpl;
  class NativeWidget;
  
  class HiDefTrackpadHandler {
      friend class HiDefTrackpadHandlerImpl;
    public:
      HiDefTrackpadHandler();
      ~HiDefTrackpadHandler();
      
      void init(HWND hwnd, NativeWidget *destination);
      void detach();
      
      void on_pointer_hit_test(WPARAM wParam);
      void on_timer();
    
    public:
      static struct TimerIdType {} TimerId;
    
    private:
      HiDefTrackpadHandlerImpl *_data;
  };
}

#endif // RICHMATH__GUI__WIN32__API__HIGH_DEFINITION_TRACKPAD_H__INCLUDED
