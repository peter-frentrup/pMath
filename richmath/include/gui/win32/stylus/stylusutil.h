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
      static ComBase<IRealTimeStylus> create(HWND hwnd);
      
      static void debug_describe_packet_data_definition(IRealTimeStylus *stylus, TABLET_CONTEXT_ID tcid);
      static void debug_describe_packet_data(IRealTimeStylus *stylus, TABLET_CONTEXT_ID tcid, LONG *data);
  };
}

#endif // __GUI__WIN32__STYLUS__STYLUSUTIL_H_
