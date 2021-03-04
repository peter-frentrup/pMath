#include <gui/win32/api/win32-highdpi.h>

#include <gui/win32/ole/combase.h>

//#include <shellscalingapi.h>

//http://www.catch22.net/tuts/undocumented-createprocess
#define STARTF_MONITOR  0x400


using namespace richmath;

//{ class Win32HighDpi ...

HRESULT (WINAPI * Win32HighDpi::GetDpiForMonitor)(HMONITOR,MONITOR_DPI_TYPE,UINT*,UINT*) = nullptr;
UINT    (WINAPI * Win32HighDpi::GetDpiForSystem)() = nullptr;
BOOL    (WINAPI * Win32HighDpi::AdjustWindowRectExForDpi)(LPRECT,DWORD,BOOL,DWORD,UINT) = nullptr;
int     (WINAPI * Win32HighDpi::GetSystemMetricsForDpi)(int,UINT) = nullptr;
BOOL    (WINAPI * Win32HighDpi::SystemParametersInfoForDpi)(UINT,UINT,PVOID,UINT,UINT) = nullptr;
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
      GetDpiForSystem = (UINT (WINAPI*)())
                         GetProcAddress(shcore, "GetDpiForSystem");
    }
  }
  
  if(!user32) {
    user32 = LoadLibrary("user32.dll");
    if(user32) {
      AdjustWindowRectExForDpi = (BOOL (WINAPI*)(LPRECT,DWORD,BOOL,DWORD,UINT))
                                 GetProcAddress(user32, "AdjustWindowRectExForDpi");
      GetSystemMetricsForDpi = (int (WINAPI*)(int,UINT))
                               GetProcAddress(user32, "GetSystemMetricsForDpi");
      SystemParametersInfoForDpi = (BOOL (WINAPI*)(UINT,UINT,PVOID,UINT,UINT))
                                   GetProcAddress(user32, "SystemParametersInfoForDpi");
    }
  }
}

Win32HighDpi::~Win32HighDpi() {
  FreeLibrary(shcore); shcore = nullptr;
  FreeLibrary(user32); user32 = nullptr;
  
  GetDpiForMonitor = nullptr;
  GetDpiForSystem = nullptr;
  AdjustWindowRectExForDpi = nullptr;
  GetSystemMetricsForDpi = nullptr;
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
  
  return get_dpi_for_system();
}

int Win32HighDpi::get_dpi_for_monitor(HMONITOR monitor) {
  if(GetDpiForMonitor) {
    UINT dpiX;
    UINT dpiY;
    if(HRbool(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) 
      return dpiY;
  }
  
  return get_dpi_for_system();
}

int Win32HighDpi::get_dpi_for_system() {
  if(GetDpiForSystem) 
    return GetDpiForSystem();
  
  // The same value, but slower (according to Microsoft):
  HDC hdc = GetDC(nullptr);
  int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
  ReleaseDC(nullptr, hdc);
  return dpi;
}
      
bool Win32HighDpi::adjust_window_rect(RECT *rect, DWORD style, bool has_menu, DWORD style_ex, int dpi) {
  if(AdjustWindowRectExForDpi) 
    return !!AdjustWindowRectExForDpi(rect, style, has_menu, style_ex, (UINT)dpi);
  else
    return !!AdjustWindowRectEx(rect, style, has_menu, style_ex);
}

int Win32HighDpi::get_system_metrics_for_dpi(int index, int dpi) {
  if(GetSystemMetricsForDpi)
    return GetSystemMetricsForDpi(index, (UINT)dpi);
  else
    return GetSystemMetrics(index);
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

HMONITOR Win32HighDpi::get_startup_monitor() {
  // http://www.catch22.net/tuts/undocumented-createprocess
  STARTUPINFOW si = { sizeof(si) };
  GetStartupInfoW(&si);
  
  if(si.dwFlags & STARTF_MONITOR)
    return (HMONITOR)si.hStdOutput;
  
  return MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
}

//} ... class Win32HighDpi
