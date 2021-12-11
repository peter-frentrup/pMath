#include <gui/win32/win32-attached-popup-window.h>

#include <gui/common-tooltips.h>
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
      bool find_anchor_screen_position(RectangleF &target_rect);
      
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
  _best_size(1, 1)
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
    document()->style->set(ClosingAction, ClosingActionDelete);
    close();
    return;
  }
  
  bool visible = IsWindowVisible(owner_wid->hwnd()) && document()->get_style(Visible, true);
  
  if(Box *anchor = source_box()) {
    auto cpk = (ControlPlacementKind)document()->get_own_style(ControlPlacement, ControlPlacementKindBottom);
    RectangleF target_rect;
    if(visible && Impl(*this).find_anchor_screen_position(target_rect)) {
      RECT rect;
      GetWindowRect(_hwnd, &rect);
            
      RectangleF popup_rect {};
      
      RECT target_rect_int;
      target_rect_int.left   = (int)target_rect.left();
      target_rect_int.right  = (int)target_rect.right();
      target_rect_int.top    = (int)target_rect.top();
      target_rect_int.bottom = (int)target_rect.bottom();
      
      if(HMONITOR hmon = MonitorFromRect(&target_rect_int, MONITOR_DEFAULTTONEAREST)) {
        MONITORINFO monitor_info;
        memset(&monitor_info, 0, sizeof(monitor_info));
        monitor_info.cbSize = sizeof(monitor_info);
        
        if(GetMonitorInfo(hmon, &monitor_info)) {
          RectangleF monitor_rect(
            Point(monitor_info.rcWork.left, monitor_info.rcWork.top), 
            Point(monitor_info.rcWork.right, monitor_info.rcWork.bottom));
          
          popup_rect = CommonTooltips::popup_placement(target_rect, _best_size, cpk, monitor_rect);
        }
        else
          popup_rect = CommonTooltips::popup_placement(target_rect, _best_size, cpk);
      }
      else
        popup_rect = CommonTooltips::popup_placement(target_rect, _best_size, cpk);
      
      int width  = (int)round(popup_rect.width);
      int height = (int)round(popup_rect.height);
      
      UINT flags = SWP_NOACTIVATE | SWP_NOZORDER;
      if(width == rect.right - rect.left && height == rect.bottom - rect.top) {
        flags |= SWP_NOSIZE;
      }
      
      if(!IsWindowVisible(_hwnd))
        flags |= SWP_SHOWWINDOW;
      
      SetWindowPos(
        _hwnd, nullptr,
        (int)round(popup_rect.x), (int)round(popup_rect.y),
        width, height,
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
    document()->style->set(ClosingAction, ClosingActionDelete);
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
  
  auto old_best_size = _best_size;
  
  _best_size = {document()->unfilled_width, document()->extents().height()};
  _best_size*= scale_factor();
  
  if(_best_size.x < 1) _best_size.x = 1;
  if(_best_size.y < 1) _best_size.y = 1;
    
  RECT outer, inner;
  GetWindowRect(_hwnd, &outer);
  GetClientRect(_hwnd, &inner);
  
  _best_size.x += outer.right  - outer.left - inner.right  + inner.left;
  _best_size.y += outer.bottom - outer.top  - inner.bottom + inner.top;
  
  if(old_best_size != _best_size) {
    invalidate_source_location();
  }
}

void Win32AttachedPopupWindow::on_close() {
  switch(document()->get_style(ClosingAction)) {
    case ClosingActionHide: {
        document()->style->set(Visible, false);
        invalidate_options();
      }
      return;
    
    case ClosingActionDelete:
    default:
      break;
  }
  
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
      case WM_ACTIVATE: {
          pmath_debug_print("[Win32AttachedPopupWindow WM_ACTIVATE %p %d %p]\n", _hwnd, wParam, lParam);
        } break;
      
      case WM_NCACTIVATE: {
        pmath_debug_print("[Win32AttachedPopupWindow: WM_NCACTIVATE %p %d (active: %p)]\n", _hwnd, wParam, GetActiveWindow());
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

bool Win32AttachedPopupWindow::Impl::find_anchor_screen_position(RectangleF &target_rect) {
  Win32Widget *owner_wid = owner_widget();
  if(!owner_wid)
    return false;
  
  if(!IsWindowVisible(owner_wid->hwnd())) 
    return false;
  
  Box *anchor = self.source_box();
  if(!anchor)
    return false;
  
  target_rect = anchor->extents().to_rectangle();
  if(!anchor->visible_rect(target_rect))
    return false;
  
  auto scale_factor = owner_wid->scale_factor();
  target_rect.x      *= scale_factor;
  target_rect.y      *= scale_factor;
  target_rect.width  *= scale_factor;
  target_rect.height *= scale_factor;
  
  target_rect.x -= GetScrollPos(owner_wid->hwnd(), SB_HORZ);
  target_rect.y -= GetScrollPos(owner_wid->hwnd(), SB_VERT);
  
  RECT rc;
  rc.left   = (int)round(target_rect.left());
  rc.top    = (int)round(target_rect.top());
  rc.right  = (int)round(target_rect.right());
  rc.bottom = (int)round(target_rect.bottom());
  
  MapWindowPoints(owner_wid->hwnd(), nullptr, (POINT*)&rc, 2);
  
  target_rect.x = rc.left;
  target_rect.y = rc.top;
  target_rect.width  = rc.right - rc.left;
  target_rect.height = rc.bottom - rc.top;
  return true;
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
