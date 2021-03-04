#ifndef RICHMATH__GUI__WIN32__API__STYLUSUTIL_H__INCLUDED
#define RICHMATH__GUI__WIN32__API__STYLUSUTIL_H__INCLUDED

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
      
      static ULONG get_stylus_sync_plugin_count(IRealTimeStylus *stylus);
      static ULONG get_stylus_sync_plugin_count(const ComBase<IRealTimeStylus> &stylus) {
        return get_stylus_sync_plugin_count(stylus.get());
      }
      
      static void debug_describe_packet_data_definition(IRealTimeStylus *stylus, TABLET_CONTEXT_ID tcid);
      static void debug_describe_packet_data(IRealTimeStylus *stylus, TABLET_CONTEXT_ID tcid, LONG *data);
  };
}

#endif // RICHMATH__GUI__WIN32__API__STYLUSUTIL_H__INCLUDED
