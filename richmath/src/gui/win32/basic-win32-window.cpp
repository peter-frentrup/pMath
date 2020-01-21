#include <cmath>
#include <cstdio>
#include <cairo-win32.h>

#include <graphics/canvas.h>
#include <gui/win32/basic-win32-window.h>
#include <gui/win32/win32-automenuhook.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/win32-highdpi.h>
#include <gui/win32/win32-tooltip-window.h>
#include <gui/win32/win32-widget.h>

#include <eval/application.h>
#include <resources.h>


using namespace richmath;

using SnapPosition = BasicWin32Window::SnapPosition;

#define CAPTION_BUTTON_INFLATE  (-2)
#define CAPTION_BUTTON_DIST     (-2)

#ifndef WM_DWMCOMPOSITIONCHANGED
#  define WM_DWMCOMPOSITIONCHANGED   0x031E
#endif

#ifndef GW_ENABLEDPOPUP
#  define GW_ENABLEDPOPUP  6
#endif


#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) > (b) ? (a) : (b))


static Hashtable<String, AutoCairoSurface> background_image_cache;

static Hashtable<int, HANDLE> composition_window_theme_for_dpi;

static AutoCairoSurface get_background_image() {
  Expr expr = Evaluate(Parse("FE`$WindowFrameImage"));

  String key(expr);
  if(key.is_null())
    key = Application::application_directory + "\\frame.png";

  if(auto result = background_image_cache.search(key))
    return *result;

  int len;
  if(char *imgname = pmath_string_to_utf8(key.get(), &len)) {
    AutoCairoSurface img(cairo_image_surface_create_from_png(imgname));

    pmath_mem_free(imgname);

    background_image_cache.set(key, img);
    return img;
  }

  return AutoCairoSurface();
}

static int _basic_window_count = 0;
static void add_basic_window() {
  ++_basic_window_count;
}

static void remove_basic_window() {
  if(--_basic_window_count != 0)
    return;

  Win32TooltipWindow::delete_global_tooltip();

  background_image_cache.clear();
  
  if(Win32Themes::CloseThemeData) {
    for(auto &e : composition_window_theme_for_dpi.entries())
      Win32Themes::CloseThemeData(e.value);
    
    composition_window_theme_for_dpi.clear();
  }
}

static bool is_left_right_bottom_frame_themed(BasicWin32Window *win) {
  /* On Windows 10 we let the OS handle left/right/bottom frame, which means those
     are completely transparent.
     If we announced in WM_NCCALCSIZE that our painting area is also in these regions,
     Win10 would not make the frame transparent, but completely opaque.
     
     On Vista/7/8/8.1 there is a visible thick frame around each window.
     We would like to put our theme image there, too (return true from here).
     But BitBlt in paint_themed() occasionally fails (?) to transfer the alpha channel,
     resulting in a black frame. (or the alpha channel is ignored...?)
     (this bug occurs on Windows 7, with two monitors sometimes when the window is enlarged
     or maximized. With the `if(rect.right - rect.left > 600)` code below it seems to happen
     always when one slowly enlarges the width above 600).
     
     To workaround this bug, we never draw into left/right/bottom frame, which is not very 
     nice, but ok :(
   */
  return false;
//  if(Win32Themes::check_osversion(10, 0))
//    return false;
//  
////  DWORD style = GetWindowLongW(win->hwnd(), GWL_STYLE);
////  if(style & WS_MAXIMIZE)
////    return false;
//  
////  RECT rect = {0};
////  GetWindowRect(win->hwnd(), &rect);
////  if(rect.right - rect.left > 600)
////    return false;
//  
//  return true;
}

static void get_nc_margins(HWND hwnd, Win32Themes::MARGINS *margins, int dpi) {
  DWORD style    = GetWindowLongW(hwnd, GWL_STYLE);
  DWORD ex_style = GetWindowLongW(hwnd, GWL_EXSTYLE);

  RECT frame = {0, 0, 0, 0};
  Win32HighDpi::adjust_window_rect(&frame, style, FALSE, ex_style, dpi);

  margins->cxLeftWidth    = -frame.left;
  margins->cyTopHeight    = -frame.top;
  margins->cxRightWidth   = frame.right;
  margins->cyBottomHeight = frame.bottom;
}

//{ class BasicWin32Window ...

bool BasicWin32Window::during_pos_changing = false;

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
    nullptr),
  min_client_height(0),
  max_client_height(-1),
  min_client_width(0),
  max_client_width(-1),
  _zorder_level(0),
  background_image(get_background_image()),
  _active(false),
  _glass_enabled(false),
  _themed_frame(false),
  _mouse_over_caption_buttons(false),
  snap_correction_x(0),
  snap_correction_y(0),
  last_moving_cx(0),
  last_moving_cy(0)
{
  memset(&_extra_glass, 0, sizeof(_extra_glass));

  add_basic_window();
}

void BasicWin32Window::after_construction() {
  BasicWin32Widget::after_construction();
}

BasicWin32Window::~BasicWin32Window() {
  remove_basic_window();
}

void BasicWin32Window::get_client_rect(RECT *rect) {
  if(_themed_frame) {
    Win32Themes::MARGINS margins;
    RECT winrect;
    
    GetWindowRect(_hwnd, &winrect);
    MapWindowPoints(nullptr, _hwnd, (POINT*)&winrect, 2);
    
    get_nc_margins(&margins);

    /* If WM_NCCALCSIZE was not yet sent by Windows, GetWindowRect assumes the default,
       i.e. default window frame. But our calculations here should be based on our
       window frame calculations for WM_NCCALCSIZE, even if that was not called yet.
     */
    rect->left   = margins.cxLeftWidth;
    rect->top    = margins.cyTopHeight;
    
    if(!is_left_right_bottom_frame_themed(this)) {
      rect->left+= winrect.left;
    }
    
    rect->right  = rect->left + winrect.right  - winrect.left - margins.cxRightWidth   - margins.cxLeftWidth;
    rect->bottom = rect->top  + winrect.bottom - winrect.top  - margins.cyBottomHeight - margins.cyTopHeight;
  }
  else
    GetClientRect(_hwnd, rect);
}

void BasicWin32Window::get_client_size(int *width, int *height) {
  RECT rect;
  get_client_rect(&rect);

  *width  = rect.right  - rect.left;
  *height = rect.bottom - rect.top;
}

void BasicWin32Window::get_glassfree_rect(RECT *rect) {
  if( _extra_glass.cxLeftWidth    == -1 &&
      _extra_glass.cxRightWidth   == -1 &&
      _extra_glass.cyTopHeight    == -1 &&
      _extra_glass.cyBottomHeight == -1)
  {
    SetRectEmpty(rect);
  }
  else {
    get_client_rect(rect);
    rect->left   += _extra_glass.cxLeftWidth;
    rect->top    += _extra_glass.cyTopHeight;
    rect->right  -= _extra_glass.cxRightWidth;
    rect->bottom -= _extra_glass.cyBottomHeight;
  }
}

void BasicWin32Window::get_nc_margins(Win32Themes::MARGINS *margins) {
  ::get_nc_margins(_hwnd, margins, Win32HighDpi::get_dpi_for_window(_hwnd));
}

//{ snapping windows & alignment ...

static void get_snap_margins(HWND hwnd, Win32Themes::MARGINS *margins) {
  memset(margins, 0, sizeof(Win32Themes::MARGINS));
  
  if(Win32Themes::check_osversion(10, 0)) {
    ::get_nc_margins(hwnd, margins, Win32HighDpi::get_dpi_for_window(hwnd));
    margins->cyTopHeight    = 0;
    margins->cxLeftWidth    = 1 - margins->cxLeftWidth;
    margins->cxRightWidth   = 1 - margins->cxRightWidth;
    margins->cyBottomHeight = 1 - margins->cyBottomHeight;
  }
}

static void adjust_snap_rect(HWND hwnd, RECT *rect) {
  Win32Themes::MARGINS margins;
  get_snap_margins(hwnd, &margins);
  rect->left-= margins.cxLeftWidth;
  rect->top-= margins.cyTopHeight;
  rect->right+= margins.cxLeftWidth;
  rect->bottom+= margins.cyBottomHeight;
}

static void get_snap_rect(HWND hwnd, RECT *rect) {
  GetWindowRect(hwnd, rect);
  adjust_snap_rect(hwnd, rect);
}

