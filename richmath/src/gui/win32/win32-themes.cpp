#include <gui/win32/win32-themes.h>

#include <cwchar>

#include <util/array.h>

using namespace richmath;

HRESULT (WINAPI *Win32Themes::DwmEnableComposition)(UINT) = 0;
HRESULT (WINAPI *Win32Themes::DwmExtendFrameIntoClientArea)(HWND,const MARGINS*) = 0;
HRESULT (WINAPI *Win32Themes::DwmSetWindowAttribute)(HWND,DWORD,LPCVOID,DWORD) = 0;
HRESULT (WINAPI *Win32Themes::DwmGetCompositionTimingInfo)(HWND,DWM_TIMING_INFO*) = 0;
HRESULT (WINAPI *Win32Themes::DwmDefWindowProc)(HWND,UINT,WPARAM,LPARAM,LRESULT*) = 0;

HANDLE (WINAPI *Win32Themes::OpenThemeData)(HWND,LPCWSTR) = 0;
HRESULT (WINAPI *Win32Themes::CloseThemeData)(HANDLE) = 0;
HRESULT (WINAPI *Win32Themes::DrawThemeBackground)(HANDLE,HDC,int,int,const RECT*,const RECT*) = 0;
HRESULT (WINAPI *Win32Themes::DrawThemeEdge)(HANDLE,HDC,int,int,LPCRECT,UINT,UINT,LPRECT) = 0;
HRESULT (WINAPI *Win32Themes::GetThemeSysFont)(HANDLE,int,LOGFONTW*) = 0;
HRESULT (WINAPI *Win32Themes::GetThemeBackgroundExtent)(HANDLE,HDC,int,int,LPCRECT,LPRECT) = 0;
HRESULT (WINAPI *Win32Themes::GetThemeBackgroundContentRect)(HANDLE,HDC hdc,int,int,LPCRECT,LPRECT) = 0;
HRESULT (WINAPI *Win32Themes::GetThemeBool)(HANDLE,int,int,int,BOOL*) = 0;
HRESULT (WINAPI *Win32Themes::GetThemeColor)(HANDLE,int,int,int,COLORREF*) = 0;
HRESULT (WINAPI *Win32Themes::GetThemeMargins)(HANDLE,HDC,int,int,int,LPRECT,MARGINS*) = 0;
HRESULT (WINAPI *Win32Themes::GetThemeMetric)(HANDLE,HDC,int,int,int,int*) = 0;
HRESULT (WINAPI *Win32Themes::GetThemeInt)(HANDLE,int,int,int,int*) = 0;
HRESULT (WINAPI *Win32Themes::GetThemeIntList)(HANDLE,int,int,int,INTLIST*) = 0;
HRESULT (WINAPI *Win32Themes::GetThemePartSize)(HANDLE hTheme,HDC,int,int,LPCRECT,THEME_SIZE,SIZE*) = 0;
HRESULT (WINAPI *Win32Themes::GetThemePosition)(HANDLE,int,int,int,POINT*) = 0;
HRESULT (WINAPI *Win32Themes::GetThemeTransitionDuration)(HANDLE,int,int,int,int,DWORD*) = 0;
HRESULT (WINAPI *Win32Themes::GetCurrentThemeName)(LPWSTR,int,LPWSTR,int,LPWSTR,int) = 0;
BOOL (WINAPI *Win32Themes::IsThemePartDefined)(HANDLE,int,int) = 0;
int (WINAPI *Win32Themes::GetThemeSysSize)(HANDLE,int) = 0;

HRESULT (WINAPI *Win32Themes::BufferedPaintInit)() = 0;
HRESULT (WINAPI *Win32Themes::BufferedPaintUnInit)() = 0;
HANDLE (WINAPI *Win32Themes::BeginBufferedPaint)(HDC,const RECT*,BP_BUFFERFORMAT,BP_PAINTPARAMS*,HDC*) = 0;
HANDLE (WINAPI *Win32Themes::EndBufferedPaint)(HANDLE,BOOL) = 0;
BOOL (WINAPI *Win32Themes::IsCompositionActive)() = 0;
BOOL (WINAPI *Win32Themes::IsThemeActive)() = 0;

