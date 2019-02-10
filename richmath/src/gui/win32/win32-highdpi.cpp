#include <gui/win32/win32-highdpi.h>

#include <gui/win32/ole/combase.h>

//#include <shellscalingapi.h>


using namespace richmath;

//{ class Win32HighDpi ...

HRESULT (WINAPI * Win32HighDpi::GetDpiForMonitor)(HMONITOR,MONITOR_DPI_TYPE,UINT*,UINT*) = nullptr;
BOOL (WINAPI *Win32HighDpi::AdjustWindowRectExForDpi)(LPRECT,DWORD,BOOL,DWORD,UINT) = nullptr;
BOOL (WINAPI *Win32HighDpi::SystemParametersInfoForDpi)(UINT,UINT,PVOID,UINT,UINT) = nullptr;
HMODULE Win32HighDpi::shcore = nullptr;
HMODULE Win32HighDpi::user32 = nullptr;

Win32HighDpi::Win32HighDpi() 
  : Base() 
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  if(!shcore) {
    shcore = LoadLibrary("shcore.dll");
    if(shcore) {
      GetDpiForMonitor = (HRESULT (WINAPI*)(HMONITOR,MONITOR_DPI_TYPE,UINT*,UINT*))
                         GetProcAddress(shcore, "GetDpiForMonitor");
    }
  }
  
  if(!user32) {
    user32 = LoadLibrary("user32.dll");
    if(user32) {
      AdjustWindowRectExForDpi = (BOOL (WINAPI*)(LPRECT,DWORD,BOOL,DWORD,UINT))
                                 GetProcAddress(user32, "AdjustWindowRectExForDpi");
      SystemParametersInfoForDpi = (BOOL (WINAPI*)(UINT,UINT,PVOID,UINT,UINT))
                                   GetProcAddress(user32, "SystemParametersInfoForDpi");
    }
  }
}

Win32HighDpi::~Win32HighDpi() {
  FreeLibrary(shcore); shcore = nullptr;
  FreeLibrary(user32); user32 = nullptr;
  
  GetDpiForMonitor = nullptr;
  AdjustWindowRectExForDpi = nullptr;
  SystemParametersInfoForDpi = nullptr;
}

void Win32HighDpi::init() {
  static Win32HighDpi high_dpi;
}

int Win32HighDpi::get_dpi_for_window(HWND hwnd) {
  if(GetDpiForMonitor) {
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    UINT dpiX;
    UINT dpiY;
    if(HRbool(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) 
      return dpiY;
  }
  //return GetDeviceCaps(hdc, LOGPIXELSY);
  return 96;
}

bool Win32HighDpi::adjust_window_rect(RECT *rect, DWORD style, bool has_menu, DWORD style_ex, int dpi) {
  if(AdjustWindowRectExForDpi) 
    return !!AdjustWindowRectExForDpi(rect, style, has_menu, style_ex, (UINT)dpi);
  else
    return !!AdjustWindowRectEx(rect, style, has_menu, style_ex);
}

bool Win32HighDpi::get_nonclient_metrics_for_dpi(NONCLIENTMETRICSW *nonclient_metrics, int dpi) {
  if(SystemParametersInfoForDpi) {
    return !!SystemParametersInfoForDpi(
      SPI_GETNONCLIENTMETRICS,
      sizeof(nonclient_metrics),
      &nonclient_metrics,
      FALSE,
      (UINT)dpi);
  }
  else {
    return !!SystemParametersInfoW(
      SPI_GETNONCLIENTMETRICS,
      sizeof(nonclient_metrics),
      &nonclient_metrics,
      FALSE);
  }
}

//} ... class Win32HighDpi