static bool snap_inside(
  const RECT &orig,
  const RECT &outer,
  const POINT *pt,
  int *dx,
  int *dy,
  int *max_dx,
  int *max_dy
) {
  bool have_snapped_x = false;
  bool have_snapped_y = false;

  if( (outer.bottom >= orig.top && outer.top    <= orig.bottom) ||
      (outer.top    >= orig.top && outer.bottom <= orig.bottom))
  {
    if(pt) {
      if(abs(outer.left - pt->x) < *max_dx) {
        *dx = *max_dx = outer.left - pt->x;
        have_snapped_x = true;
      }
      else if(abs(outer.right - pt->x) < *max_dx) {
        *dx = *max_dx = outer.right - pt->x;
        have_snapped_x = true;
      }
    }
    else if(abs(outer.left - orig.left) < *max_dx) {
      *dx = *max_dx = outer.left - orig.left;
      have_snapped_x = true;
    }
    else if(abs(outer.right - orig.right) < *max_dx) {
      *dx = *max_dx = outer.right - orig.right;
      have_snapped_x = true;
    }
  }

  if( (outer.right >= orig.left && outer.left  <= orig.right) ||
      (outer.left  >= orig.left && outer.right <= orig.right))
  {
    if(pt) {
      if(abs(outer.top - pt->y) < *max_dy) {
        *dy = *max_dy = outer.top - pt->y;
        have_snapped_y = true;
      }
      else if(abs(outer.bottom - pt->y) < *max_dy) {
        *dy = *max_dy = outer.bottom - pt->y;
        have_snapped_y = true;
      }
    }
    else if(abs(outer.top - orig.top) < *max_dy) {
      *dy = *max_dy = outer.top - orig.top;
      have_snapped_y = true;
    }
    else if(abs(outer.bottom - orig.bottom) < *max_dy) {
      *dy = *max_dy = outer.bottom - orig.bottom;
      have_snapped_y = true;
    }
  }

  if(have_snapped_x && !have_snapped_y) {
    if(abs(outer.top - orig.bottom) < *max_dy) {
      *dy = *max_dy = outer.top - orig.bottom;
      have_snapped_y = true;
    }
    else if(abs(outer.bottom - orig.top) < *max_dy) {
      *dy = *max_dy = outer.bottom - orig.top;
      have_snapped_y = true;
    }
  }
  else if(have_snapped_y && !have_snapped_x) {
    if(abs(outer.left - orig.left) < *max_dx) {
      *dx = *max_dx = outer.left - orig.left;
      have_snapped_x = true;
    }
    else if(abs(outer.right - orig.right) < *max_dx) {
      *dx = *max_dx = outer.right - orig.right;
      have_snapped_x = true;
    }
  }

  return have_snapped_x || have_snapped_y;
}

struct snap_info_t {
  HWND src;
  Hashset<HWND> *dont_snap;

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
) {
  struct snap_info_t *info = (struct snap_info_t *)dwData;
  MONITORINFO moninfo;

  memset(&moninfo, 0, sizeof(moninfo));
  moninfo.cbSize = sizeof(moninfo);

  if(GetMonitorInfo(hMonitor, &moninfo)) {
    snap_inside(
      *info->orig_rect,
      moninfo.rcWork,
      info->orig_pt,
      info->dx, info->dy,
      info->max_dx, info->max_dy);
  }

  return TRUE;
}

