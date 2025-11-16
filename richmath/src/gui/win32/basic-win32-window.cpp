#include <gui/win32/basic-win32-window.h>

#include <graphics/canvas.h>
#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-touch.h>
#include <gui/win32/api/win32-version.h>
#include <gui/win32/ole/virtual-desktops.h>
#include <gui/win32/menus/win32-automenuhook.h>
#include <gui/win32/menus/win32-menu.h>
#include <gui/win32/win32-attached-popup-window.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/win32-tooltip-window.h>
#include <gui/win32/win32-widget.h>

#include <eval/application.h>
#include <resources.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cairo-win32.h>


using namespace richmath;

using SnapPosition = BasicWin32Window::SnapPosition;


#ifndef WM_DWMCOMPOSITIONCHANGED
#  define WM_DWMCOMPOSITIONCHANGED   0x031E
#endif

#ifndef GW_ENABLEDPOPUP
#  define GW_ENABLEDPOPUP  6
#endif

#ifndef WS_EX_NOREDIRECTIONBITMAP
#  define WS_EX_NOREDIRECTIONBITMAP   0x00200000L
#endif


#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif

class BasicWin32Window::Impl {
  public:
    Impl(BasicWin32Window &_self) : self(_self) {}
    
    bool is_left_right_bottom_frame_themed();
    
    int nc_hit_test_no_system_buttons(POINT mouse_screen);
    int nc_hit_test_system_buttons(POINT mouse_screen, int fallback = HTNOWHERE);
    int nc_extra_button_at_point(POINT mouse_screen);
    
    void on_windowposchanging(WINDOWPOS *pos);
    void on_windowposchanged(WINDOWPOS *pos);
    bool on_nclbuttonup(LRESULT *result, WPARAM wParam, POINT pos);
    
    void on_entersizemove();
    void on_exitsizemove();
    
    void clear_property_store() { clear_property_store(self._hwnd); }
    static void clear_property_store(HWND hwnd);
    
    void paint_themed(HDC hdc);
    
    void find_all_snappers();
    HDWP move_all_snappers(HDWP hdwp, const RECT &new_bounds);
    void snap_rect_or_pt(RECT *windowrect, POINT *pt); // pt may be nullptr, rect must not
    
  private:
    void paint_themed_system_buttons(HDC hdc_bitmap);
    void paint_themed_caption(HDC hdc_bitmap);
    
    class CaptionToolbarPainter {
      public:
        explicit CaptionToolbarPainter(BasicWin32Window &self, int h, int dpi);
        ~CaptionToolbarPainter();
        
        ControlState button_state(int i);
        void paint(HDC hdc, const RECT &rect, const Win32CaptionButton &button, ControlState state);
      
      private:
        void paint_separator(HDC hdc, const RECT &rect);
        void paint_button(HDC hdc, const RECT &rect, ControlState state);
        void paint_proxy_icon(HDC hdc, const RECT &rect);
        void paint_label(HDC hdc, RECT rect, const Win32CaptionButton &button, ControlState state);
        
      private:
        BasicWin32Window &self;
        HFONT             symbols_font;
        HANDLE            toolbar_theme;
        int               height;
        int               dpi;
    };
    
  private:
    BasicWin32Window &self;
};

namespace {
  static class StaticResources: public IVirtualDesktopNotification_10240, public IVirtualDesktopNotification_22000 {
    public:
      StaticResources();
      
      void add_basic_window();
      void remove_basic_window();
      
      AutoCairoSurface get_background_image();
      
      HANDLE composition_window_theme(int dpi);
      void clear_theme_data();
      
      DWORD register_notifications(IVirtualDesktopNotification *handler);
      void unregister_notifications(DWORD cookie);
    
    private:
      void need_virtual_desktop_service();
      
    private:
      Hashtable<String, AutoCairoSurface> background_image_cache;
      Hashtable<int, HANDLE>              composition_window_theme_for_dpi;
      int                                 window_count;
      DWORD                               _virtual_desktop_notification_cookie;
      
      ComBase<IVirtualDesktopNotificationService> virtual_desktop_service;
      
    public:
      STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override {  return 1; }
      STDMETHODIMP_(ULONG) Release(void) override { return 1; }
       
      //
      // IVirtualDesktopNotification_10240 members
      //
      STDMETHODIMP VirtualDesktopCreated(      IVirtualDesktop *pDesktop) override;
      STDMETHODIMP VirtualDesktopDestroyBegin( IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) override;
      STDMETHODIMP VirtualDesktopDestroyFailed(IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) override;
      STDMETHODIMP VirtualDesktopDestroyed(    IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) override;
      STDMETHODIMP ViewVirtualDesktopChanged(   IApplicationView *pView) override;
      STDMETHODIMP CurrentVirtualDesktopChanged(IVirtualDesktop *pDesktopOld, IVirtualDesktop *pDesktopNew) override;
  
      //
      // IVirtualDesktopNotification_22000 members
      //
      STDMETHODIMP VirtualDesktopCreated(         IObjectArray *p0, IVirtualDesktop *pDesktop) override;
      STDMETHODIMP VirtualDesktopDestroyBegin(    IObjectArray *p0, IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) override;
      STDMETHODIMP VirtualDesktopDestroyFailed(   IObjectArray *p0, IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) override;
      STDMETHODIMP VirtualDesktopDestroyed(       IObjectArray *p0, IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) override;
      STDMETHODIMP UnknownProc7(                  int p0) override;
      STDMETHODIMP VirtualDesktopMoved(           IObjectArray *p0, IVirtualDesktop *pDesktop, int nIndexFrom, int nIndexTo) override;
      STDMETHODIMP VirtualDesktopRenamed(         IVirtualDesktop *pDesktop, HSTRING chName) override;
      //STDMETHODIMP ViewVirtualDesktopChanged(     IApplicationView *pView) override; // same as in 10240
      STDMETHODIMP CurrentVirtualDesktopChanged(  IObjectArray *p0, IVirtualDesktop *pDesktopOld, IVirtualDesktop *pDesktopNew) override;
      STDMETHODIMP VirtualDesktopWallpaperChanged(IVirtualDesktop *pDesktop, HSTRING *chPath) override;
  } static_resources;
  
  class WindowMagnetics {
    public:
      WindowMagnetics(HWND _src, Hashset<HWND> &_dont_snap);
    
    public:
      const RECT  *orig_rect;  // must not be null
      const POINT *orig_pt;    // may be null
      int dx;
      int dy;
    
    private:
      HWND src;
      Hashset<HWND> &dont_snap;
      int max_dx;
      int max_dy;
    
    public:
      void snap_all_monitors();
      void snap_all_windows();
      
    public:
      static void get_snap_margins(HWND hwnd, Win32Themes::MARGINS *margins);
      static void adjust_snap_rect(HWND hwnd, RECT *rect);
      static void get_snap_rect(HWND hwnd, RECT *rect);
    
    private:
      bool snap_inside(const RECT &outer);
  };
  
  class WindowMagnetCollector {
    public:
      WindowMagnetCollector(Hashset<HWND> &_snappers, Array<SnapPosition> &_snapper_positions);
      int min_level;
    
    private:
      HWND dst;
      RECT dst_rect;
      Hashset<HWND> &snappers;
      Array<SnapPosition> &snapper_positions;
    
    public:
      void reset_dst(HWND _dst);
      void visit_all_windows();
  };
}

// Microsoft Calculator, Settings Pannel: 0xE6E6E6 (light mode) and(?) 0x1F1F1F (dark mode)
static Color CustomTitlebarColorizationLight = Color::from_rgb24(0xE6E6E6);//Color::None;//
static Color CustomTitlebarColorizationDark  = Color::from_rgb24(0x1F1F1F);//Color::None;//

class richmath::Win32BlurBehindWindow: public BasicWin32Widget {
    using base = BasicWin32Widget;
  public:
    Win32BlurBehindWindow(BasicWin32Window *owner);
    virtual ~Win32BlurBehindWindow();
    
    void hide();
    void show();
    void show(const RECT &owner_rect);
    void show_if_owner_visible_on_screen();
    
    void colorize(bool active, bool dark_mode);
    
    static RECT blur_bounds(RECT window_rect, const Win32Themes::MARGINS &margins);
  
  public:
    static bool suppress_slow_acrylic_blur;
  
  protected:
    virtual void after_construction() override;
    bool enable_blur(COLORREF abgr);
    
    virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override;
    void on_windowposchanging(WINDOWPOS *lParam);
    
  private:
    BasicWin32Window *_owner;
};

static bool is_window_cloaked(HWND hwnd);
static bool is_window_visible_on_screen(HWND hwnd);

static POINT point_from_lparam(LPARAM lParam);
static bool touching_rectangles(RECT *touchRegion, const RECT &rect1, const RECT &rect2);
static POINT get_rect_center(const RECT &rect);
static Point to_relative_point(const RECT &frame, const POINT &point);
static POINT to_absolute_point(const RECT &frame, const Point &point);

static void get_nc_margins(HWND hwnd, Win32Themes::MARGINS *margins, int dpi);
static HBRUSH create_solid_brush_with_alpha(Color color, int alpha = 255);

template <typename TLambda>
void enum_thread_windows(TLambda callback) {
  struct Callback {
    static BOOL WINAPI static_callback(HWND hwnd, LPARAM lParam) {
      (*(TLambda*)lParam)(hwnd); 
      return TRUE;
    }
  };
  EnumThreadWindows(
    GetCurrentThreadId(), 
    Callback::static_callback, 
    (LPARAM)&callback);
}

template <typename TLambda>
void enum_display_monitors(HDC hdc, LPCRECT lprcClip, TLambda callback) {
  struct Callback {
    static BOOL WINAPI static_callback(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM lParam) {
      (*(TLambda*)lParam)(hMonitor, hdcMonitor, lprcMonitor); 
      return TRUE; 
    }
  };
  EnumDisplayMonitors(
    hdc, 
    lprcClip,
    Callback::static_callback,
    (LPARAM)&callback);
}

static bool use_custom_system_buttons() {
  return Win32Themes::use_win10_transparency();
}

static bool hit_test_is_system_button(int ht) {
  return ht == HTMINBUTTON || ht == HTMAXBUTTON || ht == HTCLOSE || ht == HTHELP;
}

static HDWP tryDeferWindowPos(
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

static HDWP tryDeferWindowPos(
  HDWP hWinPosInfo,
  HWND hWnd,
  HWND hWndInsertAfter,
  const RECT &rect,
  UINT uFlags
) {
  return tryDeferWindowPos(
           hWinPosInfo,
           hWnd, 
           hWndInsertAfter, 
           rect.left, 
           rect.top, 
           rect.right - rect.left,
           rect.bottom - rect.top,
           uFlags);
}

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
    nullptr),
  min_client_height(0),
  max_client_height(-1),
  min_client_width(0),
  max_client_width(-1),
  _zorder_level(0),
  background_image(static_resources.get_background_image()),
  _blur_behind_window(nullptr),
  _active(false),
  _hit_test_mouse_over(HTNOWHERE),
  _hit_test_mouse_down(HTNOWHERE),
  _hit_test_extra_button(-1),
  _glass_enabled(false),
  _themed_frame(false),
  _use_dark_mode(false),
  snap_correction_x(0),
  snap_correction_y(0),
  last_moving_cx(0),
  last_moving_cy(0)
{
  memset(&_extra_glass, 0, sizeof(_extra_glass));

  static_resources.add_basic_window();
  
  if(use_custom_system_buttons()) // && Win32Themes::use_win10_transparency()
    _blur_behind_window = new Win32BlurBehindWindow(this);
}

void BasicWin32Window::after_construction() {
  BasicWin32Widget::after_construction();
  
  if(_blur_behind_window)  _blur_behind_window->init();
}

BasicWin32Window::~BasicWin32Window() {
  static_resources.remove_basic_window();
  
  if(_blur_behind_window) {
    _blur_behind_window->safe_destroy(); 
    _blur_behind_window = nullptr;
  }
  
  Impl(*this).clear_property_store();
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
    
    if(!Impl(*this).is_left_right_bottom_frame_themed()) {
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

void BasicWin32Window::get_snap_alignment(bool *right, bool *bottom) {
  *right = false;
  *bottom = false;

  RECT dst_rect;
  WindowMagnetics::get_snap_rect(_hwnd, &dst_rect);
  bool align_left   = false;
  bool align_right  = false;
  bool align_top    = false;
  bool align_bottom = false;
  
  enum_thread_windows([&](HWND hwnd) {
    if(hwnd == _hwnd || !is_window_visible_on_screen(hwnd))
      return;
      
    if(auto widget = BasicWin32Widget::from_hwnd(hwnd)) {
      if(dynamic_cast<Win32BlurBehindWindow*>(widget))
        return;
      if(dynamic_cast<Win32AttachedPopupWindow*>(widget))
        return;
    }
    
    RECT rect;
    WindowMagnetics::get_snap_rect(hwnd, &rect);

    if(dst_rect.left == rect.right)
      align_left = true;

    if(dst_rect.right == rect.left)
      align_right = true;

    if(dst_rect.top == rect.bottom)
      align_top = true;

    if(dst_rect.bottom == rect.top)
      align_bottom = true;
  });
  
  *right  = align_right  && !align_left;
  *bottom = align_bottom && !align_top;

//  if( !align_left &&
//      !align_right &&
//      !align_top &&
//      !align_bottom)
//  {
//    MONITORINFO monitor_info;
//    memset(&monitor_info, 0, sizeof(monitor_info));
//    monitor_info.cbSize = sizeof(monitor_info);
//
//    HMONITOR hmon = MonitorFromWindow(_hwnd, MONITOR_DEFAULTTONEAREST);
//    if(GetMonitorInfo(hmon, &monitor_info)) {
//      *right  = (dst_rect.left + dst_rect.right) > (monitor_info.rcWork.left + monitor_info.rcWork.right);
//      *bottom = (dst_rect.top + dst_rect.bottom) > (monitor_info.rcWork.top  + monitor_info.rcWork.bottom);
//    }
//  }
}

static RECT sizing_initial_rect {};

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
    
    case 9: // drag away from window edge e.g from maximized state, change all edges
    default: 
      return;
  }
  
  
  if(GetKeyState(VK_MENU) & ~1) {
    change_left = change_right = change_left || change_right;
    change_top = change_bottom = change_top || change_bottom;
  }
  
  //POINT center { lParam->left + (lParam->right - lParam->left)/2, lParam->top + (lParam->bottom - lParam->top)/2 };
  POINT center { 
    sizing_initial_rect.left + (sizing_initial_rect.right - sizing_initial_rect.left) / 2, 
    sizing_initial_rect.top + (sizing_initial_rect.bottom - sizing_initial_rect.top) / 2 };
  
  bool minmax = false;
  if(lParam->bottom - lParam->top < minh) {
    if(change_top && change_bottom) {
      lParam->top = center.y - minh/2;
      lParam->bottom = lParam->top + minh;
    }
    else if(change_top) {
      lParam->bottom = sizing_initial_rect.bottom;
      lParam->top = lParam->bottom - minh;
    }
    else if(change_bottom) {
      lParam->top = sizing_initial_rect.top;
      lParam->bottom = lParam->top + minh;
    }
    minmax = true;
  }

  if(lParam->bottom - lParam->top > maxh && max_client_height > 0) {
    if(change_top && change_bottom) {
      lParam->top = center.y - maxh/2;
      lParam->bottom = lParam->top + maxh;
    }
    else if(change_top) {
      lParam->bottom = sizing_initial_rect.bottom;
      lParam->top = lParam->bottom - maxh;
    }
    else if(change_bottom) {
      lParam->top = sizing_initial_rect.top;
      lParam->bottom = lParam->top + maxh;
    }
    minmax = true;
  }

  if(lParam->right - lParam->left < minw) {
    if(change_left && change_right) {
      lParam->left = center.x - minw/2;
      lParam->right = lParam->left + minw;
    }
    else if(change_left) {
      lParam->right = sizing_initial_rect.right;
      lParam->left = lParam->right - minw;
    }
    else if(change_right) {
      lParam->left = sizing_initial_rect.left;
      lParam->right = lParam->left + minw;
    }
    minmax = true;
  }

  if(lParam->right - lParam->left > maxw && max_client_width > 0) {
    if(change_left && change_right) {
      lParam->left = center.x - maxw/2;
      lParam->right = lParam->left + maxw;
    }
    else if(change_left) {
      lParam->right = sizing_initial_rect.right;
      lParam->left = lParam->right - maxw;
    }
    else if(change_right) {
      lParam->left = sizing_initial_rect.left;
      lParam->right = lParam->left + maxw;
    }
    minmax = true;
  }

  if(minmax)
    return;

  POINT pt = {0, 0};
//  RECT snapping_rect = *lParam;
//  adjust_snap_rect(_hwnd, &snapping_rect);
  Win32Themes::MARGINS snap_margins;
  WindowMagnetics::get_snap_margins(_hwnd, &snap_margins);
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
  Impl(*this).snap_rect_or_pt(&snapping_rect, &pt);
  
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
  
  if(!change_left)   lParam->left   = sizing_initial_rect.left;
  if(!change_right)  lParam->right  = sizing_initial_rect.right;
  if(!change_top)    lParam->top    = sizing_initial_rect.top;
  if(!change_bottom) lParam->bottom = sizing_initial_rect.bottom;
}