HMODULE Win32Themes::dwmapi = 0;
HMODULE Win32Themes::uxtheme = 0;

void Win32Themes::init(){
  static Win32Themes::Win32Themes w;
}

Win32Themes::Win32Themes(): Base(){
  dwmapi = LoadLibrary("dwmapi.dll");
  
  if(dwmapi){
    DwmEnableComposition = (HRESULT (WINAPI *)(UINT))
      GetProcAddress(dwmapi, "DwmEnableComposition");
      
    DwmExtendFrameIntoClientArea = (HRESULT (WINAPI *)(HWND,const MARGINS*))
      GetProcAddress(dwmapi, "DwmExtendFrameIntoClientArea");
      
    DwmSetWindowAttribute = (HRESULT (WINAPI *)(HWND,DWORD,LPCVOID,DWORD))
      GetProcAddress(dwmapi, "DwmSetWindowAttribute");
    
    DwmGetCompositionTimingInfo = (HRESULT (WINAPI *)(HWND,DWM_TIMING_INFO*))
      GetProcAddress(dwmapi, "DwmGetCompositionTimingInfo");
    
    DwmDefWindowProc = (HRESULT (WINAPI *)(HWND,UINT,WPARAM,LPARAM,LRESULT*))
      GetProcAddress(dwmapi, "DwmDefWindowProc");
  }
  
  uxtheme = LoadLibrary("uxtheme.dll");
  if(uxtheme){
    OpenThemeData = (HANDLE (WINAPI *)(HWND,LPCWSTR))
      GetProcAddress(uxtheme, "OpenThemeData");
    
    CloseThemeData = (HRESULT (WINAPI *)(HANDLE))
      GetProcAddress(uxtheme, "CloseThemeData");
    
    DrawThemeBackground = (HRESULT (WINAPI *)(HANDLE,HDC,int,int,const RECT*,const RECT*))
      GetProcAddress(uxtheme, "DrawThemeBackground");
    
    DrawThemeEdge = (HRESULT (WINAPI *)(HANDLE,HDC,int,int,LPCRECT,UINT,UINT,LPRECT))
      GetProcAddress(uxtheme, "DrawThemeEdge");
    
    GetThemeSysFont = (HRESULT (WINAPI *)(HANDLE,int,LOGFONTW*))
      GetProcAddress(uxtheme, "GetThemeSysFont");
    
    GetThemeBackgroundExtent = (HRESULT (WINAPI *)(HANDLE,HDC,int,int,LPCRECT,LPRECT))
      GetProcAddress(uxtheme, "GetThemeBackgroundExtent");
    
    GetThemeBackgroundContentRect = (HRESULT (WINAPI *)(HANDLE,HDC hdc,int,int,LPCRECT,LPRECT))
      GetProcAddress(uxtheme, "GetThemeBackgroundContentRect");
    
    GetThemeBool = (HRESULT (WINAPI *)(HANDLE,int,int,int,BOOL*))
      GetProcAddress(uxtheme, "GetThemeBool");
    
    GetThemeColor = (HRESULT (WINAPI *)(HANDLE,int,int,int,COLORREF*))
      GetProcAddress(uxtheme, "GetThemeColor");

    GetThemeMargins = (HRESULT (WINAPI *)(HANDLE,HDC,int,int,int,LPRECT,MARGINS*))
      GetProcAddress(uxtheme, "GetThemeMargins");
    
    GetThemeMetric = (HRESULT (WINAPI *)(HANDLE,HDC,int,int,int,int*))
      GetProcAddress(uxtheme, "GetThemeMetric");
    
    GetThemeInt = (HRESULT (WINAPI *)(HANDLE,int,int,int,int*))
      GetProcAddress(uxtheme, "GetThemeInt");
    
    GetThemeIntList = (HRESULT (WINAPI *)(HANDLE,int,int,int,INTLIST*))
      GetProcAddress(uxtheme, "GetThemeIntList");
    
    GetThemePartSize = (HRESULT (WINAPI *)(HANDLE hTheme,HDC,int,int,LPCRECT,THEME_SIZE,SIZE*))
      GetProcAddress(uxtheme, "GetThemePartSize");
    
    GetThemePosition = (HRESULT (WINAPI *)(HANDLE,int,int,int,POINT*))
      GetProcAddress(uxtheme, "GetThemePosition");
    
    GetThemeTransitionDuration = (HRESULT (WINAPI *)(HANDLE,int,int,int,int,DWORD*))
      GetProcAddress(uxtheme, "GetThemeTransitionDuration");
    
    GetCurrentThemeName = (HRESULT (WINAPI *)(LPWSTR,int,LPWSTR,int,LPWSTR,int))
      GetProcAddress(uxtheme, "GetCurrentThemeName");
      
    IsThemePartDefined = (BOOL (WINAPI *)(HANDLE,int,int))
      GetProcAddress(uxtheme, "IsThemePartDefined");
    
    GetThemeSysSize = (int (WINAPI *)(HANDLE,int))
      GetProcAddress(uxtheme, "GetThemeSysSize");
    
    
    
    BufferedPaintInit = (HRESULT (WINAPI *)())
      GetProcAddress(uxtheme, "BufferedPaintInit");
    
    BufferedPaintUnInit = (HRESULT (WINAPI *)())
      GetProcAddress(uxtheme, "BufferedPaintUnInit");
    
    BeginBufferedPaint = (HANDLE (WINAPI *)(HDC,const RECT*,BP_BUFFERFORMAT,BP_PAINTPARAMS*,HDC*))
      GetProcAddress(uxtheme, "BeginBufferedPaint");
    
    EndBufferedPaint = (HANDLE (WINAPI *)(HANDLE,BOOL))
      GetProcAddress(uxtheme, "EndBufferedPaint");
    
    IsCompositionActive = (BOOL (WINAPI *)())
      GetProcAddress(uxtheme, "IsCompositionActive");
    
    IsThemeActive = (BOOL (WINAPI *)())
      GetProcAddress(uxtheme, "IsThemeActive");
    
    if(BufferedPaintInit)
      BufferedPaintInit();
  }
}