static BOOL CALLBACK snap_hwnd(HWND hwnd, LPARAM lParam) {
  struct snap_info_t *info = (struct snap_info_t *)lParam;

  if(hwnd != info->src && IsWindowVisible(hwnd) && !info->dont_snap->contains(hwnd)) {
    RECT rect;
    get_snap_rect(hwnd, &rect);
    
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

void BasicWin32Window::snap_rect_or_pt(RECT *snapping_rect, POINT *pt) {
  RECT current_rect;
  get_snap_rect(_hwnd, &current_rect);

  snapping_rect->left -=   snap_correction_x;
  snapping_rect->right -=  snap_correction_x;
  snapping_rect->top -=    snap_correction_y;
  snapping_rect->bottom -= snap_correction_y;

  if(pt) {
    pt->x -= snap_correction_x;
    pt->y -= snap_correction_y;
  }

  int old_dx = current_rect.left - snapping_rect->left;
  int old_dy = current_rect.top  - snapping_rect->top;

  snap_correction_x = 0;
  snap_correction_y = 0;

  int max_dx = 10;
  int max_dy = 10;
  
  struct snap_info_t info;
  info.src = _hwnd;
  info.dont_snap = &all_snappers;
  info.orig_rect = snapping_rect;
  info.orig_pt = pt;
  info.dx = &snap_correction_x;
  info.dy = &snap_correction_y;
  info.max_dx = &max_dx;
  info.max_dy = &max_dy;
  EnumDisplayMonitors(
    nullptr,
    nullptr,
    snap_monitor,
    (LPARAM)&info);

  EnumThreadWindows(GetCurrentThreadId(), snap_hwnd, (LPARAM)&info);
  
  for(auto hwnd : all_snappers.keys()) {
    RECT rect;
    get_snap_rect(hwnd, &rect);
    rect.left -=   old_dx;
    rect.right -=  old_dx;
    rect.top -=    old_dy;
    rect.bottom -= old_dy;
    
    info.orig_rect = &rect;

    EnumDisplayMonitors(
      nullptr,
      nullptr,
      snap_monitor,
      (LPARAM)&info);

    EnumThreadWindows(GetCurrentThreadId(), snap_hwnd, (LPARAM)&info);
  }
  
  snapping_rect->left +=   snap_correction_x;
  snapping_rect->right +=  snap_correction_x;
  snapping_rect->top +=    snap_correction_y;
  snapping_rect->bottom += snap_correction_y;

  if(pt) {
    pt->x += snap_correction_x;
    pt->y += snap_correction_y;
  }
}

struct find_snap_info_t {
  HWND dst;
  RECT dst_rect;
  int min_level;
  Hashset<HWND> *snappers;
  Array<SnapPosition> *snapper_positions;
};

static bool touchingRectangles(RECT *touchRegion, const RECT &rect1, const RECT &rect2) {
  touchRegion->left   = MAX(rect1.left,   rect2.left);
  touchRegion->right  = MIN(rect1.right,  rect2.right);
  touchRegion->top    = MAX(rect1.top,    rect2.top);
  touchRegion->bottom = MIN(rect1.bottom, rect2.bottom);
  return (touchRegion->left == touchRegion->right) != (touchRegion->top == touchRegion->bottom);
}

static POINT get_rect_center(const RECT &rect) {
  POINT p;
  p.x = rect.left + (rect.right - rect.left) / 2;
  p.y = rect.top + (rect.bottom - rect.top) / 2;
  return p;
}

static Point to_relative_point(const RECT &frame, const POINT &point) {
  int width = frame.right - frame.left;
  int height = frame.bottom - frame.top;
  
  return Point(
           width  > 0 ? (point.x - frame.left) / (double)width : 0.0,
           height > 0 ? (point.y - frame.top) / (double)height : 0.0);
}

static POINT to_absolute_point(const RECT &frame, const Point &point) {
  POINT p;
  p.x = (int)(frame.left + point.x * (frame.right - frame.left));
  p.y = (int)(frame.top + point.y * (frame.bottom - frame.top));
  return p;
}

BOOL CALLBACK BasicWin32Window::find_snap_hwnd(HWND hwnd, LPARAM lParam) {
  struct find_snap_info_t *info = (struct find_snap_info_t *)lParam;

  if(hwnd != info->dst && IsWindowVisible(hwnd)) {
    auto win = dynamic_cast<BasicWin32Window*>(
                              BasicWin32Widget::from_hwnd(hwnd));

    if(win && win->zorder_level() <= info->min_level)
      return TRUE;
    
    RECT rect;
    get_snap_rect(hwnd, &rect);
    
    RECT touchRegion;
    if(!touchingRectangles(&touchRegion, info->dst_rect, rect))
      return TRUE;
    
    if(info->snappers->contains(hwnd))
      return TRUE;
    
    POINT touch_center = get_rect_center(touchRegion);
    
    SnapPosition snap { 
      hwnd,
      to_relative_point(rect, touch_center),
      info->dst, 
      to_relative_point(info->dst_rect, touch_center), 
    };
    
    info->snappers->add(hwnd);
    info->snapper_positions->add(std::move(snap));
  }

  return TRUE;
}

void BasicWin32Window::find_all_snappers() {
  struct find_snap_info_t info;

  all_snappers.clear();
  all_snapper_positions.length(0);

  info.dst = _hwnd;
  info.min_level = zorder_level();
  get_snap_rect(info.dst, &info.dst_rect);
  info.snappers = &all_snappers;
  info.snapper_positions = &all_snapper_positions;

  all_snappers.add(_hwnd);

  EnumThreadWindows(GetCurrentThreadId(), find_snap_hwnd, (LPARAM)&info);

  unsigned int found = 1;
  for(int rep = 1; rep < 5 && found < all_snappers.size(); ++rep) {
    found = all_snappers.size();
    
    Hashset<HWND> old_snappers;
    old_snappers.merge(all_snappers);
    
    for(auto hwnd : old_snappers.keys()) {
      info.dst = hwnd;
      get_snap_rect(info.dst, &info.dst_rect);

      EnumThreadWindows(GetCurrentThreadId(), find_snap_hwnd, (LPARAM)&info);
    }
  }

  all_snappers.remove(_hwnd);
}

HDWP BasicWin32Window::move_all_snappers(HDWP hdwp, const RECT &new_bounds) {
  Hashtable<HWND, RECT> new_rects;
  new_rects.set(_hwnd, new_bounds);
  
  for(const auto &snap : all_snapper_positions) {
    RECT src_rect;
    if(!GetWindowRect(snap.src, &src_rect)) {
      pmath_debug_print("\n[GetWindowRect(%p) failed]", snap.src);
    }
    RECT *dst_rect = new_rects.search(snap.dst);
    
    if(!dst_rect) {
      pmath_debug_print("\n[unrecognized DST %p]", snap.dst);
      continue;
    }
    
    RECT frame = *dst_rect;
    adjust_snap_rect(snap.dst, &frame);
    POINT dst_touch_pt = to_absolute_point(frame, snap.dst_rel_touch);
    
    frame = src_rect;
    adjust_snap_rect(snap.src, &frame);
    POINT src_touch_pt = to_absolute_point(frame, snap.src_rel_touch);
    
    OffsetRect(&src_rect, dst_touch_pt.x - src_touch_pt.x, dst_touch_pt.y - src_touch_pt.y);
    new_rects.set(snap.src, src_rect);
  }
  
  new_rects.remove(_hwnd);
  for(const auto &entry : new_rects.entries()) {
    hdwp = tryDeferWindowPos(
             hdwp,
             entry.key,
             nullptr,
             entry.value.left,
             entry.value.top,
             0, 0,
             SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
  }
  
  return hdwp;
}

struct find_align_info_t {
  HWND dst;
  RECT dst_rect;
  bool align_left;
  bool align_right;
  bool align_top;
  bool align_bottom;
};

static BOOL CALLBACK find_align_hwnd(HWND hwnd, LPARAM lParam) {
  struct find_align_info_t *info = (struct find_align_info_t *)lParam;

  if(hwnd != info->dst && IsWindowVisible(hwnd)) {
    RECT rect;
    get_snap_rect(hwnd, &rect);

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

void BasicWin32Window::get_snap_alignment(bool *right, bool *bottom) {
  struct find_align_info_t info;

  *right = false;
  *bottom = false;

  memset(&info, 0, sizeof(info));
  info.dst = _hwnd;
  get_snap_rect(info.dst, &info.dst_rect);
  EnumThreadWindows(GetCurrentThreadId(), find_align_hwnd, (LPARAM)&info);

  *right  = info.align_right  && !info.align_left;
  *bottom = info.align_bottom && !info.align_top;

//  if( !info.align_left &&
//      !info.align_right &&
//      !info.align_top &&
//      !info.align_bottom)
//  {
//    MONITORINFO monitor_info;
//    memset(&monitor_info, 0, sizeof(monitor_info));
//    monitor_info.cbSize = sizeof(monitor_info);
//
//    HMONITOR hmon = MonitorFromWindow(_hwnd, MONITOR_DEFAULTTONEAREST);
//    if(GetMonitorInfo(hmon, &monitor_info)) {
//      *right  = (info.dst_rect.left       + info.dst_rect.right)
//                > (monitor_info.rcWork.left + monitor_info.rcWork.right);
//
//      *bottom = (info.dst_rect.top        + info.dst_rect.bottom)
//                > (monitor_info.rcWork.top  + monitor_info.rcWork.bottom);
//    }
//  }
}

void BasicWin32Window::on_sizing(WPARAM wParam, RECT *lParam) {
  if(all_snappers.size() > 0) {
    all_snappers.clear();
    all_snapper_positions.length(0);
  }

  Win32Themes::MARGINS margins;
  get_nc_margins(&margins);

  int minh = min_client_height + margins.cyTopHeight + margins.cyBottomHeight;
  int maxh = max_client_height + margins.cyTopHeight + margins.cyBottomHeight;
  int minw = min_client_width  + margins.cxLeftWidth + margins.cxRightWidth;
  int maxw = max_client_width  + margins.cxLeftWidth + margins.cxRightWidth;
  
  bool change_left = false;
  bool change_right = false;
  bool change_top = false;
  bool change_bottom = false;
  switch(wParam) {
    case WMSZ_LEFT:   change_left = true; break;
    case WMSZ_RIGHT:  change_right = true; break;
    case WMSZ_TOP:    change_top = true; break;
    case WMSZ_BOTTOM: change_bottom = true; break;
      
    case WMSZ_TOPLEFT:     change_top = change_left = true; break;
    case WMSZ_TOPRIGHT:    change_top = change_right = true; break;
    case WMSZ_BOTTOMLEFT:  change_bottom = change_left = true; break;
    case WMSZ_BOTTOMRIGHT: change_bottom = change_right = true; break;
  }
  
  
  if(GetKeyState(VK_MENU) & ~1) {
    change_left = change_right = change_left || change_right;
    change_top = change_bottom = change_top || change_bottom;
  }
  
  //POINT center { lParam->left + (lParam->right - lParam->left)/2, lParam->top + (lParam->bottom - lParam->top)/2 };
  POINT center { last_moving_cx, last_moving_cy };
  
  bool minmax = false;
  if(lParam->bottom - lParam->top < minh) {
    if(change_top && change_bottom) {
      lParam->top = center.y - minh/2;
      lParam->bottom = lParam->top + minh;
    }
    else if(change_top)
      lParam->top = lParam->bottom - minh;
    else
      lParam->bottom = lParam->top + minh;

    minmax = true;
  }

  if(lParam->bottom - lParam->top > maxh && max_client_height > 0) {
    if(change_top && change_bottom) {
      lParam->top = center.y - maxh/2;
      lParam->bottom = lParam->top + maxh;
    }
    else if(change_top)
      lParam->top = lParam->bottom - maxh;
    else
      lParam->bottom = lParam->top + maxh;

    minmax = true;
  }

  if(lParam->right - lParam->left < minw) {
    if(change_left && change_right) {
      lParam->left = center.x - minw/2;
      lParam->right = lParam->left + minw;
    }
    else if(change_left)
      lParam->left = lParam->right - minw;
    else
      lParam->right = lParam->left + minw;

    minmax = true;
  }

  if(lParam->right - lParam->left > maxw && max_client_width > 0) {
    if(change_left && change_right) {
      lParam->left = center.x - maxw/2;
      lParam->right = lParam->left + maxw;
    }
    else if(change_left)
      lParam->left = lParam->right - maxw;
    else
      lParam->right = lParam->left + maxw;

    minmax = true;
  }

  if(minmax)
    return;

  POINT pt = {0, 0};
//  RECT snapping_rect = *lParam;
//  adjust_snap_rect(_hwnd, &snapping_rect);
  Win32Themes::MARGINS snap_margins;
  get_snap_margins(_hwnd, &snap_margins);
  RECT snapping_rect;
  snapping_rect.left   = lParam->left   - snap_margins.cxLeftWidth;
  snapping_rect.top    = lParam->top    - snap_margins.cyTopHeight;
  snapping_rect.right  = lParam->right  + snap_margins.cxRightWidth;
  snapping_rect.bottom = lParam->bottom + snap_margins.cyBottomHeight;

  switch(wParam) {
    case WMSZ_BOTTOM:
      pt.y = snapping_rect.bottom;
      break;

    case WMSZ_BOTTOMLEFT:
      pt.x = snapping_rect.left;
      pt.y = snapping_rect.bottom;
      break;

    case WMSZ_BOTTOMRIGHT:
      pt.x = snapping_rect.right;
      pt.y = snapping_rect.bottom;
      break;

    case WMSZ_TOP:
      pt.y = snapping_rect.top;
      break;

    case WMSZ_TOPLEFT:
      pt.x = snapping_rect.left;
      pt.y = snapping_rect.top;
      break;

    case WMSZ_TOPRIGHT:
      pt.x = snapping_rect.right;
      pt.y = snapping_rect.top;
      break;

    case WMSZ_LEFT:
      pt.x = snapping_rect.left;
      break;

    case WMSZ_RIGHT:
      pt.x = snapping_rect.right;
      break;
  }

  int old_snap_dx = snap_correction_x;
  int old_snap_dy = snap_correction_y;

  snap_correction_x = 0;
  snap_correction_y = 0;
  snap_rect_or_pt(&snapping_rect, &pt);
  
  switch(wParam) {
    case WMSZ_BOTTOM:
      snap_correction_x = old_snap_dx;
      lParam->bottom = pt.y - snap_margins.cyBottomHeight;
      break;

    case WMSZ_BOTTOMLEFT:
      lParam->left   = pt.x + snap_margins.cxLeftWidth;
      lParam->bottom = pt.y - snap_margins.cyBottomHeight;
      break;

    case WMSZ_BOTTOMRIGHT:
      lParam->right  = pt.x - snap_margins.cxRightWidth;
      lParam->bottom = pt.y - snap_margins.cyBottomHeight;
      break;

    case WMSZ_TOP:
      snap_correction_x = old_snap_dx;
      lParam->top = pt.y + snap_margins.cyTopHeight;
      break;

    case WMSZ_TOPLEFT:
      lParam->left = pt.x + snap_margins.cxLeftWidth;
      lParam->top  = pt.y + snap_margins.cyTopHeight;
      break;

    case WMSZ_TOPRIGHT:
      lParam->right = pt.x - snap_margins.cxRightWidth;
      lParam->top   = pt.y + snap_margins.cyTopHeight;
      break;

    case WMSZ_LEFT:
      snap_correction_y = old_snap_dy;
      lParam->left = pt.x + snap_margins.cxLeftWidth;
      break;

    case WMSZ_RIGHT:
      snap_correction_y = old_snap_dy;
      lParam->right = pt.x - snap_margins.cxRightWidth;
      break;
  }
  
  if(change_left && change_right) {
    switch(wParam) {
      case WMSZ_LEFT:
      case WMSZ_BOTTOMLEFT:
      case WMSZ_TOPLEFT:
        lParam->right = center.x + (center.x - lParam->left);
        break;
      
      case WMSZ_RIGHT:
      case WMSZ_BOTTOMRIGHT:
      case WMSZ_TOPRIGHT:
        lParam->left = center.x - (lParam->right - center.x);
        break;
    }
  }
  
  if(change_top && change_bottom) {
    switch(wParam) {
      case WMSZ_TOP:
      case WMSZ_TOPLEFT:
      case WMSZ_TOPRIGHT:
        lParam->bottom = center.y + (center.y - lParam->top);
        break;
      
      case WMSZ_BOTTOM:
      case WMSZ_BOTTOMLEFT:
      case WMSZ_BOTTOMRIGHT:
        lParam->top = center.y - (lParam->bottom - center.y);
        break;
    }
  }
}

void BasicWin32Window::on_moving(RECT *lParam) {
  RECT rect;
  GetWindowRect(_hwnd, &rect);
  
  Win32Themes::MARGINS snap_margins;
  get_snap_margins(_hwnd, &snap_margins);
  
  if( rect.right - rect.left != lParam->right - lParam->left ||
      rect.bottom - rect.top != lParam->bottom - lParam->top)
  {
    /* Width and/or height does not match. So lParam represents Aero Snap rectangle.
       That does not have invisible window borders, so we remove our additional margins
     */
     memset(&snap_margins, 0, sizeof(snap_margins));
  }
  
  RECT snapping_rect;
  snapping_rect.left   = lParam->left   - snap_margins.cxLeftWidth;
  snapping_rect.top    = lParam->top    - snap_margins.cyTopHeight;
  snapping_rect.right  = lParam->right  + snap_margins.cxRightWidth;
  snapping_rect.bottom = lParam->bottom + snap_margins.cyBottomHeight;
  
  snap_rect_or_pt(&snapping_rect, nullptr);
  lParam->left   = snapping_rect.left   + snap_margins.cxLeftWidth;
  lParam->top    = snapping_rect.top    + snap_margins.cyTopHeight;
  lParam->right  = snapping_rect.right  - snap_margins.cxRightWidth;
  lParam->bottom = snapping_rect.bottom - snap_margins.cyBottomHeight;

  HDWP hdwp = BeginDeferWindowPos(all_snappers.size());
  
  int cx = rect.left + (rect.right - rect.left)/2;
  int cy = rect.top  + (rect.bottom - rect.top)/2;
  if( cx != last_moving_cx || cy != last_moving_cy)
    hdwp = move_all_snappers(hdwp, rect);
  
  EndDeferWindowPos(hdwp);

  last_moving_cx = cx;
  last_moving_cy = cy;
}

void BasicWin32Window::on_move(LPARAM lParam) {
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

  HDWP hdwp = BeginDeferWindowPos(1 + all_snappers.size());

  hdwp = tryDeferWindowPos(
           hdwp,
           _hwnd,
           nullptr,
           rect.left,
           rect.top,
           1,
           1,
           SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

  int cx = rect.left + (rect.right - rect.left)/2;
  int cy = rect.top  + (rect.bottom - rect.top)/2;
  if( cx != last_moving_cx || cy != last_moving_cy)
    hdwp = move_all_snappers(hdwp, rect);

  EndDeferWindowPos(hdwp);

  last_moving_cx = cx;
  last_moving_cy = cy;
}

//} ... snapping windows & alignment

void BasicWin32Window::on_theme_changed() {
  _glass_enabled = false;
  _themed_frame = false;

  if(Win32Themes::IsCompositionActive && Win32Themes::IsCompositionActive()) {
    _glass_enabled = true;
    _themed_frame = 0 == (WS_EX_TOOLWINDOW & GetWindowLongW(_hwnd, GWL_EXSTYLE));
  }
  else if(Win32Themes::IsThemeActive && Win32Themes::IsThemeActive()) {
    _glass_enabled = Win32Themes::current_theme_is_aero();
  }

  if(Win32Themes::CloseThemeData) {
    for(auto &e : composition_window_theme_for_dpi.entries())
      Win32Themes::CloseThemeData(e.value);
  
    composition_window_theme_for_dpi.clear();
  }

  extend_glass(&_extra_glass);

  SetWindowPos(_hwnd, 0, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void BasicWin32Window::paint_themed(HDC hdc) {
  if(_themed_frame) {
    RECT rect;
    GetClientRect(_hwnd, &rect);
    
    /* Create a DIB compatible with hdc. 
       cairo_win32_surface_create_with_dib() would only create a DIB compatible with 
       the primary monitor.
     */
    cairo_surface_t *ddb = cairo_win32_surface_create(hdc);
    cairo_surface_t *surface = cairo_surface_create_similar(ddb,
                                 CAIRO_CONTENT_COLOR_ALPHA,
                                 rect.right  - rect.left,
                                 rect.bottom - rect.top);
    cairo_surface_destroy(ddb);
    if(cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
      pmath_debug_print("[paint_themed: cairo_surface_create_similar() failed]");
      cairo_surface_destroy(surface);
      return;
    }
    
    HDC bmp_dc = cairo_win32_surface_get_dc(surface);
    SetLayout(bmp_dc, GetLayout(hdc));

    cairo_t *cr = cairo_create(surface);
    {
      Canvas canvas(cr);

      paint_background(&canvas, 0, 0);
    }
    cairo_destroy(cr);

    cairo_surface_flush(surface);

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
    
    //paint_themed_caption(hdc);

    cairo_surface_destroy(surface);
  }
  else {
  }
}

static void get_system_button_bounds(HWND hwnd, RECT *rect) {
  //DwmGetWindowAttribute(hwnd, DWMWA_CAPTION_BUTTON_BOUNDS, rect, sizeof(RECT));

  TITLEBARINFOEX tbi;
  memset(&tbi, 0, sizeof(tbi));
  tbi.cbSize = sizeof(tbi);

  SendMessageW(hwnd, WM_GETTITLEBARINFOEX, 0, (LPARAM)&tbi);

  memset(rect, 0, sizeof(RECT));
  for(int i = 2; i <= 5; ++i)
    UnionRect(rect, rect, &tbi.rgrect[i]);

  MapWindowPoints(nullptr, hwnd, (POINT*)rect, 2);
}

static void get_system_menu_bounds(HWND hwnd, RECT *rect, int dpi) {
  memset(rect, 0, sizeof(RECT));
  
  GetWindowRect(hwnd, rect);
  
  RECT buttons;
  get_system_button_bounds(hwnd, &buttons);
  MapWindowPoints(hwnd, nullptr, (POINT *)&buttons, 2);
  int invisible_top = buttons.top - rect->top - 1;
  
  DWORD style    = GetWindowLongW(hwnd, GWL_STYLE);
  DWORD ex_style = GetWindowLongW(hwnd, GWL_EXSTYLE);
  RECT neg_margins = { 0, 0, 0, 0 };
  Win32HighDpi::adjust_window_rect(&neg_margins, style, FALSE, ex_style, dpi);
  
  int caption_h = Win32HighDpi::get_system_metrics_for_dpi(SM_CYCAPTION, dpi);
  int icon_w    = Win32HighDpi::get_system_metrics_for_dpi(SM_CXSMICON, dpi);
  int icon_h    = Win32HighDpi::get_system_metrics_for_dpi(SM_CYSMICON, dpi);
  
  int visible_top = -neg_margins.top - invisible_top;
  
  rect->top+= invisible_top + (visible_top - icon_h) / 2;
  rect->bottom = rect->top + icon_h;
  
  if(ex_style & WS_EX_LAYOUTRTL) {
    rect->right-= -neg_margins.left + 1;
    if(Win32Themes::check_osversion(10, 0) && (style & WS_MAXIMIZE) == 0) {
      /* In Windows 10, the left/right/bottom frame is invisible, so the icon indents more when not maximized. */
      rect->right-= -neg_margins.left;
    }
    rect->left = rect->right - icon_w;
  }
  else {
    rect->left += -neg_margins.left + 1;
    if(Win32Themes::check_osversion(10, 0) && (style & WS_MAXIMIZE) == 0) {
      /* In Windows 10, the left/right/bottom frame is invisible, so the icon indents more when not maximized. */
      rect->left+= -neg_margins.left;
    }
    rect->right = rect->left + icon_w;
  }
  
  MapWindowPoints(nullptr, hwnd, (POINT*)rect, 2);
}

COLORREF BasicWin32Window::title_font_color(bool glass_enabled, int dpi, bool active) {
  if(!glass_enabled) {
    //return GetSysColor(active ? COLOR_BTNTEXT : COLOR_GRAYTEXT);
    return GetSysColor(COLOR_BTNTEXT);
  }
  
  if(HANDLE theme = composition_window_theme(dpi)) {
//    if(Win32Themes::check_osversion(10, 0) && Win32Themes::DwmGetColorizationParameters) {
//      Win32Themes::DWM_COLORIZATION_PARAMS params = {0};
//      Win32Themes::DwmGetColorizationParameters(&params);
//      
//      // TODO: only use text-on-accent-backgound color if HKCU\SOFTWARE\Microsoft\Windows\DWM\ColorPrevalence = 1
//      dtt_opts.crText = Win32Themes::get_window_title_text_color(&params, _active);
//    }
    if(active) {
      Win32Themes::ColorizationInfo colorization;
      if(Win32Themes::try_read_win10_colorization(&colorization)) {
        if(colorization.has_accent_color_in_active_titlebar)
          return colorization.text_on_accent_color;
      }
    }
    else {
      /* Inactive titlebar text color seems to be 0x999999u on Windows 10.
         What does COLOR_INACTIVECAPTIONTEXT give? Should we hard-code 0x999999u?
         
         https://github.com/res2k/Windows10Colors/blob/master/Windows10Colors/Windows10Colors.cpp#L544
         blends 40% COLOR_INACTIVECAPTIONTEXT with 60% COLOR_INACTIVECAPTION
         Note that 0.4 * 0x00 + 0.6 * 0xFF = 0x99.
       */
      if(Win32Themes::check_osversion(10, 0)) {
        return 0x999999;
      }
    }
    
    if(Win32Themes::IsCompositionActive && Win32Themes::IsCompositionActive())
      return Win32Themes::GetThemeSysColor(theme, COLOR_CAPTIONTEXT);
    else
      return Win32Themes::GetThemeSysColor(theme, active ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT);
  }
  
  return GetSysColor(active ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT);
}

void BasicWin32Window::paint_themed_caption(HDC hdc_bitmap) {
  if( !_themed_frame                 ||
      !Win32Themes::OpenThemeData    ||
      !Win32Themes::CloseThemeData   ||
      !Win32Themes::GetThemeSysColor ||
      !Win32Themes::GetThemeSysFont  ||
      !Win32Themes::DrawThemeTextEx)
  {
    return;
  }

  int dpi = Win32HighDpi::get_dpi_for_window(_hwnd);
  if(HANDLE theme = composition_window_theme(dpi)) {
  
    Win32Themes::DTTOPTS dtt_opts;
    memset(&dtt_opts, 0, sizeof(dtt_opts));
    dtt_opts.dwSize    = sizeof(dtt_opts);
    dtt_opts.dwFlags   = DTT_COMPOSITED | DTT_GLOWSIZE | DTT_TEXTCOLOR;
    dtt_opts.iGlowSize = MulDiv(10, dpi, 96);
    dtt_opts.crText    = title_font_color(true, dpi, _active);
    
#define MAX_STR_LEN 1024
    WCHAR str[MAX_STR_LEN];
    GetWindowTextW(_hwnd, str, MAX_STR_LEN);
    str[MAX_STR_LEN - 1] = '\0';

    LOGFONTW log_font;
    HFONT old_font = nullptr;
    if(SUCCEEDED(Win32Themes::GetThemeSysFont(theme, 801, &log_font))) {
      // TMT_CAPTIONFONT = 801
      /* GetThemeSysFont scales according to the current logical screen dpi. */
      int logical_screen_dpi = Win32HighDpi::get_dpi_for_system();
      log_font.lfHeight = MulDiv(log_font.lfHeight, dpi, logical_screen_dpi);
      HFONT font = CreateFontIndirectW(&log_font);
      old_font = (HFONT)SelectObject(hdc_bitmap, font);
    }

    bool center_caption = false;

    // center caption on Windows 8 or 8.1
    if(Win32Themes::check_osversion(6, 2) && !Win32Themes::check_osversion(10, 0)) {
      center_caption = true;
    }

    int flags = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
    
    // TODO: temporarily scale GDI context to 96 dpi coordinates to write propper font size.
    //int map_mode = GetMapMode(hdc_bitmap);
    //SetMapMode(hdc_bitmap, MM_LOENGLISH);
    
    Win32Themes::MARGINS nc;
    ::get_nc_margins(_hwnd, &nc, dpi);
    RECT menu, rect, buttons;
    get_system_button_bounds(_hwnd, &buttons);
    get_system_menu_bounds(_hwnd, &menu, dpi);
    if(center_caption) {
      flags |= DT_CENTER;

      RECT calc_rect = {0};
      //calc_rect.right = buttons.right;

      dtt_opts.dwFlags |= DTT_CALCRECT;

      Win32Themes::DrawThemeTextEx(
        theme,
        hdc_bitmap,
        0, 0,
        str, -1,
        flags | DT_CALCRECT,
        &calc_rect,
        &dtt_opts);

      dtt_opts.dwFlags &= ~DTT_CALCRECT;

      rect.left   = buttons.right - buttons.left;
      rect.right  = buttons.left;
      rect.top    = MAX(0, buttons.top - 1); 
      rect.bottom = nc.cyTopHeight;

      if(calc_rect.right + MulDiv(8, dpi, 96) > rect.right - rect.left) {
        rect.left = menu.right + MulDiv(4, dpi, 96);
      }
    }
    else {
      rect.right  = buttons.left;
      rect.left   = menu.right + MulDiv(4, dpi, 96);
      rect.top    = MAX(0, buttons.top - 1); 
      rect.bottom = nc.cyTopHeight;
    }

    Win32Themes::DrawThemeTextEx(
      theme,
      hdc_bitmap,
      0, 0,
      str, -1,
      flags,
      &rect,
      &dtt_opts);
      
    //SetMapMode(hdc_bitmap, map_mode);

    if(GetClassNameW(_hwnd, str, MAX_STR_LEN)) {
      WNDCLASSEXW wndcl;
      memset(&wndcl, 0, sizeof(wndcl));

      GetClassInfoExW(GetModuleHandle(0), str, &wndcl);
      HICON icon = (HICON)LoadImageW(wndcl.hInstance, MAKEINTRESOURCEW(ICO_APP_MAIN),
                                     IMAGE_ICON,
                                     menu.right - menu.left,
                                     menu.bottom - menu.top,
                                     LR_DEFAULTCOLOR);
      if(icon) {
        DrawIconEx(
          hdc_bitmap,
          menu.left,
          menu.top,
          icon,
          menu.right - menu.left,
          menu.bottom - menu.top,
          0,
          nullptr,
          DI_NORMAL);
        
        DestroyIcon(icon);
      }
    }

    if(old_font) {
      HFONT font = (HFONT)SelectObject(hdc_bitmap, old_font);
      DeleteObject(font);
    }
  }
}

void BasicWin32Window::extend_glass(const Win32Themes::MARGINS *margins) {
  if(margins != &_extra_glass) {
    if(0 == memcmp(&_extra_glass, &margins, sizeof(_extra_glass)))
      return;

    memcpy(&_extra_glass, margins, sizeof(_extra_glass));
  }

  if(Win32Themes::DwmExtendFrameIntoClientArea) {

    if( margins->cxLeftWidth    == -1 &&
        margins->cxRightWidth   == -1 &&
        margins->cyTopHeight    == -1 &&
        margins->cyBottomHeight == -1)
    {
      Win32Themes::DwmExtendFrameIntoClientArea(_hwnd, margins);
    }
    else {
      Win32Themes::MARGINS nc;
      memset(&nc, 0, sizeof(nc));
      if(_themed_frame)
        get_nc_margins(&nc);
      
      if(!is_left_right_bottom_frame_themed(this)) {
        nc.cxLeftWidth = 0;
        nc.cxRightWidth = 0;
        nc.cyBottomHeight = 0;
      }
      
      nc.cxLeftWidth += margins->cxLeftWidth;
      nc.cxRightWidth += margins->cxRightWidth;
      nc.cyTopHeight += margins->cyTopHeight;
      nc.cyBottomHeight += margins->cyBottomHeight;

      Win32Themes::DwmExtendFrameIntoClientArea(_hwnd, &nc);
    }

    // Inform application of the frame change.
    SetWindowPos(
      _hwnd,
      nullptr,
      0, //client.left,
      0, //client.top,
      1, //client.right  - client.left,
      1, //client.bottom - client.top,
      SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    if(_themed_frame) {
      invalidate_non_child();
      //RECT client;
      //GetClientRect(_hwnd, &client);
      //
      //RECT rect;
      //rect.left   = nc.cxLeftWidth - 3;
      //rect.right  = nc.cxLeftWidth;
      //rect.top    = 0;
      //rect.bottom = client.bottom;
      //InvalidateRect(_hwnd, &rect, FALSE);
      //
      //rect.left   = client.right - nc.cxRightWidth;
      //rect.right  = rect.left + 3;
      //InvalidateRect(_hwnd, &rect, FALSE);
    }
  }
}

bool BasicWin32Window::is_closed() {
  return !IsWindowVisible(_hwnd);
}

void BasicWin32Window::paint_background(Canvas *canvas, HWND child, bool wallpaper_only) {
  RECT rect, child_rect;
  Win32Themes::MARGINS margins = {0};

  GetWindowRect(child, &child_rect);
  GetWindowRect(_hwnd, &rect);
  get_nc_margins(&margins);

  paint_background(
    canvas,
    child_rect.left - rect.left - margins.cxLeftWidth,
    child_rect.top  - rect.top,
    wallpaper_only);
}

void BasicWin32Window::paint_background(Canvas *canvas, int x, int y, bool wallpaper_only) {
  canvas->save();
  {
    canvas->reset_matrix();
    cairo_reset_clip(canvas->cairo());

    RECT window_rect;
    GetWindowRect(_hwnd, &window_rect);
    MapWindowPoints(nullptr, _hwnd, (POINT *)&window_rect, 2);

    canvas->translate(-x, -y);

    RECT glassfree;
    get_glassfree_rect(&glassfree);
//    glassfree.left   -= window_rect.left;
//    glassfree.right  -= window_rect.left;
    glassfree.top    -= window_rect.top;
    glassfree.bottom -= window_rect.top;

    if(!wallpaper_only) {
      if( !Win32Themes::IsCompositionActive ||
          !Win32Themes::IsCompositionActive())
      {
        if(glass_enabled()) {
          Color color;

          if(_active)
            color = Color::from_bgr24(GetSysColor(COLOR_GRADIENTACTIVECAPTION));
          else
            color = Color::from_bgr24(GetSysColor(COLOR_GRADIENTINACTIVECAPTION));

          canvas->set_color(color);
          canvas->paint();
        }
        else {
          Color color = Color::from_bgr24(GetSysColor(COLOR_BTNFACE));
          
          canvas->set_color(color);
          canvas->paint();
        }
      }
      else {
        cairo_set_operator(canvas->cairo(), CAIRO_OPERATOR_CLEAR);
        canvas->set_color(Color::Black, 0.0);
        canvas->paint();
        cairo_set_operator(canvas->cairo(), CAIRO_OPERATOR_OVER);
      }

      if(!IsRectEmpty(&glassfree)) {
        Color color = Color::from_bgr24(GetSysColor(COLOR_BTNFACE));
        
        canvas->move_to(glassfree.left,  glassfree.top);
        canvas->line_to(glassfree.left,  glassfree.bottom);
        canvas->line_to(glassfree.right, glassfree.bottom);
        canvas->line_to(glassfree.right, glassfree.top);
        canvas->close_path();

        canvas->set_color(color);
        canvas->fill();
      }
    }

    on_paint_background(canvas);

    if(_themed_frame) {
      LONG style_ex = GetWindowLongW(_hwnd, GWL_EXSTYLE);
      RECT client_rect;
      GetClientRect(_hwnd, &client_rect);
      get_glassfree_rect(&glassfree);
      
      CanvasAutoSave saved(canvas);
      if(style_ex & WS_EX_LAYOUTRTL) {
        /* RTL layout: Windows client coordinates are right-to-left (0,0) is at top right,
           but Cairo coordinates are left-to-right.
           Temporarily make Cairo use Windows client coordinates.
         */
        canvas->translate(
          client_rect.right - client_rect.left,
          0);
        canvas->scale(-1, 1);
      }

      cairo_reset_clip(canvas->cairo());

      int buttonradius;
      int frameradius;
      float buttons_alpha = 1.0f;

      bool is_win8_or_newer = Win32Themes::check_osversion(6, 2);

      if(_mouse_over_caption_buttons) {
        if(is_win8_or_newer)
          buttons_alpha = 0.8f;
        else
          buttons_alpha = 1.0f;
      }
      else if(_active) {
        if(is_win8_or_newer)
          buttons_alpha = 0.5f;
        else
          buttons_alpha = 0.8f;
      }
      else {
        if(is_win8_or_newer)
          buttons_alpha = 0.4f;
        else
          buttons_alpha = 0.4f;
      }

      if(is_win8_or_newer) { // Windows 8 or newer
        buttonradius = 1;
        frameradius  = 1;
      }
      else if(style_ex & WS_EX_TOOLWINDOW) {
        buttonradius = 1;
        frameradius  = 1;
      }
      else {
        buttonradius = 5;
        frameradius  = 6; //Win32HighDpi::get_system_metrics_for_dpi(SM_CXFRAME)-2;
      }

      if(!IsRectEmpty(&glassfree) && !is_win8_or_newer) { // show border between glass/nonglass on Windows Vista and 7
        cairo_set_operator(canvas->cairo(), CAIRO_OPERATOR_DEST_OUT);

        canvas->move_to(client_rect.left,  client_rect.top);
        canvas->line_to(client_rect.right, client_rect.top);
        canvas->line_to(client_rect.right, client_rect.bottom);
        canvas->line_to(client_rect.left,  client_rect.bottom);
        canvas->close_path();

        canvas->move_to(glassfree.left,  glassfree.top);
        canvas->line_to(glassfree.left,  glassfree.bottom);
        canvas->line_to(glassfree.right, glassfree.bottom);
        canvas->line_to(glassfree.right, glassfree.top);
        canvas->close_path();

        canvas->clip();

        cairo_set_line_width(canvas->cairo(), 3);

        canvas->move_to(window_rect.left,  window_rect.top);
        canvas->line_to(window_rect.right, window_rect.top);
        canvas->line_to(window_rect.right, window_rect.bottom);
        canvas->line_to(window_rect.left,  window_rect.bottom);
        canvas->close_path();

        canvas->move_to(glassfree.left,  glassfree.top);
        canvas->line_to(glassfree.left,  glassfree.bottom);
        canvas->line_to(glassfree.right, glassfree.bottom);
        canvas->line_to(glassfree.right, glassfree.top);
        canvas->close_path();

        canvas->set_color(Color::White, 1 - 0.1);
        canvas->stroke();

        cairo_reset_clip(canvas->cairo());
      }

      { // small alpha value above the min/max/close buttons
        RECT buttons;
        get_system_button_bounds(_hwnd, &buttons);

        if(style_ex & WS_EX_TOOLWINDOW) {
          canvas->move_to(buttons.left,  buttons.top);
          canvas->line_to(buttons.right, buttons.top);
          canvas->line_to(buttons.right, buttons.bottom);
          canvas->line_to(buttons.left,  buttons.bottom);
          canvas->close_path();
        }
        else {
          canvas->move_to(buttons.left + 0.5,   buttons.top);
          canvas->arc(    buttons.left + 0.5  + buttonradius, buttons.bottom - 0.5 - buttonradius, buttonradius, M_PI,   M_PI / 2, true);
          canvas->arc(    buttons.right - 0.5 - buttonradius, buttons.bottom - 0.5 - buttonradius, buttonradius, M_PI / 2, 0,      true);
          canvas->line_to(buttons.right - 0.5,  buttons.top);
          canvas->close_path();
        }

        canvas->set_color(Color::White, buttons_alpha);
        cairo_set_operator(canvas->cairo(), CAIRO_OPERATOR_DEST_OUT);

//        if(_mouse_over_caption_buttons) {
//          cairo_set_operator(canvas->cairo(), CAIRO_OPERATOR_CLEAR);
//        }
//        else if(_active) {
//          canvas->set_color(Color::White, 1 - 0.2);
//          cairo_set_operator(canvas->cairo(), CAIRO_OPERATOR_DEST_OUT);
//        }
//        else {
//          canvas->set_color(Color::White, 1 - 0.6);
//          cairo_set_operator(canvas->cairo(), CAIRO_OPERATOR_DEST_OUT);
//        }

        canvas->fill();
      }

      if( client_rect.left - window_rect.left > frameradius || 
          window_rect.right - client_rect.right > frameradius)
      { // make the edges round again
        canvas->move_to(client_rect.left,  client_rect.top);
        canvas->line_to(client_rect.left,  client_rect.bottom);
        canvas->line_to(client_rect.right, client_rect.bottom);
        canvas->line_to(client_rect.right, client_rect.top);
        canvas->close_path();

        {
          window_rect.top+= 1;

          canvas->move_to(window_rect.left, window_rect.top + frameradius);
          canvas->arc(window_rect.left  + frameradius, window_rect.top    + frameradius, frameradius,     M_PI,     3 * M_PI / 2, false);
          canvas->arc(window_rect.right - frameradius, window_rect.top    + frameradius, frameradius, 3 * M_PI / 2, 2 * M_PI,     false);
          canvas->arc(window_rect.right - frameradius, window_rect.bottom - frameradius, frameradius, 0,                M_PI / 2, false);
          canvas->arc(window_rect.left  + frameradius, window_rect.bottom - frameradius, frameradius,     M_PI / 2,     M_PI,     false);
          canvas->close_path();
        }

        cairo_set_operator(canvas->cairo(), CAIRO_OPERATOR_CLEAR);
        canvas->fill();
      }
    }
  }
  canvas->restore();
}

void BasicWin32Window::on_paint_background(Canvas *canvas) {
  if( !_themed_frame          ||
      !background_image.ptr() ||
      cairo_surface_status(background_image.ptr()) != CAIRO_STATUS_SUCCESS)
  {
    return;
  }
  
  RECT rect, glassfree;
  GetClientRect(_hwnd, &rect);
  get_glassfree_rect(&glassfree);
  
  int x = rect.right - cairo_image_surface_get_width(background_image.ptr());
  cairo_set_source_surface(canvas->cairo(), background_image.ptr(), x, 0);
  //canvas->set_color(0xff0000);

  canvas->move_to(rect.left,  rect.top);
  canvas->line_to(rect.right, rect.top);
  canvas->line_to(rect.right, rect.bottom);
  canvas->line_to(rect.left,  rect.bottom);
  canvas->close_path();

  if(!IsRectEmpty(&glassfree)) {
    canvas->move_to(glassfree.left,  glassfree.top);
    canvas->line_to(glassfree.left,  glassfree.bottom);
    canvas->line_to(glassfree.right, glassfree.bottom);
    canvas->line_to(glassfree.right, glassfree.top);
    canvas->close_path();
  }

  canvas->clip();
  canvas->paint_with_alpha(0.8);

  if(!IsRectEmpty(&glassfree)) {
    cairo_reset_clip(canvas->cairo());
    canvas->move_to(glassfree.left,  glassfree.top);
    canvas->line_to(glassfree.left,  glassfree.bottom);
    canvas->line_to(glassfree.right, glassfree.bottom);
    canvas->line_to(glassfree.right, glassfree.top);
    canvas->close_path();
    canvas->clip();

    canvas->paint_with_alpha(0.25);
  }
}

int BasicWin32Window::dpi() {
  return Win32HighDpi::get_dpi_for_window(_hwnd);
}

LRESULT BasicWin32Window::nc_hit_test(WPARAM wParam, LPARAM lParam) {
  /* Note that this function does not return HTCLOSE, HTMAXBUTTON, HTMINBUTTON because 
     DwmDefWindowProc() already did the job when the mouse is over one of these buttons
   */
  POINT mouse_screen = { (short)LOWORD(lParam), (short)HIWORD(lParam)};
  int dpi = Win32HighDpi::get_dpi_for_window(_hwnd);

  Win32Themes::MARGINS margins;
  ::get_nc_margins(_hwnd, &margins, dpi);
//  margins.cxLeftWidth    += _extra_glass.cxLeftWidth;
//  margins.cxRightWidth   += _extra_glass.cxRightWidth;
//  margins.cyTopHeight    += _extra_glass.cyTopHeight;
//  margins.cyBottomHeight += _extra_glass.cyBottomHeight;

  RECT rect;
  GetWindowRect(_hwnd, &rect);
  
  DWORD style    = GetWindowLongW(_hwnd, GWL_STYLE);
  DWORD ex_style = GetWindowLongW(_hwnd, GWL_EXSTYLE);
  RECT rcFrame = { 0, 0, 0, 0 };
  Win32HighDpi::adjust_window_rect(&rcFrame, style & ~WS_CAPTION, FALSE, ex_style, dpi);
  
  USHORT uRow = 2;
  USHORT uCol = 1;

  if( mouse_screen.y >= rect.top &&
      mouse_screen.y < rect.top - rcFrame.top)
  {
    uRow = 0;
  }
  else if(mouse_screen.y >= rect.top - rcFrame.top &&
          mouse_screen.y < rect.top + margins.cyTopHeight)
  {
    uRow = 1;
  }
  else if(mouse_screen.y < rect.bottom &&
          mouse_screen.y >= rect.bottom - margins.cyBottomHeight)
  {
    uRow = 3;
  }

  if( mouse_screen.x >= rect.left &&
      mouse_screen.x < rect.left + margins.cxLeftWidth)
  {
    uCol = 0;
  }
  else if(mouse_screen.x < rect.right &&
          mouse_screen.x >= rect.right - margins.cxRightWidth)
  {
    uCol = 2;
  }
  
  LRESULT hit_tests[4][3] = {
    { HTTOPLEFT,    HTTOP,     HTTOPRIGHT    },
    { HTLEFT,       HTCAPTION, HTRIGHT       },
    { HTLEFT,       HTCLIENT,  HTRIGHT       },
    { HTBOTTOMLEFT, HTBOTTOM,  HTBOTTOMRIGHT },
  };
  
  LRESULT result = hit_tests[uRow][uCol];
  
  if(result == HTCAPTION) {
    POINT mouse_client = mouse_screen;
    ScreenToClient(_hwnd, &mouse_client);
    
    RECT menu;
    get_system_menu_bounds(_hwnd, &menu, dpi);
    if( mouse_client.x < menu.right && mouse_client.y < menu.bottom) {
      return HTSYSMENU;
    }
  }
  
  return result;
}

struct remove_child_rgn_info_t {
  HWND parent;
  HRGN region;
};

static BOOL CALLBACK remove_child_rgn_callback(HWND hwnd, LPARAM lParam) {
  struct remove_child_rgn_info_t *info = (struct remove_child_rgn_info_t *)lParam;

  RECT rect;
  GetWindowRect(hwnd, &rect);
  MapWindowPoints(nullptr, info->parent, (POINT *)&rect, 2);

  HRGN rectrgn = CreateRectRgnIndirect(&rect);
  CombineRgn(info->region, info->region, rectrgn, RGN_DIFF);
  DeleteObject(rectrgn);

  return TRUE;
}

void BasicWin32Window::invalidate_non_child() {
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

void BasicWin32Window::invalidate_caption() {
  if(_themed_frame) {
    RECT rect;
    Win32Themes::MARGINS mar;
    GetClientRect(_hwnd, &rect);
    get_nc_margins(&mar);

    rect.bottom = mar.cyTopHeight;
    InvalidateRect(_hwnd, &rect, FALSE);
  }
}

struct redraw_glass_info_t {
  RECT inner;
};

static BOOL CALLBACK redraw_glass_callback(HWND hwnd, LPARAM lParam) {
  auto info = (struct redraw_glass_info_t *)lParam;

  if(auto wid = dynamic_cast<Win32Widget *>(BasicWin32Widget::from_hwnd(hwnd))) {
    RECT rect, rect2;
    GetWindowRect(hwnd, &rect);
    UnionRect(&rect2, &rect, &info->inner);

    if(!EqualRect(&info->inner, &rect2)) {
      wid->force_redraw();
    }
  }

  return TRUE;
}

struct find_popup_info_t {
  HWND popup;

  DWORD thread_id;
};

static BOOL CALLBACK find_popup_callback(HWND hwnd, LPARAM lParam) {
  struct find_popup_info_t *info = (struct find_popup_info_t *)lParam;

  if(GetWindowThreadProcessId(hwnd, 0) != info->thread_id)
    return TRUE;

  if(!IsWindowVisible(hwnd))
    return TRUE;

  if(!IsWindowEnabled(hwnd))
    return TRUE;

  if(dynamic_cast<Win32Widget *>(BasicWin32Widget::from_hwnd(hwnd)))
    return TRUE;

  if(GetWindow(hwnd, GW_OWNER) != nullptr) {
    info->popup = hwnd;
    return FALSE;
  }

  return TRUE;
}

void BasicWin32Window::finish_apply_title(String displayed_title) {
  displayed_title+= String::FromChar(0);
  
  const wchar_t *str = (const wchar_t*)displayed_title.buffer();
  if(str)
    SetWindowTextW(_hwnd, str);
}

LRESULT BasicWin32Window::callback(UINT message, WPARAM wParam, LPARAM lParam) {
  LRESULT dwm_result = 0;

  bool calldwp = true;

  if(Win32Themes::DwmDefWindowProc)
    calldwp = !Win32Themes::DwmDefWindowProc(_hwnd, message, wParam, lParam, &dwm_result);

  switch(message) {
    case WM_NCACTIVATE: {
        _active = wParam;

        if( !Win32Themes::IsCompositionActive ||
            !Win32Themes::IsCompositionActive())
        {
          struct redraw_glass_info_t info;

          get_glassfree_rect(&info.inner);
          MapWindowPoints(_hwnd, nullptr, (POINT*)&info.inner, 2);

          EnumChildWindows(_hwnd, redraw_glass_callback, (LPARAM)&info);
        }

        if(_themed_frame) {
          int dpi = Win32HighDpi::get_dpi_for_window(_hwnd);
          HDC hdc = GetDC(_hwnd);

          RECT rect;
          GetClientRect(_hwnd, &rect);
          IntersectClipRect(hdc, 0, 0,
                            rect.right,
                            Win32HighDpi::get_system_metrics_for_dpi(SM_CYFRAME, dpi) 
                              + Win32HighDpi::get_system_metrics_for_dpi(SM_CYCAPTION, dpi));
          paint_themed(hdc);

          ReleaseDC(_hwnd, hdc);
        }
      } break;
  }

  if(!initializing()) {
    switch(message) {
//{ sizing & moving ...
      case WM_SIZING:
        on_sizing(wParam, (RECT *)lParam);
        break;

      case WM_MOVING:
        on_moving((RECT *)lParam);
        return 1;

      case WM_SIZE:
        invalidate_non_child();
        break;

      case WM_MOVE:
        on_move(lParam);
        break;

      case WM_ENTERSIZEMOVE: {
          RECT rect;
          GetWindowRect(_hwnd, &rect);
          last_moving_cx = rect.left + (rect.right - rect.left)/2;
          last_moving_cy = rect.top  + (rect.bottom - rect.top)/2;

          find_all_snappers();
        } break;

      case WM_EXITSIZEMOVE:
        all_snappers.clear();
        all_snapper_positions.length(0);
        break;
//} ... sizing & moving

      case WM_WINDOWPOSCHANGING: {
          WINDOWPOS *pos = (WINDOWPOS *)lParam;

          if(!during_pos_changing) {
            if(0 == (pos->flags & SWP_NOZORDER)) { // changing Z order...
              during_pos_changing = true; // prevent recursive call
              // place behind all windows with higher zorder_level

              HWND active_window    = GetActiveWindow();
              HWND own_popup_window = GetWindow(_hwnd, GW_ENABLEDPOPUP);
              if(own_popup_window == _hwnd)
                own_popup_window = nullptr;

              struct find_popup_info_t popup_info;
              popup_info.thread_id = GetWindowThreadProcessId(_hwnd, 0);
              popup_info.popup     = own_popup_window;
              EnumWindows(find_popup_callback, (LPARAM)&popup_info);

              BasicWin32Window *last_higher = nullptr;
              for(auto win : CommonDocumentWindow::All) {
                if(BasicWin32Window *next = dynamic_cast<BasicWin32Window*>(win)) {
                  if(next == this)
                    continue;
                  if(next->zorder_level() > zorder_level()) {
                    last_higher = next;
                    break;
                  }
                }
              }
//              BasicWin32Window *next = _next_window;
//              while(next != this) {
//                if(next->zorder_level() > zorder_level()) {
//                  last_higher = next;
//                  break;
//                }
//                next = next->_next_window;
//              }

              // get all windows from higher level, sorted from back to front
              static Array<BasicWin32Window *> all_higher;
              all_higher.length(0);
              if(last_higher) {
                HWND next_hwnd = GetNextWindow(last_higher->hwnd(), GW_HWNDNEXT);
                while(next_hwnd) {
                  auto wnd = dynamic_cast<BasicWin32Window*>(BasicWin32Widget::from_hwnd(next_hwnd));

                  if(wnd && wnd->zorder_level() > zorder_level())
                    last_higher = wnd;

                  next_hwnd = GetNextWindow(next_hwnd, GW_HWNDNEXT);
                }

                all_higher.add(last_higher);

                next_hwnd = GetNextWindow(last_higher->hwnd(), GW_HWNDPREV);
                while(next_hwnd) {
                  auto wnd = dynamic_cast<BasicWin32Window*>(BasicWin32Widget::from_hwnd(next_hwnd));

                  if(wnd && wnd->zorder_level() > zorder_level())
                    all_higher.add(wnd);

                  next_hwnd = GetNextWindow(next_hwnd, GW_HWNDPREV);
                }
              }

              HDWP hdwp = BeginDeferWindowPos(all_higher.length());

              /* Put the higher-level windows to the top again and place this window
                 behind. */
              if(all_higher.length() > 0) {
                if(active_window == _hwnd)
                  pos->hwndInsertAfter = all_higher[0]->hwnd();

                for(auto window : all_higher) {
                  if(!window->is_closed()) {
                    hdwp = tryDeferWindowPos(
                             hdwp,
                             window->hwnd(), HWND_TOP, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                  }
                }
              }

              if(popup_info.popup != 0) {
                //pmath_debug_print("[popup_info.popup = %p]\n", popup_info.popup);
                hdwp = tryDeferWindowPos(
                         hdwp,
                         popup_info.popup, HWND_TOP, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
              }

              EndDeferWindowPos(hdwp);

              during_pos_changing = false;
            }
          }
        } break;

      case WM_DPICHANGED: { // Windows 8.1 and above
        int dpiX = LOWORD(wParam);
        //int dpiY = HIWORD(wParam);
        // According to MSDN, "The values of the X-axis and the Y-axis are identical for Windows apps."
        
        // TODO: invalidate all dpi dependent fonts etc.
        
        RECT *suggested_rect = (RECT*)lParam;
        // TODO: update all snapped windows' positions...
        SetWindowPos(
          hwnd(),
          NULL,
          suggested_rect->left,
          suggested_rect->top,
          suggested_rect->right - suggested_rect->left,
          suggested_rect->bottom - suggested_rect->top,
          SWP_NOZORDER | SWP_NOACTIVATE);
      } break;
        
      case WM_THEMECHANGED:
      case WM_DWMCOMPOSITIONCHANGED: {
          if(Win32Themes::DwmEnableComposition)
            Win32Themes::DwmEnableComposition(1);

          on_theme_changed();
        } break;

      case WM_ERASEBKGND:
        return 1;

      case WM_PAINT: {
          if(_themed_frame) {
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
          if(wParam && _themed_frame) {
            NCCALCSIZE_PARAMS *calcsize_params = (NCCALCSIZE_PARAMS*)lParam;
            
            if(!is_left_right_bottom_frame_themed(this)) {
              Win32Themes::MARGINS margins = {0};
              get_nc_margins(&margins);
              
              calcsize_params->rgrc[0].left+= margins.cxLeftWidth;
              calcsize_params->rgrc[0].right-= margins.cxRightWidth;
              calcsize_params->rgrc[0].bottom-= margins.cyBottomHeight;
            }
            SetRectEmpty(&calcsize_params->rgrc[1]);
            SetRectEmpty(&calcsize_params->rgrc[2]);

            return WVR_VALIDRECTS | WVR_REDRAW;
          }
        } break;

      case WM_NCMOUSEMOVE: {
          if( wParam == HTMINBUTTON ||
              wParam == HTMAXBUTTON ||
              wParam == HTCLOSE     ||
              wParam == HTHELP)
          {
            if(!_mouse_over_caption_buttons)
              invalidate_caption();
            _mouse_over_caption_buttons = true;
          }
          else {
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
          if(_themed_frame && (wParam == HTCAPTION || wParam == HTSYSMENU)) {
            if(HMENU menu = GetSystemMenu(_hwnd, FALSE)) {
              POINT pt;
              pt.x = (short)LOWORD(lParam);
              pt.y = (short)HIWORD(lParam);
              
              int cmd;
              {
                Win32AutoMenuHook menu_hook(menu, nullptr, false, false);
                
                cmd = TrackPopupMenu(
                        menu,
                        GetSystemMetrics(SM_MENUDROPALIGNMENT) | TPM_RETURNCMD,
                        pt.x,
                        pt.y,
                        0,
                        _hwnd,
                        nullptr);
              }

              if(cmd)
                SendMessageW(_hwnd, WM_SYSCOMMAND, cmd, 0);
              return 0;
            }
          }
        } break;

      case WM_NCLBUTTONUP: {
          if(_themed_frame && wParam == HTSYSMENU) {
            if(HMENU menu = GetSystemMenu(_hwnd, FALSE)) {
              TPMPARAMS tpm;
              memset(&tpm, 0, sizeof(tpm));
              tpm.cbSize = sizeof(tpm);
              GetWindowRect(_hwnd, &tpm.rcExclude);

              tpm.rcExclude.left +=  GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
              tpm.rcExclude.top +=   GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
              tpm.rcExclude.right -= GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
              tpm.rcExclude.bottom = tpm.rcExclude.top + GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CXPADDEDBORDER);

              //POINT pt;
              //pt.x = (short)LOWORD(lParam);
              //pt.y = (short)HIWORD(lParam);

              int x;
              UINT align;
              if(GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0) {
                align = TPM_LEFTALIGN;
                x = tpm.rcExclude.left;
              }
              else {
                align = TPM_RIGHTALIGN;
                x = tpm.rcExclude.right;
              }

              int cmd;
              {
                Win32AutoMenuHook menu_hook(menu, nullptr, false, false);
                
                cmd = TrackPopupMenuEx(
                        menu,
                        align | TPM_RETURNCMD | TPM_NONOTIFY,
                        x,
                        tpm.rcExclude.bottom,
                        _hwnd,
                        &tpm);
              }
              
              if(cmd)
                SendMessageW(_hwnd, WM_SYSCOMMAND, cmd, 0);
              return 0;
            }
          }
        } break;

      case WM_SETICON:
      case WM_SETTEXT: {
          invalidate_caption();
        } break;

      default: break;
    }
  }

  if(calldwp)
    return BasicWin32Widget::callback(message, wParam, lParam);
  else
    return dwm_result;
}

HDWP BasicWin32Window::tryDeferWindowPos(
  HDWP hWinPosInfo,
  HWND hWnd,
  HWND hWndInsertAfter,
  int x,
  int y,
  int cx,
  int cy,
  UINT uFlags
) {
  if(hWinPosInfo) {
    return DeferWindowPos(
             hWinPosInfo,
             hWnd,
             hWndInsertAfter,
             x,
             y,
             cx,
             cy,
             uFlags);
  }
  else {
    SetWindowPos(
      hWnd,
      hWndInsertAfter,
      x,
      y,
      cx,
      cy,
      uFlags);

    return nullptr;
  }
}

HANDLE BasicWin32Window::composition_window_theme(int dpi) {
  if(HANDLE *h = composition_window_theme_for_dpi.search(dpi)) 
    return *h;
  
  if(Win32Themes::OpenThemeDataForDpi) {
    HANDLE h = Win32Themes::OpenThemeDataForDpi(nullptr, L"CompositedWindow::Window", (UINT)dpi);
    if(h) 
      composition_window_theme_for_dpi.set(dpi, h);
      
    return h;
  }
  
  if(Win32Themes::OpenThemeData) {
    HANDLE h = Win32Themes::OpenThemeData(nullptr, L"CompositedWindow::Window");
    if(h) 
      composition_window_theme_for_dpi.set(dpi, h);
      
    return h;
  }
  
  return nullptr;
}

//} ... class BasicWin32Window
