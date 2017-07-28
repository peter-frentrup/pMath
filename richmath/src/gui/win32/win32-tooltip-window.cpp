#define _WIN32_WINNT 0x0600
#define WINVER       0x0600

#include <gui/win32/win32-tooltip-window.h>

#include <boxes/mathsequence.h>
#include <boxes/section.h>

#include <gui/win32/win32-themes.h>

#include <cmath>
#include <cstdio>


using namespace richmath;

static Win32TooltipWindow *tooltip_window = 0;

static const wchar_t win32_tooltip_class_name[] = L"RichmathWin32Tooltip";

//{ class Win32TooltipWindow ...

Win32TooltipWindow::Win32TooltipWindow()
  : Win32Widget(
    new Document(),
    WS_EX_TOOLWINDOW,
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
  Win32Widget::after_construction();
  
  if(!document()->style)
    document()->style = new Style;
  document()->style->set(Editable,            false);
  document()->style->set(Selectable,          false);
  document()->style->set(ShowSectionBracket,  false);
  document()->select(0, 0, 0);
}

Win32TooltipWindow::~Win32TooltipWindow() {
  if(this == tooltip_window)
    tooltip_window = 0;
}

void Win32TooltipWindow::move_global_tooltip() {
  if(!tooltip_window || !IsWindowVisible(tooltip_window->_hwnd))
    return;
    
  tooltip_window->resize(true);
}

void Win32TooltipWindow::show_global_tooltip(Expr boxes) {
  if(!tooltip_window) {
    tooltip_window = new Win32TooltipWindow();
    tooltip_window->init();
  }
  
  if(tooltip_window->_content_expr != boxes) {
    tooltip_window->_content_expr = boxes;
    
    Document *doc = tooltip_window->document();
    doc->remove(0, doc->length());
    
    Style *style = new Style;
    style->set(BaseStyleName,       "ControlStyle");
    style->set(SectionMarginLeft,   0);
    style->set(SectionMarginTop,    0);
    style->set(SectionMarginRight,  0);
    style->set(SectionMarginBottom, 0);
    
    boxes = Call(Symbol(PMATH_SYMBOL_BUTTONBOX),
                 boxes,
                 Rule(Symbol(PMATH_SYMBOL_BUTTONFRAME),
                      String("TooltipWindow")));
                      
    MathSection *section = new MathSection(style);
    section->content()->load_from_object(boxes, BoxInputFlags::FormatNumbers);
    doc->insert(0, section);
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
    delete tooltip_window;
}

void Win32TooltipWindow::page_size(float *w, float *h) {
  Win32Widget::page_size(w, h);
  *w = HUGE_VAL;
}

void Win32TooltipWindow::resize(bool just_move) {
  if(!just_move) {
    if(Win32Themes::IsThemeActive
        && Win32Themes::IsThemeActive()) {
      SetWindowRgn(_hwnd, CreateRoundRectRgn(0, 0, best_width + 1, best_height + 1, 4, 4), FALSE);
    }
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

void Win32TooltipWindow::paint_background(Canvas *canvas) {
  Win32Widget::paint_background(canvas);
  
//  RECT rect;
//  GetClientRect(_hwnd, &rect);
//
//  ControlPainter::std->draw_container(
//    canvas,
//    TooltipWindow,
//    Normal,
//    0, 0,
//    rect.right,
//    rect.bottom);
}

void Win32TooltipWindow::paint_canvas(Canvas *canvas, bool resize_only) {
  Win32Widget::paint_canvas(canvas, resize_only);
  
  int old_bh = best_height;
  int old_bw = best_width;
  
  best_height = (int)floorf(document()->extents().height() * scale_factor() + 0.5);
  best_width  = (int)floorf(document()->unfilled_width     * scale_factor() + 0.5);
  
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
  if(!initializing()) {
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
  
  return Win32Widget::callback(message, wParam, lParam);
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
