#define _WIN32_WINNT 0x0600

#include <math.h>
#include <stdio.h>
#include <cairo-win32.h>

#include <graphics/canvas.h>
#include <gui/win32/basic-win32-window.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/win32-widget.h>

#include <eval/client.h>

using namespace richmath;

#define CAPTION_BUTTON_INFLATE  (-2)
#define CAPTION_BUTTON_DIST     (-2)

static cairo_surface_t *background_image = 0;
int background_image_cnt = 0;

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
  _snap_affinity(false),
  _themed_frame(false),
  _mouse_over_caption_buttons(false),
  snap_correction_x(0),
  snap_correction_y(0),
  last_moving_x(0),
  last_moving_y(0)
{
  memset(&_extra_glass, 0, sizeof(_extra_glass));
  
  ++background_image_cnt;
  if(!background_image){
    Expr e = Expr(pmath_evaluate(
      pmath_parse_string(
        PMATH_C_STRING(
          "FE`$WindowFrameImage"))));
      
    String s(e);
    if(!s.is_valid())
      s = Client::application_directory + "\\frame.png";
    int len;
    char *imgname = pmath_string_to_utf8(s.get(), &len);
    
    if(imgname){
      background_image = cairo_image_surface_create_from_png(imgname);
      
      pmath_mem_free(imgname);
    }
  }
}

void BasicWin32Window::after_construction(){
  BasicWin32Widget::after_construction();
}

BasicWin32Window::~BasicWin32Window(){
  if(--background_image_cnt == 0
  && background_image){
    cairo_surface_destroy(background_image);
    background_image = 0;
  }
}

