#include <gui/win32/win32-highdpi.h>

#include <gui/win32/ole/combase.h>

//#include <shellscalingapi.h>


using namespace richmath;

//{ class Win32HighDpi ...

HRESULT (WINAPI * Win32HighDpi::GetDpiForMonitor)(HMONITOR,MONITOR_DPI_TYPE,UINT*,UINT*) = nullptr;
HMODULE Win32HighDpi::shcore = nullptr;

Win32HighDpi::Win32HighDpi() 
  : Base() 
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  if(shcore)
    return;
  
  shcore = LoadLibrary("shcore.dll");
  if(shcore) {
    GetDpiForMonitor = (HRESULT (WINAPI*)(HMONITOR,MONITOR_DPI_TYPE,UINT*,UINT*))
                       GetProcAddress(shcore, "GetDpiForMonitor");
    
  }
}

Win32HighDpi::~Win32HighDpi() {
  FreeLibrary(shcore); shcore = nullptr;
  
  GetDpiForMonitor = nullptr;
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

//} ... class Win32HighDpi
