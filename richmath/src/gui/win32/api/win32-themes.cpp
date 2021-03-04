#include <gui/win32/api/win32-themes.h>

#include <cstdio>
#include <cwchar>

#include <util/array.h>
#include <gui/win32/ole/combase.h>


using namespace richmath;

HRESULT(WINAPI * Win32Themes::DwmEnableComposition)(UINT) = nullptr;
HRESULT(WINAPI * Win32Themes::DwmExtendFrameIntoClientArea)(HWND, const MARGINS *) = nullptr;
HRESULT(WINAPI * Win32Themes::DwmGetWindowAttribute)(HWND, DWORD, PVOID, DWORD) = nullptr;
HRESULT(WINAPI * Win32Themes::DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD) = nullptr;
HRESULT(WINAPI * Win32Themes::DwmGetCompositionTimingInfo)(HWND, DWM_TIMING_INFO *) = nullptr;
HRESULT(WINAPI * Win32Themes::DwmGetColorizationParameters)(Win32Themes::DWM_COLORIZATION_PARAMS *) = nullptr;
HRESULT(WINAPI * Win32Themes::DwmDefWindowProc)(HWND, UINT, WPARAM, LPARAM, LRESULT *) = nullptr;
HRESULT(WINAPI * Win32Themes::DwmEnableBlurBehindWindow)(HWND, const DWM_BLURBEHIND*) = nullptr;

HRESULT(WINAPI * Win32Themes::DwmpActivateLivePreview_win7)(BOOL, HWND, HWND, LivePreviewTrigger) = nullptr;
HRESULT(WINAPI * Win32Themes::DwmpActivateLivePreview_win81)(BOOL, HWND, HWND, LivePreviewTrigger, RECT *) = nullptr;

BOOL(WINAPI * Win32Themes::GetWindowCompositionAttribute)(HWND, WINCOMPATTRDATA*) = nullptr;
BOOL(WINAPI * Win32Themes::SetWindowCompositionAttribute)(HWND, const WINCOMPATTRDATA*) = nullptr;
      
HANDLE(WINAPI * Win32Themes::OpenThemeData)(HWND, LPCWSTR) = nullptr;
HANDLE(WINAPI * Win32Themes::OpenThemeDataForDpi)(HWND, LPCWSTR, UINT) = nullptr;
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
HMODULE Win32Themes::user32 = nullptr;

void Win32Themes::init() {
  static Win32Themes w;
}

