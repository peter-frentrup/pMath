#define _WIN32_WINNT 0x0600

#include <stdio.h>
#include <cairo-win32.h>

#include <graphics/canvas.h>
#include <gui/win32/basic-win32-window.h>

using namespace richmath;

#define CAPTION_BUTTON_INFLATE  (-2)
#define CAPTION_BUTTON_DIST     (-2)

//{ class BasicWin32Window ...

BasicWin32Window::BasicWin32Window(
  DWORD style_ex, 
  DWORD style,
  int x, 
  int y, 
  int width,
  int height)
: BasicWin32Widget(
    style_ex, 
    (style & ~WS_CHILD), 
    x, 
    y, 
    width, 
    height,
    NULL),
  min_client_height(0),
  max_client_height(-1),
  min_client_width(0),
  max_client_width(-1),
  _active(false),
  _glass_enabled(false),
  _snap_affinity(0),
  _special_frame(false),
  snap_correction_x(0),
  snap_correction_y(0),
  last_moving_x(0),
  last_moving_y(0)
{
  memset(&_extra_glass, 0, sizeof(_extra_glass));
}

void BasicWin32Window::after_construction(){
  BasicWin32Widget::after_construction();
}

BasicWin32Window::~BasicWin32Window(){
}

void BasicWin32Window::get_client_rect(RECT *rect){
//  Win32Themes::MARGINS margins;
//  RECT winrect;
//  
//  GetWindowRect(_hwnd, &winrect);
//  get_nc_margins(&margins);
//  
//  rect->left   = margins.cxLeftWidth;
//  rect->top    = margins.cyTopHeight;
//  rect->right  = winrect.right  - winrect.left - margins.cxRightWidth;//   - margins.cxLeftWidth;
//  rect->bottom = winrect.bottom - winrect.top  - margins.cyBottomHeight;// - margins.cyTopHeight;
  
  GetClientRect(_hwnd, rect);
}

void BasicWin32Window::get_client_size(int *width, int *height){
  RECT rect;
  get_client_rect(&rect);
  
  *width  = rect.right  - rect.left;
  *height = rect.bottom - rect.top;
}

void BasicWin32Window::get_glassfree_rect(RECT *rect){
  if(_extra_glass.cxLeftWidth    == -1
  && _extra_glass.cxRightWidth   == -1
  && _extra_glass.cyTopHeight    == -1
  && _extra_glass.cyBottomHeight == -1){
    SetRectEmpty(rect);
  }
  else{
    get_client_rect(rect);
    rect->left   += _extra_glass.cxLeftWidth;
    rect->top    += _extra_glass.cyTopHeight;
    rect->right  -= _extra_glass.cxRightWidth;
    rect->bottom -= _extra_glass.cyBottomHeight;
  }
}

void BasicWin32Window::get_nc_margins(Win32Themes::MARGINS *margins){
  DWORD style    = GetWindowLongW(_hwnd, GWL_STYLE);
  DWORD ex_style = GetWindowLongW(_hwnd, GWL_EXSTYLE);
  
  RECT frame = {0,0,0,0};
  AdjustWindowRectEx(&frame, style, FALSE, ex_style);
  
  margins->cxLeftWidth    = -frame.left;
  margins->cyTopHeight    = -frame.top;
  margins->cxRightWidth   = frame.right;
  margins->cyBottomHeight = frame.bottom;
}

//{ snapping windows & alignment ...