void BasicWin32Window::on_moving(RECT *lParam) {
  RECT rect;
  GetWindowRect(_hwnd, &rect);
  
  Win32Themes::MARGINS snap_margins;
  WindowMagnetics::get_snap_margins(_hwnd, &snap_margins);
  
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
  
  Impl(*this).snap_rect_or_pt(&snapping_rect, nullptr);
  lParam->left   = snapping_rect.left   + snap_margins.cxLeftWidth;
  lParam->top    = snapping_rect.top    + snap_margins.cyTopHeight;
  lParam->right  = snapping_rect.right  - snap_margins.cxRightWidth;
  lParam->bottom = snapping_rect.bottom - snap_margins.cyBottomHeight;

  int num_windows = all_snappers.size();
  HDWP hdwp = BeginDeferWindowPos(num_windows);

  int cx = rect.left + (rect.right - rect.left)/2;
  int cy = rect.top  + (rect.bottom - rect.top)/2;
  if( cx != last_moving_cx || cy != last_moving_cy) {
    hdwp = Impl(*this).move_all_snappers(hdwp, rect);
  }
  
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

  int num_windows = 1 + all_snappers.size();
  if(_blur_behind_window)
    ++num_windows;
  
  HDWP hdwp = BeginDeferWindowPos(num_windows);

  hdwp = tryDeferWindowPos(
           hdwp,
           _hwnd,
           nullptr,
           rect.left,
           rect.top,
           1,
           1,
           SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

  if(_blur_behind_window && is_window_visible_on_screen(_blur_behind_window->hwnd())) {
    Win32Themes::MARGINS margins = {};
    get_nc_margins(&margins);
    
    hdwp = tryDeferWindowPos(
             hdwp,
             _blur_behind_window->hwnd(),
             nullptr,
             Win32BlurBehindWindow::blur_bounds(rect, margins),
             SWP_NOACTIVATE | SWP_NOZORDER);
  }
  
  int cx = rect.left + (rect.right - rect.left)/2;
  int cy = rect.top  + (rect.bottom - rect.top)/2;
  if( cx != last_moving_cx || cy != last_moving_cy)
    hdwp = Impl(*this).move_all_snappers(hdwp, rect);

  EndDeferWindowPos(hdwp);

  last_moving_cx = cx;
  last_moving_cy = cy;
}

//} ... snapping windows & alignment

void BasicWin32Window::on_theme_changed() {
  _glass_enabled = false;
  _themed_frame = false;
  
  if(WS_CAPTION & GetWindowLongW(_hwnd, GWL_STYLE)) {
    if(Win32Themes::IsCompositionActive && Win32Themes::IsCompositionActive()) {
      _glass_enabled = true;
  //    if(Win32Themes::use_win10_transparency())
  //      _themed_frame = true;
  //    else
        _themed_frame = !(WS_EX_TOOLWINDOW & GetWindowLongW(_hwnd, GWL_EXSTYLE));
    }
    else if(Win32Themes::IsThemeActive && Win32Themes::IsThemeActive()) {
      _glass_enabled = Win32Themes::current_theme_is_aero();
    }
  }
  
  static_resources.clear_theme_data();
  
  if(_glass_enabled) {
    if(Win32Themes::use_win10_transparency()) {
      if(Win32Themes::DwmEnableBlurBehindWindow) {
        Win32Themes::DWM_BLURBEHIND bb = {};
        bb.dwFlags = Win32Themes::DWM_BB_ENABLE;
        bb.fEnable = TRUE;
        bb.dwFlags |= Win32Themes::DWM_BB_BLURREGION;
        bb.hRgnBlur = CreateRectRgn(0, 0, 1, 1);
        HRreport(Win32Themes::DwmEnableBlurBehindWindow(_hwnd, &bb));
      }
      
      if(!_blur_behind_window) {
        _blur_behind_window = new Win32BlurBehindWindow(this);
        _blur_behind_window->init();
      }
      
      _blur_behind_window->colorize(_active, _use_dark_mode);
      
      if(is_window_visible_on_screen(_hwnd))
        _blur_behind_window->show();
      else
        _blur_behind_window->hide();
    }
    else if(Win32Version::is_windows_10_or_newer()) {
      if(Win32Themes::DwmEnableBlurBehindWindow) {
        Win32Themes::DWM_BLURBEHIND bb = {};
        bb.dwFlags = Win32Themes::DWM_BB_ENABLE | Win32Themes::DWM_BB_BLURREGION;
        bb.fEnable = TRUE;
        HRreport(Win32Themes::DwmEnableBlurBehindWindow(_hwnd, &bb));
      }
      
      if(_blur_behind_window)
        _blur_behind_window->hide();
    }
  }
  else {
    if(Win32Version::is_windows_10_or_newer()) {
      // FIXME: WS_BORDER (WindorFrame->"ThinFrame") is still drawn white instead of gray by Windows
      
      if(Win32Themes::DwmEnableBlurBehindWindow) {
        Win32Themes::DWM_BLURBEHIND bb = {};
        bb.dwFlags = Win32Themes::DWM_BB_ENABLE;
        bb.fEnable = FALSE;
        HRreport(Win32Themes::DwmEnableBlurBehindWindow(_hwnd, &bb));
      }
    }
    
    if(_blur_behind_window) {
      delete _blur_behind_window;
      _blur_behind_window = nullptr;
    }
  }
    
  extend_glass(_extra_glass);

  SetWindowPos(_hwnd, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void BasicWin32Window::use_dark_mode(bool dark_mode) {
  if(_use_dark_mode == dark_mode)
    return;
  
  // TODO: when was dark mode and DarkMode_Explorer theme introduced? Maybe Windows 10 (1809) build 17763 ?
  
  _use_dark_mode = dark_mode;
  if(Win32Themes::SetWindowTheme) {
    if(_use_dark_mode)
      Win32Themes::SetWindowTheme(_hwnd, L"DarkMode_Explorer", nullptr);
    else
      Win32Themes::SetWindowTheme(_hwnd, L"Explorer", nullptr);
  }
  
  // Only needed for palettes, because their titlebar is drawn by Windows and not by us:
  Win32Themes::try_set_dark_mode_frame(_hwnd, dark_mode);
    
  if(_blur_behind_window)
    _blur_behind_window->colorize(_active, _use_dark_mode);
  
  invalidate_caption();
}

static void get_system_button_bounds(HWND hwnd, RECT *minimize, RECT *maximize, RECT *close) {
  TITLEBARINFOEX tbi;
  memset(&tbi, 0, sizeof(tbi));
  tbi.cbSize = sizeof(tbi);

  SendMessageW(hwnd, WM_GETTITLEBARINFOEX, 0, (LPARAM)&tbi);
  
  *minimize = tbi.rgrect[2];
  *maximize = tbi.rgrect[3];
  *close    = tbi.rgrect[5];
  
  if(IsRectEmpty(close)) {
    /* Workaround for Windows 10 bug (probably since build 1809): 
        Sending WM_GETTITLEBARINFOEX returns nothing when two monitors are used, 
        but works fine for only one monitor.
     */
    //pmath_debug_print("[get_system_button_bounds: win 10 bug]\n");
    
    if(Win32Themes::DwmGetWindowAttribute) {
      RECT rect = {};
      if(HRbool(Win32Themes::DwmGetWindowAttribute(hwnd, Win32Themes::DWMWA_CAPTION_BUTTON_BOUNDS, &rect, sizeof(RECT)))) {
        DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
        
        // DWMWA_CAPTION_BUTTON_BOUNDS are in window coordinates, not client coordinates.
        // TODO: correctly adjust the rectangle to tightly fit the button bounds. 
        // The value 6 does not seem exactly correct with 150 DPI
        rect.right -= 6;
        
        minimize->top    = maximize->top    = close->top    = rect.top;
        minimize->bottom = maximize->bottom = close->bottom = rect.bottom;
        
        if(0 == (style & (WS_MINIMIZEBOX | WS_MAXIMIZEBOX))) {
          minimize->left = minimize->right = maximize->left = maximize->right = close->left = rect.left;
        }
        else {
          minimize->left = rect.left;
          minimize->right = maximize->left = rect.left + (rect.right - rect.left) / 3;
          maximize->right = close->left = maximize->left + (rect.right - rect.left) / 3;
        }
        close->right = rect.right;
        
        return;
      }
    }
  }
  else if(Win32Version::is_windows_10_or_newer() /*use_custom_system_buttons()*/) {
    int dpi = Win32HighDpi::get_dpi_for_window(hwnd);
    Win32Themes::MARGINS margins;
    get_nc_margins(hwnd, &margins, dpi);
    if(WS_EX_TOOLWINDOW & GetWindowLongW(hwnd, GWL_EXSTYLE)) {
      close->bottom = close->top + margins.cyTopHeight - Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
      close->top += Win32HighDpi::get_system_metrics_for_dpi(SM_CYFIXEDFRAME, dpi) +
                    Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
      close->right-= 1;
    }
    else {
      DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
      int pad = Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
      if(WS_MAXIMIZE & style) {
        int top_frame = Win32HighDpi::get_system_metrics_for_dpi(SM_CYFRAME, dpi) + pad;
        
        minimize->top-= top_frame;
        maximize->top-= top_frame;
        close->top   -= top_frame;
      }
      
      minimize->bottom = maximize->bottom = close->bottom = close->top + margins.cyTopHeight - 2;
      
      if(WS_MAXIMIZE & style) {
        minimize->bottom -= pad;
        maximize->bottom -= pad;
        close->bottom    -= pad;
      }
    }
  }
  
  MapWindowPoints(nullptr, hwnd, (POINT*)minimize, 2);
  MapWindowPoints(nullptr, hwnd, (POINT*)maximize, 2);
  MapWindowPoints(nullptr, hwnd, (POINT*)close, 2);
}

static void get_system_button_bounds(HWND hwnd, RECT *rect) {
  TITLEBARINFOEX tbi;
  memset(&tbi, 0, sizeof(tbi));
  tbi.cbSize = sizeof(tbi);

  SendMessageW(hwnd, WM_GETTITLEBARINFOEX, 0, (LPARAM)&tbi);

  memset(rect, 0, sizeof(RECT));
  for(int i = 2; i <= 5; ++i)
    UnionRect(rect, rect, &tbi.rgrect[i]);
  
  if(rect->left == 0 && rect->right == 0 && rect->top == 0 && rect->bottom == 0) {
    /* Workaround for Windows 10 bug (probably since build 1809): 
        Sending WM_GETTITLEBARINFOEX returns nothing when two monitors are used, 
        but works fine for only one monitor.
     */
    //pmath_debug_print("[get_system_button_bounds: win 10 bug]\n");
    
    if(Win32Themes::DwmGetWindowAttribute) {
      if(HRbool(Win32Themes::DwmGetWindowAttribute(hwnd, Win32Themes::DWMWA_CAPTION_BUTTON_BOUNDS, rect, sizeof(RECT)))) {
        // DWMWA_CAPTION_BUTTON_BOUNDS are in window coordinates, not client coordinates.
        // TODO: correctly adjust the rectangle to tightly fit the button bounds. 
        // The value 6 does not seem exactly correct with 150 DPI
        rect->right -= 6;
        return;
      }
    }
  }
  else if(Win32Version::is_windows_10_or_newer() /*use_custom_system_buttons()*/) {
    int dpi = Win32HighDpi::get_dpi_for_window(hwnd);
    Win32Themes::MARGINS margins;
    get_nc_margins(hwnd, &margins, dpi);
    
    if(WS_EX_TOOLWINDOW & GetWindowLongW(hwnd, GWL_EXSTYLE)) {
      rect->bottom = rect->top + margins.cyTopHeight - Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
      rect->top += Win32HighDpi::get_system_metrics_for_dpi(SM_CYFIXEDFRAME, dpi) +
                   Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
      rect->right-= 1;
    }
    else {
      DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
      int pad = Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
      if(WS_MAXIMIZE & style) {
        int top_frame = Win32HighDpi::get_system_metrics_for_dpi(SM_CYFRAME, dpi) + pad;
        
        rect->top-= top_frame;
      }
      
      rect->bottom = rect->top + margins.cyTopHeight - 2;
      
      if(WS_MAXIMIZE & style) {
        rect->bottom -= pad;
      }
    }
  }
  
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
  
  //int caption_h = Win32HighDpi::get_system_metrics_for_dpi(SM_CYCAPTION, dpi);
  int icon_w    = Win32HighDpi::get_system_metrics_for_dpi(SM_CXSMICON, dpi);
  int icon_h    = Win32HighDpi::get_system_metrics_for_dpi(SM_CYSMICON, dpi);
  
  int visible_top = -neg_margins.top - invisible_top;
  if(style & WS_MAXIMIZE) {
    if(!Win32Version::is_windows_10_or_newer() || Win32Themes::use_win10_transparency()) {
      visible_top+= Win32HighDpi::get_system_metrics_for_dpi(SM_CYFRAME, dpi) + 
                    Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
    }
  }
  
//  pmath_debug_print("[invisible_top = %d, visible_top = %d]\n", invisible_top, visible_top);
  
  rect->top+= invisible_top + (visible_top - icon_h) / 2;
  rect->bottom = rect->top + icon_h;
  
  if(ex_style & WS_EX_LAYOUTRTL) {
    rect->right-= -neg_margins.left + 1;
    if(Win32Version::is_windows_10_or_newer() && (style & WS_MAXIMIZE) == 0) {
      /* In Windows 10, the left/right/bottom frame is invisible, so the icon indents more when not maximized. */
      rect->right-= -neg_margins.left;
    }
    rect->left = rect->right - icon_w;
  }
  else {
    rect->left += -neg_margins.left + 1;
    if(Win32Version::is_windows_10_or_newer() && (style & WS_MAXIMIZE) == 0) {
      /* In Windows 10, the left/right/bottom frame is invisible, so the icon indents more when not maximized. */
      rect->left+= -neg_margins.left;
    }
    rect->right = rect->left + icon_w;
  }
  
  MapWindowPoints(nullptr, hwnd, (POINT*)rect, 2);
}

COLORREF BasicWin32Window::title_font_color(bool glass_enabled, int dpi, bool active, bool dark_mode) {
  if(!glass_enabled) {
    //return GetSysColor(active ? COLOR_BTNTEXT : COLOR_GRAYTEXT);
    return GetSysColor(COLOR_BTNTEXT);
  }
  
  if(HANDLE theme = composition_window_theme(dpi)) {
//    if(Win32Version::is_windows_10_or_newer() && Win32Themes::DwmGetColorizationParameters) {
//      Win32Themes::DWM_COLORIZATION_PARAMS params = {0};
//      Win32Themes::DwmGetColorizationParameters(&params);
//      
//      // TODO: only use text-on-accent-backgound color if HKCU\SOFTWARE\Microsoft\Windows\DWM\ColorPrevalence = 1
//      dtt_opts.crText = Win32Themes::get_window_title_text_color(&params, _active);
//    }
    if(Win32Version::is_windows_10_or_newer()) {
      if(active) {
        if(Win32Themes::use_win10_transparency()) {
          if(Color custom_color = dark_mode ? CustomTitlebarColorizationDark : CustomTitlebarColorizationLight) {
            if(custom_color.is_light())
              return 0x000000;
            else
              return 0xFFFFFF;
          }
        }
        
        Win32Themes::ColorizationInfo colorization;
        if(Win32Themes::try_read_win10_colorization(&colorization)) {
          if(colorization.has_accent_color_in_active_titlebar)
            return colorization.text_on_accent_color;
        }

        if(dark_mode)
          return 0xFFFFFF; // else COLOR_CAPTIONTEXT below
      }
      else {
        /* On Windows 10, Inactive titlebar text color seems to be 0x999999u (light mode) or 0xAAAAAA (dark mode).
           What does COLOR_INACTIVECAPTIONTEXT give? Should we hard-code 0x999999u?
           
           https://github.com/res2k/Windows10Colors/blob/master/Windows10Colors/Windows10Colors.cpp#L544
           blends 40% COLOR_INACTIVECAPTIONTEXT with 60% COLOR_INACTIVECAPTION
           Note that 0.4 * 0x00 + 0.6 * 0xFF = 0x99.
         */
        return dark_mode ? 0xAAAAAAu : 0x999999u;
        // Microsoft Calculator uses 0x7A7A7A when inactive (on 0xE6E6E6 background, light mode)
        // Microsoft Edge       uses 0x7A7A7A when inactive (on 0xCCCCCC background, light mode)
      }
    }
    
    if(Win32Themes::IsCompositionActive && Win32Themes::IsCompositionActive())
      return Win32Themes::GetThemeSysColor(theme, COLOR_CAPTIONTEXT);
    else
      return Win32Themes::GetThemeSysColor(theme, active ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT);
  }
  
  return GetSysColor(active ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT);
}

void BasicWin32Window::lost_blur_behind_window(Win32BlurBehindWindow *bb) {
  if(bb == _blur_behind_window) {
    _blur_behind_window = nullptr;
    
    if(Win32Themes::DwmEnableBlurBehindWindow) {
      Win32Themes::DWM_BLURBEHIND bb = {};
      bb.dwFlags = Win32Themes::DWM_BB_ENABLE | Win32Themes::DWM_BB_BLURREGION;
      bb.fEnable = TRUE;
      HRreport(Win32Themes::DwmEnableBlurBehindWindow(_hwnd, &bb));
    }
  }
}

void BasicWin32Window::extend_glass(const Win32Themes::MARGINS &margins) {
  if(&_extra_glass != &margins) {
    _extra_glass = margins;
  }

  if(Win32Themes::DwmExtendFrameIntoClientArea) {
    if( margins.cxLeftWidth    == -1 &&
        margins.cxRightWidth   == -1 &&
        margins.cyTopHeight    == -1 &&
        margins.cyBottomHeight == -1)
    {
      Win32Themes::DwmExtendFrameIntoClientArea(_hwnd, &margins);
    }
    else {
      Win32Themes::MARGINS nc;
      memset(&nc, 0, sizeof(nc));
      if(_themed_frame)
        get_nc_margins(&nc);
      
      if(!Impl(*this).is_left_right_bottom_frame_themed()) {
        nc.cxLeftWidth = 0;
        nc.cxRightWidth = 0;
        nc.cyBottomHeight = 0;
      }
      
      nc.cxLeftWidth += margins.cxLeftWidth;
      nc.cxRightWidth += margins.cxRightWidth;
      if(use_custom_system_buttons()) {
        // Windows 10, no accent color in titlebars:
        // MARGINS != {0,0,0,0} will cause white border on all sides.
        // See https://github.com/microsoft/terminal/issues/3425#issuecomment-558943616
        //nc.cyTopHeight = 1;
        nc.cyTopHeight = 0;
      }
      else {
        nc.cyTopHeight += margins.cyTopHeight;
        nc.cyBottomHeight += margins.cyBottomHeight;
      }
      
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

void BasicWin32Window::paint_background_at(Canvas &canvas, HWND child, bool wallpaper_only) {
  RECT rect, child_rect;
  Win32Themes::MARGINS margins = {0};

  GetWindowRect(child, &child_rect);
  GetWindowRect(_hwnd, &rect);
  get_nc_margins(&margins);

  paint_background_at(
    canvas,
    POINT { child_rect.left - rect.left - margins.cxLeftWidth, child_rect.top  - rect.top },
    wallpaper_only);
}

static void add_rect(Canvas &canvas, const RECT &rect) {
  canvas.move_to(rect.left, rect.top);
  canvas.line_to(rect.left, rect.bottom);
  canvas.line_to(rect.right, rect.bottom);
  canvas.line_to(rect.right, rect.top);
  canvas.close_path();
}

void BasicWin32Window::paint_background_at(Canvas &canvas, POINT pos, bool wallpaper_only) {
  CanvasAutoSave saved(canvas);
  
  canvas.reset_matrix();
  cairo_reset_clip(canvas.cairo());

  RECT window_rect;
  GetWindowRect(_hwnd, &window_rect);
  MapWindowPoints(nullptr, _hwnd, (POINT *)&window_rect, 2);

  canvas.translate(-pos.x, -pos.y);

  RECT glassfree;
  get_glassfree_rect(&glassfree);
//    glassfree.left   -= window_rect.left;
//    glassfree.right  -= window_rect.left;
  glassfree.top    -= window_rect.top;
  glassfree.bottom -= window_rect.top;
  
  Color bg_color = Color::None;
  if(!wallpaper_only) {
    if( !Win32Themes::IsCompositionActive || !Win32Themes::IsCompositionActive()) {
      if(glass_enabled()) {
        if(_active)
          bg_color = Color::from_bgr24(GetSysColor(COLOR_GRADIENTACTIVECAPTION));
        else
          bg_color = Color::from_bgr24(GetSysColor(COLOR_GRADIENTINACTIVECAPTION));

        canvas.set_color(bg_color);
        canvas.paint();
      }
      else {
        bg_color = Color::from_bgr24(GetSysColor(COLOR_BTNFACE));
        
        canvas.set_color(bg_color);
        canvas.paint();
      }
    }
    else {
      cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_CLEAR);
      canvas.set_color(Color::Black, 0.0);
      canvas.paint();
      cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_OVER);
    }

    if(!IsRectEmpty(&glassfree)) {
      Color color = Win32ControlPainter::win32_painter.win32_button_face_color(_use_dark_mode);
      
      add_rect(canvas, glassfree);
      
      canvas.set_color(color);
      canvas.fill();
    }
  }
  
  paint_background(canvas);
  
  if(_themed_frame) {
    LONG style_ex = GetWindowLongW(_hwnd, GWL_EXSTYLE);
    RECT client_rect;
    GetClientRect(_hwnd, &client_rect);
    get_glassfree_rect(&glassfree);
    
    if(style_ex & WS_EX_LAYOUTRTL) {
      /* RTL layout: Windows client coordinates are right-to-left (0,0) is at top right,
         but Cairo coordinates are left-to-right.
         Temporarily make Cairo use Windows client coordinates.
       */
      canvas.translate(
        client_rect.right - client_rect.left,
        0);
      canvas.scale(-1, 1);
    }

    cairo_reset_clip(canvas.cairo());

    int buttonradius;
    int frameradius;
    float buttons_alpha = 1.0f;

    bool is_win8_or_newer = Win32Version::is_windows_8_or_newer();
    if(hit_test_is_system_button(_hit_test_mouse_over)) {
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

    if(is_win8_or_newer) {
      buttonradius = 1;
      frameradius  = 1;
      
      if(Win32Version::is_windows_11_or_newer() && Win32Themes::DwmGetWindowAttribute) {
        DWORD corner_pref = Win32Themes::DWMWCP_DEFAULT;
        
        if(!HRbool(Win32Themes::DwmGetWindowAttribute(_hwnd, Win32Themes::DWMWA_WINDOW_CORNER_PREFERENCE, &corner_pref, sizeof(corner_pref))))
          corner_pref = Win32Themes::DWMWCP_DEFAULT;
        
        if(corner_pref == Win32Themes::DWMWCP_DEFAULT) {
          if(style_ex & WS_EX_TOOLWINDOW) {
            corner_pref = Win32Themes::DWMWCP_ROUNDSMALL;
          }
          else {
            corner_pref = Win32Themes::DWMWCP_ROUND;
          }
        }
        
        switch(corner_pref) {
          case Win32Themes::DWMWCP_ROUND:      frameradius = 8; break;
          case Win32Themes::DWMWCP_ROUNDSMALL: frameradius = 4; break;
          default:
          case Win32Themes::DWMWCP_DONOTROUND: frameradius = 1; break;
        }
        
        if(_blur_behind_window && Win32Themes::DwmSetWindowAttribute) {
          Win32Themes::DwmSetWindowAttribute(_blur_behind_window->hwnd(), Win32Themes::DWMWA_WINDOW_CORNER_PREFERENCE, &corner_pref, sizeof(corner_pref));
        }
      }
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
      cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_DEST_OUT);

      canvas.move_to(client_rect.left,  client_rect.top);
      canvas.line_to(client_rect.right, client_rect.top);
      canvas.line_to(client_rect.right, client_rect.bottom);
      canvas.line_to(client_rect.left,  client_rect.bottom);
      canvas.close_path();

      canvas.move_to(glassfree.left,  glassfree.top);
      canvas.line_to(glassfree.left,  glassfree.bottom);
      canvas.line_to(glassfree.right, glassfree.bottom);
      canvas.line_to(glassfree.right, glassfree.top);
      canvas.close_path();

      canvas.clip();

      canvas.line_width(3);

      canvas.move_to(window_rect.left,  window_rect.top);
      canvas.line_to(window_rect.right, window_rect.top);
      canvas.line_to(window_rect.right, window_rect.bottom);
      canvas.line_to(window_rect.left,  window_rect.bottom);
      canvas.close_path();

      canvas.move_to(glassfree.left,  glassfree.top);
      canvas.line_to(glassfree.left,  glassfree.bottom);
      canvas.line_to(glassfree.right, glassfree.bottom);
      canvas.line_to(glassfree.right, glassfree.top);
      canvas.close_path();

      canvas.set_color(Color::White, 1 - 0.1);
      canvas.stroke();

      cairo_reset_clip(canvas.cairo());
    }

    if(use_custom_system_buttons()) {
      if(hit_test_is_system_button(_hit_test_mouse_over)) {
        RECT min_rect;
        RECT max_rect;
        RECT close_rect;
        
        get_system_button_bounds(_hwnd, &min_rect, &max_rect, &close_rect);
        
        Color fg_color = Color::from_bgr24(title_font_color(_glass_enabled, Win32HighDpi::get_dpi_for_window(_hwnd), _active, _use_dark_mode));
        
        add_rect(canvas, min_rect);
        if(bg_color.is_valid()) {
          canvas.set_color(bg_color, buttons_alpha);
          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_OVER);
        }
        else{
          canvas.set_color(Color::White, buttons_alpha);
          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_DEST_OUT);
        }
        if(_hit_test_mouse_over == HTMINBUTTON) {
          canvas.fill_preserve();
          canvas.set_color(fg_color, (_hit_test_mouse_down == _hit_test_mouse_over) ? 0.3 : 0.2);
          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_OVER);
        }
        canvas.fill();
        
        add_rect(canvas, max_rect);
        if(bg_color.is_valid()) {
          canvas.set_color(bg_color, buttons_alpha);
          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_OVER);
        }
        else{
          canvas.set_color(Color::White, buttons_alpha);
          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_DEST_OUT);
        }
        if(_hit_test_mouse_over == HTMAXBUTTON) {
          canvas.fill_preserve();
          canvas.set_color(fg_color, (_hit_test_mouse_down == _hit_test_mouse_over) ? 0.3 : 0.2);
          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_OVER);
        }
        canvas.fill();
        
        add_rect(canvas, close_rect);
        if(bg_color.is_valid()) {
          canvas.set_color(bg_color, buttons_alpha);
          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_OVER);
        }
        else{
          canvas.set_color(Color::White, buttons_alpha);
          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_DEST_OUT);
        }
        if(_hit_test_mouse_over == HTCLOSE) {
          // TODO: obtain this color from Windows
          static const Color CloseButtonRed = Color::from_rgb24(0xE81123);
          
          canvas.fill_preserve();
          canvas.set_color(
            CloseButtonRed, 
            (_hit_test_mouse_down == _hit_test_mouse_over) ? 0.6 : 1.0);
          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_OVER);
        }
        canvas.fill();
      }
      else {
        RECT buttons;
        get_system_button_bounds(_hwnd, &buttons);
        
        add_rect(canvas, buttons);

//        canvas.set_color(Color::White, buttons_alpha);
//        cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_DEST_OUT);
//        canvas.fill_preserve();
        
        if(bg_color.is_valid()) {
          canvas.set_color(bg_color, buttons_alpha);
          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_OVER);
        }
        else{
          canvas.set_color(Color::White, buttons_alpha);
          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_DEST_OUT);
        }
        canvas.fill();
      }
    } 
    else { // small alpha value above the min/max/close buttons
      RECT buttons;
      get_system_button_bounds(_hwnd, &buttons);

      if(style_ex & WS_EX_TOOLWINDOW) {
        add_rect(canvas, buttons);
      }
      else {
        canvas.move_to(buttons.left + 0.5,   buttons.top);
        canvas.arc(    buttons.left + 0.5  + buttonradius, buttons.bottom - 0.5 - buttonradius, buttonradius, M_PI,   M_PI / 2, true);
        canvas.arc(    buttons.right - 0.5 - buttonradius, buttons.bottom - 0.5 - buttonradius, buttonradius, M_PI / 2, 0,      true);
        canvas.line_to(buttons.right - 0.5,  buttons.top);
        canvas.close_path();
      }

      canvas.set_color(Color::White, buttons_alpha);
      cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_DEST_OUT);