Win32Themes::~Win32Themes(){
  if(BufferedPaintUnInit)
    BufferedPaintUnInit();
  
  FreeLibrary(dwmapi);   dwmapi = 0;
  FreeLibrary(uxtheme);  uxtheme = 0;
  
  DwmEnableComposition = 0;
  DwmExtendFrameIntoClientArea = 0;
  DwmSetWindowAttribute = 0;
  DwmGetCompositionTimingInfo = 0;
  DwmDefWindowProc = 0;
  
  OpenThemeData = 0;
  CloseThemeData = 0;
  DrawThemeBackground = 0;
  DrawThemeEdge = 0;
  GetThemeSysFont = 0;
  GetThemeBackgroundExtent = 0;
  GetThemeBackgroundContentRect = 0;
  GetThemeBool = 0;
  GetThemeColor = 0;
  GetThemeMargins = 0;
  GetThemeMetric = 0;
  GetThemeInt = 0;
  GetThemeIntList = 0;
  GetThemePartSize = 0;
  GetThemePosition = 0;
  GetThemeTransitionDuration = 0;
  GetCurrentThemeName = 0;
  IsThemePartDefined = 0;
  GetThemeSysSize = 0;

  BufferedPaintInit = 0;
  BufferedPaintUnInit = 0;
  BeginBufferedPaint = 0;
  EndBufferedPaint = 0;
  IsCompositionActive = 0;
  IsThemeActive = 0;
}

bool Win32Themes::current_theme_is_aero(){
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
  
  for(int i = 0;i < namelen;++i){
    if(towlower(filebuf[len - namelen + i]) != name[i])
      return false;
  }
  
  return len == namelen || filebuf[len - namelen - 1] == '\\';
}
