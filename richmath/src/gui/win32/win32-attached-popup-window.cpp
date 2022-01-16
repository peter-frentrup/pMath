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
      void adjust_target_rect(WindowFrameType wft, ControlPlacementKind cpk, RectangleF &target_rect);
      
      void on_windowposchanged(const WINDOWPOS &wp);

      static const wchar_t class_name[];
    
    private:
      bool on_nccalcsize_simple(RECT &rect, LRESULT &res);
      bool on_nccalcsize_complex(NCCALCSIZE_PARAMS &params, LRESULT &res);
    public:
      bool on_nccalcsize(WPARAM wParam, LPARAM lParam, LRESULT &res);
      int triangle_tip_size();
      void update_window_shape(WindowFrameType wft, ControlPlacementKind cpk, const RectangleF &window_rect, const RectangleF &target_rect);
      
      bool on_ncpaint(HRGN clipRgn);
      
    private:
      Win32AttachedPopupWindow &self;
  };
  
  enum class Side {
    Bottom, Left, Right, Top, 
  };
}

using namespace richmath;

static Side control_placement_side(ControlPlacementKind cpk);

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
    case WindowFrameThinCallout:
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
      auto wft = (WindowFrameType)document()->get_own_style(WindowFrame);
      
      Impl(*this).adjust_target_rect(wft, cpk, target_rect);
      
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
      
      Impl(*this).update_window_shape(wft, cpk, popup_rect, target_rect);
      
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
      
      case WM_NCCALCSIZE: {
        LRESULT res = 0;
        if(Impl(*this).on_nccalcsize(wParam, lParam, res))
          return res;
      } break;
      
      case WM_NCPAINT: {
        if(Impl(*this).on_ncpaint((HRGN)wParam))
          return 0;
      } break;
        
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

void Win32AttachedPopupWindow::Impl::adjust_target_rect(WindowFrameType wft, ControlPlacementKind cpk, RectangleF &target_rect) {
  switch(wft) {
    case WindowFrameThinCallout: {
      int tri_size = triangle_tip_size();
      int inset = tri_size / 4;
//      int min_size = tri_size;
      switch(control_placement_side(cpk)) {
        case Side::Left: 
        case Side::Right: 
          target_rect.grow(-inset, 0);
//          if(target_rect.height < min_size)
//            target_rect.grow(0, min_size - target_rect.height);
          break;
        
        case Side::Top:
        case Side::Bottom: 
          target_rect.grow(0, -inset);
//          if(target_rect.width < min_size)
//            target_rect.grow(min_size - target_rect.width, 0);
          break;
       }
     } break;
  }
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

bool Win32AttachedPopupWindow::Impl::on_nccalcsize(WPARAM wParam, LPARAM lParam, LRESULT &res) {
  if(wParam)
    return on_nccalcsize_complex(*(NCCALCSIZE_PARAMS*)lParam, res);
  else
    return on_nccalcsize_simple(*(RECT*)lParam, res);
}

bool Win32AttachedPopupWindow::Impl::on_nccalcsize_simple(RECT &rect, LRESULT &res) {
  switch(self.document()->get_own_style(WindowFrame)) {
    case WindowFrameThinCallout: {
      auto side = control_placement_side((ControlPlacementKind)self.document()->get_own_style(ControlPlacement, ControlPlacementKindBottom));
      switch(side) {
        case Side::Left:   rect.right-=  triangle_tip_size(); break;
        case Side::Right:  rect.left+=   triangle_tip_size(); break;
        case Side::Top:    rect.bottom-= triangle_tip_size(); break;
        case Side::Bottom: rect.top+=    triangle_tip_size(); break;
      }
      
      rect.top+=    2;
      rect.left+=   2;
      rect.right-=  2;
      rect.bottom-= 2;
      res = 0;
      return true;
    } break;
  }
  return false;
}

bool Win32AttachedPopupWindow::Impl::on_nccalcsize_complex(NCCALCSIZE_PARAMS &params, LRESULT &res) {
  if(!on_nccalcsize_simple(params.rgrc[0], res))
    return false;
  
  //return false;
  res = 0;
  return true;
}

int Win32AttachedPopupWindow::Impl::triangle_tip_size() {
  int dpi = Win32HighDpi::get_dpi_for_window(self.hwnd());
  
  int size = MulDiv(20, dpi, 96);
  
  RECT rect;
  if(GetWindowRect(self.hwnd(), &rect)) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    int max_size = std::min(width, height) / 2;
    if(size > max_size)
      size = max_size;
  }
  
  return size;
}

