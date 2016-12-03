#include <gui/win32/win32-scrollbar-overlay.h>

#include <stdio.h>


using namespace richmath;

namespace richmath {
  class Win32ScrollBarOverlayImpl {
    private:
      Win32ScrollBarOverlay &self;
      
    public:
      Win32ScrollBarOverlayImpl(Win32ScrollBarOverlay &_self)
        : self(_self)
      {
      }
      
      HWND scrollbar_owner() {
        return *self.scrollbar_owner_ptr;
      }
      
    public:
      void update_regions() {
        if(!IsWindowVisible(self.hwnd()))
          return;
        
        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask  = SIF_RANGE | SIF_PAGE;
        GetScrollInfo(scrollbar_owner(), SB_VERT, &si);
        
        fprintf(stderr, "[update_regions %d..%d, %d]\n", si.nMin, si.nMax, si.nPage);
        
        if(si.nMax - si.nMin <= si.nPage)
          return;
        
        float range = (si.nMax - si.nMin) / self.scale;
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
      
    private:
      HRGN get_indicator_region(const Indicator &indicator, const RECT &rect, float range) {
        int w = rect.right - rect.left;
        int h = rect.bottom - rect.top;
        
        if(w <= 0 || h <= 0)
          return nullptr;
          
        if(indicator.position < 0 || indicator.position > range)
          return nullptr;
          
        int dx = w / 4;
        int dy = w / 4;
        int x = rect.left + w / 2 - dx / 2;
        int y = (int)(dy / 2 + (h - dy) * indicator.position / range);
        
        fprintf(stderr, "[indic (%d,%d) %d x %d]", x,y,dx,dy);
        
        return CreateRectRgn(x, y, x + dx, y + dy);
      }
      
    public:
      void on_paint(WPARAM wParam, LPARAM lParam) {
        PAINTSTRUCT paintStruct;
        HDC dc = BeginPaint(self.hwnd(), &paintStruct);
        SetLayout(dc, 0);
        on_paint(dc, true);
        EndPaint(self.hwnd(), &paintStruct);
      }
      
      void on_paint(HDC dc, bool from_wmpaint) {
        RECT rect;
        GetClientRect(self.hwnd(), &rect);
        // TODO: use indicator colors
        HBRUSH brush = CreateSolidBrush(0xFF0000);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
      }
      
    public:
      void on_scrollbar_owner_windowposchanging(WINDOWPOS *wpos) {
        update_bounds(wpos->x, wpos->y, wpos->cx, wpos->cy);
      }
      
      void on_scrollbar_owner_size() {
        RECT outer;
        GetWindowRect(scrollbar_owner(), &outer);
        
        HWND parent = GetParent(self.hwnd());
        
        POINT pt{outer.left, outer.top};
        ScreenToClient(parent, &pt);
        
        update_bounds(pt.x, pt.y, outer.right - outer.left, outer.bottom - outer.top);
      }
      
    private:
      void update_bounds(int x, int y, int width, int height) {
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
        
        fprintf(
          stderr,
          "[update overlay bounds: (%d, %d) %d x %d, %d indicators]\n",
          new_overlay_left,
          new_overlay_top,
          new_overlay_width,
          new_overlay_height,
          self.indicators.length());
        
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
  };
}

//{ class Win32ScrollbarOverlay ...

Win32ScrollBarOverlay::Win32ScrollBarOverlay(HWND *parent_ptr, HWND *scrollbar_owner_ptr)
  : BasicWin32Widget(0, WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, parent_ptr),
    scrollbar_owner_ptr(scrollbar_owner_ptr),
    scale(1.0f)
{
}

void Win32ScrollBarOverlay::set_scale(float _scale) {
  scale = _scale;
}

void Win32ScrollBarOverlay::update() {
  Win32ScrollBarOverlayImpl(*this).on_scrollbar_owner_size();
  Win32ScrollBarOverlayImpl(*this).update_regions();
}

void Win32ScrollBarOverlay::clear() {
  indicators.length(0);
}

void Win32ScrollBarOverlay::add(float position, int style) {
  indicators.add(Indicator{position, (unsigned)style});
}

void Win32ScrollBarOverlay::handle_scrollbar_owner_callback(UINT message, WPARAM wParam, LPARAM lParam) {
  switch(message) {
    case WM_WINDOWPOSCHANGING:
      Win32ScrollBarOverlayImpl(*this).on_scrollbar_owner_windowposchanging((WINDOWPOS*)lParam);
      return;
      
    case WM_SIZE:
      Win32ScrollBarOverlayImpl(*this).on_scrollbar_owner_size();
      return;
  }
}

LRESULT Win32ScrollBarOverlay::callback(UINT message, WPARAM wParam, LPARAM lParam) {
  switch(message) {
    case WM_NCHITTEST:
      return HTTRANSPARENT;
      
    case WM_PAINT:
      Win32ScrollBarOverlayImpl(*this).on_paint(wParam, lParam);
      return 0;
  }
  
  return BasicWin32Widget::callback(message, wParam, lParam);
}

//} ... class Win32ScrollbarOverlay
