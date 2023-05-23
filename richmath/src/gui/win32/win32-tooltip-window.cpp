#include <gui/win32/win32-tooltip-window.h>

#include <boxes/mathsequence.h>
#include <boxes/section.h>

#include <gui/common-tooltips.h>
#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-themes.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif


using namespace richmath;

static Win32TooltipWindow *tooltip_window = nullptr;

static const wchar_t win32_tooltip_class_name[] = L"RichmathWin32Tooltip";

//{ class Win32TooltipWindow ...

Win32TooltipWindow::Win32TooltipWindow()
  : base(
    new Document(),
    WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
    WS_POPUP | WS_CLIPCHILDREN | WS_DISABLED,
    0,
    0,
    100,
    100,
    nullptr)
{
  init_tooltip_class();
  set_window_class_name(win32_tooltip_class_name);
}

void Win32TooltipWindow::after_construction() {
  base::after_construction();
  
  document()->style.set(Editable,            false);
  document()->style.set(Selectable,          AutoBoolFalse);
  document()->style.set(ShowSectionBracket,  AutoBoolFalse);
  document()->select(nullptr, 0, 0);
}

Win32TooltipWindow::~Win32TooltipWindow() {
  if(this == tooltip_window)
    tooltip_window = nullptr;
}

void Win32TooltipWindow::move_global_tooltip() {
  if(!tooltip_window || !IsWindowVisible(tooltip_window->_hwnd))
    return;
    
  tooltip_window->resize(true);
}

void Win32TooltipWindow::show_global_tooltip(Box *source, Expr boxes, SharedPtr<Stylesheet> stylesheet) {
  if(!tooltip_window) {
    tooltip_window = new Win32TooltipWindow();
    tooltip_window->init();
  }
  
  tooltip_window->source_box(source);
  if(tooltip_window->_content_expr != boxes) {
    tooltip_window->_content_expr = boxes;
    
    CommonTooltips::load_content(
      tooltip_window->document(), 
      PMATH_CPP_MOVE(boxes), 
      PMATH_CPP_MOVE(stylesheet));
  }
  
  if(!IsWindowVisible(tooltip_window->_hwnd))
    ShowWindow(tooltip_window->_hwnd, SW_SHOWNA);
    
  move_global_tooltip();
}

void Win32TooltipWindow::hide_global_tooltip() {
  if(tooltip_window) {
    ShowWindow(tooltip_window->_hwnd, SW_HIDE);
    tooltip_window->_content_expr = Expr(PMATH_UNDEFINED);
  }
}

void Win32TooltipWindow::delete_global_tooltip() {
  if(tooltip_window)
    tooltip_window->safe_destroy();
}

Vector2F Win32TooltipWindow::page_size() {
  Vector2F size = base::page_size();
  size.x = HUGE_VAL;
  return size;
}

bool Win32TooltipWindow::is_using_dark_mode() {
  static int recursion = 0;
  
  for(Box *src = source_box(); src; src = src->parent()) {
    ControlContext *ctx = dynamic_cast<ControlContext*>(src);
    if(!ctx) {
      if(Document *doc = dynamic_cast<Document*>(src))
        ctx = doc->native();
    }
    
    if(ctx) {
      bool result = false;
      if(recursion < 2) {
        ++recursion;
        result = ctx->is_using_dark_mode();
        --recursion;
      }
      return result;
    }
  }
  
  return false;
}

int Win32TooltipWindow::dpi() {
  return Win32HighDpi::get_dpi_for_window(_hwnd);
}

