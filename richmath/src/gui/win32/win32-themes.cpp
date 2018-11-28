#include <gui/win32/win32-themes.h>

#include <cstdio>
#include <cwchar>

#include <util/array.h>

using namespace richmath;

HRESULT(WINAPI * Win32Themes::DwmEnableComposition)(UINT) = nullptr;
HRESULT(WINAPI * Win32Themes::DwmExtendFrameIntoClientArea)(HWND, const MARGINS *) = nullptr;
HRESULT(WINAPI * Win32Themes::DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD) = nullptr;
HRESULT(WINAPI * Win32Themes::DwmGetCompositionTimingInfo)(HWND, DWM_TIMING_INFO *) = nullptr;
HRESULT(WINAPI * Win32Themes::DwmGetColorizationParameters)(Win32Themes::DWM_COLORIZATION_PARAMS *) = nullptr;
HRESULT(WINAPI * Win32Themes::DwmDefWindowProc)(HWND, UINT, WPARAM, LPARAM, LRESULT *) = nullptr;

HANDLE(WINAPI * Win32Themes::OpenThemeData)(HWND, LPCWSTR) = nullptr;
HRESULT(WINAPI * Win32Themes::CloseThemeData)(HANDLE) = nullptr;
HRESULT(WINAPI * Win32Themes::DrawThemeBackground)(HANDLE, HDC, int, int, const RECT *, const RECT *) = nullptr;
HRESULT(WINAPI * Win32Themes::DrawThemeEdge)(HANDLE, HDC, int, int, LPCRECT, UINT, UINT, LPRECT) = nullptr;
HRESULT(WINAPI * Win32Themes::DrawThemeTextEx)(HANDLE, HDC, int, int, LPCWSTR, int, DWORD, LPRECT, const DTTOPTS *) = nullptr;
HRESULT(WINAPI * Win32Themes::GetThemeSysFont)(HANDLE, int, LOGFONTW *) = nullptr;
COLORREF(WINAPI * Win32Themes::GetThemeSysColor)(HANDLE, int) = nullptr;
HRESULT(WINAPI * Win32Themes::GetThemeBackgroundExtent)(HANDLE, HDC, int, int, LPCRECT, LPRECT) = nullptr;
HRESULT(WINAPI * Win32Themes::GetThemeBackgroundContentRect)(HANDLE, HDC hdc, int, int, LPCRECT, LPRECT) = nullptr;
HRESULT(WINAPI * Win32Themes::GetThemeBool)(HANDLE, int, int, int, BOOL *) = nullptr;
HRESULT(WINAPI * Win32Themes::GetThemeColor)(HANDLE, int, int, int, COLORREF *) = nullptr;
HRESULT(WINAPI * Win32Themes::GetThemeMargins)(HANDLE, HDC, int, int, int, LPRECT, MARGINS *) = nullptr;
HRESULT(WINAPI * Win32Themes::GetThemeMetric)(HANDLE, HDC, int, int, int, int *) = nullptr;
HRESULT(WINAPI * Win32Themes::GetThemeInt)(HANDLE, int, int, int, int *) = nullptr;
HRESULT(WINAPI * Win32Themes::GetThemeIntList)(HANDLE, int, int, int, INTLIST *) = nullptr;
HRESULT(WINAPI * Win32Themes::GetThemePartSize)(HANDLE hTheme, HDC, int, int, LPCRECT, THEME_SIZE, SIZE *) = nullptr;
HRESULT(WINAPI * Win32Themes::GetThemePosition)(HANDLE, int, int, int, POINT *) = nullptr;
HRESULT(WINAPI * Win32Themes::GetThemeTransitionDuration)(HANDLE, int, int, int, int, DWORD *) = nullptr;
HRESULT(WINAPI * Win32Themes::GetCurrentThemeName)(LPWSTR, int, LPWSTR, int, LPWSTR, int) = nullptr;
BOOL (WINAPI * Win32Themes::IsAppThemed)(void) = nullptr;
BOOL (WINAPI * Win32Themes::IsThemePartDefined)(HANDLE, int, int) = nullptr;
int (WINAPI * Win32Themes::GetThemeSysSize)(HANDLE, int) = nullptr;
HRESULT(WINAPI * Win32Themes::SetWindowTheme)(HWND, LPCWSTR, LPCWSTR) = nullptr;