void Win32AttachedPopupWindow::Impl::update_window_shape(WindowFrameType wft, ControlPlacementKind cpk, const RectangleF &window_rect, const RectangleF &target_rect) {
  switch(wft) {
    case WindowFrameThinCallout: {
      auto side = control_placement_side(cpk);
      Interval<float> main_side(0,0);
      Interval<float> common(0,0);
      switch(side) {
        case Side::Left:
        case Side::Right:
          main_side = window_rect.y_interval();
          common = main_side.intersect(target_rect.y_interval());
          break;
        
        case Side::Top:
        case Side::Bottom:
          main_side = window_rect.x_interval();
          common = main_side.intersect(target_rect.x_interval());
          break;
      }
      
      auto tip_size = triangle_tip_size();
      auto main_center   = main_side.from + main_side.length() / 2;
      int triangle_base_direction = 1; // -1 = /|   0 = /\   1 = |\  .
      if(common.to < main_side.from + main_side.length() / 3)
        triangle_base_direction = 1;
      else if(common.from > main_side.to - main_side.length() / 3)
        triangle_base_direction = -1;
      else
        triangle_base_direction = 0;
        
      auto triangle_height = (triangle_base_direction == 0) ? tip_size - tip_size/4 : tip_size;
      auto common_center = common.from + common.length() / 2;
      Interval<float> triangle_range {
        common_center - (triangle_base_direction <= 0 ? triangle_height : 0), 
        common_center + (triangle_base_direction >= 0 ? triangle_height : 0)};
      
      triangle_range = main_side.snap(triangle_range);
      
      RECT main_rect;
      main_rect.left   = 0;
      main_rect.top    = 0;
      main_rect.right  = (int)window_rect.width;
      main_rect.bottom = (int)window_rect.height;
      
      POINT triangle_points[3];
      switch(side) {
        case Side::Left:
        case Side::Right:
          triangle_points[0].y = (int)triangle_range.from - window_rect.y;
          triangle_points[1].y = (int)common_center       - window_rect.y;
          triangle_points[2].y = (int)triangle_range.to   - window_rect.y;
          break;
        
        case Side::Top:
        case Side::Bottom:
          triangle_points[0].x = (int)triangle_range.from - window_rect.x;
          triangle_points[1].x = (int)common_center       - window_rect.x;
          triangle_points[2].x = (int)triangle_range.to   - window_rect.x;
          break;
      }
      
      switch(side) {
        case Side::Left:   triangle_points[1].x = main_rect.right  ; break;
        case Side::Right:  triangle_points[1].x = main_rect.left   ; break;
        case Side::Top:    triangle_points[1].y = main_rect.bottom ; break;
        case Side::Bottom: triangle_points[1].y = main_rect.top    ; break;
      }
      
      switch(side) {
        case Side::Left:   triangle_points[0].x = triangle_points[1].x = triangle_points[2].x = main_rect.right  -= tip_size; break;
        case Side::Right:  triangle_points[0].x = triangle_points[1].x = triangle_points[2].x = main_rect.left   += tip_size; break;
        case Side::Top:    triangle_points[0].y = triangle_points[1].y = triangle_points[2].y = main_rect.bottom -= tip_size; break;
        case Side::Bottom: triangle_points[0].y = triangle_points[1].y = triangle_points[2].y = main_rect.top    += tip_size; break;
      }
      
      switch(side) {
        case Side::Left:   triangle_points[1].x += triangle_height; break;
        case Side::Right:  triangle_points[1].x -= triangle_height; break;
        case Side::Top:    triangle_points[1].y += triangle_height; break;
        case Side::Bottom: triangle_points[1].y -= triangle_height; break;
      }
      
      HRGN triangle_rgn = CreatePolygonRgn(triangle_points, 3, WINDING);
      HRGN rgn = CreateRectRgnIndirect(&main_rect);
      CombineRgn(rgn, rgn, triangle_rgn, RGN_OR);
      DeleteObject(triangle_rgn);
      SetWindowRgn(self.hwnd(), rgn, TRUE);
    } break;
  }
}

bool Win32AttachedPopupWindow::Impl::on_ncpaint(HRGN clipRgn) {
  bool success = false;
  
  switch(self.document()->get_own_style(WindowFrame)) {
    case WindowFrameThinCallout: {
      HRGN hrgn = CreateRectRgn(0,0,0,0);
      int region_type = GetWindowRgn(self.hwnd(), hrgn);
      if(region_type != ERROR) {
        HDC hdc = GetDCEx(self.hwnd(), clipRgn, DCX_CACHE | DCX_WINDOW | DCX_INTERSECTRGN); //
        
        if(Color bg = self.document()->get_style(Background, Color::None)) {
          HBRUSH hbr = CreateSolidBrush(bg.to_bgr24());
          FillRgn(hdc, hrgn, hbr);
          DeleteObject(hbr);
        }
        else if(self.is_using_dark_mode()) {
          HBRUSH hbr = CreateSolidBrush(Win32ControlPainter::win32_painter.win32_button_face_color(true).to_bgr24());
          FillRgn(hdc, hrgn, hbr);
          DeleteObject(hbr);
        }
        else {
          FillRgn(hdc, hrgn, GetSysColorBrush(COLOR_BTNFACE));
        }
        FrameRgn(hdc, hrgn, GetSysColorBrush(COLOR_WINDOWFRAME), 1, 1);
        
        ReleaseDC(self.hwnd(), hdc);
        success = true;
      }
      DeleteObject(hrgn);
    } break;
  }
  return success;
}

//} ... class Win32AttachedPopupWindow::Impl

static Side control_placement_side(ControlPlacementKind cpk) {
  switch(cpk) {
    case ControlPlacementKindLeft:   return Side::Left;
    case ControlPlacementKindRight:  return Side::Right;
    case ControlPlacementKindTop:    return Side::Top;
    default:
    case ControlPlacementKindBottom: return Side::Bottom;
  }
}