void Win32TooltipWindow::resize(bool just_move) {
  if(!just_move) {
    BoxRadius radii;
    ControlPainter::std->calc_container_radii(*this, ContainerType::TooltipWindow, &radii);
    float xrad = std::min(std::min(radii.top_left_x, radii.top_right_x), std::min(radii.bottom_left_x, radii.bottom_right_x));
    float yrad = std::min(std::min(radii.top_left_y, radii.top_right_y), std::min(radii.bottom_left_y, radii.bottom_right_y));
    
    int rad_w = xrad * 2 / 0.75;
    int rad_h = yrad * 2 / 0.75;
    if(rad_w > 0 && rad_h > 0) // Win32Themes::IsThemeActive && Win32Themes::IsThemeActive()
      SetWindowRgn(_hwnd, CreateRoundRectRgn(0, 0, best_width + 1, best_height + 1, rad_w, rad_h), FALSE);
    else
      SetWindowRgn(_hwnd, nullptr, FALSE);
  }
  
  MONITORINFO moninfo;
  memset(&moninfo, 0, sizeof(moninfo));
  moninfo.cbSize = sizeof(moninfo);
  
  HMONITOR hmon = MonitorFromWindow(_hwnd, MONITOR_DEFAULTTONEAREST);
  GetMonitorInfo(hmon, &moninfo);
  
  int cx = GetSystemMetrics(SM_CXSMICON);
  int cy = GetSystemMetrics(SM_CYSMICON);
  
  POINT pt;
  GetCursorPos(&pt);
  
  if(pt.x + cx + best_width > moninfo.rcMonitor.right) {
    pt.x -= cx + best_width;
    
    if(pt.x < moninfo.rcMonitor.left)
      pt.x = moninfo.rcMonitor.left;
  }
  else
    pt.x += cx;
    
  if(pt.y + cy + best_height > moninfo.rcMonitor.bottom) {
    pt.y -= cy + best_height;
    
    if(pt.y < moninfo.rcMonitor.top)
      pt.y = moninfo.rcMonitor.top;
  }
  else
    pt.y += cy;
    
  SetWindowPos(_hwnd,
               HWND_TOPMOST,
               pt.x,
               pt.y,
               best_width,
               best_height,
               just_move ? SWP_NOSIZE | SWP_NOACTIVATE : SWP_NOZORDER | SWP_NOACTIVATE);
}

void Win32TooltipWindow::paint_canvas(Canvas &canvas, bool resize_only) {
  base::paint_canvas(canvas, resize_only);
  
  int old_bh = best_height;
  int old_bw = best_width;
  
  best_height = (int)round(document()->extents().height() * scale_factor());
  best_width  = (int)round(document()->unfilled_width     * scale_factor());
  
  if(best_height < 1)
    best_height = 1;
    
  if(best_width < 1)
    best_width = 1;
    
  RECT outer, inner;
  GetWindowRect(_hwnd, &outer);
  GetClientRect(_hwnd, &inner);
  
  best_width +=  outer.right  - outer.left - inner.right  + inner.left;
  best_height += outer.bottom - outer.top  - inner.bottom + inner.top;
  
  if(old_bw != best_width || old_bh != best_height) {
    resize(false);
  }
}

LRESULT Win32TooltipWindow::callback(UINT message, WPARAM wParam, LPARAM lParam) {
  if(!initializing() && !destroying()) {
    switch(message) {
      case WM_NCHITTEST:
        return HTNOWHERE;
        
      case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
        
      case WM_ACTIVATEAPP:
        if(!wParam)
          ShowWindow(_hwnd, SW_HIDE);
        break;
    }
  }
  
  return base::callback(message, wParam, lParam);
}

void Win32TooltipWindow::init_tooltip_class() {
  static bool window_class_initialized = false;
  if(window_class_initialized)
    return;
    
  WNDCLASSEXW wincl;
  
  memset(&wincl, 0, sizeof(wincl));
  
  wincl.cbSize        = sizeof(wincl);
  wincl.hInstance     = GetModuleHandle(0);
  wincl.lpszClassName = win32_tooltip_class_name;
  wincl.lpfnWndProc   = BasicWin32Widget::window_proc;
  wincl.style         = CS_DROPSHADOW;
  
  if(!RegisterClassExW(&wincl)) {
    perror("init_tooltip_class() failed\n");
    abort();
  }
  
  window_class_initialized = true;
}

//} ... class Win32TooltipWindow