HRESULT(WINAPI * Win32Themes::BufferedPaintInit)(void) = nullptr;
HRESULT(WINAPI * Win32Themes::BufferedPaintUnInit)(void) = nullptr;
HRESULT(WINAPI * Win32Themes::BufferedPaintStopAllAnimations)(HWND) = nullptr;
HANDLE(WINAPI * Win32Themes::BeginBufferedPaint)(HDC, const RECT *, BP_BUFFERFORMAT, BP_PAINTPARAMS *, HDC *) = nullptr;
HANDLE(WINAPI * Win32Themes::EndBufferedPaint)(HANDLE, BOOL) = nullptr;
BOOL (WINAPI * Win32Themes::IsCompositionActive)(void) = nullptr;
BOOL (WINAPI * Win32Themes::IsThemeActive)(void) = nullptr;

HMODULE Win32Themes::dwmapi = nullptr;
HMODULE Win32Themes::uxtheme = nullptr;
HMODULE Win32Themes::usp10dll = nullptr;

void Win32Themes::init() {
  static Win32Themes w;
}

Win32Themes::Win32Themes()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  dwmapi = LoadLibrary("dwmapi.dll");
  if(dwmapi) {
    DwmEnableComposition = (HRESULT(WINAPI *)(UINT))
                           GetProcAddress(dwmapi, "DwmEnableComposition");
                           
    DwmExtendFrameIntoClientArea = (HRESULT(WINAPI *)(HWND, const MARGINS *))
                                   GetProcAddress(dwmapi, "DwmExtendFrameIntoClientArea");
                                   
    DwmSetWindowAttribute = (HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD))
                            GetProcAddress(dwmapi, "DwmSetWindowAttribute");
                            
    DwmGetColorizationParameters = (HRESULT(WINAPI *)(DWM_COLORIZATION_PARAMS*))
                                   GetProcAddress(dwmapi, (LPCSTR)127);
                                   
    DwmGetCompositionTimingInfo = (HRESULT(WINAPI *)(HWND, DWM_TIMING_INFO *))
                                  GetProcAddress(dwmapi, "DwmGetCompositionTimingInfo");
                                  
    DwmDefWindowProc = (HRESULT(WINAPI *)(HWND, UINT, WPARAM, LPARAM, LRESULT *))
                       GetProcAddress(dwmapi, "DwmDefWindowProc");
  }
  
  uxtheme = LoadLibrary("uxtheme.dll");
  if(uxtheme) {
    OpenThemeData = (HANDLE(WINAPI *)(HWND, LPCWSTR))
                    GetProcAddress(uxtheme, "OpenThemeData");
                    
    CloseThemeData = (HRESULT(WINAPI *)(HANDLE))
                     GetProcAddress(uxtheme, "CloseThemeData");
                     
    DrawThemeBackground = (HRESULT(WINAPI *)(HANDLE, HDC, int, int, const RECT *, const RECT *))
                          GetProcAddress(uxtheme, "DrawThemeBackground");
                          
    DrawThemeEdge = (HRESULT(WINAPI *)(HANDLE, HDC, int, int, LPCRECT, UINT, UINT, LPRECT))
                    GetProcAddress(uxtheme, "DrawThemeEdge");
                    
    DrawThemeTextEx = (HRESULT(WINAPI *)(HANDLE, HDC, int, int, LPCWSTR, int, DWORD, LPRECT, const DTTOPTS *))
                      GetProcAddress(uxtheme, "DrawThemeTextEx");
                      
    GetThemeSysFont = (HRESULT(WINAPI *)(HANDLE, int, LOGFONTW *))
                      GetProcAddress(uxtheme, "GetThemeSysFont");
                      
    GetThemeSysColor = (COLORREF(WINAPI *)(HANDLE, int))
                       GetProcAddress(uxtheme, "GetThemeSysColor");
                       
    GetThemeBackgroundExtent = (HRESULT(WINAPI *)(HANDLE, HDC, int, int, LPCRECT, LPRECT))
                               GetProcAddress(uxtheme, "GetThemeBackgroundExtent");
                               
    GetThemeBackgroundContentRect = (HRESULT(WINAPI *)(HANDLE, HDC hdc, int, int, LPCRECT, LPRECT))
                                    GetProcAddress(uxtheme, "GetThemeBackgroundContentRect");
                                    
    GetThemeBool = (HRESULT(WINAPI *)(HANDLE, int, int, int, BOOL *))
                   GetProcAddress(uxtheme, "GetThemeBool");
                   
    GetThemeColor = (HRESULT(WINAPI *)(HANDLE, int, int, int, COLORREF *))
                    GetProcAddress(uxtheme, "GetThemeColor");
                    
    GetThemeMargins = (HRESULT(WINAPI *)(HANDLE, HDC, int, int, int, LPRECT, MARGINS *))
                      GetProcAddress(uxtheme, "GetThemeMargins");
                      
    GetThemeMetric = (HRESULT(WINAPI *)(HANDLE, HDC, int, int, int, int *))
                     GetProcAddress(uxtheme, "GetThemeMetric");
                     
    GetThemeInt = (HRESULT(WINAPI *)(HANDLE, int, int, int, int *))
                  GetProcAddress(uxtheme, "GetThemeInt");
                  
    GetThemeIntList = (HRESULT(WINAPI *)(HANDLE, int, int, int, INTLIST *))
                      GetProcAddress(uxtheme, "GetThemeIntList");
                      
    GetThemePartSize = (HRESULT(WINAPI *)(HANDLE hTheme, HDC, int, int, LPCRECT, THEME_SIZE, SIZE *))
                       GetProcAddress(uxtheme, "GetThemePartSize");
                       
    GetThemePosition = (HRESULT(WINAPI *)(HANDLE, int, int, int, POINT *))
                       GetProcAddress(uxtheme, "GetThemePosition");
                       
    GetThemeTransitionDuration = (HRESULT(WINAPI *)(HANDLE, int, int, int, int, DWORD *))
                                 GetProcAddress(uxtheme, "GetThemeTransitionDuration");
                                 
    GetCurrentThemeName = (HRESULT(WINAPI *)(LPWSTR, int, LPWSTR, int, LPWSTR, int))
                          GetProcAddress(uxtheme, "GetCurrentThemeName");
                          
    IsAppThemed = (BOOL (WINAPI *)(void))
                  GetProcAddress(uxtheme, "IsAppThemed");
    
    IsThemePartDefined = (BOOL (WINAPI *)(HANDLE, int, int))
                         GetProcAddress(uxtheme, "IsThemePartDefined");
                         
    GetThemeSysSize = (int (WINAPI *)(HANDLE, int))
                      GetProcAddress(uxtheme, "GetThemeSysSize");
                      
    SetWindowTheme = (HRESULT(WINAPI *)(HWND, LPCWSTR, LPCWSTR))
                     GetProcAddress(uxtheme, "SetWindowTheme");
                     
                     
                     
    BufferedPaintInit = (HRESULT(WINAPI *)(void))
                        GetProcAddress(uxtheme, "BufferedPaintInit");
                        
    BufferedPaintUnInit = (HRESULT(WINAPI *)(void))
                          GetProcAddress(uxtheme, "BufferedPaintUnInit");
                          
    BufferedPaintStopAllAnimations = (HRESULT(WINAPI *)(HWND))
                                     GetProcAddress(uxtheme, "BufferedPaintStopAllAnimations");
                                     
    BeginBufferedPaint = (HANDLE(WINAPI *)(HDC, const RECT *, BP_BUFFERFORMAT, BP_PAINTPARAMS *, HDC *))
                         GetProcAddress(uxtheme, "BeginBufferedPaint");
                         
    EndBufferedPaint = (HANDLE(WINAPI *)(HANDLE, BOOL))
                       GetProcAddress(uxtheme, "EndBufferedPaint");
                       
    IsCompositionActive = (BOOL (WINAPI *)(void))
                          GetProcAddress(uxtheme, "IsCompositionActive");
                          
    IsThemeActive = (BOOL (WINAPI *)(void))
                    GetProcAddress(uxtheme, "IsThemeActive");
                    
    if(BufferedPaintInit)
      BufferedPaintInit();
  }
}