void BasicWin32Window::snap_affinity(int value){
  if(_snap_affinity == value)
    return;
  
  _snap_affinity = value;
  all_snappers.clear();
}

  static bool snap_inside(
    const RECT &orig,
    const RECT &outer,
    const POINT *pt,
    int *dx,
    int *dy,
    int *max_dx,
    int *max_dy
  ){
    bool have_snapped_x = false;
    bool have_snapped_y = false;
    
    if((outer.bottom >= orig.top && outer.top    <= orig.bottom)
    || (outer.top    >= orig.top && outer.bottom <= orig.bottom)){
      if(pt){
        if(abs(outer.left - pt->x) < *max_dx){
          *dx = *max_dx = outer.left - pt->x;
          have_snapped_x = true;
        }
        else if(abs(outer.right - pt->x) < *max_dx){
          *dx = *max_dx = outer.right - pt->x;
          have_snapped_x = true;
        }
      }
      else if(abs(outer.left - orig.left) < *max_dx){
        *dx = *max_dx = outer.left - orig.left;
        have_snapped_x = true;
      }
      else if(abs(outer.right - orig.right) < *max_dx){
        *dx = *max_dx = outer.right - orig.right;
        have_snapped_x = true;
      }
    }
    
    if((outer.right >= orig.left && outer.left  <= orig.right)
    || (outer.left  >= orig.left && outer.right <= orig.right)){
      if(pt){
        if(abs(outer.top - pt->y) < *max_dy){
          *dy = *max_dy = outer.top - pt->y;
          have_snapped_y = true;
        }
        else if(abs(outer.bottom - pt->y) < *max_dy){
          *dy = *max_dy = outer.bottom - pt->y;
          have_snapped_y = true;
        }
      }
      else if(abs(outer.top - orig.top) < *max_dy){
        *dy = *max_dy = outer.top - orig.top;
        have_snapped_y = true;
      }
      else if(abs(outer.bottom - orig.bottom) < *max_dy){
        *dy = *max_dy = outer.bottom - orig.bottom;
        have_snapped_y = true;
      }
    }
    
    if(have_snapped_x && !have_snapped_y){
      if(abs(outer.top - orig.bottom) < *max_dy){
        *dy = *max_dy = outer.top - orig.bottom;
        have_snapped_y = true;
      }
      else if(abs(outer.bottom - orig.top) < *max_dy){
        *dy = *max_dy = outer.bottom - orig.top;
        have_snapped_y = true;
      }
    }
    else if(have_snapped_y && !have_snapped_x){
      if(abs(outer.left - orig.left) < *max_dx){
        *dx = *max_dx = outer.left - orig.left;
        have_snapped_x = true;
      }
      else if(abs(outer.right - orig.right) < *max_dx){
        *dx = *max_dx = outer.right - orig.right;
        have_snapped_x = true;
      }
    }
    
    return have_snapped_x || have_snapped_y;
  }
  
  struct snap_info_t{
    HWND src;
    Hashtable<HWND,Void,cast_hash> *dont_snap;
    
    const RECT  *orig_rect;
    const POINT *orig_pt;
    int *dx;
    int *dy;
    int *max_dx;
    int *max_dy;
  };
  
  static BOOL CALLBACK snap_monitor(
    HMONITOR hMonitor,
    HDC      hdcMonitor,
    LPRECT   lprcMonitor,
    LPARAM   dwData
  ){
    struct snap_info_t *info = (struct snap_info_t*)dwData;
    MONITORINFO moninfo;
      
    memset(&moninfo, 0, sizeof(moninfo));
    moninfo.cbSize = sizeof(moninfo);
    
    if(GetMonitorInfo(hMonitor, &moninfo)){
      snap_inside(
        *info->orig_rect, 
        moninfo.rcWork, 
        info->orig_pt,
        info->dx, info->dy, 
        info->max_dx, info->max_dy);
    }

    return TRUE;
  }

  static BOOL CALLBACK snap_hwnd(HWND hwnd, LPARAM lParam){
    struct snap_info_t *info = (struct snap_info_t*)lParam;
    
    if(hwnd != info->src 
    && IsWindowVisible(hwnd)
    && !info->dont_snap->search(hwnd)){
      RECT rect;
      GetWindowRect(hwnd, &rect);
      
      int tmp = rect.left;
      rect.left = rect.right;
      rect.right = tmp;
      
      tmp = rect.top;
      rect.top = rect.bottom;
      rect.bottom = tmp;
      
      snap_inside(
        *info->orig_rect, 
        rect, 
        info->orig_pt,
        info->dx, info->dy, 
        info->max_dx, info->max_dy);
    }
    
    return TRUE;
  }
  
