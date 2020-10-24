#include <gui/win32/win32-attached-popup-window.h>

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

#include <algorithm>


namespace richmath {
  class Win32AttachedPopupWindow::Impl {
    public:
      Impl(Win32AttachedPopupWindow &self) : self{self} {}
      
      static void init_window_class();
      static HWND *get_owner_hwnd_location(Document *doc);
      
      Win32Widget *owner_widget();
      bool find_anchor_screen_position(POINT &pos);
      
      void on_windowposchanged(const WINDOWPOS &wp);

      static const wchar_t class_name[];
      
    private:
      Win32AttachedPopupWindow &self;
  };
}

using namespace richmath;


const wchar_t Win32AttachedPopupWindow::Impl::class_name[] = L"RichmathWin32Popup";

//{ class Win32AttachedPopupWindow ...

Win32AttachedPopupWindow::Win32AttachedPopupWindow(Document *owner, Box *anchor) 
  : base(
    new Document(),
    WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
    WS_POPUP | WS_CLIPCHILDREN | WS_BORDER,
    0,
    0,
    100,
    100,
    Impl::get_owner_hwnd_location(owner)),
  _active{false},
  _best_width{1},
  _best_height{1}
{
  pmath_debug_print("[new Win32AttachedPopupWindow %p]\n", this);
  Impl::init_window_class();
  set_window_class_name(Impl::class_name);
  
  owner_document(owner);
  source_box(anchor);
  
  _autohide_vertical_scrollbar = true;
}

Win32AttachedPopupWindow::~Win32AttachedPopupWindow() {
  pmath_debug_print("[Win32AttachedPopupWindow %p destroyed]\n", this);
  if(Document *owner = owner_document()) 
    owner->popup_window_closed(document());
}

void Win32AttachedPopupWindow::after_construction() {
  base::after_construction();
  
  Style::reset(document()->style, "AttachedPopupWindow");
  
  if(Document *owner = owner_document()) {
    document()->stylesheet(owner->stylesheet());
  }
  
  document()->style->set(Visible,                         true);
  document()->style->set(InternalHasModifiedWindowOption, true);
  document()->select(nullptr, 0, 0);
}

void Win32AttachedPopupWindow::close() {
  pmath_debug_print("[Win32AttachedPopupWindow %p: close]\n", this);
  //SendMessageW(_hwnd, WM_CLOSE, 0, 0);
  on_close();
}
    