//        if(hit_test_is_system_button(_hit_test_mouse_over)) {
//          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_CLEAR);
//        }
//        else if(_active) {
//          canvas.set_color(Color::White, 1 - 0.2);
//          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_DEST_OUT);
//        }
//        else {
//          canvas.set_color(Color::White, 1 - 0.6);
//          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_DEST_OUT);
//        }

      canvas.fill();
    }
    
    if(_hit_test_extra_button >= 0) {
      int dpi = Win32HighDpi::get_dpi_for_window(_hwnd);
      RECT extra_buttons;
      get_system_menu_bounds(_hwnd, &extra_buttons, dpi);
      
      extra_buttons.left = extra_buttons.right;
      extra_buttons.top    -= MulDiv(2, dpi, 96);
      extra_buttons.bottom += MulDiv(4, dpi, 96);
      
      bool have_shade = false;
      int x = extra_buttons.left;
      for(auto &button : extra_caption_buttons()) {
        int w = MulDiv(button.dx_96dpi, dpi, 96);
        
        if(button.flags & Win32CaptionButton::Button) {
          if(!have_shade) {
            extra_buttons.left = x;
            have_shade = true;
          }
          
          extra_buttons.right = x + w;
        }
        
        x+= w;
      }
      
      if(have_shade) {
        add_rect(canvas, extra_buttons);
        
        canvas.set_color(Color::White, buttons_alpha);
        cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_DEST_OUT);
        canvas.fill();
      }
    }
    
    if( client_rect.left - window_rect.left < frameradius || 
        window_rect.right - client_rect.right < frameradius)
    { // make the edges round again
      canvas.move_to(client_rect.left,  client_rect.top);
      canvas.line_to(client_rect.left,  client_rect.bottom);
      canvas.line_to(client_rect.right, client_rect.bottom);
      canvas.line_to(client_rect.right, client_rect.top);
      canvas.close_path();

      {
        window_rect.top+= 1;

        canvas.move_to(window_rect.left, window_rect.top + frameradius);
        canvas.arc(window_rect.left  + frameradius, window_rect.top    + frameradius, frameradius,     M_PI,     3 * M_PI / 2, false);
        canvas.arc(window_rect.right - frameradius, window_rect.top    + frameradius, frameradius, 3 * M_PI / 2, 2 * M_PI,     false);
        canvas.arc(window_rect.right - frameradius, window_rect.bottom - frameradius, frameradius, 0,                M_PI / 2, false);
        canvas.arc(window_rect.left  + frameradius, window_rect.bottom - frameradius, frameradius,     M_PI / 2,     M_PI,     false);
        canvas.close_path();
      }

      cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_CLEAR);
      canvas.fill();
    }
    else if(use_custom_system_buttons()) {
      canvas.move_to(client_rect.left, client_rect.top);
      canvas.line_to(client_rect.right, client_rect.top);
      canvas.line_to(client_rect.right, client_rect.top + 1);
      canvas.line_to(client_rect.left, client_rect.top + 1);
      if(_active) {
        Win32Themes::ColorizationInfo col_info {};
        if(Win32Themes::try_read_win10_colorization(&col_info) && !col_info.has_accent_color_in_active_titlebar) {
          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_OVER);
          canvas.set_color(Color::Black, 0.5);
        }
        else {
          cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_CLEAR);
          canvas.set_color(Color::Black, 0.0);
        }
      }
      else {
        cairo_set_operator(canvas.cairo(), CAIRO_OPERATOR_OVER);
        canvas.set_color(Color::from_rgb24(0x565656), 0.5);
      }
      canvas.fill();
    }
  }
}

