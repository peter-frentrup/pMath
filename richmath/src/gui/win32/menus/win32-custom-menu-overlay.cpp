#include <gui/win32/menus/win32-custom-menu-overlay.h>


namespace richmath {
  class Win32CustomMenuOverlay::Impl {
    public:
      Impl(Win32CustomMenuOverlay &self);
      
      static void init_window_class();
      
    private:
      static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    
    private:
      Win32CustomMenuOverlay &self;
  };
}

using namespace richmath;

static ATOM menu_overlay_class = 0;
static const wchar_t menu_overlay_class_name[] = L"RichmathMenuOverlay";

//{ class Win32CustomMenuOverlay ...

Win32CustomMenuOverlay::~Win32CustomMenuOverlay() {
  if(control) {
    SetWindowLongPtr(control, GWLP_USERDATA, 0);
  }
}

void Win32CustomMenuOverlay::update_rect(HWND hwnd, HMENU menu) {
  RECT rect;
  bool valid_rect = calc_rect(rect, hwnd, menu);
  
  if(!control) {
    prepare_menu_window_for_children(hwnd);
    Impl::init_window_class();
    
    control = CreateWindowExW(
                WS_EX_NOACTIVATE,
                menu_overlay_class_name,
                L"",
                WS_CHILD | WS_VISIBLE,
                rect.left,
                rect.top,
                rect.right - rect.left,
                rect.bottom - rect.top,
                hwnd,
                nullptr,
                nullptr,
                this);
  }
  else if(valid_rect) {
    MoveWindow(
      control,
      rect.left,
      rect.top,
      rect.right - rect.left,
      rect.bottom - rect.top,
      TRUE);
  }
}

bool Win32CustomMenuOverlay::calc_rect(RECT &rect, HWND hwnd, HMENU menu) {
  return Win32MenuItemOverlay::calc_rect(rect, hwnd, menu, Win32MenuItemOverlay::All);
}

String Win32CustomMenuOverlay::text() {
  if(!control)
    return String{};
  
  int len = GetWindowTextLengthW(control);
  
  pmath_string_t str = pmath_string_new_raw(len + 1);
  uint16_t *buf;
  if(pmath_string_begin_write(&str, &buf, &len)) {
    len = GetWindowTextW(control, (wchar_t*)buf, len);
    pmath_string_end_write(&str, &buf);
  }
  else
    len = 0;
  
  str = pmath_string_part(str, 0, len);
  return String{str};
}

void Win32CustomMenuOverlay::text(String str) {
  if(!control)
    return;
  
  str += String::FromChar(0);
  if(const uint16_t *buf = str.buffer())
    SetWindowTextW(control, (const wchar_t*)buf);
}

bool Win32CustomMenuOverlay::on_create(CREATESTRUCTW *args) {
  return true;
}

LRESULT Win32CustomMenuOverlay::on_wndproc(UINT message, WPARAM wParam, LPARAM lParam) {
  switch(message) {
    case WM_CREATE: return on_create((CREATESTRUCTW*)lParam) ? 0 : (LRESULT)(-1);
    
    case WM_PRINT:
    case WM_PRINTCLIENT: 
      on_paint((HDC)wParam);
      return 0;
    case WM_PAINT: {
        PAINTSTRUCT paintStruct;
        HDC dc = BeginPaint(control, &paintStruct);
        SetLayout(dc, 0);
        SendMessageW(GetParent(control), WM_CTLCOLORSTATIC, (WPARAM)dc, (LPARAM)control);
        on_paint(dc);
        EndPaint(control, &paintStruct);
      } return 0;
  }
  return DefWindowProcW(control, message, wParam, lParam);
}

//} ... class Win32CustomMenuOverlay

//{ class Win32CustomMenuOverlay::Impl ...

Win32CustomMenuOverlay::Impl::Impl(Win32CustomMenuOverlay &self)
: self{self}
{
}

void Win32CustomMenuOverlay::Impl::init_window_class() {
  static bool initialized = false;
  if(initialized)
    return;
  
  WNDCLASSEXW wincl;
  
  memset(&wincl, 0, sizeof(wincl));
  
  wincl.cbSize = sizeof(wincl);
  wincl.hInstance = GetModuleHandleW(nullptr);
  wincl.lpszClassName = menu_overlay_class_name;
  wincl.lpfnWndProc = window_proc;
  
  menu_overlay_class = RegisterClassExW(&wincl);
  if(!menu_overlay_class) {
    perror("Win32CustomMenuOverlay::Impl::init_window_class() failed\n");
    abort();
  }
  
  initialized = true;
}

LRESULT CALLBACK Win32CustomMenuOverlay::Impl::window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  Win32CustomMenuOverlay *overlay = (Win32CustomMenuOverlay *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  
  if(!overlay && message == WM_NCCREATE) {
    overlay = (Win32CustomMenuOverlay *)(((CREATESTRUCT *)lParam)->lpCreateParams);
    
    if(!overlay)
      return FALSE;
      
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)overlay);
    
    overlay->control = hwnd;
  }
  
  return overlay->on_wndproc(message, wParam, lParam);
}

//} ... class Win32MenuSearchOverlay::Impl