void BasicWin32Window::snap_rect_or_pt(RECT *windowrect, POINT *pt){
  RECT current_rect;
  GetWindowRect(_hwnd, &current_rect);
  
  windowrect->left-=   snap_correction_x;
  windowrect->right-=  snap_correction_x;
  windowrect->top-=    snap_correction_y;
  windowrect->bottom-= snap_correction_y;
  
  if(pt){
    pt->x-= snap_correction_x;
    pt->y-= snap_correction_y;
  }
  
  int old_dx = current_rect.left - windowrect->left;
  int old_dy = current_rect.top  - windowrect->top;
  
  snap_correction_x = 0;
  snap_correction_y = 0;
  
  int max_dx = 10;
  int max_dy = 10;
  
  struct snap_info_t info;
  info.src = _hwnd;
  info.dont_snap = &all_snappers;
  info.orig_rect = windowrect;
  info.orig_pt = pt;
  info.dx = &snap_correction_x;
  info.dy = &snap_correction_y;
  info.max_dx = &max_dx;
  info.max_dy = &max_dy;
  EnumDisplayMonitors(
    NULL,
    NULL,
    snap_monitor,
    (LPARAM)&info);
  
  EnumThreadWindows(GetCurrentThreadId(), snap_hwnd, (LPARAM)&info);
  
  unsigned int i, count;
  for(i = count = 0;count < all_snappers.size();++i){
    Entry<HWND,Void> *e = all_snappers.entry(i);
    
    if(e){
      ++count;
      
      RECT rect;
      GetWindowRect(e->key, &rect);
      rect.left-=   old_dx;
      rect.right-=  old_dx;
      rect.top-=    old_dy;
      rect.bottom-= old_dy;
      
      info.orig_rect = &rect;
      
      EnumDisplayMonitors(
        NULL,
        NULL,
        snap_monitor,
        (LPARAM)&info);
      
      EnumThreadWindows(GetCurrentThreadId(), snap_hwnd, (LPARAM)&info);
    }
  }
  
  windowrect->left+=   snap_correction_x;
  windowrect->right+=  snap_correction_x;
  windowrect->top+=    snap_correction_y;
  windowrect->bottom+= snap_correction_y;
  
  if(pt){
    pt->x+= snap_correction_x;
    pt->y+= snap_correction_y;
  }
}

  struct find_snap_info_t{
    HWND dst;
    RECT dst_rect;
    int min_affinity;
    Hashtable<HWND,Void,cast_hash> *snappers;
  };
  
BOOL CALLBACK BasicWin32Window::find_snap_hwnd(HWND hwnd, LPARAM lParam){
  struct find_snap_info_t *info = (struct find_snap_info_t*)lParam;
  
  if(hwnd != info->dst && IsWindowVisible(hwnd)){
    BasicWin32Window *win = dynamic_cast<BasicWin32Window*>(
      BasicWin32Widget::from_hwnd(hwnd));
    
    if(win && win->_snap_affinity <= info->min_affinity)
      return TRUE;
    
    RECT rect;
    GetWindowRect(hwnd, &rect);
    
    if(rect.left  == info->dst_rect.right
    || rect.right == info->dst_rect.left){
      if(rect.bottom > info->dst_rect.top
      && rect.top    < info->dst_rect.bottom){
        info->snappers->set(hwnd, Void());
        return TRUE;
      }
    }
    
    if(rect.top    == info->dst_rect.bottom
    || rect.bottom == info->dst_rect.top){
      if(rect.right > info->dst_rect.left
      && rect.left  < info->dst_rect.right){
        info->snappers->set(hwnd, Void());
        return TRUE;
      }
    }
  }
  
  return TRUE;
}
  
void BasicWin32Window::find_all_snappers(){
  struct find_snap_info_t info;
  
  all_snappers.clear();
  
  info.dst = _hwnd;
  info.min_affinity = _snap_affinity;
  GetWindowRect(info.dst, &info.dst_rect);
  info.snappers = &all_snappers;
  
  all_snappers.set(_hwnd, Void());
  
  EnumThreadWindows(GetCurrentThreadId(), find_snap_hwnd, (LPARAM)&info);
  
  unsigned int found = 1;
  for(int rep = 1;rep < 5 && found < all_snappers.size();++rep){
    found = all_snappers.size();
    
    Hashtable<HWND,Void,cast_hash> more_snappers;
    
    for(unsigned i = 0, count = 0;count < all_snappers.size();++i){
      Entry<HWND,Void> *e = all_snappers.entry(i);
      
      if(e){
        ++count;
        
        info.dst = e->key;
        GetWindowRect(info.dst, &info.dst_rect);
        info.snappers = &more_snappers;
        
        EnumThreadWindows(GetCurrentThreadId(), find_snap_hwnd, (LPARAM)&info);
      }
    }
    
    all_snappers.merge(more_snappers);
  }
  
  all_snappers.remove(_hwnd);
}

