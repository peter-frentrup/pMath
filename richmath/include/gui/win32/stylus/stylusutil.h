#ifndef __GUI__WIN32__STYLUS__STYLUSUTIL_H_
#define __GUI__WIN32__STYLUS__STYLUSUTIL_H_

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/win32/ole/combase.h>
#include <windows.h>
#include <rtscom.h>

namespace richmath {
  class StylusUtil {
    public:
      static ComBase<IRealTimeStylus> create_stylus_for_window(HWND hwnd);
      static ComBase<IGestureRecognizer> create_gesture_recognizer();
      
      static ULONG get_stylus_sync_plugin_count(IRealTimeStylus *stylus);
      static ULONG get_stylus_sync_plugin_count(const ComBase<IRealTimeStylus> &stylus) {
        return get_stylus_sync_plugin_count(stylus.get());
      }
      
      static void debug_describe_packet_data_definition(IRealTimeStylus *stylus, TABLET_CONTEXT_ID tcid);
      static void debug_describe_packet_data(IRealTimeStylus *stylus, TABLET_CONTEXT_ID tcid, LONG *data);
      
      static void debug_describe_gesture(GESTURE_DATA *gesture);
  };
}

#endif // __GUI__WIN32__STYLUS__STYLUSUTIL_H_
