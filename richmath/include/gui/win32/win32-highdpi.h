#ifndef RICHMATH__GUI__WIN32_HIGHDPI_H__INCLUDED
#define RICHMATH__GUI__WIN32_HIGHDPI_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif


#include <windows.h>
#include <util/base.h>

/* Per-monitor DPI awareness is available since Windows 8.1 for top level windows.
   On older systems, we fall back to system DPI awareness.
 */

#ifndef WM_DPICHANGED
#  define WM_DPICHANGED   0x02E0
#endif

namespace richmath {
  class Win32HighDpi final: public Base {
    public:
      static void init();
      
      static int get_dpi_for_window(HWND hwnd);
      
      static bool adjust_window_rect(RECT *rect, DWORD style, bool has_menu, DWORD style_ex, int dpi);
      
    private:
      enum MONITOR_DPI_TYPE {
        MDT_EFFECTIVE_DPI = 0,
        MDT_ANGULAR_DPI = 1,
        MDT_RAW_DPI = 2,
        MDT_DEFAULT = MDT_EFFECTIVE_DPI
      };
      
    private:
      static HRESULT (WINAPI *GetDpiForMonitor)(HMONITOR,MONITOR_DPI_TYPE,UINT*,UINT*);
      static BOOL (WINAPI *AdjustWindowRectExForDpi)(LPRECT,DWORD,BOOL,DWORD,UINT);
      
    private:
      static HMODULE shcore;
      static HMODULE user32;
      
    private:
      Win32HighDpi();
      ~Win32HighDpi();
  };
}

#endif // RICHMATH__GUI__WIN32_HIGHDPI_H__INCLUDED