void BasicWin32Window::move_all_snappers(int dx, int dy){
  if(dx != 0 || dy != 0){
    unsigned int i, count;
    for(i = count = 0;count < all_snappers.size();++i){
      Entry<HWND,Void> *e = all_snappers.entry(i);
      
      if(e){
        ++count;
        
        RECT rect;
        GetWindowRect(e->key, &rect);
        
        SetWindowPos(
          e->key,
          NULL,
          rect.left + dx,
          rect.top + dy,
          0, 0,
          SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
      }
    }
  }
}

  struct find_align_info_t{
    HWND dst;
    RECT dst_rect;
    bool align_left;
    bool align_right;
    bool align_top;
    bool align_bottom;
  };
  
  static BOOL CALLBACK find_align_hwnd(HWND hwnd, LPARAM lParam){
    struct find_align_info_t *info = (struct find_align_info_t*)lParam;
    
    if(hwnd != info->dst && IsWindowVisible(hwnd)){
      RECT rect;
      GetWindowRect(hwnd, &rect);
      
      if(info->dst_rect.left == rect.right)
        info->align_left = true;
      
      if(info->dst_rect.right == rect.left)
        info->align_right = true;
      
      if(info->dst_rect.top == rect.bottom)
        info->align_top = true;
      
      if(info->dst_rect.bottom == rect.top)
        info->align_bottom = true;
    }
    
    return TRUE;
  }

void BasicWin32Window::get_snap_alignment(bool *right, bool *bottom){
  struct find_align_info_t info;
  
  *right = false;
  *bottom = false;
  
  memset(&info, 0, sizeof(info));
  info.dst = _hwnd;
  GetWindowRect(info.dst, &info.dst_rect);
  EnumThreadWindows(GetCurrentThreadId(), find_align_hwnd, (LPARAM)&info);
  
  *right  = info.align_right  && !info.align_left;
  *bottom = info.align_bottom && !info.align_top;
  
  if(!info.align_left
  && !info.align_right
  && !info.align_top
  && !info.align_bottom){
    MONITORINFO monitor_info;
    memset(&monitor_info, 0, sizeof(monitor_info));
    monitor_info.cbSize = sizeof(monitor_info);
    
    HMONITOR hmon = MonitorFromWindow(_hwnd, MONITOR_DEFAULTTONEAREST);
    if(GetMonitorInfo(hmon, &monitor_info)){
      *right  = (info.dst_rect.left       + info.dst_rect.right) 
              > (monitor_info.rcWork.left + monitor_info.rcWork.right);
      
      *bottom = (info.dst_rect.top        + info.dst_rect.bottom) 
              > (monitor_info.rcWork.top  + monitor_info.rcWork.bottom);
    }
  }
}

void BasicWin32Window::on_sizing(WPARAM wParam, RECT *lParam){
  if(all_snappers.size() > 0)
    all_snappers.clear();
  
  Win32Themes::MARGINS margins;
  get_nc_margins(&margins);
  
  int minh = min_client_height + margins.cyTopHeight + margins.cyBottomHeight;
  int maxh = max_client_height + margins.cyTopHeight + margins.cyBottomHeight;
  int minw = min_client_width  + margins.cxLeftWidth + margins.cxRightWidth;
  int maxw = max_client_width  + margins.cxLeftWidth + margins.cxRightWidth;
  
  bool minmax = false;
  if(lParam->bottom - lParam->top < minh){
    if(wParam == WMSZ_TOP
    || wParam == WMSZ_TOPLEFT
    || wParam == WMSZ_TOPRIGHT)
      lParam->top = lParam->bottom - minh;
    else
      lParam->bottom = lParam->top + minh;
    
    minmax = true;
  }
  
  if(lParam->bottom - lParam->top > maxh && max_client_height > 0){
    if(wParam == WMSZ_TOP
    || wParam == WMSZ_TOPLEFT
    || wParam == WMSZ_TOPRIGHT)
      lParam->top = lParam->bottom - maxh;
    else
      lParam->bottom = lParam->top + maxh;
    
    minmax = true;
  }
  
  if(lParam->right - lParam->left < minw){
    if(wParam == WMSZ_LEFT
    || wParam == WMSZ_TOPLEFT
    || wParam == WMSZ_TOPRIGHT)
      lParam->left = lParam->right - minw;
    else
      lParam->right = lParam->left + minw;
    
    minmax = true;
  }
  
  if(lParam->right - lParam->left > maxw && max_client_width > 0){
    if(wParam == WMSZ_LEFT
    || wParam == WMSZ_TOPLEFT
    || wParam == WMSZ_TOPRIGHT)
      lParam->left = lParam->right - maxw;
    else
      lParam->right = lParam->left + maxw;
    
    minmax = true;
  }
  
  if(minmax)
    return;
  
  POINT pt = {0,0};
  RECT snapping_rect;
  memcpy(&snapping_rect, lParam, sizeof(snapping_rect));
  
  switch(wParam){
    case WMSZ_BOTTOM:
      pt.y = lParam->bottom;
      break;
      
    case WMSZ_BOTTOMLEFT:
      pt.x = lParam->left;
      pt.y = lParam->bottom;
      break;
      
    case WMSZ_BOTTOMRIGHT:
      pt.x = lParam->right;
      pt.y = lParam->bottom;
      break;
      
    case WMSZ_TOP:
      pt.y = lParam->top;
      break;
      
    case WMSZ_TOPLEFT:
      pt.x = lParam->left;
      pt.y = lParam->top;
      break;
      
    case WMSZ_TOPRIGHT:
      pt.x = lParam->right;
      pt.y = lParam->top;
      break;
      
    case WMSZ_LEFT:
      pt.x = lParam->left;
      break;
      
    case WMSZ_RIGHT:
      pt.x = lParam->right;
      break;
  }
  
  int old_snap_dx = snap_correction_x;
  int old_snap_dy = snap_correction_y;
  
  snap_correction_x = 0;
  snap_correction_y = 0;
  snap_rect_or_pt(&snapping_rect, &pt);
  
  switch(wParam){
    case WMSZ_BOTTOM:
      snap_correction_x = old_snap_dx;
      lParam->bottom = pt.y;
      break;
      
    case WMSZ_BOTTOMLEFT:
      lParam->left   = pt.x;
      lParam->bottom = pt.y;
      break;
      
    case WMSZ_BOTTOMRIGHT:
      lParam->right  = pt.x;
      lParam->bottom = pt.y;
      break;
      
    case WMSZ_TOP:
      snap_correction_x = old_snap_dx;
      lParam->top = pt.y;
      break;
      
    case WMSZ_TOPLEFT:
      lParam->left = pt.x;
      lParam->top  = pt.y;
      break;
      
    case WMSZ_TOPRIGHT:
      lParam->right = pt.x;
      lParam->top   = pt.y;
      break;
      
    case WMSZ_LEFT:
      snap_correction_y = old_snap_dy;
      lParam->left = pt.x;
      break;
      
    case WMSZ_RIGHT:
      snap_correction_y = old_snap_dy;
      lParam->right = pt.x;
      break;
  }
}

void BasicWin32Window::on_moving(RECT *lParam){
  RECT rect;
  GetWindowRect(_hwnd, &rect);
  
  snap_rect_or_pt(lParam, 0);
  
//  move_all_snappers(
//    rect.left - last_moving_x, 
//    rect.top  - last_moving_y);
  
  last_moving_x = rect.left;
  last_moving_y = rect.top;
}

void BasicWin32Window::on_move(LPARAM Param){
  /* When a window is moved out of screen (on the top), Windows snaps it 
     back so that the caption bar is allways on screen. 
     This can arise when the window is moved by a mouse drag in the 
     non-caption-but-glass-area (_top_glass_area or _bottom_glass_area).
     
     We have to check for this situation (differend window position in 
     WM_MOVE than specified in WM_MOVING) to also move all snapped windows. 
     Otherwise the user would have no chance to get such a snapped window back 
     to screen.
   */
  RECT rect;
  GetWindowRect(_hwnd, &rect);
  
  move_all_snappers(
    rect.left - last_moving_x, 
    rect.top  - last_moving_y);
  
  last_moving_x = rect.left;
  last_moving_y = rect.top;
}

//} ... snapping windows & alignment

void BasicWin32Window::on_theme_changed(){
  _glass_enabled = false;
  
  if(Win32Themes::IsCompositionActive
  && Win32Themes::IsCompositionActive()){
    _glass_enabled = true;
    return;
  }
  
  if(Win32Themes::IsThemeActive
  && Win32Themes::IsThemeActive()){
    _glass_enabled = Win32Themes::current_theme_is_aero();
    return;
  }
}

void BasicWin32Window::extend_glass(Win32Themes::MARGINS *margins){
  memcpy(&_extra_glass, margins, sizeof(_extra_glass));
  
  if(Win32Themes::DwmExtendFrameIntoClientArea)
    Win32Themes::DwmExtendFrameIntoClientArea(_hwnd, margins);
}

int BasicWin32Window::get_frame_color(HWND child){
  RECT rect;
  GetWindowRect(child, &rect);
  
  POINT pt = {rect.left + (rect.right  - rect.right)/2,
              rect.top  + (rect.bottom - rect.top)/2};
  
  ScreenToClient(_hwnd, &pt);
  
  return get_frame_color(pt.x, pt.y);
}

int BasicWin32Window::get_frame_color(int x, int y){
  int color = GetSysColor(COLOR_MENU);
  
  if(glass_enabled()){
    RECT glassfree;
    get_glassfree_rect(&glassfree);
    
    POINT pt = {x, y};
    if(!PtInRect(&glassfree, pt)){
      if(Win32Themes::IsCompositionActive
      && Win32Themes::IsCompositionActive())
        return -1;
      
      if(_active)
        color = GetSysColor(COLOR_GRADIENTACTIVECAPTION);
      else
        color = GetSysColor(COLOR_GRADIENTINACTIVECAPTION);
    }
  }
  
  return ((color & 0xFF0000) >> 16)
       |  (color & 0x00FF00)
       | ((color & 0x0000FF) << 16);
}

LRESULT BasicWin32Window::callback(UINT message, WPARAM wParam, LPARAM lParam){
  LRESULT dwm_result = 0;
  
  bool calldwp = true;
  
  if(Win32Themes::DwmDefWindowProc)
    calldwp = !Win32Themes::DwmDefWindowProc(_hwnd, message, wParam, lParam, &dwm_result);
  
  switch(message){
    case WM_NCACTIVATE: {
      _active = wParam;
      
      RECT outer;
      RECT inner;
      
      GetClientRect(_hwnd, &outer);
      get_glassfree_rect(&inner);
      
      HRGN rgn_o = CreateRectRgnIndirect(&outer);
      HRGN rgn_i = CreateRectRgnIndirect(&inner);
      CombineRgn(rgn_o, rgn_o, rgn_i, RGN_DIFF);
      
      InvalidateRgn(_hwnd, rgn_o, FALSE);
      
      DeleteObject(rgn_o);
      DeleteObject(rgn_i);
    } break;
  }
  
  if(!initializing()){
    switch(message){
      //{ sizing & moving ...  
      case WM_SIZING: {
        on_sizing(wParam, (RECT*)lParam);
      } break;
      
      case WM_MOVING: {
        on_moving((RECT*)lParam);
      } break;
      
      case WM_MOVE: {
        on_move(lParam);
      } break;
      
      case WM_ENTERSIZEMOVE: {
        RECT rect;
        GetWindowRect(_hwnd, &rect);
        last_moving_x = rect.left;
        last_moving_y = rect.top;
        
        find_all_snappers();
      } break;
      
      case WM_EXITSIZEMOVE: {
        all_snappers.clear();
      } break;
      //} ... sizing & moving
      
      case WM_THEMECHANGED:
      case WM_DWMCOMPOSITIONCHANGED: {
        if(Win32Themes::DwmEnableComposition)
          Win32Themes::DwmEnableComposition(1);
          
        on_theme_changed();
      } break;
      
      case WM_ERASEBKGND:
        return 1;
      
      default: break;
    }
  }
  
  if(calldwp)
    return BasicWin32Widget::callback(message, wParam, lParam);
  else
    return dwm_result;
}

//} ... class BasicWin32Window
