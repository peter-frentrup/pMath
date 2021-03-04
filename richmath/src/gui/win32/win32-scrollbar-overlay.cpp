#include <gui/win32/win32-scrollbar-overlay.h>

#include <gui/win32/api/win32-themes.h>

using namespace richmath;

namespace richmath {
  class Win32ScrollBarOverlay::Impl {
    private:
      Win32ScrollBarOverlay &self;
      
    public:
      Impl(Win32ScrollBarOverlay &self) : self(self) {}
      
      HWND scrollbar_owner() { return *self.scrollbar_owner_ptr; }
      
    public:
      void update_regions();
      
    private:
      float get_range();
      HRGN get_indicator_region(const Indicator &indicator, const RECT &rect, float range);
      
    public:
      void on_paint(WPARAM wParam, LPARAM lParam);
      void on_paint(HDC dc, bool from_wmpaint);
      
    public:
      void on_scrollbar_owner_windowposchanging(WINDOWPOS *wpos);
      void on_scrollbar_owner_size();
      
    private:
      void update_bounds(int x, int y, int width, int height);
  };
}

//{ class Win32ScrollBarOverlay ...

Win32ScrollBarOverlay::Win32ScrollBarOverlay(HWND *parent_ptr, HWND *scrollbar_owner_ptr)
  : base(
      Win32Themes::is_windows_8_or_newer() ? WS_EX_LAYERED : 0,
      WS_CHILD | WS_VISIBLE, 
      0, 
      0, 
      1, 
      1, 
      parent_ptr),
    scrollbar_owner_ptr(scrollbar_owner_ptr),
    scale(1.0f)
{
}

void Win32ScrollBarOverlay::after_construction() {
  base::after_construction();
  
  // only supported for child windows on Windows 8 or later:
  if(GetWindowLong(_hwnd, GWL_EXSTYLE) & WS_EX_LAYERED)
    SetLayeredWindowAttributes(_hwnd, CLR_NONE, 0xFF, LWA_ALPHA);
}

void Win32ScrollBarOverlay::set_scale(float _scale) {
  scale = _scale;
}

void Win32ScrollBarOverlay::update() {
  Impl(*this).on_scrollbar_owner_size();
  Impl(*this).update_regions();
}

void Win32ScrollBarOverlay::clear() {
  indicators.length(0);
}

void Win32ScrollBarOverlay::add(float position, Color color, IndicatorLane lane) {
  if(color)
    indicators.add(Indicator{position, (unsigned)color.to_rgb24(), (unsigned)lane});
}

void Win32ScrollBarOverlay::handle_scrollbar_owner_callback(UINT message, WPARAM wParam, LPARAM lParam) {
  switch(message) {
    case WM_WINDOWPOSCHANGING:
      Impl(*this).on_scrollbar_owner_windowposchanging((WINDOWPOS*)lParam);
      return;
      
    case WM_SIZE:
      Impl(*this).on_scrollbar_owner_size();
      return;
  }
}

LRESULT Win32ScrollBarOverlay::callback(UINT message, WPARAM wParam, LPARAM lParam) {
  switch(message) {
    case WM_NCHITTEST:
      return HTTRANSPARENT;
      
    case WM_ERASEBKGND:
      return 1;
      
    case WM_PAINT:
      Impl(*this).on_paint(wParam, lParam);
      return 0;
  }
  
  return base::callback(message, wParam, lParam);
}

//} ... class Win32ScrollBarOverlay

//{ class Win32ScrollBarOverlay::Impl ...

void Win32ScrollBarOverlay::Impl::update_regions() {
  if(!IsWindowVisible(self.hwnd()))
    return;
    
  float range = get_range();
  if(range <= 0)
    return;
    
  HRGN region = CreateRectRgn(0, 0, 0, 0);
  
  RECT rect;
  GetClientRect(self.hwnd(), &rect);
  
  for(auto indicator : self.indicators) {
    if(auto rgn = get_indicator_region(indicator, rect, range)) {
      CombineRgn(region, region, rgn, RGN_OR);
      DeleteObject(rgn);
    }
  }
  
  SetWindowRgn(self.hwnd(), region, FALSE);
  InvalidateRect(self.hwnd(), nullptr, TRUE);
}

float Win32ScrollBarOverlay::Impl::get_range() {
  SCROLLINFO si;
  si.cbSize = sizeof(si);
  si.fMask  = SIF_RANGE | SIF_PAGE;
  GetScrollInfo(scrollbar_owner(), SB_VERT, &si);
  
  if(si.nMax - si.nMin <= si.nPage)
    return 0.0;
    
  return (si.nMax - si.nMin) / self.scale;
}