void BasicWin32Window::get_client_rect(RECT *rect){
  if(_themed_frame){
    Win32Themes::MARGINS margins;
    RECT winrect;
    
    GetWindowRect(_hwnd, &winrect);
    get_nc_margins(&margins);
    
    rect->left   = margins.cxLeftWidth;
    rect->top    = margins.cyTopHeight;
    rect->right  = winrect.right  - winrect.left - margins.cxRightWidth;//   - margins.cxLeftWidth;
    rect->bottom = winrect.bottom - winrect.top  - margins.cyBottomHeight;// - margins.cyTopHeight;
  }
  else
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
  if(!_themed_frame){
    memset(margins, 0, sizeof(Win32Themes::MARGINS));
    return;
  }
  
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
  _themed_frame = false;
  
  if(Win32Themes::IsCompositionActive
  && Win32Themes::IsCompositionActive()){
    _glass_enabled = true;
    _themed_frame = 0 == (WS_EX_TOOLWINDOW & GetWindowLong(_hwnd, GWL_EXSTYLE));
  }
  else if(Win32Themes::IsThemeActive
  && Win32Themes::IsThemeActive()){
    _glass_enabled = Win32Themes::current_theme_is_aero();
  }
  
  extend_glass(&_extra_glass);
  
  SetWindowPos(_hwnd, 0, 0, 0, 0, 0, 
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

void BasicWin32Window::paint_themed(HDC hdc){
  if(_themed_frame){
    RECT rect;
    GetClientRect(_hwnd, &rect);
    
    cairo_surface_t *surface = cairo_win32_surface_create_with_dib(
      CAIRO_FORMAT_ARGB32,
      rect.right  - rect.left, 
      rect.bottom - rect.top);
    
    cairo_t *cr = cairo_create(surface);
    {
      Canvas canvas(cr);
      
      paint_background(&canvas, 0, 0);
    }
    cairo_destroy(cr);
    
    HDC bmp_dc = cairo_win32_surface_get_dc(surface);
    paint_themed_caption(bmp_dc);
    BitBlt(
      hdc, 
      rect.left, 
      rect.top, 
      rect.right  - rect.left, 
      rect.bottom - rect.top,
      bmp_dc, 
      0, 
      0, 
      SRCCOPY);
      
    cairo_surface_destroy(surface);
  }
  else{
  }
}

  void get_system_button_bounds(HWND hwnd, RECT *rect){
    //DwmGetWindowAttribute(hwnd, DWMWA_CAPTION_BUTTON_BOUNDS, rect, sizeof(RECT));
    
    // TITLEBARINFOEX:
    struct {
      DWORD cbSize;
      RECT rcTitleBar;
      DWORD rgstate[6];
      RECT rgrect[6];
    } tbi;
    
    memset(&tbi, 0, sizeof(tbi));
    tbi.cbSize = sizeof(tbi);
    
    // WM_GETTITLEBARINFOEX = 0x033F
    SendMessageW(hwnd, 0x033F, 0, (LPARAM)&tbi);
    
    memset(rect, 0, sizeof(RECT));
    for(int i = 2;i <= 5;++i)
      UnionRect(rect, rect, &tbi.rgrect[i]);
    
    POINT *pt = (POINT*)rect;
    ScreenToClient(hwnd, &pt[0]);
    ScreenToClient(hwnd, &pt[1]);
  }

void BasicWin32Window::paint_themed_caption(HDC hdc_bitmap){
  if(!_themed_frame
  || !Win32Themes::OpenThemeData
  || !Win32Themes::CloseThemeData
  || !Win32Themes::GetThemeSysColor
  || !Win32Themes::GetThemeSysFont
  || !Win32Themes::DrawThemeTextEx)
    return;

  HANDLE theme = Win32Themes::OpenThemeData(NULL, L"CompositedWindow::Window");
  if(theme){
    Win32Themes::DTTOPTS dtt_opts;
    memset(&dtt_opts, 0, sizeof(dtt_opts));
    dtt_opts.crText = Win32Themes::GetThemeSysColor(theme, _active ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT);
    dtt_opts.dwSize = sizeof(dtt_opts);
    dtt_opts.dwFlags = DTT_COMPOSITED | DTT_GLOWSIZE | DTT_TEXTCOLOR;
    dtt_opts.iGlowSize = 10;
    
    #define MAX_STR_LEN 1024
    WCHAR str[MAX_STR_LEN];
    GetWindowTextW(_hwnd, str, MAX_STR_LEN);
    str[MAX_STR_LEN-1] = '\0';
    
    LOGFONTW log_font;
    HFONT old_font = NULL;
    if(SUCCEEDED(Win32Themes::GetThemeSysFont(theme, 801, &log_font))){
    // TMT_CAPTIONFONT = 801
      HFONT font = CreateFontIndirectW(&log_font);
      old_font = (HFONT)SelectObject(hdc_bitmap, font);
    }
    
    int frame_x = GetSystemMetrics(SM_CXSIZEFRAME);
    int frame_y = GetSystemMetrics(SM_CYSIZEFRAME);
    
    RECT rect;
    get_system_button_bounds(_hwnd, &rect);
    rect.right  = rect.left;
    rect.left   = frame_x + GetSystemMetrics(SM_CXSMICON) + 4;
    rect.top    = frame_y;
    rect.bottom = rect.top + GetSystemMetrics(SM_CYCAPTION);
    
    Win32Themes::DrawThemeTextEx(
      theme,
      hdc_bitmap, 
      0, 0, 
      str, -1, 
      DT_LEFT | DT_END_ELLIPSIS, 
      &rect, 
      &dtt_opts);
    
    if(GetClassNameW(_hwnd, str, MAX_STR_LEN)){
      WNDCLASSEXW wndcl;
      memset(&wndcl, 0, sizeof(wndcl));
      
      GetClassInfoExW(GetModuleHandle(0), str, &wndcl);
      HICON icon = wndcl.hIconSm;
      if(!icon) icon = wndcl.hIcon;
      if(icon){
        DrawIconEx(hdc_bitmap, frame_x, frame_y, icon,
          GetSystemMetrics(SM_CXSMICON),
          GetSystemMetrics(SM_CYSMICON),
          0,
          NULL,
          DI_NORMAL);
      }
    }
    
    if(old_font){
      SelectObject(hdc_bitmap, old_font);
    }
    Win32Themes::CloseThemeData(theme);
  }
}

void BasicWin32Window::extend_glass(Win32Themes::MARGINS *margins){
  if(margins != &_extra_glass)
    memcpy(&_extra_glass, margins, sizeof(_extra_glass));
  
  if(Win32Themes::DwmExtendFrameIntoClientArea){
    Win32Themes::MARGINS nc;
    get_nc_margins(&nc);
    
    nc.cxLeftWidth+=    margins->cxLeftWidth;
    nc.cxRightWidth+=   margins->cxRightWidth;
    nc.cyTopHeight+=    margins->cyTopHeight;
    nc.cyBottomHeight+= margins->cyBottomHeight;
    
    Win32Themes::DwmExtendFrameIntoClientArea(_hwnd, &nc);
  
    RECT client;
    GetWindowRect(_hwnd, &client);

    // Inform application of the frame change.
    SetWindowPos(
      _hwnd, 
      NULL, 
      client.left, 
      client.top,
      client.right  - client.left, 
      client.bottom - client.top,
      SWP_FRAMECHANGED);
  }
}

void BasicWin32Window::paint_background(Canvas *canvas, HWND child){
  RECT rect, child_rect;
  
  GetWindowRect(child, &child_rect);
  GetWindowRect(_hwnd, &rect);
  
  paint_background(
    canvas, 
    child_rect.left - rect.left, 
    child_rect.top  - rect.top);
}

void BasicWin32Window::paint_background(Canvas *canvas, int x, int y){
  canvas->save();
  {
    cairo_matrix_t mat;
    cairo_matrix_init_identity(&mat);
    cairo_set_matrix(canvas->cairo(), &mat);
    
    cairo_reset_clip(canvas->cairo());
    
    RECT rect;
    GetWindowRect(_hwnd, &rect);
    ScreenToClient(_hwnd, (POINT*)&rect);
    
    canvas->translate(-x, -y);
    
    RECT glassfree;
    get_glassfree_rect(&glassfree);
    glassfree.left-=   rect.left;
    glassfree.right-=  rect.left;
    glassfree.top-=    rect.top;
    glassfree.bottom-= rect.top;
    
    if(!Win32Themes::IsCompositionActive
    || !Win32Themes::IsCompositionActive()){
      if(glass_enabled()){
        int color;
        
        if(_active)
          color = GetSysColor(COLOR_GRADIENTACTIVECAPTION);
        else
          color = GetSysColor(COLOR_GRADIENTINACTIVECAPTION);
          
        color = ((color & 0xFF0000) >> 16)
              |  (color & 0x00FF00)
              | ((color & 0x0000FF) << 16);
            
        canvas->set_color(color);
        canvas->paint();
      }
      else{
        int color = GetSysColor(COLOR_MENU);
        
        color = ((color & 0xFF0000) >> 16)
              |  (color & 0x00FF00)
              | ((color & 0x0000FF) << 16);
              
        canvas->set_color(color);
        canvas->paint();
      }
    }
    else{
      canvas->set_color(0x000000, 0);
      canvas->paint();
    }
    
    if(!IsRectEmpty(&glassfree)){
      int color = GetSysColor(COLOR_MENU);
      
      color = ((color & 0xFF0000) >> 16)
            |  (color & 0x00FF00)
            | ((color & 0x0000FF) << 16);
            
      canvas->move_to(glassfree.left,  glassfree.top);
      canvas->line_to(glassfree.left,  glassfree.bottom);
      canvas->line_to(glassfree.right, glassfree.bottom);
      canvas->line_to(glassfree.right, glassfree.top);
      canvas->close_path();
      
      canvas->set_color(color);
      canvas->fill();
    }
    
    on_paint_background(canvas);
    
    if(_themed_frame){
      LONG style_ex = GetWindowLong(_hwnd, GWL_EXSTYLE);
      RECT rect, glassfree;
      GetClientRect(_hwnd, &rect);
      get_glassfree_rect(&glassfree);
      
      cairo_reset_clip(canvas->cairo());
      
      { // ensure maximum alpha of 0.8 on the glass area border
        cairo_set_operator(canvas->cairo(), CAIRO_OPERATOR_DEST_OUT);
        
        canvas->move_to(rect.left,  rect.top);
        canvas->line_to(rect.right, rect.top);
        canvas->line_to(rect.right, rect.bottom);
        canvas->line_to(rect.left,  rect.bottom);
        canvas->close_path();
        
        canvas->move_to(glassfree.left,  glassfree.top);
        canvas->line_to(glassfree.left,  glassfree.bottom);
        canvas->line_to(glassfree.right, glassfree.bottom);
        canvas->line_to(glassfree.right, glassfree.top);
        canvas->close_path();
        
        canvas->clip();
        //canvas->paint_with_alpha(1 - 0.8);
        
        cairo_set_line_width(canvas->cairo(), 3);
        
        canvas->move_to(rect.left,  rect.top);
        canvas->line_to(rect.right, rect.top);
        canvas->line_to(rect.right, rect.bottom);
        canvas->line_to(rect.left,  rect.bottom);
        canvas->close_path();
        
        canvas->move_to(glassfree.left,  glassfree.top);
        canvas->line_to(glassfree.left,  glassfree.bottom);
        canvas->line_to(glassfree.right, glassfree.bottom);
        canvas->line_to(glassfree.right, glassfree.top);
        canvas->close_path();
        
        canvas->set_color(0xffffff, 1 - 0.1);
        canvas->stroke();
        
        cairo_reset_clip(canvas->cairo());
      }
      
      { // small alpha value above the min/max/close buttons
        RECT buttons;
        get_system_button_bounds(_hwnd, &buttons);
        
        if(style_ex & WS_EX_TOOLWINDOW){
          canvas->move_to(buttons.left,  buttons.top);
          canvas->line_to(buttons.right, buttons.top);
          canvas->line_to(buttons.right, buttons.bottom);
          canvas->line_to(buttons.left,  buttons.bottom);
          canvas->close_path();
        }
        else{
          int buttonradius = 5;
          canvas->move_to(     buttons.left+0.5,   buttons.top);
          canvas->arc(buttons.left+0.5  + buttonradius, buttons.bottom-0.5 - buttonradius, buttonradius, M_PI,   M_PI/2, true);
          canvas->arc(buttons.right-0.5 - buttonradius, buttons.bottom-0.5 - buttonradius, buttonradius, M_PI/2, 0,      true);
          canvas->line_to(     buttons.right-0.5,  buttons.top);
          canvas->close_path();
        }
        
        if(_mouse_over_caption_buttons){
          cairo_set_operator(canvas->cairo(), CAIRO_OPERATOR_CLEAR);
        }
        else if(_active){
          canvas->set_color(0xffffff, 1 - 0.2);
          cairo_set_operator(canvas->cairo(), CAIRO_OPERATOR_DEST_OUT);
        }
        else{
          canvas->set_color(0xffffff, 1 - 0.6);
          cairo_set_operator(canvas->cairo(), CAIRO_OPERATOR_DEST_OUT);
        }
        
        canvas->fill();
      }
      
      { // make the edges round again
        int frameradius;
        
        if(style_ex & WS_EX_TOOLWINDOW)
          frameradius = 1;
        else
          frameradius = GetSystemMetrics(SM_CXFRAME)-2;
        
        canvas->move_to(rect.left,  rect.top);
        canvas->line_to(rect.left,  rect.bottom);
        canvas->line_to(rect.right, rect.bottom);
        canvas->line_to(rect.right, rect.top);
        canvas->close_path();
        
        {
          InflateRect(&rect, -1, -1);
          canvas->move_to(rect.left, rect.top + frameradius);
          canvas->arc(rect.left  + frameradius, rect.top    + frameradius, frameradius, M_PI,     3*M_PI/2, false);
          canvas->arc(rect.right - frameradius, rect.top    + frameradius, frameradius, 3*M_PI/2, 2*M_PI,   false);
          canvas->arc(rect.right - frameradius, rect.bottom - frameradius, frameradius, 0,          M_PI/2, false);
          canvas->arc(rect.left  + frameradius, rect.bottom - frameradius, frameradius, M_PI/2,     M_PI,   false);
          canvas->close_path();
          
          InflateRect(&rect, 1, 1);
        }
        
        cairo_set_operator(canvas->cairo(), CAIRO_OPERATOR_CLEAR);
        canvas->fill();
      }
    }
  }
  canvas->restore();
}

void BasicWin32Window::on_paint_background(Canvas *canvas){
  if(!_themed_frame)
    return;

  if(background_image 
  && cairo_surface_status(background_image) == CAIRO_STATUS_SUCCESS){
    RECT rect, glassfree;
    GetClientRect(_hwnd, &rect);
    get_glassfree_rect(&glassfree);
    
    int x = rect.right - cairo_image_surface_get_width(background_image);
    cairo_set_source_surface(canvas->cairo(), background_image, x, 0);
    //canvas->set_color(0xff0000);
    
    canvas->move_to(rect.left,  rect.top);
    canvas->line_to(rect.right, rect.top);
    canvas->line_to(rect.right, rect.bottom);
    canvas->line_to(rect.left,  rect.bottom);
    canvas->close_path();
    
    canvas->move_to(glassfree.left,  glassfree.top);
    canvas->line_to(glassfree.left,  glassfree.bottom);
    canvas->line_to(glassfree.right, glassfree.bottom);
    canvas->line_to(glassfree.right, glassfree.top);
    canvas->close_path();
    
    canvas->clip();
    canvas->paint_with_alpha(0.8);
    
    cairo_reset_clip(canvas->cairo());
    canvas->move_to(glassfree.left,  glassfree.top);
    canvas->line_to(glassfree.left,  glassfree.bottom);
    canvas->line_to(glassfree.right, glassfree.bottom);
    canvas->line_to(glassfree.right, glassfree.top);
    canvas->close_path();
    canvas->clip();
    
    canvas->paint_with_alpha(0.4);
  }
}

LRESULT BasicWin32Window::nc_hit_test(WPARAM wParam, LPARAM lParam){
  POINT ptMouse = { (short)LOWORD(lParam), (short)HIWORD(lParam)};
  
  Win32Themes::MARGINS margins;
  get_nc_margins(&margins);
  margins.cxLeftWidth   += _extra_glass.cxLeftWidth;
  margins.cxRightWidth  += _extra_glass.cxRightWidth;
  margins.cyTopHeight   += _extra_glass.cyTopHeight;
  margins.cyBottomHeight+= _extra_glass.cyBottomHeight;
  
  RECT rcWindow;
  GetWindowRect(_hwnd, &rcWindow);

  RECT rcFrame = { 0,0,0,0 };
  AdjustWindowRectEx(&rcFrame, WS_OVERLAPPEDWINDOW & ~WS_CAPTION, FALSE, NULL);

  USHORT uRow = 1;
  USHORT uCol = 1;
  bool fOnResizeBorder = false;
  
  if(ptMouse.y >= rcWindow.top 
  && ptMouse.y < rcWindow.top + margins.cyTopHeight){
    fOnResizeBorder = (ptMouse.y < (rcWindow.top - rcFrame.top));
    uRow = 0;
  }
  else if(ptMouse.y < rcWindow.bottom 
  && ptMouse.y >= rcWindow.bottom - margins.cyBottomHeight){
    uRow = 2;
  }

  if(ptMouse.x >= rcWindow.left 
  && ptMouse.x < rcWindow.left + margins.cxLeftWidth){
    uCol = 0;
  }
  else if(ptMouse.x < rcWindow.right 
  && ptMouse.x >= rcWindow.right - margins.cxRightWidth){
    uCol = 2;
  }
  
  if(!fOnResizeBorder 
  && ptMouse.x <= rcWindow.left + GetSystemMetrics(SM_CXSMICON) - rcFrame.left
  && ptMouse.y <= rcWindow.top  + GetSystemMetrics(SM_CYSMICON) - rcFrame.top){
    return HTSYSMENU;
  }
  
  LRESULT hitTests[3][3] = {
    { fOnResizeBorder ? HTTOPLEFT : HTLEFT, fOnResizeBorder ? HTTOP : HTCAPTION, fOnResizeBorder ? HTTOPRIGHT : HTRIGHT },
    { HTLEFT,                               HTCLIENT,                            HTRIGHT       },
    { HTBOTTOMLEFT,                         HTBOTTOM,                            HTBOTTOMRIGHT },
  };

  return hitTests[uRow][uCol];
}

  struct remove_child_rgn_info_t{
    HWND parent;
    HRGN region;
  };

  static BOOL CALLBACK remove_child_rgn_callback(HWND hwnd, LPARAM lParam){
    struct remove_child_rgn_info_t *info = (struct remove_child_rgn_info_t*)lParam;
    
    RECT rect;
    GetWindowRect(hwnd, &rect);
    MapWindowPoints(NULL, info->parent, (POINT*)&rect, 2);
    
    HRGN rectrgn = CreateRectRgnIndirect(&rect);
    CombineRgn(info->region, info->region, rectrgn, RGN_DIFF);
    DeleteObject(rectrgn);
    
    return TRUE;
  }
  
void BasicWin32Window::invalidate_non_child(){
  if(!_themed_frame)
    return;
    
  struct remove_child_rgn_info_t info;
  
  RECT rect;
  GetClientRect(_hwnd, &rect);
  
  info.parent = _hwnd;
  info.region = CreateRectRgnIndirect(&rect);
  
  EnumChildWindows(_hwnd, remove_child_rgn_callback, (LPARAM)&info);
  
  InvalidateRgn(_hwnd, info.region, FALSE);
  
  DeleteObject(info.region);
}

void BasicWin32Window::invalidate_caption(){
  if(_themed_frame){
    RECT rect;
    Win32Themes::MARGINS mar;
    GetClientRect(_hwnd, &rect);
    get_nc_margins(&mar);
    
    rect.bottom = mar.cyTopHeight;
    InvalidateRect(_hwnd, &rect, FALSE);
  }
}

  struct redraw_glass_info_t{
    RECT inner;
  };

  static BOOL CALLBACK redraw_glass_callback(HWND hwnd, LPARAM lParam){
    struct redraw_glass_info_t *info = (struct redraw_glass_info_t*)lParam;
    
    Win32Widget *wid = dynamic_cast<Win32Widget*>(BasicWin32Widget::from_hwnd(hwnd));
    if(wid){
      RECT rect, rect2;
      GetWindowRect(hwnd, &rect);
      UnionRect(&rect2, &rect, &info->inner);
      
      if(!EqualRect(&info->inner, &rect2)){
        wid->force_redraw();
      }
    }
    
    return TRUE;
  }

LRESULT BasicWin32Window::callback(UINT message, WPARAM wParam, LPARAM lParam){
  LRESULT dwm_result = 0;
  
  bool calldwp = true;
  
  if(Win32Themes::DwmDefWindowProc)
    calldwp = !Win32Themes::DwmDefWindowProc(_hwnd, message, wParam, lParam, &dwm_result);
  
  switch(message){
    case WM_NCACTIVATE: {
      if(wParam){
        struct redraw_glass_info_t info;
        
        get_glassfree_rect(&info.inner);
        POINT *pt = (POINT*)&info.inner;
        ClientToScreen(_hwnd, &pt[0]);
        ClientToScreen(_hwnd, &pt[1]);
        
        EnumChildWindows(_hwnd, redraw_glass_callback, (LPARAM)&info);
      }
      
      _active = wParam;
      if(_themed_frame){
        HDC hdc = GetDC(_hwnd);
        
        RECT rect;
        GetClientRect(_hwnd, &rect);
        IntersectClipRect(hdc, 0, 0, 
          rect.right, 
          GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION));
        paint_themed(hdc);
        
        ReleaseDC(_hwnd, hdc);
      }
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
      
      case WM_SIZE: {
        invalidate_non_child();
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
        
      case WM_PAINT: {
        if(_themed_frame){
          RECT client;
          GetClientRect(_hwnd, &client);
          
          PAINTSTRUCT ps;
          HDC hdc = BeginPaint(_hwnd, &ps);
          paint_themed(hdc);
          EndPaint(_hwnd, &ps);
          return 0;
        }
      } break;
      
      case WM_NCHITTEST: {
        if(!calldwp)
          return dwm_result;
        
        if(_themed_frame)
          return nc_hit_test(wParam, lParam);
      } break;
      
      case WM_NCCALCSIZE: {
        if(wParam && _themed_frame)
          return 0;//WVR_ALIGNTOP | WVR_ALIGNRIGHT;
      } break;
      
      case WM_NCMOUSEMOVE: {
        if(wParam == HTMINBUTTON 
        || wParam == HTMAXBUTTON 
        || wParam == HTCLOSE
        || wParam == HTHELP){
          if(!_mouse_over_caption_buttons)
            invalidate_caption();
          _mouse_over_caption_buttons = true;
        }
        else{
          if(_mouse_over_caption_buttons)
            invalidate_caption();
          _mouse_over_caption_buttons = false;
        }
      } break;
      
      case WM_NCMOUSELEAVE: {
        if(_mouse_over_caption_buttons)
          invalidate_caption();
          
        _mouse_over_caption_buttons = false;
      } break;
      
      case WM_NCRBUTTONUP: {
        if(_themed_frame
        && (wParam == HTCAPTION || wParam == HTSYSMENU)){
          HMENU menu = GetSystemMenu(_hwnd, FALSE);
          
          if(menu){
            POINT pt;
            pt.x = (short)LOWORD(lParam);
            pt.y = (short)HIWORD(lParam);
            
            int cmd = TrackPopupMenu(
              menu,
              GetSystemMetrics(SM_MENUDROPALIGNMENT) | TPM_RETURNCMD,
              pt.x,
              pt.y,
              0,
              _hwnd,
              NULL);
              
            if(cmd)
              SendMessageW(_hwnd, WM_SYSCOMMAND, cmd, 0);
            return 0;
          }
        }
      } break;
      
      case WM_NCLBUTTONUP: {
        if(_themed_frame
        && wParam == HTSYSMENU){
          HMENU menu = GetSystemMenu(_hwnd, FALSE);
          
          if(menu){
            TPMPARAMS tpm;
            memset(&tpm, 0, sizeof(tpm));
            tpm.cbSize = sizeof(tpm);
            GetWindowRect(_hwnd, &tpm.rcExclude);
            
            tpm.rcExclude.left+=  GetSystemMetrics(SM_CXSIZEFRAME);
            tpm.rcExclude.top+=   GetSystemMetrics(SM_CYSIZEFRAME);
            tpm.rcExclude.right-= GetSystemMetrics(SM_CXSIZEFRAME);
            tpm.rcExclude.bottom = tpm.rcExclude.top  + GetSystemMetrics(SM_CYCAPTION);
            
            POINT pt;
            pt.x = (short)LOWORD(lParam);
            pt.y = (short)HIWORD(lParam);
            
            int x;
            UINT align;
            if(GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0){
              align = TPM_LEFTALIGN;
              x = tpm.rcExclude.left;
            }
            else{
              align = TPM_RIGHTALIGN;
              x = tpm.rcExclude.right;
            }
            
            int cmd = TrackPopupMenuEx(
              menu,
              GetSystemMetrics(SM_MENUDROPALIGNMENT) | TPM_RETURNCMD | TPM_NONOTIFY,
              x,
              tpm.rcExclude.bottom,
              _hwnd,
              &tpm);
              
            if(cmd)
              SendMessageW(_hwnd, WM_SYSCOMMAND, cmd, 0);
            return 0;
          }
        }
      } break;
      
      default: break;
    }
  }
  
  if(calldwp)
    return BasicWin32Widget::callback(message, wParam, lParam);
  else
    return dwm_result;
}

//} ... class BasicWin32Window