Win32Themes::~Win32Themes() {
  if(BufferedPaintUnInit)
    BufferedPaintUnInit();
    
  FreeLibrary(dwmapi);   dwmapi = nullptr;
  FreeLibrary(uxtheme);  uxtheme = nullptr;
  FreeLibrary(usp10dll); usp10dll = nullptr;
  
  DwmEnableComposition = nullptr;
  DwmExtendFrameIntoClientArea = nullptr;
  DwmSetWindowAttribute = nullptr;
  DwmGetColorizationParameters = nullptr;
  DwmGetCompositionTimingInfo = nullptr;
  DwmDefWindowProc = nullptr;
  
  OpenThemeData = nullptr;
  CloseThemeData = nullptr;
  DrawThemeBackground = nullptr;
  DrawThemeEdge = nullptr;
  DrawThemeTextEx = nullptr;
  GetThemeSysFont = nullptr;
  GetThemeSysColor = nullptr;
  GetThemeBackgroundExtent = nullptr;
  GetThemeBackgroundContentRect = nullptr;
  GetThemeBool = nullptr;
  GetThemeColor = nullptr;
  GetThemeMargins = nullptr;
  GetThemeMetric = nullptr;
  GetThemeInt = nullptr;
  GetThemeIntList = nullptr;
  GetThemePartSize = nullptr;
  GetThemePosition = nullptr;
  GetThemeTransitionDuration = nullptr;
  GetCurrentThemeName = nullptr;
  IsAppThemed = nullptr;
  IsThemePartDefined = nullptr;
  GetThemeSysSize = nullptr;
  SetWindowTheme = nullptr;
  
  BufferedPaintInit = nullptr;
  BufferedPaintUnInit = nullptr;
  BufferedPaintStopAllAnimations = nullptr;
  BeginBufferedPaint = nullptr;
  EndBufferedPaint = nullptr;
  IsCompositionActive = nullptr;
  IsThemeActive = nullptr;
}