Win32Themes::Win32Themes()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  if(!user32) {
    user32 = LoadLibrary("user32.dll");
    if(user32) {
      GetWindowCompositionAttribute = (BOOL(WINAPI *)(HWND, WINCOMPATTRDATA*))
                                      GetProcAddress(user32, "GetWindowCompositionAttribute");
      
      SetWindowCompositionAttribute = (BOOL(WINAPI *)(HWND, const WINCOMPATTRDATA*))
                                      GetProcAddress(user32, "SetWindowCompositionAttribute");
                                     
    }
  }
  
  if(!dwmapi) {
    dwmapi = LoadLibrary("dwmapi.dll");
    if(dwmapi) {
      DwmEnableComposition = (HRESULT(WINAPI *)(UINT))
                             GetProcAddress(dwmapi, "DwmEnableComposition");
                             
      DwmExtendFrameIntoClientArea = (HRESULT(WINAPI *)(HWND, const MARGINS *))
                                     GetProcAddress(dwmapi, "DwmExtendFrameIntoClientArea");
                                     
      DwmGetWindowAttribute = (HRESULT(WINAPI *)(HWND, DWORD, PVOID, DWORD))
                              GetProcAddress(dwmapi, "DwmGetWindowAttribute");
                                     
      DwmSetWindowAttribute = (HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD))
                              GetProcAddress(dwmapi, "DwmSetWindowAttribute");
                              
      DwmGetColorizationParameters = (HRESULT(WINAPI *)(DWM_COLORIZATION_PARAMS*))
                                     GetProcAddress(dwmapi, (LPCSTR)127);
                                     
      DwmGetCompositionTimingInfo = (HRESULT(WINAPI *)(HWND, DWM_TIMING_INFO *))
                                    GetProcAddress(dwmapi, "DwmGetCompositionTimingInfo");
                                    
      DwmDefWindowProc = (HRESULT(WINAPI *)(HWND, UINT, WPARAM, LPARAM, LRESULT *))
                         GetProcAddress(dwmapi, "DwmDefWindowProc");
      
      DwmEnableBlurBehindWindow = (HRESULT(WINAPI *)(HWND, const DWM_BLURBEHIND*))
                                  GetProcAddress(dwmapi, "DwmEnableBlurBehindWindow");
      
      if(is_windows_8_1_or_newer()) {
        DwmpActivateLivePreview_win81 = (HRESULT(WINAPI *)(BOOL, HWND, HWND, LivePreviewTrigger, RECT*))
                                        GetProcAddress(dwmapi, MAKEINTRESOURCEA(113));
      }
      else if(is_windows_7_or_newer()) {
        DwmpActivateLivePreview_win7 = (HRESULT(WINAPI *)(BOOL, HWND, HWND, LivePreviewTrigger))
                                       GetProcAddress(dwmapi, MAKEINTRESOURCEA(113));
      }
    }
  }
  
  if(!uxtheme) {
    uxtheme = LoadLibrary("uxtheme.dll");
    if(uxtheme) {
      OpenThemeData = (HANDLE(WINAPI *)(HWND, LPCWSTR))
                      GetProcAddress(uxtheme, "OpenThemeData");
                      
      OpenThemeDataForDpi = (HANDLE(WINAPI *)(HWND, LPCWSTR, UINT))
                            GetProcAddress(uxtheme, "OpenThemeDataForDpi");
                            
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
}

Win32Themes::~Win32Themes() {
  if(BufferedPaintUnInit)
    BufferedPaintUnInit();
    
  FreeLibrary(dwmapi);  dwmapi = nullptr;
  FreeLibrary(uxtheme); uxtheme = nullptr;
  FreeLibrary(user32);  user32 = nullptr;
  
  GetWindowCompositionAttribute = nullptr;
  SetWindowCompositionAttribute = nullptr;
  
  DwmEnableComposition = nullptr;
  DwmExtendFrameIntoClientArea = nullptr;
  DwmGetWindowAttribute = nullptr;
  DwmSetWindowAttribute = nullptr;
  DwmGetColorizationParameters = nullptr;
  DwmGetCompositionTimingInfo = nullptr;
  DwmDefWindowProc = nullptr;
  DwmEnableBlurBehindWindow = nullptr;
  
  DwmpActivateLivePreview_win7 = nullptr;
  DwmpActivateLivePreview_win81 = nullptr;
  
  OpenThemeData = nullptr;
  OpenThemeDataForDpi = nullptr;
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

bool Win32Themes::check_osversion(int min_major, int min_minor, int min_build) {
  OSVERSIONINFO osvi;
  memset(&osvi, 0, sizeof(osvi));
  osvi.dwOSVersionInfoSize = sizeof(osvi);
  GetVersionEx(&osvi);
  
  if((int)osvi.dwMajorVersion > min_major)
    return true;
    
  if((int)osvi.dwMajorVersion < min_major)
    return false;
    
  if((int)osvi.dwMinorVersion > min_minor)
    return true;
    
  if((int)osvi.dwMinorVersion < min_minor)
    return false;
    
  return (int)osvi.dwBuildNumber >= min_build;
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
    
  DWORD accent_color_rgb = params->color & 0xFFFFFFu;
  
  int red   = (accent_color_rgb & 0xFF0000) >> 16;
  int green = (accent_color_rgb & 0x00FF00) >> 8;
  int blue  = (accent_color_rgb & 0x0000FF);
  
  double gray = (30.0 * red + 59.0 * green + 11.0 * blue) / 25500.0;
  if(gray >= 0.5)
    return 0x000000;
    
  return 0xFFFFFF;
}

bool Win32Themes::try_read_win10_colorization(ColorizationInfo *info) {
  if(!info || !is_windows_10_or_newer())
    return false;
  
  // TODO: use WinRT API (Windows.UI.ViewManagement.IUISettings3), cf. https://github.com/res2k/Windows10Colors
  bool result = false;
  HKEY key = nullptr;
  LONG status = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\DWM", 0, KEY_READ, &key);
  if(status == ERROR_SUCCESS) {
    info->accent_color = 0;
    DWORD size = sizeof(DWORD);
    LONG status_ac = RegGetValueW(key, nullptr, L"AccentColor", RRF_RT_REG_DWORD, nullptr, &info->accent_color, &size);
    info->accent_color &= 0xFFFFFFu;
    
    DWORD color_prevalence = 0;
    size = sizeof(DWORD);
    LONG status_cp = RegGetValueW(key, nullptr, L"ColorPrevalence", RRF_RT_REG_DWORD, nullptr, &color_prevalence, &size);
    
    if(status_ac == ERROR_SUCCESS && status_cp == ERROR_SUCCESS) {
      int blue  = (info->accent_color & 0xFF0000) >> 16;
      int green = (info->accent_color & 0x00FF00) >> 8;
      int red   = (info->accent_color & 0x0000FF);
      
      double gray = (30.0 * red + 59.0 * green + 11.0 * blue) / 25500.0;
      if(gray >= 0.5)
        info->text_on_accent_color = 0x000000;
      else
        info->text_on_accent_color = 0xFFFFFF;
      
      info->has_accent_color_in_active_titlebar = color_prevalence == 1;
      result = true;
    }
    RegCloseKey(key);
  }
  
  return result;
}

bool Win32Themes::use_high_contrast() {
  HIGHCONTRASTW hc;
  memset(&hc, 0, sizeof(hc));
  hc.cbSize = sizeof(hc);
  if(SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0)) {
    return (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
  }
  return false;
}

bool Win32Themes::use_win10_transparency() {
  if(use_high_contrast())
    return false;
  
  // https://superuser.com/questions/1245923/registry-keys-to-change-personalization-settings
  if(!is_windows_10_or_newer())
    return false;
  
  bool result = false;
  HKEY key = nullptr;
  LONG status = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &key);
  if(status == ERROR_SUCCESS) {
    
    DWORD enable_transparency = 0;
    DWORD size = sizeof(DWORD);
    LONG status_et = RegGetValueW(key, nullptr, L"EnableTransparency", RRF_RT_REG_DWORD, nullptr, &enable_transparency, &size);
    if(status_et == ERROR_SUCCESS) {
      result = !!enable_transparency;
    }
  
    RegCloseKey(key);
  }
  
  return result;
}

void Win32Themes::try_set_dark_mode_frame(HWND hwnd, bool dark_mode) {
//  // Before 1903, we should SetPropW(hwnd, L"UseImmersiveDarkModeColors", dark_mode), see https://github.com/ysc3839/win32-darkmode
//  if(is_windows_10_1903_or_newer()) {
//    if(SetWindowCompositionAttribute) {
//      WINCOMPATTRDATA data {};
//      data.attr = UndocumentedWindowCompositionAttribute::UseDarkModeColors;
//      BOOL value = dark_mode;
//      data.data = &value;
//      data.data_size = sizeof(value);
//      SetWindowCompositionAttribute(hwnd, &data);
//    }
//  }
  if(DwmSetWindowAttribute) { // See https://github.com/microsoft/Terminal/issues/299
    BOOL value = dark_mode;
    if(is_windows_10_1909_or_newer()) {
      HRreport(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_new, &value, sizeof(value)));
    }
    else if(is_windows_10_1809_or_newer()) {
      HRreport(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_old, &value, sizeof(value)));
    }
  }
}

bool Win32Themes::activate_aero_peak(bool activate, HWND exclude, HWND insert_before, LivePreviewTrigger trigger) {
  if(DwmpActivateLivePreview_win81) 
    return HRbool(DwmpActivateLivePreview_win81(activate, exclude, insert_before, trigger, nullptr));
  
  if(DwmpActivateLivePreview_win7)
    return HRbool(DwmpActivateLivePreview_win7(activate, exclude, insert_before, trigger));
  
  return false;
}