HRGN Win32ScrollBarOverlay::Impl::get_indicator_region(const Indicator &indicator, const RECT &rect, float range) {
  int w = rect.right - rect.left;
  int h = rect.bottom - rect.top;
  
  if(w <= 0 || h <= 0)
    return nullptr;
    
  if(indicator.position < 0 || indicator.position > range)
    return nullptr;
  
  int x, dx, dy;
  switch((IndicatorLane)indicator.lane) {
    case IndicatorLane::All:
      dx = w;
      dy = w / 8;
      x = rect.left;
      break;
    case IndicatorLane::Middle:
    default:
      dx = w / 3;
      dy = w / 3;
      x = rect.left + w / 2 - dx / 2; 
      break;
  }
  
  if(dx < 1) dx = 1;
  if(dy < 1) dy = 1;
  
  int y = (int)(-dy / 2 + h * indicator.position / range);
  
  return CreateRectRgn(x, y, x + dx, y + dy);
}

void Win32ScrollBarOverlay::Impl::on_paint(WPARAM wParam, LPARAM lParam) {
  PAINTSTRUCT paintStruct;
  HDC dc = BeginPaint(self.hwnd(), &paintStruct);
  SetLayout(dc, 0);
  on_paint(dc, true);
  EndPaint(self.hwnd(), &paintStruct);
}

void Win32ScrollBarOverlay::Impl::on_paint(HDC dc, bool from_wmpaint) {
  RECT rect;
  GetClientRect(self.hwnd(), &rect);
  
  float range = get_range();
  if(range <= 0)
    return;
    
  for(auto indicator : self.indicators) {
    if(auto rgn = get_indicator_region(indicator, rect, range)) {
      unsigned color = (  (indicator.color & 0xFF0000) >> 16)
                       |  (indicator.color & 0x00FF00)
                       | ((indicator.color & 0x0000FF) << 16);
      HBRUSH brush = CreateSolidBrush(color);
      FillRgn(dc, rgn, brush);
      DeleteObject(rgn);
      DeleteObject(brush);
    }
  }
}

void Win32ScrollBarOverlay::Impl::on_scrollbar_owner_windowposchanging(WINDOWPOS *wpos) {
  update_bounds(wpos->x, wpos->y, wpos->cx, wpos->cy);
}

void Win32ScrollBarOverlay::Impl::on_scrollbar_owner_size() {
  RECT outer;
  GetWindowRect(scrollbar_owner(), &outer);
  
  HWND parent = GetParent(self.hwnd());
  
  POINT pt{outer.left, outer.top};
  ScreenToClient(parent, &pt);
  
  update_bounds(pt.x, pt.y, outer.right - outer.left, outer.bottom - outer.top);
}

void Win32ScrollBarOverlay::Impl::update_bounds(int x, int y, int width, int height) {
  RECT client;
  GetClientRect(scrollbar_owner(), &client);
  
  RECT outer;
  GetWindowRect(scrollbar_owner(), &outer);
  
  POINT pt{0, 0};
  ClientToScreen(scrollbar_owner(), &pt);
  
  int margin_left   = pt.x - outer.left;
  int margin_right  = outer.right - pt.x - client.right;
  int margin_top    = pt.y - outer.top;
  int margin_bottom = outer.bottom - pt.y - client.bottom;
  
  int v_arrow_height = GetSystemMetrics(SM_CYVSCROLL);
  
  RECT old_overlay;
  GetWindowRect(self.hwnd(), &old_overlay);
  
  int new_overlay_left   = x + width - margin_right;
  int new_overlay_top    = y + margin_top + v_arrow_height;
  int new_overlay_width  = margin_right;
  int new_overlay_height = height - margin_bottom - margin_top - 2 * v_arrow_height;
  
  bool same_pos = new_overlay_left == old_overlay.left &&
                  new_overlay_top == old_overlay.top;
  bool same_size = new_overlay_width == old_overlay.right - old_overlay.left &&
                   new_overlay_height == old_overlay.bottom - old_overlay.top;
                   
  if(!same_pos || !same_size) {
    UINT flags = SWP_NOACTIVATE | SWP_NOZORDER;
    if(same_pos)
      flags |= SWP_NOMOVE;
    if(same_size)
      flags |= SWP_NOSIZE;
      
    SetWindowPos(
      self.hwnd(),
      nullptr,
      new_overlay_left,
      new_overlay_top,
      new_overlay_width,
      new_overlay_height,
      flags);
  }
}

//} ... class Win32ScrollBarOverlay::Impl