void BasicWin32Window::paint_background(Canvas &canvas) {
  if( !_themed_frame          ||
      !background_image.ptr() ||
      cairo_surface_status(background_image.ptr()) != CAIRO_STATUS_SUCCESS)
  {
    return;
  }
  
  CanvasAutoSave saved(canvas);

  RECT rect, glassfree;
  GetClientRect(_hwnd, &rect);
  get_glassfree_rect(&glassfree);
  
  int x = rect.right - cairo_image_surface_get_width(background_image.ptr());
  cairo_set_source_surface(canvas.cairo(), background_image.ptr(), x, 0);
  //canvas.set_color(0xff0000);

  canvas.move_to(rect.left,  rect.top);
  canvas.line_to(rect.right, rect.top);
  canvas.line_to(rect.right, rect.bottom);
  canvas.line_to(rect.left,  rect.bottom);
  canvas.close_path();

  if(!IsRectEmpty(&glassfree)) {
    canvas.move_to(glassfree.left,  glassfree.top);
    canvas.line_to(glassfree.left,  glassfree.bottom);
    canvas.line_to(glassfree.right, glassfree.bottom);
    canvas.line_to(glassfree.right, glassfree.top);
    canvas.close_path();
  }

  canvas.clip();
  canvas.paint_with_alpha(0.8);

  if(!IsRectEmpty(&glassfree)) {
    cairo_reset_clip(canvas.cairo());
    canvas.move_to(glassfree.left,  glassfree.top);
    canvas.line_to(glassfree.left,  glassfree.bottom);
    canvas.line_to(glassfree.right, glassfree.bottom);
    canvas.line_to(glassfree.right, glassfree.top);
    canvas.close_path();
    canvas.clip();

    canvas.paint_with_alpha(0.25);
  }
}

