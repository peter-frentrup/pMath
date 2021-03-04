#include <gui/win32/win32-attached-popup-window.h>

#include <gui/documents.h>
#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/win32-control-painter.h>

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

#include <algorithm>
#include <cmath>


namespace richmath {
  namespace strings {
    extern String AttachedPopupWindow;
  }
  
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
    WS_POPUP | WS_CLIPCHILDREN,// | WS_BORDER,
    0,
    0,
    100,
    100,
    Impl::get_owner_hwnd_location(owner)),
  _active{false},
  _best_width{1},
  _best_height{1}
{
  Impl::init_window_class();
  set_window_class_name(Impl::class_name);
  
  owner_document(owner);
  source_box(anchor);
  
  _autohide_vertical_scrollbar = true;
}

Win32AttachedPopupWindow::~Win32AttachedPopupWindow() {
  if(Document *owner = owner_document()) 
    owner->popup_window_closed(document());
}

void Win32AttachedPopupWindow::after_construction() {
  base::after_construction();
  
  Style::reset(document()->style, strings::AttachedPopupWindow);
  
  if(Document *owner = owner_document()) {
    document()->stylesheet(owner->stylesheet());
  }
  
  document()->style->set(Visible,                         true);
  document()->style->set(InternalHasModifiedWindowOption, true);
  document()->select(nullptr, 0, 0);
}

void Win32AttachedPopupWindow::close() {
  //SendMessageW(_hwnd, WM_CLOSE, 0, 0);
  on_close();
}
    
void Win32AttachedPopupWindow::invalidate_options() {
  base::invalidate_options();
  
  switch(document()->get_own_style(WindowFrame)) {
    default:
    case WindowFrameNone:
      SetWindowLongW(_hwnd, GWL_STYLE, GetWindowLongW(_hwnd, GWL_STYLE) & ~WS_BORDER);
      break;
    
    case WindowFrameThin:
      SetWindowLongW(_hwnd, GWL_STYLE, GetWindowLongW(_hwnd, GWL_STYLE) | WS_BORDER);
      break;
  }
  
  invalidate_source_location();
}

bool Win32AttachedPopupWindow::is_using_dark_mode() {
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
  
  if(Document *owner = owner_document()) {
    bool result = false;
    if(recursion < 2) {
      ++recursion;
      result = owner->native()->is_using_dark_mode();
      --recursion;
    }
    return result;
  }
  
  return base::is_using_dark_mode();
}

int Win32AttachedPopupWindow::dpi() {
  return Win32HighDpi::get_dpi_for_window(_hwnd);
}

void Win32AttachedPopupWindow::invalidate_source_location() {
  Win32Widget *owner_wid = Impl(*this).owner_widget();
  if(!owner_wid) {
    pmath_debug_print("[Win32AttachedPopupWindow: lost owner window]\n");
    close();
    return;
  }
  
  bool visible = IsWindowVisible(owner_wid->hwnd()) && document()->get_style(Visible, true);
  
  if(Box *anchor = source_box()) {
    POINT pos;
    if(visible && Impl(*this).find_anchor_screen_position(pos)) {
      int width  = _best_width;
      int height = _best_height;
      
      RECT rect;
      GetWindowRect(_hwnd, &rect);
      
      RECT client_rect;
      GetClientRect(_hwnd, &client_rect);
      width += (rect.right - rect.left) - (client_rect.right - client_rect.left);
      height+= (rect.bottom - rect.top) - (client_rect.bottom - client_rect.top);
      
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
      
      if(!IsWindowVisible(_hwnd))
        flags |= SWP_SHOWWINDOW;
      
      SetWindowPos(
        _hwnd, nullptr,
        pos.x, pos.y, width, height,
        flags);
    }
    else {
      if(IsWindowVisible(_hwnd)) {
        SetWindowPos(
          _hwnd, nullptr,
          0, 0, 1, 1,
          SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
      }
    }
  }
  else {
    pmath_debug_print("[Win32AttachedPopupWindow: lost anchor]\n");
    close();
  }
}

void Win32AttachedPopupWindow::paint_background(Canvas &canvas) {
//  RECT rect;
////  GetClientRect(_hwnd, &rect);
//  GetWindowRect(_hwnd, &rect); OffsetRect(&rect, -rect.left, -rect.top);
//  ControlPainter::std->draw_container(
//    *this, canvas, ContainerType::PopupPanel, ControlState::Normal, 
//    RectangleF{Point(rect.left, rect.top), Point(rect.right, rect.bottom)});
  canvas.set_color(Win32ControlPainter::win32_painter.win32_button_face_color(is_using_dark_mode()));
  canvas.paint();
}

void Win32AttachedPopupWindow::paint_canvas(Canvas &canvas, bool resize_only) {
  base::paint_canvas(canvas, resize_only);
  
  int old_bh = _best_height;
  int old_bw = _best_width;
  
  _best_height = (int)round(document()->extents().height() * scale_factor());
  _best_width  = (int)round(document()->unfilled_width     * scale_factor());
  
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
    invalidate_source_location();
  }
}

void Win32AttachedPopupWindow::on_close() {
  if(Document *owner = owner_document()) 
    owner->popup_window_closed(document());
  
  base::on_close();
}

void Win32AttachedPopupWindow::do_set_current_document() {
  Documents::current(document());
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
  
  Point anchor_point = {0, anchor->extents().descent};
  RectangleF rect = anchor->extents().to_rectangle();
  if(!anchor->visible_rect(rect))
    return false;
  
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  anchor->transformation(nullptr, &mat);
  
  anchor_point = Canvas::transform_point(mat, anchor_point);
  anchor_point.x *= owner_wid->scale_factor();
  anchor_point.y *= owner_wid->scale_factor();
  
  pos = { (int)round(anchor_point.x), (int)round(anchor_point.y) };
  pos.x -= GetScrollPos(owner_wid->hwnd(), SB_HORZ);
  pos.y -= GetScrollPos(owner_wid->hwnd(), SB_VERT);
  return ClientToScreen(owner_wid->hwnd(), &pos);
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