void Win32AttachedPopupWindow::invalidate_options() {
  base::invalidate_options();
  
  bool owner_visible = false;
  if(Win32Widget *wid = Impl(*this).owner_widget()) {
    owner_visible = !!IsWindowVisible(wid->hwnd());
  }
  
  if(document()->get_style(Visible, true) && owner_visible) {
    if(!IsWindowVisible(_hwnd)) {
      SetWindowPos(
        _hwnd, nullptr,
        0, 0, 1, 1,
        SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
  }
  else {
    if(IsWindowVisible(_hwnd))
      SetWindowPos(
        _hwnd, nullptr,
        0, 0, 1, 1,
        SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
  
  anchor_location_changed();
}

void Win32AttachedPopupWindow::anchor_location_changed() {
  Win32Widget *owner_wid = Impl(*this).owner_widget();
  if(!owner_wid) {
    pmath_debug_print("[Win32AttachedPopupWindow: lost owner window]\n");
    close();
    return;
  }
  
  if(Box *anchor = source_box()) {
    POINT pos;
    if(Impl(*this).find_anchor_screen_position(pos)) {
      RECT rect;
      GetWindowRect(_hwnd, &rect);
      int width  = rect.right - rect.left;
      int height = rect.bottom - rect.top;
      
      if(HMONITOR hmon = MonitorFromPoint(pos, MONITOR_DEFAULTTONEAREST)) {
        MONITORINFO monitor_info;
        memset(&monitor_info, 0, sizeof(monitor_info));
        monitor_info.cbSize = sizeof(monitor_info);
        
        if(GetMonitorInfo(hmon, &monitor_info)) {
          width  = std::max(1, std::min(_best_width,  (int)(monitor_info.rcWork.right - pos.x)));
          height = std::max(1, std::min(_best_height, (int)(monitor_info.rcWork.bottom - pos.y)));
        }
      }
      
      UINT flags = SWP_NOACTIVATE | SWP_NOZORDER;
      if(width == rect.right - rect.left && height == rect.bottom - rect.top) {
        flags |= SWP_NOSIZE;
      }
      
      SetWindowPos(
        _hwnd, nullptr,
        pos.x, pos.y, width, height,
        flags);
    }
  }
  else {
    pmath_debug_print("[Win32AttachedPopupWindow: lost anchor]\n");
    close();
  }
}

void Win32AttachedPopupWindow::paint_canvas(Canvas &canvas, bool resize_only) {
  base::paint_canvas(canvas, resize_only);
  
  int old_bh = _best_height;
  int old_bw = _best_width;
  
  _best_height = (int)floorf(document()->extents().height() * scale_factor() + 0.5);
  _best_width  = (int)floorf(document()->unfilled_width     * scale_factor() + 0.5);
  
  if(_best_height < 1)
    _best_height = 1;
    
  if(_best_width < 1)
    _best_width = 1;
    
  RECT outer, inner;
  GetWindowRect(_hwnd, &outer);
  GetClientRect(_hwnd, &inner);
  
  _best_width +=  outer.right  - outer.left - inner.right  + inner.left;
  _best_height += outer.bottom - outer.top  - inner.bottom + inner.top;
  
  if(old_bw != _best_width || old_bh != _best_height) {
    anchor_location_changed();
  }
}

void Win32AttachedPopupWindow::on_close() {
  if(Document *owner = owner_document()) 
    owner->popup_window_closed(document());
  
  base::on_close();
}

LRESULT Win32AttachedPopupWindow::callback(UINT message, WPARAM wParam, LPARAM lParam) {
  if(!initializing()) {
    switch(message) {
      case WM_NCACTIVATE: {
        _active = wParam;
      } break;
      
      case WM_ACTIVATEAPP:
        if(!wParam) {
          pmath_debug_print("[Win32AttachedPopupWindow: deactivated app]\n");
          //close();
        }
        break;
        
      case WM_WINDOWPOSCHANGED: 
        Impl(*this).on_windowposchanged(*(const WINDOWPOS*)lParam);
        break;
    }
  }
  
  return base::callback(message, wParam, lParam);
}

//} ... class Win32AttachedPopupWindow

//{ class Win32AttachedPopupWindow::Impl ...

void Win32AttachedPopupWindow::Impl::init_window_class() {
  static bool window_class_initialized = false;
  if(window_class_initialized)
    return;
    
  WNDCLASSEXW wincl;
  
  memset(&wincl, 0, sizeof(wincl));
  
  wincl.cbSize        = sizeof(wincl);
  wincl.hInstance     = GetModuleHandle(0);
  wincl.lpszClassName = class_name;
  wincl.lpfnWndProc   = BasicWin32Widget::window_proc;
  wincl.style         = CS_DROPSHADOW;
  
  if(!RegisterClassExW(&wincl)) {
    perror("Win32AttachedPopupWindow::Impl::init_window_class() failed\n");
    abort();
  }
  
  window_class_initialized = true;
}

HWND *Win32AttachedPopupWindow::Impl::get_owner_hwnd_location(Document *doc) {
  if(!doc)
    return nullptr;
  
  auto wnd = dynamic_cast<Win32Widget*>(doc->native());
  if(!wnd)
    return nullptr;
  
  return &wnd->hwnd();
}

Win32Widget *Win32AttachedPopupWindow::Impl::owner_widget() {
  Document *owner = self.owner_document();
  if(Document *owner = self.owner_document())
    return dynamic_cast<Win32Widget*>(owner->native());
  
  return nullptr;
}

bool Win32AttachedPopupWindow::Impl::find_anchor_screen_position(POINT &pos) {
  pos = {};
  
  Win32Widget *owner_wid = owner_widget();
  if(!owner_wid)
    return false;
  
  if(!IsWindowVisible(owner_wid->hwnd())) 
    return false;
  
  Box *anchor = self.source_box();
  if(!anchor)
    return false;
  
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  anchor->transformation(nullptr, &mat);
  
  Point anchor_point = {0, anchor->extents().descent};
  anchor_point = Canvas::transform_point(mat, anchor_point);
  anchor_point.x *= owner_wid->scale_factor();
  anchor_point.y *= owner_wid->scale_factor();
  
  pos = { (int)floor(anchor_point.x + 0.5), (int)floor(anchor_point.y + 0.5) };
  pos.x -= GetScrollPos(owner_wid->hwnd(), SB_HORZ);
  pos.y -= GetScrollPos(owner_wid->hwnd(), SB_VERT);
  return !!ClientToScreen(owner_wid->hwnd(), &pos);
}

void Win32AttachedPopupWindow::Impl::on_windowposchanged(const WINDOWPOS &wp) {
  if(wp.flags & SWP_HIDEWINDOW) {
    self.document()->invalidate_popup_window_positions();
    return;
  }
  
  if((wp.flags & SWP_NOSIZE) && !(wp.flags & SWP_NOMOVE)) {
    self.document()->invalidate_popup_window_positions();
    return;
  }
}

//} ... class Win32AttachedPopupWindow::Impl