int BasicWin32Window::dpi() {
  return Win32HighDpi::get_dpi_for_window(_hwnd);
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

void BasicWin32Window::finish_apply_title(String displayed_title) {
  displayed_title+= String::FromChar(0);
  
  const wchar_t *str = displayed_title.buffer_wchar();
  if(str)
    SetWindowTextW(_hwnd, str);
}

void BasicWin32Window::on_close() {
  deregister_self();
  ShowWindow(_hwnd, SW_HIDE);
  BasicWin32Widget::on_close();
}

LRESULT BasicWin32Window::callback(UINT message, WPARAM wParam, LPARAM lParam) {
  LRESULT dwm_result = 0;

  bool calldwp = true;

  if(Win32Themes::DwmDefWindowProc)
    calldwp = !Win32Themes::DwmDefWindowProc(_hwnd, message, wParam, lParam, &dwm_result);

  switch(message) {
    case WM_NCACTIVATE: {
        pmath_debug_print("[BasicWin32Window: WM_NCACTIVATE %p %d (active: %p)]\n", _hwnd, wParam, GetActiveWindow());
        _active = wParam;
        
        if(_blur_behind_window) 
          _blur_behind_window->colorize(wParam, _use_dark_mode);
        
        if( !Win32Themes::IsCompositionActive || !Win32Themes::IsCompositionActive()) {
          struct redraw_glass_info_t info;

          get_glassfree_rect(&info.inner);
          MapWindowPoints(_hwnd, nullptr, (POINT*)&info.inner, 2);

          EnumChildWindows(_hwnd, redraw_glass_callback, (LPARAM)&info);
        }
        
//        if(Win32Themes::DwmSetWindowAttribute) {
//          DWORD policy = Win32Themes::DWMNCRP_ENABLED; 
//          Win32Themes::DwmSetWindowAttribute(
//            _hwnd, 
//            Win32Themes::DWMWA_NCRENDERING_POLICY,
//            &policy,
//            sizeof(policy));
//        }

        if(_themed_frame) {
          int dpi = Win32HighDpi::get_dpi_for_window(_hwnd);
          HDC hdc = GetDC(_hwnd);

          RECT rect;
          GetClientRect(_hwnd, &rect);
          IntersectClipRect(hdc, 0, 0,
                            rect.right,
                            Win32HighDpi::get_system_metrics_for_dpi(SM_CYFRAME, dpi) + 
                            Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi) + 
                            Win32HighDpi::get_system_metrics_for_dpi(SM_CYCAPTION, dpi));
          Impl(*this).paint_themed(hdc);

          ReleaseDC(_hwnd, hdc);
        }
      } break;
  }

  if(!initializing() && !destroying()) {
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

      case WM_ENTERSIZEMOVE: 
        Impl(*this).on_entersizemove();
        break;

      case WM_EXITSIZEMOVE:
        Impl(*this).on_exitsizemove();
        break;
//} ... sizing & moving
      
      case WM_SHOWWINDOW: {
          bool will_be_visible = !!wParam;
          if(_blur_behind_window) {
            if(will_be_visible && Win32Themes::use_win10_transparency()) 
              _blur_behind_window->show();
            else 
              _blur_behind_window->hide();
          }
        } break;
      
      case WM_WINDOWPOSCHANGING: {
          Impl(*this).on_windowposchanging((WINDOWPOS *)lParam);
        } break;

      case WM_WINDOWPOSCHANGED: {
          Impl(*this).on_windowposchanged((WINDOWPOS *)lParam);
        } break;

      case WM_DPICHANGED: { // Windows 8.1 and above
        //int dpiX = LOWORD(wParam);
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
      
      case WM_SYSCOLORCHANGE: {
          on_theme_changed();
        } break;
        
      case WM_SETTINGCHANGE: {
          // TODO(?): check if lParam == "ImmersiveColorSet"
          on_theme_changed();
        } break;
        
      case WM_THEMECHANGED:
      case WM_DWMCOMPOSITIONCHANGED: {
          if(Win32Themes::DwmEnableComposition)
            Win32Themes::DwmEnableComposition(1);

          on_theme_changed();
        } break;

      case WM_ERASEBKGND:
        return 1;
        
//      case WM_NCPAINT: {
//          HDC hdc = GetDCEx(_hwnd, (HRGN)wParam, DCX_WINDOW|DCX_INTERSECTRGN);
//          if(HBRUSH brush = create_solid_brush_with_alpha(Color::from_rgb24(0x00FF00), 255)) {
//            RECT tmp_rect = {-5,0,30,30};
//            
//            FillRect(hdc, &tmp_rect, brush);
//            
//            DeleteObject(brush);
//          }
//          ReleaseDC(_hwnd, hdc);
//          return 0;
//        } break;

      case WM_PAINT: {
          if(_themed_frame) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(_hwnd, &ps);
            Impl(*this).paint_themed(hdc);
            EndPaint(_hwnd, &ps);
            return 0;
          }
        } break;

      case WM_NCHITTEST: {
          if(!calldwp)
            return dwm_result;

          if(_themed_frame) {
            if(use_custom_system_buttons()) {
              int btn = Impl(*this).nc_hit_test_system_buttons(point_from_lparam(lParam));
              if(btn)
                return btn;
            }
            return Impl(*this).nc_hit_test_no_system_buttons(point_from_lparam(lParam));
          }
        } break;

      case WM_NCCALCSIZE: {
          if(wParam && _themed_frame) {
            NCCALCSIZE_PARAMS *calcsize_params = (NCCALCSIZE_PARAMS*)lParam;
            
            if(!Impl(*this).is_left_right_bottom_frame_themed()) {
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
          POINT pt = point_from_lparam(lParam);
          int8_t new_hit = Impl(*this).nc_hit_test_system_buttons(pt, wParam);
          
          int8_t new_hit_extra_btn = -1;
          if(new_hit == HTOBJECT) {
            new_hit_extra_btn = Impl(*this).nc_extra_button_at_point(pt);
          }
          
          if(_hit_test_mouse_over != new_hit || _hit_test_extra_button != new_hit_extra_btn) {
            invalidate_caption();
            _hit_test_mouse_over   = new_hit;
            _hit_test_extra_button = new_hit_extra_btn;
          }
        } break;

      case WM_NCMOUSELEAVE: {
          if(_hit_test_extra_button >= 0 || hit_test_is_system_button(_hit_test_mouse_over))
            invalidate_caption();
            
          _hit_test_mouse_over   = HTNOWHERE;
          _hit_test_extra_button = -1;
        } break;

      case WM_NCRBUTTONUP: {
          if(_themed_frame && (wParam == HTCAPTION || wParam == HTSYSMENU || wParam == HTOBJECT)) {
            if(HMENU menu = WIN32report(GetSystemMenu(_hwnd, FALSE))) {
              POINT pt;
              pt.x = (short)LOWORD(lParam);
              pt.y = (short)HIWORD(lParam);
              
              UINT align;
              if(GetSystemMetrics(SM_MENUDROPALIGNMENT)) {
                align = TPM_RIGHTALIGN;
              }
              else {
                align = TPM_LEFTALIGN;
              }
              
              MenuExitInfo exit_info;
              DWORD cmd;
              {
                Win32AutoMenuHook menu_hook(menu, _hwnd, nullptr, false, false);
                Win32Menu::use_dark_mode   = is_using_dark_mode();
                Win32Menu::use_large_items = Win32Touch::get_mouse_message_source() == DeviceKind::Touch;
                
                WIN32report(cmd = TrackPopupMenu(
                        menu,
                        align | TPM_RETURNCMD,
                        pt.x,
                        pt.y,
                        0,
                        _hwnd,
                        nullptr));
                
                exit_info = menu_hook.exit_info;
              }

              if(!cmd && !exit_info.handle_after_exit()) {
                if(exit_info.reason == MenuExitReason::ExplicitCmd)
                  cmd = exit_info.cmd;
              }
      
              if(cmd)
                SendMessageW(_hwnd, WM_SYSCOMMAND, cmd, 0);
              return 0;
            }
          }
        } break;
      
      case WM_NCLBUTTONDBLCLK:
      case WM_NCLBUTTONDOWN: {
          POINT pt = point_from_lparam(lParam);
          _hit_test_mouse_down = Impl(*this).nc_hit_test_system_buttons(pt, wParam);
          
          if(_hit_test_extra_button >= 0 || use_custom_system_buttons()) {
            if(_hit_test_extra_button >= 0 || hit_test_is_system_button(_hit_test_mouse_down)) {
              invalidate_caption();
              return 0;
            }
          }
        } break;
        
      case WM_NCLBUTTONUP: {
        LRESULT res = 0;
        if(Impl(*this).on_nclbuttonup(&res, wParam, point_from_lparam(lParam)))
          return res;
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

HANDLE BasicWin32Window::composition_window_theme(int dpi) {
  return static_resources.composition_window_theme(dpi);
}

void BasicWin32Window::virtual_desktop_changed() {
  pmath_debug_print("[virtual_desktop_changed, cloaked=%s]\n", is_window_cloaked(_hwnd) ? "yes" : "no");
  
  if(_blur_behind_window) {
    _blur_behind_window->show_if_owner_visible_on_screen();
  }
}

//} ... class BasicWin32Window

//{ class BasicWin32Window::Impl ...

bool BasicWin32Window::Impl::is_left_right_bottom_frame_themed() {
  /* On Windows 10 we let the OS handle left/right/bottom frame, which means those
     are completely transparent except a one-pixel wide line when the window is focussed.
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
//  if(Win32Version::is_windows_10_or_newer())
//    return false;
//  
////  DWORD style = GetWindowLongW(self._hwnd, GWL_STYLE);
////  if(style & WS_MAXIMIZE)
////    return false;
//  
////  RECT rect = {0};
////  GetWindowRect(self._hwnd, &rect);
////  if(rect.right - rect.left > 600)
////    return false;
//  
//  return true;
}

int BasicWin32Window::Impl::nc_hit_test_no_system_buttons(POINT mouse_screen) {
  /* Note that DwmDefWindowProc() will return HTCLOSE, HTMAXBUTTON, HTMINBUTTON,
     when we use the DWM title bar. But from Windows 10 (build ???) on, we use a 
     custom blur effect and draw our own min/max/close buttons and thus need to 
     hit-test those, too.
   */
  int dpi = Win32HighDpi::get_dpi_for_window(self._hwnd);
  
  Win32Themes::MARGINS margins;
  ::get_nc_margins(self._hwnd, &margins, dpi);
//  margins.cxLeftWidth    += _extra_glass.cxLeftWidth;
//  margins.cxRightWidth   += _extra_glass.cxRightWidth;
//  margins.cyTopHeight    += _extra_glass.cyTopHeight;
//  margins.cyBottomHeight += _extra_glass.cyBottomHeight;

  RECT rect;
  GetWindowRect(self._hwnd, &rect);
  
  DWORD style    = GetWindowLongW(self._hwnd, GWL_STYLE);
  DWORD ex_style = GetWindowLongW(self._hwnd, GWL_EXSTYLE);
  
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
    ScreenToClient(self._hwnd, &mouse_client);
    
    RECT menu;
    get_system_menu_bounds(self._hwnd, &menu, dpi);
    if( mouse_client.x < menu.right && mouse_client.y < menu.bottom) {
      return HTSYSMENU;
    }
    
    menu.left = menu.right;
    menu.top = 0;
    menu.bottom = margins.cyTopHeight;
    
    for(auto &button : self.extra_caption_buttons()) {
      int w = MulDiv(button.dx_96dpi, dpi, 96);
      menu.right+= w;
      
      if(button.is_mouse_sensitive() && PtInRect(&menu, mouse_client))
        return HTOBJECT;
      
      menu.left = menu.right;
    }
  }
  
  return result;
}

int BasicWin32Window::Impl::nc_hit_test_system_buttons(POINT mouse_screen, int fallback) {
  POINT mouse_client = mouse_screen;
  ScreenToClient(self._hwnd, &mouse_client);
  
  DWORD style = GetWindowLongW(self._hwnd, GWL_STYLE);
  
  RECT min_rect;
  RECT max_rect;
  RECT close_rect;
  get_system_button_bounds(self._hwnd, &min_rect, &max_rect, &close_rect);
  if(PtInRect(&close_rect, mouse_client))
    return HTCLOSE;
  
  if((style & WS_MAXIMIZEBOX) && PtInRect(&max_rect, mouse_client))
    return HTMAXBUTTON;
  
  if((style & WS_MINIMIZEBOX) && PtInRect(&min_rect, mouse_client))
    return HTMINBUTTON;
  
  return fallback;
}

int BasicWin32Window::Impl::nc_extra_button_at_point(POINT mouse_screen) {
  int dpi = Win32HighDpi::get_dpi_for_window(self._hwnd);
  
  POINT mouse_client = mouse_screen;
  ScreenToClient(self._hwnd, &mouse_client);
  
  RECT rect;
  get_system_menu_bounds(self._hwnd, &rect, dpi);
  
  Win32Themes::MARGINS nc;
  ::get_nc_margins(self._hwnd, &nc, dpi);
  
  rect.top = 0;
  rect.bottom = std::max(rect.bottom, (LONG)nc.cyTopHeight);
  rect.left = rect.right;
  int i = 0;
  for(auto &button : self.extra_caption_buttons()) {
    rect.right+= MulDiv(button.dx_96dpi, dpi, 96);
    
    if(PtInRect(&rect, mouse_client))
      return i;
    
    rect.left = rect.right;
    ++i;
  }
  
  return -1;
}

struct FindPopup {
  HWND popup;
  DWORD thread_id;
  
  static BOOL CALLBACK callback(HWND hwnd, LPARAM lParam) {
    return ((FindPopup*)lParam)->callback(hwnd);
  }
  
  bool callback(HWND hwnd) {
    if(GetWindowThreadProcessId(hwnd, nullptr) != thread_id)
      return true;

    if(!IsWindowVisible(hwnd))
      return true;

    if(!IsWindowEnabled(hwnd))
      return true;

    if(dynamic_cast<Win32Widget *>(BasicWin32Widget::from_hwnd(hwnd)))
      return true;

    if(GetWindow(hwnd, GW_OWNER) != nullptr) {
      popup = hwnd;
      return false;
    }

    return true;
  }
};

void BasicWin32Window::Impl::on_windowposchanging(WINDOWPOS *pos) {
  static bool during_pos_changing = false;
  
  if(!during_pos_changing) {
    if(0 == (pos->flags & SWP_NOZORDER)) { // changing Z order...
      during_pos_changing = true; // prevent recursive call
      // place behind all windows with higher zorder_level

      HWND active_window    = GetActiveWindow();
      HWND own_popup_window = GetWindow(self._hwnd, GW_ENABLEDPOPUP);
      if(own_popup_window == self._hwnd)
        own_popup_window = nullptr;

      FindPopup popup_info;
      popup_info.thread_id = GetWindowThreadProcessId(self._hwnd, 0);
      popup_info.popup     = own_popup_window;
      EnumWindows(FindPopup::callback, (LPARAM)&popup_info);

      BasicWin32Window *last_higher = nullptr;
      for(auto win : CommonDocumentWindow::All) {
        if(BasicWin32Window *next = dynamic_cast<BasicWin32Window*>(win)) {
          if(next == &self)
            continue;
          if(next->zorder_level() > self.zorder_level()) {
            last_higher = next;
            break;
          }
        }
      }
//      BasicWin32Window *next = _next_window;
//      while(next != this) {
//        if(next->zorder_level() > zorder_level()) {
//          last_higher = next;
//          break;
//        }
//        next = next->_next_window;
//      }

      // get all windows from higher level, sorted from back to front
      static Array<BasicWin32Window *> all_higher;
      all_higher.length(0);
      if(last_higher) {
        HWND next_hwnd = GetNextWindow(last_higher->hwnd(), GW_HWNDNEXT);
        while(next_hwnd) {
          auto wnd = dynamic_cast<BasicWin32Window*>(BasicWin32Widget::from_hwnd(next_hwnd));

          if(wnd && wnd->zorder_level() > self.zorder_level())
            last_higher = wnd;

          next_hwnd = GetNextWindow(next_hwnd, GW_HWNDNEXT);
        }

        all_higher.add(last_higher);

        next_hwnd = GetNextWindow(last_higher->hwnd(), GW_HWNDPREV);
        while(next_hwnd) {
          auto wnd = dynamic_cast<BasicWin32Window*>(BasicWin32Widget::from_hwnd(next_hwnd));

          if(wnd && wnd->zorder_level() > self.zorder_level())
            all_higher.add(wnd);

          next_hwnd = GetNextWindow(next_hwnd, GW_HWNDPREV);
        }
      }
      
      int num_windows = all_higher.length();
      HDWP hdwp = BeginDeferWindowPos(num_windows);
      
      /* Put the higher-level windows to the top again and place this window
         behind. */
      if(all_higher.length() > 0) {
        if(active_window == self._hwnd)
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
}

void BasicWin32Window::Impl::on_windowposchanged(WINDOWPOS *pos) {
  if(self._blur_behind_window) {
    if(is_window_visible_on_screen(self._hwnd) && Win32Themes::use_win10_transparency()) 
      self._blur_behind_window->show(RECT {pos->x, pos->y, pos->x + pos->cx, pos->y + pos->cy});
    else 
      self._blur_behind_window->hide();
  }
}

bool BasicWin32Window::Impl::on_nclbuttonup(LRESULT *result, WPARAM wParam, POINT pos) {
  if(self._hit_test_extra_button >= 0 || use_custom_system_buttons()) {
    if(self._hit_test_extra_button >= 0 || hit_test_is_system_button(self._hit_test_mouse_down))
      self.invalidate_caption();
    
    int8_t new_hit = Impl(*this).nc_hit_test_system_buttons(pos, wParam);
    auto   old_hit = self._hit_test_mouse_down;
    self._hit_test_mouse_down = HTNOWHERE;
    
    if(new_hit == old_hit) {
      switch(old_hit) {
        case HTMINBUTTON:
          if(GetWindowLong(self._hwnd, GWL_STYLE) & WS_MINIMIZE)
            SendMessageW(self._hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
          else
            SendMessageW(self._hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
          *result = 0;
          return true;
          
        case HTMAXBUTTON:
          if(GetWindowLong(self._hwnd, GWL_STYLE) & WS_MAXIMIZE)
            SendMessageW(self._hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
          else
            SendMessageW(self._hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
          *result = 0;
          return true;
          
        case HTCLOSE:
          SendMessageW(self._hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
          *result = 0;
          return true;
        
        case HTOBJECT:
          if(self._hit_test_extra_button >= 0) {
            auto buttons = self.extra_caption_buttons();
            if(self._hit_test_extra_button < buttons.length()) {
              if(DWORD cmdId = buttons[self._hit_test_extra_button].cmdId) {
                SendMessageW(self._hwnd, WM_SYSCOMMAND, cmdId, 0);
                return true;
              }
            }
          }
          break;
      }
    }
  }
  
  if(self._themed_frame && wParam == HTSYSMENU) {
    Win32Menu::use_dark_mode = self.is_using_dark_mode();
    if(HMENU menu = GetSystemMenu(self._hwnd, FALSE)) {
      TPMPARAMS tpm;
      memset(&tpm, 0, sizeof(tpm));
      tpm.cbSize = sizeof(tpm);
      GetWindowRect(self._hwnd, &tpm.rcExclude);
      
      int dpi = self.dpi();
      
      tpm.rcExclude.left +=  Win32HighDpi::get_system_metrics_for_dpi(SM_CXSIZEFRAME, dpi) + Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
      tpm.rcExclude.top +=   Win32HighDpi::get_system_metrics_for_dpi(SM_CYSIZEFRAME, dpi) + Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
      tpm.rcExclude.right -= Win32HighDpi::get_system_metrics_for_dpi(SM_CXSIZEFRAME, dpi) + Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
      tpm.rcExclude.bottom = tpm.rcExclude.top + Win32HighDpi::get_system_metrics_for_dpi(SM_CYCAPTION, dpi);

      int x;
      UINT align;
      DWORD ex_style = GetWindowLongW(self._hwnd, GWL_EXSTYLE);
      if(ex_style & WS_EX_LAYOUTRTL) {
        align = TPM_RIGHTALIGN;
        x = tpm.rcExclude.right;
      }
      else {
        align = TPM_LEFTALIGN;
        x = tpm.rcExclude.left;
      }
      
      MenuExitInfo exit_info;
      DWORD cmd;
      {
        Win32AutoMenuHook menu_hook(menu, self._hwnd, nullptr, false, false);
        Win32Menu::use_large_items = Win32Touch::get_mouse_message_source() == DeviceKind::Touch;
        
        WIN32report(cmd = TrackPopupMenuEx(
                menu,
                align | TPM_RETURNCMD | TPM_NONOTIFY,
                x,
                tpm.rcExclude.bottom,
                self._hwnd,
                &tpm));
        
        exit_info = menu_hook.exit_info;
      }

      if(!cmd && !exit_info.handle_after_exit()) {
        if(exit_info.reason == MenuExitReason::ExplicitCmd)
          cmd = exit_info.cmd;
      }

      if(cmd)
        SendMessageW(self._hwnd, WM_SYSCOMMAND, cmd, 0);
      *result = 0;
      return true;
    }
  }
  
  return false;
}

void BasicWin32Window::Impl::on_entersizemove() {
  GetWindowRect(self._hwnd, &sizing_initial_rect);
  self.last_moving_cx = sizing_initial_rect.left + (sizing_initial_rect.right  - sizing_initial_rect.left) / 2;
  self.last_moving_cy = sizing_initial_rect.top  + (sizing_initial_rect.bottom - sizing_initial_rect.top ) / 2;

  find_all_snappers();
  if(Win32Version::is_windows_11_or_newer()) {
    // Windows 11 does not support blur behind on inactive windows and gives solid black instead
    // and it treats our _blur_behind_window as inactive.
    // Also, Acrylic windows are not particularly slow to drag on Windows 11, unlike Windows 10
    Win32BlurBehindWindow::suppress_slow_acrylic_blur = false;
  }
  else {
    Win32BlurBehindWindow::suppress_slow_acrylic_blur = true;
  }
  if(self._blur_behind_window)
    self._blur_behind_window->colorize(self._active, self._use_dark_mode);
  
  for(auto hwnd : self.all_snappers) {
    if(auto win = dynamic_cast<BasicWin32Window*>(BasicWin32Widget::from_hwnd(hwnd))) {
      if(win->_blur_behind_window)
        win->_blur_behind_window->colorize(win->_active, win->_use_dark_mode);
    }
  }
}

void BasicWin32Window::Impl::on_exitsizemove() {
  self.all_snappers.clear();
  self.all_snapper_positions.length(0);
  
  Win32BlurBehindWindow::suppress_slow_acrylic_blur = false;
  if(self._blur_behind_window)
    self._blur_behind_window->colorize(self._active, self._use_dark_mode);
  
  for(auto hwnd : self.all_snappers) {
    if(auto win = dynamic_cast<BasicWin32Window*>(BasicWin32Widget::from_hwnd(hwnd))) {
      if(win->_blur_behind_window)
        win->_blur_behind_window->colorize(win->_active, win->_use_dark_mode);
    }
  }
}

void BasicWin32Window::Impl::clear_property_store(HWND hwnd) {
  if(!hwnd)
    return;
  
  HMODULE shell32 = LoadLibrary("shell32.dll");
  if(shell32) {
    HRESULT (WINAPI * p_SHGetPropertyStoreForWindow)(HWND, REFIID, void **);
    p_SHGetPropertyStoreForWindow = (HRESULT (WINAPI*)(HWND, REFIID, void **))
      GetProcAddress(shell32, "SHGetPropertyStoreForWindow");
    
    if(p_SHGetPropertyStoreForWindow) {
      ComBase<IPropertyStore> props;
      HRreport(p_SHGetPropertyStoreForWindow(hwnd, props.iid(), (void**)props.get_address_of()));
      if(props) {
        DWORD count;
        if(!HRbool(props->GetCount(&count)))
          count = 0;
        
        pmath_debug_print("[BasicWin32Window::Impl::clear_property_store: %d entries]\n", (int)count);
        
        for(; count > 0; --count) {
          PROPERTYKEY key;
          if(HRbool(props->GetAt(count - 1, &key))) {
            PROPVARIANT empty = {};
            HRreport(props->SetValue(key, empty));
          }
        }
        
        HRreport(props->Commit());
      }
    }
    
    FreeLibrary(shell32);
  }
}

void BasicWin32Window::Impl::paint_themed(HDC hdc) {
  if(self._themed_frame) {
    RECT rect;
    GetClientRect(self._hwnd, &rect);
    
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
    
    bool opaque = false;
    if(Win32Version::is_windows_10_or_newer()) {
      if(!Win32Themes::use_win10_transparency()) {
        Win32Themes::ColorizationInfo colorization;
        if(Win32Themes::try_read_win10_colorization(&colorization)) {
          DWORD bgr;
          
          if(self._active && colorization.has_accent_color_in_active_titlebar) 
            bgr = colorization.accent_color;
          else if(self._use_dark_mode)
            bgr = 0x000000u;
          else
            bgr = 0xFFFFFFu;
          
          if(HBRUSH brush = create_solid_brush_with_alpha(Color::from_bgr24(bgr), 255)) {
            opaque = true;
            FillRect(bmp_dc, &rect, brush);
            DeleteObject(brush);
          }
        }
      }
    }

    cairo_t *cr = cairo_create(surface);
    {
      Canvas canvas(cr);

      self.paint_background_at(canvas, POINT{ 0, 0 });
    }
    cairo_destroy(cr);

    cairo_surface_flush(surface);
    
    if(use_custom_system_buttons())
      paint_themed_system_buttons(bmp_dc);
    
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
    
    if(opaque && !self._active) {
      if(HBRUSH brush = create_solid_brush_with_alpha(Color::from_rgb24(0x808080), 255)) {
        RECT tmp_rect = {rect.left, 0, rect.right, 1};
        
        FillRect(hdc, &tmp_rect, brush);
        //FrameRect(hdc, &rect, brush);
        
        DeleteObject(brush);
      }
    }
  }
  else {
  }
}

void BasicWin32Window::Impl::paint_themed_system_buttons(HDC hdc_bitmap) {
  if( !self._themed_frame            ||
      !Win32Themes::OpenThemeData    ||
      !Win32Themes::CloseThemeData   ||
      !Win32Themes::GetThemeSysColor ||
      !Win32Themes::GetThemeSysFont  ||
      !Win32Themes::DrawThemeTextEx)
  {
    return;
  }
    
  RECT min_rect;
  RECT max_rect;
  RECT close_rect;
  
  get_system_button_bounds(self._hwnd, &min_rect, &max_rect, &close_rect);
  
  int dpi = Win32HighDpi::get_dpi_for_window(self._hwnd);
  if(HANDLE theme = composition_window_theme(dpi)) {
    if(Win32Version::is_windows_10_or_newer()) {
      DWORD style = GetWindowLong(self._hwnd, GWL_STYLE);
  
      HFONT font = CreateFontW(
                     (close_rect.bottom - close_rect.top) / 3,
                     0,
                     0,
                     0,
                     FW_NORMAL,
                     FALSE,
                     FALSE,
                     FALSE,
                     DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS,
                     CLIP_DEFAULT_PRECIS,
                     DEFAULT_QUALITY,
                     DEFAULT_PITCH | FF_DONTCARE,
                     Win32Themes::symbol_font_name());
      HFONT old_font = (HFONT)SelectObject(hdc_bitmap, font);
      
      if(style & WS_MAXIMIZE) {
        int dy = Win32HighDpi::get_system_metrics_for_dpi(SM_CYFRAME, dpi) + 
                 Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
        
        min_rect.top+= dy;
        max_rect.top+= dy;
        close_rect.top+= dy;
      }
      
      static const wchar_t CloseSymbol[] = L"\xE8BB";
      static const wchar_t MinimizeSymbol[] = L"\xE921";
      static const wchar_t MaximizeSymbol[] = L"\xE922";
      static const wchar_t RestoreSymbol[] = L"\xE923";
      
      COLORREF col        = title_font_color(true, dpi, self._active, self._use_dark_mode);
      COLORREF active_col = self._active ? col : title_font_color(true, dpi, true, self._use_dark_mode);
      Win32Themes::DTTOPTS dtt_opts;
      memset(&dtt_opts, 0, sizeof(dtt_opts));
      dtt_opts.dwSize    = sizeof(dtt_opts);
      dtt_opts.dwFlags   = DTT_COMPOSITED | DTT_TEXTCOLOR;
      
      dtt_opts.crText = (self._hit_test_mouse_over == HTCLOSE) ? 0xFFFFFF : col; 
      Win32Themes::DrawThemeTextEx(
        theme,
        hdc_bitmap,
        0, 0,
        CloseSymbol, -1,
        DT_VCENTER | DT_CENTER | DT_SINGLELINE,
        &close_rect,
        &dtt_opts);
      
      if(style & WS_MINIMIZEBOX) {
        dtt_opts.crText = self._active ? col : (self._hit_test_mouse_over == HTMINBUTTON) ? active_col : col; 
        Win32Themes::DrawThemeTextEx(
          theme,
          hdc_bitmap,
          0, 0,
          (style & WS_MINIMIZE) ? RestoreSymbol : MinimizeSymbol, -1,
          DT_VCENTER | DT_CENTER | DT_SINGLELINE,
          &min_rect,
          &dtt_opts);
      }
      
      if(style & WS_MAXIMIZEBOX) {
        dtt_opts.crText = self._active ? col : (self._hit_test_mouse_over == HTMAXBUTTON) ? active_col : col; 
        Win32Themes::DrawThemeTextEx(
          theme,
          hdc_bitmap,
          0, 0,
          (style & WS_MAXIMIZE) ? RestoreSymbol : MaximizeSymbol, -1,
          DT_VCENTER | DT_CENTER | DT_SINGLELINE,
          &max_rect,
          &dtt_opts);
      }
      
      SelectObject(hdc_bitmap, old_font);
      DeleteObject(font);
    }
  }
//  
//  add_rect(canvas, close_rect);
//  canvas.set_color(Color::from_rgb(1, 0, 0), 0.9);
//  canvas.fill();
}

void BasicWin32Window::Impl::paint_themed_caption(HDC hdc_bitmap) {
  if( !self._themed_frame            ||
      !Win32Themes::OpenThemeData    ||
      !Win32Themes::CloseThemeData   ||
      !Win32Themes::GetThemeSysColor ||
      !Win32Themes::GetThemeSysFont  ||
      !Win32Themes::DrawThemeTextEx)
  {
    return;
  }

  int dpi = Win32HighDpi::get_dpi_for_window(self._hwnd);
  if(HANDLE theme = composition_window_theme(dpi)) {
  
    Win32Themes::DTTOPTS dtt_opts;
    memset(&dtt_opts, 0, sizeof(dtt_opts));
    dtt_opts.dwSize    = sizeof(dtt_opts);
    dtt_opts.dwFlags   = DTT_COMPOSITED | DTT_GLOWSIZE | DTT_TEXTCOLOR;
    dtt_opts.iGlowSize = MulDiv(10, dpi, 96);
    dtt_opts.crText    = title_font_color(true, dpi, self._active, self._use_dark_mode);
    
#define MAX_STR_LEN 1024
    WCHAR str[MAX_STR_LEN];
    GetWindowTextW(self._hwnd, str, MAX_STR_LEN);
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
//    if(Win32Version::is_windows_8_or_newer() && !Win32Version::is_windows_10_or_newer()) {
//      center_caption = true;
//    }
    int content_alignment = 0;
    if(SUCCEEDED(Win32Themes::GetThemeInt(theme, 1 /* WP_CAPTION */, 0 /* default state */, 4006, &content_alignment))) {
      // TMT_CONTENTALIGNMENT = 4006
      // CA_LEFT = 0, CA_CENTER = 1, CA_RIGHT = 2
      // TODO: use WP_CAPTION (1), WP_SMALLCAPTION (2), WP_MAXCAPTION (5), or WP_SMALLMAXCAPTION (6)
      
      center_caption = content_alignment != 0;
    }
    
    Win32Themes::MARGINS nc;
    ::get_nc_margins(self._hwnd, &nc, dpi);
    RECT menu, rect, buttons;
    get_system_button_bounds(self._hwnd, &buttons);
    get_system_menu_bounds(self._hwnd, &menu, dpi);
    
    RECT btn_rect = menu;
    btn_rect.left = menu.right;
    
    
    CaptionToolbarPainter toolbar(self, menu.bottom - menu.top, dpi);
    
    btn_rect.top    -= MulDiv(2, dpi, 96);
    btn_rect.bottom += MulDiv(4, dpi, 96);
    
    int i = -1;
    for(auto &button : self.extra_caption_buttons()) {
      ++i;
      
      btn_rect.right += MulDiv(button.dx_96dpi, dpi, 96);
      toolbar.paint(hdc_bitmap, btn_rect, button, toolbar.button_state(i));
      btn_rect.left = btn_rect.right;
    }
    
    int flags = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
    
    if(center_caption) {
      flags |= DT_CENTER;

      RECT calc_rect = {0};

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
      
      int lr = std::max(btn_rect.right - menu.left, buttons.right - buttons.left);
      
      rect.left   = lr;
      rect.right  = buttons.right - lr;

      if(calc_rect.right + MulDiv(8, dpi, 96) > rect.right - rect.left) {
        rect.left = btn_rect.right + MulDiv(4, dpi, 96);
        rect.right = buttons.left;
      }
    }
    else {
      rect.right  = buttons.left;
      rect.left   = btn_rect.right + MulDiv(4, dpi, 96);
    }
    
    DWORD style = GetWindowLongW(self._hwnd, GWL_STYLE);
    if(style & WS_MAXIMIZE) {
      rect.top = Win32HighDpi::get_system_metrics_for_dpi(SM_CYFRAME, dpi) + 
                 Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
    }
    else {
      rect.top = std::max(0L, buttons.top - 1); 
    }
    rect.bottom = nc.cyTopHeight;

    Win32Themes::DrawThemeTextEx(
      theme,
      hdc_bitmap,
      0, 0,
      str, -1,
      flags,
      &rect,
      &dtt_opts);
      
    //SetMapMode(hdc_bitmap, map_mode);

    if(GetClassNameW(self._hwnd, str, MAX_STR_LEN)) {
      WNDCLASSEXW wndcl;
      memset(&wndcl, 0, sizeof(wndcl));

      GetClassInfoExW(GetModuleHandleW(nullptr), str, &wndcl);
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

void BasicWin32Window::Impl::find_all_snappers() {
  self.all_snappers.clear();
  self.all_snapper_positions.length(0);

  self.all_snappers.add(self._hwnd);
  
  WindowMagnetCollector collector{self.all_snappers, self.all_snapper_positions};
  collector.min_level = self.zorder_level();
  collector.reset_dst(self._hwnd);
  collector.visit_all_windows();
  
  unsigned int found = 1;
  for(int rep = 1; rep < 5 && found < self.all_snappers.size(); ++rep) {
    found = self.all_snappers.size();
    
    Hashset<HWND> old_snappers;
    old_snappers.merge(self.all_snappers);
    
    for(auto hwnd : old_snappers.keys()) {
      collector.reset_dst(hwnd);
      collector.visit_all_windows();
    }
  }

  self.all_snappers.remove(self._hwnd);
}

HDWP BasicWin32Window::Impl::move_all_snappers(HDWP hdwp, const RECT &new_bounds) {
  Hashtable<HWND, RECT> new_rects;
  new_rects.set(self._hwnd, new_bounds);
  
  for(const auto &snap : self.all_snapper_positions) {
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
    WindowMagnetics::adjust_snap_rect(snap.dst, &frame);
    POINT dst_touch_pt = to_absolute_point(frame, snap.dst_rel_touch);
    
    frame = src_rect;
    WindowMagnetics::adjust_snap_rect(snap.src, &frame);
    POINT src_touch_pt = to_absolute_point(frame, snap.src_rel_touch);
    
    OffsetRect(&src_rect, dst_touch_pt.x - src_touch_pt.x, dst_touch_pt.y - src_touch_pt.y);
    new_rects.set(snap.src, src_rect);
  }
  
  new_rects.remove(self._hwnd);
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

void BasicWin32Window::Impl::snap_rect_or_pt(RECT *snapping_rect, POINT *pt) {
  RECT current_rect;
  WindowMagnetics::get_snap_rect(self._hwnd, &current_rect);

  snapping_rect->left -=   self.snap_correction_x;
  snapping_rect->right -=  self.snap_correction_x;
  snapping_rect->top -=    self.snap_correction_y;
  snapping_rect->bottom -= self.snap_correction_y;

  if(pt) {
    pt->x -= self.snap_correction_x;
    pt->y -= self.snap_correction_y;
  }

  int old_dx = current_rect.left - snapping_rect->left;
  int old_dy = current_rect.top  - snapping_rect->top;

  WindowMagnetics magnetics {self._hwnd, self.all_snappers};
  magnetics.orig_rect = snapping_rect;
  magnetics.orig_pt = pt;
  magnetics.snap_all_monitors();
  magnetics.snap_all_windows();
  
  for(auto hwnd : self.all_snappers.keys()) {
    RECT rect;
    WindowMagnetics::get_snap_rect(hwnd, &rect);
    rect.left -=   old_dx;
    rect.right -=  old_dx;
    rect.top -=    old_dy;
    rect.bottom -= old_dy;
    
    magnetics.orig_rect = &rect;
    
    magnetics.snap_all_monitors();
    magnetics.snap_all_windows();
  }
  
  self.snap_correction_x = magnetics.dx;
  self.snap_correction_y = magnetics.dy;

  snapping_rect->left +=   magnetics.dx;
  snapping_rect->right +=  magnetics.dx;
  snapping_rect->top +=    magnetics.dy;
  snapping_rect->bottom += magnetics.dy;

  if(pt) {
    pt->x += magnetics.dx;
    pt->y += magnetics.dy;
  }
}

//} ... class BasicWin32Window::Impl

//{ ... class BasicWin32Window::Impl::CaptionToolbarPainter

BasicWin32Window::Impl::CaptionToolbarPainter::CaptionToolbarPainter(BasicWin32Window &self, int h, int dpi)
: self(self),
  height(h),
  dpi(dpi)
{
  symbols_font = CreateFontW(
                   h, 0, 0, 0,
                   FW_NORMAL,
                   FALSE,
                   FALSE,
                   FALSE,
                   DEFAULT_CHARSET,
                   OUT_DEFAULT_PRECIS,
                   CLIP_DEFAULT_PRECIS,
                   ANTIALIASED_QUALITY,//DEFAULT_QUALITY,//
                   DEFAULT_PITCH | FF_DONTCARE,
                   Win32Themes::symbol_font_name());
                           
  toolbar_theme = Win32ControlPainter::win32_painter.get_control_theme(
                    self, ContainerType::PaletteButton, ControlState::Normal, nullptr, nullptr);
}

BasicWin32Window::Impl::CaptionToolbarPainter::~CaptionToolbarPainter() {
  DeleteObject(symbols_font);
}

ControlState BasicWin32Window::Impl::CaptionToolbarPainter::button_state(int i) {
  if(i != self._hit_test_extra_button)
    return ControlState::Normal;
  else if(self._hit_test_mouse_down == self._hit_test_mouse_over)
    return ControlState::PressedHovered;
  else
    return ControlState::Hovered;
}
    
void BasicWin32Window::Impl::CaptionToolbarPainter::paint(HDC hdc, const RECT &rect, const Win32CaptionButton &button, ControlState state) {
  if(button.flags & Win32CaptionButton::Separator) paint_separator( hdc, rect);
  if(button.flags & Win32CaptionButton::Button)    paint_button(    hdc, rect, state);
  if(button.flags & Win32CaptionButton::ProxyIcon) paint_proxy_icon(hdc, rect);
  if(button.label)                                 paint_label(     hdc, rect, button, state);
}

void BasicWin32Window::Impl::CaptionToolbarPainter::paint_separator(HDC hdc, const RECT &rect) {
  int theme_part  = 5; // TP_SEPARATOR
  int theme_state = 1; // TS_NORMAL
  
  SIZE size = {0, 0};
  Win32Themes::GetThemePartSize(toolbar_theme, hdc, theme_part, theme_state, nullptr, Win32Themes::TS_TRUE, &size);
  
  if(size.cx == 0)
    size.cx = rect.right - rect.left;
  
  RECT separator_rect;
  separator_rect.left   = rect.left + (rect.right - rect.left - size.cx) / 2;
  separator_rect.right  = separator_rect.left + size.cx;
  separator_rect.top    = rect.top    + MulDiv(2, dpi, 96);
  separator_rect.bottom = rect.bottom - MulDiv(2, dpi, 96);
  
  Win32Themes::DrawThemeBackground(
    toolbar_theme,
    hdc,
    theme_part,
    theme_state,
    &separator_rect,
    nullptr);
}

void BasicWin32Window::Impl::CaptionToolbarPainter::paint_button(HDC hdc, const RECT &rect, ControlState state) {
  int theme_state = 1;
  switch(state) {
    case ControlState::Pressed:
    case ControlState::PressedHovered: theme_state = 3; break; // TP_PRESSED
    case ControlState::Hot:
    case ControlState::Hovered:        theme_state = 2; break; // TP_HOT
    default:
    case ControlState::Normal:         theme_state = 1; break; // TP_NORMAL
  }
  
  Win32Themes::DrawThemeBackground(
    toolbar_theme,
    hdc,
    1, // TP_BUTTON
    theme_state,
    &rect,
    nullptr);
}

void BasicWin32Window::Impl::CaptionToolbarPainter::paint_proxy_icon(HDC hdc, const RECT &rect) {
  int icon_w = Win32HighDpi::get_system_metrics_for_dpi(SM_CXSMICON, dpi);
  int icon_h = Win32HighDpi::get_system_metrics_for_dpi(SM_CYSMICON, dpi);
  
  HICON icon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(ICO_FILE),
                                 IMAGE_ICON,
                                 icon_w, icon_h,
                                 LR_DEFAULTCOLOR);
  if(icon) {
    DrawIconEx(
      hdc,
      rect.left + (rect.right - rect.left - icon_w) / 2,
      rect.top  + (rect.bottom - rect.top - icon_h) / 2,
      icon,
      icon_w,
      icon_h,
      0,
      nullptr,
      DI_NORMAL);
    
    DestroyIcon(icon);
  }
}

void BasicWin32Window::Impl::CaptionToolbarPainter::paint_label(HDC hdc, RECT rect, const Win32CaptionButton &button, ControlState state) {
  HFONT tmp = nullptr;
  if(button.flags & Win32CaptionButton::UseIconFont) {
    tmp = (HFONT)SelectObject(hdc, symbols_font);
  }
  
  int dx = 0;
  if(state == ControlState::Pressed || state == ControlState::PressedHovered) {
    dx = MulDiv(1, dpi, 96);
    OffsetRect(&rect, dx, 0);
  }
  
  Win32Themes::DTTOPTS dtt_opts = {};
  dtt_opts.dwSize    = sizeof(dtt_opts);
  dtt_opts.dwFlags   = DTT_COMPOSITED | DTT_GLOWSIZE | DTT_TEXTCOLOR;
  dtt_opts.iGlowSize = MulDiv(10, dpi, 96);
  dtt_opts.crText    = self.title_font_color(true, dpi, self._active, self._use_dark_mode);
  
  Win32Themes::DrawThemeTextEx(
    toolbar_theme, //theme, <--- window theme ?
    hdc,
    0, 0,
    button.label, -1,
    DT_CENTER | DT_VCENTER | DT_SINGLELINE,
    &rect,
    &dtt_opts);
  
  if(tmp) 
    (void)SelectObject(hdc, tmp);
}

//} ... class BasicWin32Window::Impl::CaptionToolbarPainter

//{ class Win32BlurBehindWindow ...

bool Win32BlurBehindWindow::suppress_slow_acrylic_blur = false;

Win32BlurBehindWindow::Win32BlurBehindWindow(BasicWin32Window *owner) 
  : base(
      WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP,
      WS_POPUP,// | WS_DISABLED,
      0,
      0,
      1,
      1,
      nullptr),
    _owner(owner)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  RICHMATH_ASSERT(_owner != nullptr);
}

Win32BlurBehindWindow::~Win32BlurBehindWindow() {
  _owner->lost_blur_behind_window(this);
}

void Win32BlurBehindWindow::after_construction() {
  base::after_construction();
  // for debugging purposes:
  SetWindowTextW(_hwnd, L"BlurBehind");
  enable_blur(0xff0000FFu);
}

bool Win32BlurBehindWindow::enable_blur(COLORREF abgr) {
  if(!Win32Themes::SetWindowCompositionAttribute)
    return false;
  
  Win32Themes::AccentPolicy accent_policy = {};
  accent_policy.flags = Win32Themes::AccentFlagMixWithGradientColor;
  accent_policy.gradient_color = abgr;
  accent_policy.animation_id = 0;
  
  accent_policy.accent_state = Win32Themes::AccentState::EnableBlurBehind;
  if(!suppress_slow_acrylic_blur) {
    // Note that on Windows 10, Acrylic Blur Behind is very slow (window would lag when moving) 
    // because it uses a much larger blur radius.
    // On Windows 11 it does not seem to be so bad.
    if(Win32Version::is_windows_10_1803_or_newer()) {
      accent_policy.accent_state = Win32Themes::AccentState::EnableAcrylicBlurBehind;
    }
    
    // TODO: enable "Mica" on Windows 11 ?
  }
  
  Win32Themes::WINCOMPATTRDATA data = {};
  data.attr = Win32Themes::UndocumentedWindowCompositionAttribute::AccentPolicy;
  data.data = &accent_policy;
  data.data_size = sizeof(accent_policy);
  if(!Win32Themes::SetWindowCompositionAttribute(_hwnd, &data)) {
    pmath_debug_print("[Win32BlurBehindWindow: SetWindowCompositionAttribute failed]\n");
    return false;
  }
  
  return true;
}

RECT Win32BlurBehindWindow::blur_bounds(RECT window_rect, const Win32Themes::MARGINS &margins) {
  window_rect.left+=   margins.cxLeftWidth;
  window_rect.right-=  margins.cxRightWidth;
  window_rect.top+=    1;//margins.cyTopHeight;
  window_rect.bottom-= margins.cyBottomHeight;
  
  if(Win32Version::is_windows_11_or_newer()) {
    // Reduce by 1 px, otherwise we see a 1 px wide gap inside DWM painted frame of the ower window
    // That did not happen on Windows 10.
    window_rect.left   -= 1;
    window_rect.right  += 1;
    window_rect.top    -= 1;
    window_rect.bottom += 1;
  }
  
  return window_rect;
}

LRESULT Win32BlurBehindWindow::callback(UINT message, WPARAM wParam, LPARAM lParam) {
  switch(message) {
    case WM_WINDOWPOSCHANGING: 
      on_windowposchanging((WINDOWPOS*)lParam);
      return 0;
  }
  
  return base::callback(message, wParam, lParam);
}

void Win32BlurBehindWindow::on_windowposchanging(WINDOWPOS *lParam) {
  if((lParam->flags & SWP_NOZORDER)) {
    lParam->hwndInsertAfter = _owner->hwnd();
  }
}

void Win32BlurBehindWindow::hide() {
  ShowWindow(_hwnd, SW_HIDE);
}

void Win32BlurBehindWindow::show() {
  RECT rect;
  GetWindowRect(_hwnd, &rect);
  show(rect);
}

void Win32BlurBehindWindow::show(const RECT &owner_rect) {
  Win32Themes::MARGINS margins = {};
  _owner->get_nc_margins(&margins);
  
  RECT rect = blur_bounds(owner_rect, margins);
  
  SetWindowPos(
    _hwnd,
    _owner->hwnd(),
    rect.left, 
    rect.top,
    rect.right - rect.left,
    rect.bottom - rect.top,
    SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void Win32BlurBehindWindow::show_if_owner_visible_on_screen() {
  if(is_window_visible_on_screen(_owner->hwnd())) {
    if(!is_window_visible_on_screen(_hwnd))
      show();
  }
  else {
    if(is_window_visible_on_screen(_hwnd))
      hide();
  }
}

void Win32BlurBehindWindow::colorize(bool active, bool dark_mode) {
  DWORD alpha = 0xFFu;
  if(Win32Themes::use_win10_transparency() && active) {
    alpha = 0xBFu; // 0.75 * 0xFF
  }
  
  Color custom = dark_mode ? CustomTitlebarColorizationDark : CustomTitlebarColorizationLight;
  COLORREF bgr;
  if(custom.is_valid()) {
    bgr = custom.to_bgr24();
  }
  else {
    Win32Themes::ColorizationInfo colorization;
    if(Win32Themes::try_read_win10_colorization(&colorization)) {
      if(active && colorization.has_accent_color_in_active_titlebar) 
        bgr = colorization.accent_color;
      else if(dark_mode)
        bgr = 0x000000u;
      else
        bgr = 0xFFFFFFu;
    }
    else if(active)
      bgr = GetSysColor(COLOR_GRADIENTACTIVECAPTION);
    else
      bgr = GetSysColor(COLOR_GRADIENTINACTIVECAPTION);
  }
  
  enable_blur((alpha << 24u) | bgr);
}

//} ... Win32BlurBehindWindow

//{ class StaticResources ...

StaticResources::StaticResources()
  : window_count(0),
    _virtual_desktop_notification_cookie(0)
{
}

void StaticResources::add_basic_window() {
  if(++window_count != 1)
    return;
  
  _virtual_desktop_notification_cookie = register_notifications(static_cast<IVirtualDesktopNotification_22000*>(this));
}

void StaticResources::remove_basic_window() {
  if(--window_count != 0)
    return;
  
  unregister_notifications(_virtual_desktop_notification_cookie);
  _virtual_desktop_notification_cookie = 0;
  
  virtual_desktop_service.reset();
  Win32TooltipWindow::delete_global_tooltip();

  background_image_cache.clear();
  clear_theme_data();
}

AutoCairoSurface StaticResources::get_background_image() {
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

HANDLE StaticResources::composition_window_theme(int dpi) {
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

void StaticResources::clear_theme_data() {
  if(Win32Themes::CloseThemeData) {
    for(auto &e : composition_window_theme_for_dpi.entries())
      Win32Themes::CloseThemeData(e.value);
    
    composition_window_theme_for_dpi.clear();
  }
}

void StaticResources::need_virtual_desktop_service() {
  if(virtual_desktop_service)
    return;
  
  ComBase<IServiceProvider> service_provider;
  if(HRbool(CoCreateInstance(
      CLSID_ImmersiveShell, nullptr, CLSCTX_LOCAL_SERVER,
      service_provider.iid(), (void**)service_provider.get_address_of()))) 
  {
    HRreport(service_provider->QueryService(CLSID_VirtualDesktopNotificationService, virtual_desktop_service.get_address_of()));
  }
}

DWORD StaticResources::register_notifications(IVirtualDesktopNotification *handler) {
  need_virtual_desktop_service();
  
  DWORD cookie = 0;
  if(virtual_desktop_service) {
    HRreport(virtual_desktop_service->Register(handler, &cookie));
  }
  return cookie;
}

void StaticResources::unregister_notifications(DWORD cookie) {
  if(virtual_desktop_service) {
    HRreport(virtual_desktop_service->Unregister(cookie));
    return;
  }
  if(cookie) {
    HRreport(E_UNEXPECTED);
  }
}

STDMETHODIMP StaticResources::QueryInterface(REFIID iid, void **ppvObject) {
  if(iid == IID_IVirtualDesktopNotification_10240 || iid == IID_IUnknown) {
    auto res = static_cast<IVirtualDesktopNotification_10240*>(this);
    res->AddRef();
    *ppvObject = res;
    return S_OK;
  }
  
  if(iid == IID_IVirtualDesktopNotification_22000) {
    auto res = static_cast<IVirtualDesktopNotification_22000*>(this);
    res->AddRef();
    *ppvObject = res;
    return S_OK;
  }
  
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

STDMETHODIMP StaticResources::VirtualDesktopCreated(IVirtualDesktop *pDesktop) {
  pmath_debug_print("[VirtualDesktopCreated %p]\n", pDesktop);
  return S_OK;
}

STDMETHODIMP StaticResources::VirtualDesktopCreated(IObjectArray *p0, IVirtualDesktop *pDesktop) {
  pmath_debug_print("[VirtualDesktopCreated %p %p]\n", p0, pDesktop);
  return S_OK;
}

STDMETHODIMP StaticResources::VirtualDesktopDestroyBegin(IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) {
  pmath_debug_print("[VirtualDesktopDestroyBegin %p %p]\n", pDesktopDestroyed, pDesktopFallback);
  return S_OK;
}

STDMETHODIMP StaticResources::VirtualDesktopDestroyBegin(IObjectArray *p0, IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) {
  pmath_debug_print("[VirtualDesktopDestroyBegin %p %p %p]\n", p0, pDesktopDestroyed, pDesktopFallback);
  return S_OK;
}

STDMETHODIMP StaticResources::VirtualDesktopDestroyFailed(IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) { 
  pmath_debug_print("[VirtualDesktopDestroyFailed %p %p]\n", pDesktopDestroyed, pDesktopFallback);
  return S_OK;
}

STDMETHODIMP StaticResources::VirtualDesktopDestroyFailed(IObjectArray *p0, IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) {
  pmath_debug_print("[VirtualDesktopDestroyFailed %p %p %p]\n", p0, pDesktopDestroyed, pDesktopFallback);
  return S_OK;
}

STDMETHODIMP StaticResources::VirtualDesktopDestroyed(IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) {
  pmath_debug_print("[VirtualDesktopDestroyed %p %p]\n", pDesktopDestroyed, pDesktopFallback);
  return S_OK;
}

STDMETHODIMP StaticResources::VirtualDesktopDestroyed(IObjectArray *p0, IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) {
  pmath_debug_print("[VirtualDesktopDestroyed %p %p %p]\n", p0, pDesktopDestroyed, pDesktopFallback);
  return S_OK;
}

STDMETHODIMP StaticResources::UnknownProc7(int p0) {
  pmath_debug_print("[UnknownProc7 %d=0x%x]\n", p0, p0);
  return S_OK; // just a notification.
}

STDMETHODIMP StaticResources::VirtualDesktopMoved(IObjectArray *p0, IVirtualDesktop *pDesktop, int nIndexFrom, int nIndexTo) {
  pmath_debug_print("[VirtualDesktopMoved %p %p %d %d]\n", p0, pDesktop, nIndexFrom, nIndexTo);
  return S_OK;
}

STDMETHODIMP StaticResources::VirtualDesktopRenamed(IVirtualDesktop *pDesktop, HSTRING chName) {
  pmath_debug_print("[VirtualDesktopRenamed %p %p]\n", pDesktop, chName);
  return S_OK;
}

STDMETHODIMP StaticResources::ViewVirtualDesktopChanged(IApplicationView *pView) {
  pmath_debug_print("[ViewVirtualDesktopChanged %p]\n", pView);
  //pmath_debug_print("[ViewVirtualDesktopChanged %p, cloaked=%s]\n", pView, is_window_cloaked(_hwnd) ? "yes" : "no");
  //
  //if(_blur_behind_window) _blur_behind_window->show_if_owner_visible_on_screen();
  for(CommonDocumentWindow *win : CommonDocumentWindow::All) {
    if(auto platform_win = dynamic_cast<BasicWin32Window*>(win)) {
      platform_win->virtual_desktop_changed();
    }
  }
  return S_OK;
}

STDMETHODIMP StaticResources::CurrentVirtualDesktopChanged(IVirtualDesktop *pDesktopOld, IVirtualDesktop *pDesktopNew) {
  pmath_debug_print("[CurrentVirtualDesktopChanged %p %p]\n", pDesktopOld, pDesktopNew);
  for(CommonDocumentWindow *win : CommonDocumentWindow::All) {
    if(auto platform_win = dynamic_cast<BasicWin32Window*>(win)) {
      platform_win->virtual_desktop_changed();
    }
  }
  return S_OK;
}

STDMETHODIMP StaticResources::CurrentVirtualDesktopChanged(IObjectArray *p0, IVirtualDesktop *pDesktopOld, IVirtualDesktop *pDesktopNew) {
  pmath_debug_print("[CurrentVirtualDesktopChanged %p %p %p]\n", p0, pDesktopOld, pDesktopNew);
  for(CommonDocumentWindow *win : CommonDocumentWindow::All) {
    if(auto platform_win = dynamic_cast<BasicWin32Window*>(win)) {
      platform_win->virtual_desktop_changed();
    }
  }
  return S_OK;
}

STDMETHODIMP StaticResources::VirtualDesktopWallpaperChanged(IVirtualDesktop *pDesktop, HSTRING *chPath) {
  pmath_debug_print("[VirtualDesktopWallpaperChanged %p %p]\n", pDesktop, chPath);
  return S_OK;
}

//} ... class StaticResources

//{ class WindowMagnetics ...

WindowMagnetics::WindowMagnetics(HWND _src, Hashset<HWND> &_dont_snap)
: dx{0}, dy{0}, 
  src{_src},
  dont_snap{_dont_snap}
{
  int dpi = Win32HighDpi::get_dpi_for_window(src);
  max_dx = max_dy = MulDiv(10, dpi, 96);
}

void WindowMagnetics::get_snap_margins(HWND hwnd, Win32Themes::MARGINS *margins) {
  memset(margins, 0, sizeof(Win32Themes::MARGINS));
  
  if(Win32Version::is_windows_10_or_newer()) {
    ::get_nc_margins(hwnd, margins, Win32HighDpi::get_dpi_for_window(hwnd));
    margins->cyTopHeight    = 0;
    margins->cxLeftWidth    = 1 - margins->cxLeftWidth;
    margins->cxRightWidth   = 1 - margins->cxRightWidth;
    margins->cyBottomHeight = 1 - margins->cyBottomHeight;
  }
}

void WindowMagnetics::adjust_snap_rect(HWND hwnd, RECT *rect) {
  Win32Themes::MARGINS margins;
  get_snap_margins(hwnd, &margins);
  rect->left-= margins.cxLeftWidth;
  rect->top-= margins.cyTopHeight;
  rect->right+= margins.cxLeftWidth;
  rect->bottom+= margins.cyBottomHeight;
}

void WindowMagnetics::get_snap_rect(HWND hwnd, RECT *rect) {
  GetWindowRect(hwnd, rect);
  adjust_snap_rect(hwnd, rect);
}

bool WindowMagnetics::snap_inside(const RECT &outer) {
  RICHMATH_ASSERT(orig_rect != nullptr);
  
  bool have_snapped_x = false;
  bool have_snapped_y = false;

  if( (outer.bottom >= orig_rect->top && outer.top    <= orig_rect->bottom) ||
      (outer.top    >= orig_rect->top && outer.bottom <= orig_rect->bottom))
  {
    int dx_left;
    int dx_right;
    if(orig_pt) {
      dx_left  = outer.left - orig_pt->x;
      dx_right = outer.right - orig_pt->x;
    }
    else {
      dx_left  = outer.left - orig_rect->left;
      dx_right = outer.right - orig_rect->right;
    }
    
    if(abs(dx_left) < max_dx) {
      if(dx_left == dx_right || abs(dx_left) < abs(dx_right)) {
        dx = max_dx = dx_left;
        have_snapped_x = true;
      }
      else if(abs(dx_right) < abs(dx_left)){
        dx = max_dx = dx_right;
        have_snapped_x = true;
      }
    }
    else if(abs(dx_right) < max_dx) {
      dx = max_dx = dx_right;
      have_snapped_x = true;
    }
  }

  if( (outer.right >= orig_rect->left && outer.left  <= orig_rect->right) ||
      (outer.left  >= orig_rect->left && outer.right <= orig_rect->right))
  {
    int dy_top;
    int dy_bottom;
    if(orig_pt) {
      dy_top    = outer.top    - orig_pt->y;
      dy_bottom = outer.bottom - orig_pt->y;
    }
    else {
      dy_top    = outer.top    - orig_rect->top;
      dy_bottom = outer.bottom - orig_rect->bottom;
    }
    
    if(abs(dy_top) < max_dy) {
      if(dy_top == dy_bottom || abs(dy_top) < abs(dy_bottom)) {
        dy = max_dy = dy_top;
        have_snapped_y = true;
      }
      else if(abs(dy_bottom) < abs(dy_top)) {
        dy = max_dy = dy_bottom;
        have_snapped_y = true;
      }
    }
    else if(abs(dy_bottom) < max_dy) {
      dy = max_dy = dy_bottom;
      have_snapped_y = true;
    }
  }

  if(have_snapped_x && !have_snapped_y) {
    int dx_top    = outer.top - orig_rect->bottom;
    int dx_bottom = outer.bottom - orig_rect->top;
    
    if(abs(dx_top) < max_dy) {
      if(dx_top == dx_bottom || abs(dx_top) < abs(dx_bottom)) {
        dy = max_dy = dx_top;
        have_snapped_y = true;
      }
      else if(abs(dx_bottom) < abs(dx_top)) {
        dy = max_dy = dx_bottom;
        have_snapped_y = true;
      }
    }
    else if(abs(dx_bottom) < max_dy) {
      dy = max_dy = dx_bottom;
      have_snapped_y = true;
    }
  }
  else if(have_snapped_y && !have_snapped_x) {
    int dx_left  = outer.left  - orig_rect->right;
    int dx_right = outer.right - orig_rect->left;
    
    if(abs(dx_left) < max_dx) {
      if(dx_left == dx_right || abs(dx_left) < abs(dx_right)) {
        dx = max_dx = dx_left;
        have_snapped_x = true;
      }
      else if(abs(dx_right) < abs(dx_left)) {
        dx = max_dx = dx_right;
        have_snapped_x = true;
      }
    }
    else if(abs(dx_right) < max_dx) {
      dx = max_dx = dx_right;
      have_snapped_x = true;
    }
  }

  return have_snapped_x || have_snapped_y;
}

void WindowMagnetics::snap_all_monitors() {
  enum_display_monitors(nullptr, nullptr, [&](HMONITOR hMonitor, HDC hdcMonitor, RECT *lprcMonitor) {
    MONITORINFO moninfo;

    memset(&moninfo, 0, sizeof(moninfo));
    moninfo.cbSize = sizeof(moninfo);

    if(GetMonitorInfo(hMonitor, &moninfo)) 
      snap_inside(moninfo.rcWork);
  });
}

void WindowMagnetics::snap_all_windows() {
  enum_thread_windows([&](HWND hwnd) {
    if(hwnd == src || !is_window_visible_on_screen(hwnd) || dont_snap.contains(hwnd))
      return;
    
    if(auto widget = BasicWin32Widget::from_hwnd(hwnd)) {
      if(dynamic_cast<Win32BlurBehindWindow*>(widget))
        return;
      if(dynamic_cast<Win32AttachedPopupWindow*>(widget))
        return;
    }
    
    RECT rect;
    get_snap_rect(hwnd, &rect);
    
    int tmp = rect.left;
    rect.left = rect.right;
    rect.right = tmp;

    tmp = rect.top;
    rect.top = rect.bottom;
    rect.bottom = tmp;

    snap_inside(rect);
  });
}

//} ... class WindowMagnetics

//{ class WindowMagnetCollector ...

void WindowMagnetCollector::reset_dst(HWND _dst) {
  dst = _dst;
  WindowMagnetics::get_snap_rect(dst, &dst_rect);
}

WindowMagnetCollector::WindowMagnetCollector(Hashset<HWND> &_snappers, Array<SnapPosition> &_snapper_positions) 
: snappers{_snappers},
  snapper_positions{_snapper_positions}
{}

void WindowMagnetCollector::visit_all_windows() {
  enum_thread_windows([&](HWND hwnd) {
    if(hwnd == dst || !is_window_visible_on_screen(hwnd))
      return;
      
    if(auto widget = BasicWin32Widget::from_hwnd(hwnd)) {
      if(dynamic_cast<Win32BlurBehindWindow*>(widget))
        return;
      if(dynamic_cast<Win32AttachedPopupWindow*>(widget))
        return;
      
      if(auto win = dynamic_cast<BasicWin32Window*>(widget)) {
        if(win->zorder_level() <= min_level)
          return;
      }
    }
    
    RECT rect;
    WindowMagnetics::get_snap_rect(hwnd, &rect);
    
    RECT touchRegion;
    if(!touching_rectangles(&touchRegion, dst_rect, rect))
      return;
    
    if(snappers.contains(hwnd))
      return;
    
    POINT touch_center = get_rect_center(touchRegion);
    
    SnapPosition snap { 
      hwnd,
      to_relative_point(rect, touch_center),
      dst, 
      to_relative_point(dst_rect, touch_center), 
    };
    
    snappers.add(hwnd);
    snapper_positions.add(PMATH_CPP_MOVE(snap));
  });
}

//} ... class WindowMagnetCollector

static bool is_window_cloaked(HWND hwnd) {
  BOOL is_cloaked = FALSE;
  return Win32Themes::DwmGetWindowAttribute &&
         SUCCEEDED(Win32Themes::DwmGetWindowAttribute(hwnd, Win32Themes::DWMWA_CLOAKED, &is_cloaked, sizeof(is_cloaked))) &&
         is_cloaked;
}

static bool is_window_visible_on_screen(HWND hwnd) {
  return IsWindowVisible(hwnd) & !is_window_cloaked(hwnd);
}

static POINT point_from_lparam(LPARAM lParam) {
  return POINT { (short)LOWORD(lParam), (short)HIWORD(lParam) };
}

static bool touching_rectangles(RECT *touchRegion, const RECT &rect1, const RECT &rect2) {
  touchRegion->left   = std::max(rect1.left,   rect2.left);
  touchRegion->right  = std::min(rect1.right,  rect2.right);
  touchRegion->top    = std::max(rect1.top,    rect2.top);
  touchRegion->bottom = std::min(rect1.bottom, rect2.bottom);
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

static HBRUSH create_solid_brush_with_alpha(Color color, int alpha) {
  // https://delphihaven.wordpress.com/2010/09/06/custom-drawing-on-glass-2/
  BITMAPINFO bmi = {0};
  bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
  bmi.bmiHeader.biWidth = 1;
  bmi.bmiHeader.biHeight = 1;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  
  int rgb = color.to_rgb24();
  bmi.bmiColors[0].rgbBlue  = (rgb & 0x0000FF);
  bmi.bmiColors[0].rgbGreen = (rgb & 0x00FF00) >> 8;
  bmi.bmiColors[0].rgbRed   = (rgb & 0xFF0000) >> 16;
  bmi.bmiColors[0].rgbReserved = 0xFF & alpha;
  
  return CreateDIBPatternBrushPt(&bmi, DIB_RGB_COLORS);
}