bool Win32Themes::current_theme_is_aero() {
  if(!GetCurrentThemeName)
    return false;
    
  Array<WCHAR> filebuf(1024);
  
  GetCurrentThemeName(
    filebuf.items(),
    filebuf.length(),
    0, 0,
    0, 0);
    
  filebuf[ filebuf.length()  - 1] = L'\0';
  
  int len = 0;
  while(filebuf[len])
    ++len;
    
  static const char *name = "aero.msstyles";
  
  int namelen = strlen(name);
  if(len < namelen)
    return false;
    
  for(int i = 0; i < namelen; ++i) {
    if(towlower(filebuf[len - namelen + i]) != name[i])
      return false;
  }
  
  return len == namelen || filebuf[len - namelen - 1] == '\\';
}

bool Win32Themes::check_osversion(int min_major, int min_minor) {
  OSVERSIONINFO osvi;
  memset(&osvi, 0, sizeof(osvi));
  osvi.dwOSVersionInfoSize = sizeof(osvi);
  GetVersionEx(&osvi);
  
  if((int)osvi.dwMajorVersion > min_major)
    return true;
    
  return (int)osvi.dwMajorVersion == min_major && (int)osvi.dwMinorVersion >= min_minor;
}

DWORD Win32Themes::get_window_title_text_color(const DWM_COLORIZATION_PARAMS *params, bool active) {
  assert(params);
  
  /* See PaintDotNet.SystemLayer.UIUtil.TryGetWindowColorizationInfo() which uses registry key */
  
//  fprintf(stderr, "[colorization:                      color=%x]\n", params->color);
//  fprintf(stderr, "[colorization:                  afterglow=%x]\n", params->afterglow);
//  fprintf(stderr, "[colorization:              color_balance=%x]\n", params->color_balance);
//  fprintf(stderr, "[colorization:          afterglow_balance=%x]\n", params->afterglow_balance);
//  fprintf(stderr, "[colorization:               blur_balance=%x]\n", params->blur_balance);
//  fprintf(stderr, "[colorization: glass_reflection_intensity=%x]\n", params->glass_reflection_intensity);
//  fprintf(stderr, "[colorization:               opaque_blend=%x]\n", params->opaque_blend);

  if(!active)
    return 0x999999u;
    
  DWORD accent_color = params->color & 0xFFFFFFu;
  
  int red   = (accent_color & 0xFF0000) >> 16;
  int green = (accent_color & 0x00FF00) >> 8;
  int blue  = (accent_color & 0x0000FF);
  
  double gray = (30.0 * red + 59.0 * green + 11.0 * blue) / 25500.0;
  if(gray >= 0.5)
    return 0x000000;
    
  return 0xFFFFFF;
}

bool Win32Themes::try_read_win10_colorization(ColorizationInfo *info) {
  if(!info || !check_osversion(10, 0))
    return false;
    
  bool result = false;
  HKEY key;
  LONG status = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\DWM", 0, KEY_READ, &key);
  if(status == ERROR_SUCCESS) {
    DWORD accent_color_bgr = 0;
    DWORD size = sizeof(DWORD);
    LONG status_ac = RegGetValueW(key, nullptr, L"AccentColor", RRF_RT_REG_DWORD, nullptr, &accent_color_bgr, &size);
    
    DWORD color_prevalence = 0;
    size = sizeof(DWORD);
    LONG status_cp = RegGetValueW(key, nullptr, L"ColorPrevalence", RRF_RT_REG_DWORD, nullptr, &color_prevalence, &size);
    
    if(status_ac == ERROR_SUCCESS && status_cp == ERROR_SUCCESS) {
      int blue  = (accent_color_bgr & 0xFF0000) >> 16;
      int green = (accent_color_bgr & 0x00FF00) >> 8;
      int red   = (accent_color_bgr & 0x0000FF);
      
      double gray = (30.0 * red + 59.0 * green + 11.0 * blue) / 25500.0;
      if(gray >= 0.5)
        info->text_on_accent_color = 0x000000;
      else
        info->text_on_accent_color = 0xFFFFFF;
        
      info->accent_color = (red << 16) | (green << 8) | blue;
      info->has_accent_color_in_active_titlebar = color_prevalence == 1;
      result = true;
    }
  }
  
  RegCloseKey(key);
  return result;
}

