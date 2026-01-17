#include <gui/win32/win32-attached-popup-window.h>

#include <graphics/callout-triangle.h>

#include <gui/common-tooltips.h>
#include <gui/documents.h>
#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-version.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/win32-document-window.h>

#include <boxes/abstractsequence.h>


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
      void adjust_target_rect(WindowFrameType wft, ControlPlacementKind cpk, const RECT &window_rect, RectangleF &target_rect);
      
      void on_windowposchanged(const WINDOWPOS &wp);

      static const wchar_t class_name[];
    
    public:
      void on_after_nccalcsize(WPARAM wParam, LPARAM lParam, LRESULT &res);
      int triangle_tip_size(int window_width, int window_height);
      int triangle_tip_size(const RECT       &rect) { return triangle_tip_size(rect.right - rect.left, rect.bottom - rect.top); }
      int triangle_tip_size(const RectangleF &rect) { return triangle_tip_size((int)rect.width, (int)rect.height); }
      void update_window_shape(WindowFrameType wft, ControlPlacementKind cpk, const RectangleF &window_rect, const RectangleF &target_rect);
      
      bool on_ncpaint(HRGN clipRgn);
      
    private:
      Win32AttachedPopupWindow &self;
  };
}

using namespace richmath;

POINT richmath::discretize(const Point &p) {
  return { (int)(p.x + 0.5f), (int)(p.y + 0.5f) };
}

RECT richmath::discretize(const RectangleF &rect) {
  return { 
    (int)round_directed(rect.left(),   0 /* +1 */, false), 
    (int)round_directed(rect.top(),    0 /* +1 */, false), 
    (int)round_directed(rect.right(),  0 /* -1 */, false), 
    (int)round_directed(rect.bottom(), 0 /* -1 */, false) }; 
}

const wchar_t Win32AttachedPopupWindow::Impl::class_name[] = L"RichmathWin32Popup";

//{ class Win32AttachedPopupWindow ...

Win32AttachedPopupWindow::Win32AttachedPopupWindow(Document *owner, const SelectionReference &anchor) 
  : base(
    new Document(),
    WS_EX_TOOLWINDOW,
    WS_POPUP | WS_CLIPCHILDREN,// | WS_BORDER,
    0,
    0,
    100,
    100,
    Impl::get_owner_hwnd_location(owner)),
  _active{false},
  _best_size(1, 1),
  _last_target_rect(0,0,0,0)
{
  Impl::init_window_class();
  set_window_class_name(Impl::class_name);
  
  owner_document(owner);
  source_range(anchor);
  
  _autohide_vertical_scrollbar = true;
}

Win32AttachedPopupWindow::~Win32AttachedPopupWindow() {
  if(Document *owner = owner_document()) 
    owner->popup_window_closed(document());
}

void Win32AttachedPopupWindow::after_construction() {
  base::after_construction();
  
  document()->style.reset(strings::AttachedPopupWindow);
  
  if(Document *owner = owner_document()) {
    document()->stylesheet(owner->stylesheet());
  }
  
  document()->style.set(Visible,                         true);
  document()->style.set(InternalHasModifiedWindowOption, true);
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
    case WindowFrameThinCallout:
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

bool Win32AttachedPopupWindow::is_focused_widget() {
  if(is_foreground_window() && base::is_focused_widget())
    return true;
  
  Document *owner_doc = owner_document();
  Document *doc = document();
  if(doc && owner_doc && owner_doc->is_input_stealing_popup(doc))
    return owner_doc->native()->is_focused_widget();
  
  return false;
}      

int Win32AttachedPopupWindow::dpi() {
  return Win32HighDpi::get_dpi_for_window(_hwnd);
}

void Win32AttachedPopupWindow::invalidate_source_location() {
  Win32Widget *owner_wid = Impl(*this).owner_widget();
  if(!owner_wid) {
    pmath_debug_print("[Win32AttachedPopupWindow: lost owner window]\n");
    document()->style.set(ClosingAction, ClosingActionDelete);
    close();
    return;
  }
  
  bool visible = IsWindowVisible(owner_wid->hwnd()) && document()->get_style(Visible, true);
  
  if(Box *anchor = source_box()) {
    auto cpk = (ControlPlacementKind)document()->get_own_style(ControlPlacement, ControlPlacementKindBottom);
    RectangleF target_rect;
    if(visible && Impl(*this).find_anchor_screen_position(target_rect)) {
      auto wft = (WindowFrameType)document()->get_own_style(WindowFrame);
      
      RECT rect;
      GetWindowRect(_hwnd, &rect);
      
      Impl(*this).adjust_target_rect(wft, cpk, rect, target_rect);
      
      RectangleF popup_rect {};
      
      RECT target_rect_int;
      target_rect_int.left   = (int)target_rect.left();
      target_rect_int.right  = (int)target_rect.right();
      target_rect_int.top    = (int)target_rect.top();
      target_rect_int.bottom = (int)target_rect.bottom();
      
      bool need_v_scroll = false;
      bool need_h_scroll = false;
      Vector2F scrollbars_size = { 0.0f, 0.0f };
      
      if(HMONITOR hmon = MonitorFromRect(&target_rect_int, MONITOR_DEFAULTTONEAREST)) {
        MONITORINFO monitor_info;
        memset(&monitor_info, 0, sizeof(monitor_info));
        monitor_info.cbSize = sizeof(monitor_info);
        
        if(GetMonitorInfo(hmon, &monitor_info)) {
          RectangleF monitor_rect(
            Point(monitor_info.rcWork.left, monitor_info.rcWork.top), 
            Point(monitor_info.rcWork.right, monitor_info.rcWork.bottom));
          
          popup_rect = CommonTooltips::popup_placement(target_rect, _best_size, cpk, monitor_rect);
          
          if(popup_rect.height + 1 <= _best_size.y) {
            need_v_scroll = true;
            scrollbars_size.x = Win32HighDpi::get_system_metrics_for_dpi(SM_CXVSCROLL, dpi());
          }
          
          if(popup_rect.width + 1 <= _best_size.x) {
            need_h_scroll = true;
            scrollbars_size.y = Win32HighDpi::get_system_metrics_for_dpi(SM_CYHSCROLL, dpi());
          }
          
          if(need_v_scroll || need_h_scroll) {
            popup_rect = CommonTooltips::popup_placement(target_rect, _best_size + scrollbars_size, cpk, monitor_rect);
          }
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
      
      SCROLLINFO si = { sizeof(SCROLLINFO) };
      si.fMask = SIF_PAGE | SIF_RANGE;
      si.nMin = 0;
      si.nMax = (int)_best_size.y;
      si.nPage = need_v_scroll ? (int)popup_rect.height : si.nMax + 1;
      SetScrollInfo(_hwnd, SB_VERT, &si, TRUE);
      
      si.nMax = (int)_best_size.x;
      si.nPage = need_h_scroll ? (int)popup_rect.width : si.nMax + 1;
      SetScrollInfo(_hwnd, SB_HORZ, &si, TRUE);
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
    document()->style.set(ClosingAction, ClosingActionDelete);
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
  
  // includes scrollbars:
  RECT outer, inner;
  GetWindowRect(_hwnd, &outer);
  GetClientRect(_hwnd, &inner);
  
  _best_size.x += outer.right  - outer.left - inner.right  + inner.left;
  _best_size.y += outer.bottom - outer.top  - inner.bottom + inner.top;
  
  int _dpi = dpi();
  
  Length w = document()->get_own_style(ImageSizeHorizontal, SymbolicSize::Automatic);
  Length h = document()->get_own_style(ImageSizeVertical,   SymbolicSize::Automatic);
  
  // TODO: convert scaled and symbolic sizes relative to monitor width/height
  if(w != SymbolicSize::Automatic || h != SymbolicSize::Automatic) {
    if(w.is_explicit_abs())
      _best_size.x = std::max(1.0f, w.explicit_abs_value() * _dpi / 72);
    
    if(h.is_explicit_abs())
      _best_size.y = std::max(1.0f, h.explicit_abs_value() * _dpi / 72);
  }
  
  // subtract scrollbars size:
  DWORD style = GetWindowLongW(_hwnd, GWL_STYLE);
  if(style & (WS_VSCROLL | WS_HSCROLL)) {
    if(style & WS_VSCROLL)
      _best_size.x -= Win32HighDpi::get_system_metrics_for_dpi(SM_CXVSCROLL, _dpi);
    
    if(style & WS_HSCROLL)
      _best_size.y -= Win32HighDpi::get_system_metrics_for_dpi(SM_CYHSCROLL, _dpi);
  }
  
  if(old_best_size != _best_size) {
    invalidate_source_location();
  }
}

void Win32AttachedPopupWindow::on_close() {
  switch(document()->get_style(ClosingAction)) {
    case ClosingActionHide: {
        document()->style.set(Visible, false);
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

void Win32AttachedPopupWindow::do_set_selected_document() {
  Documents::selected_document(document());
}

LRESULT Win32AttachedPopupWindow::callback(UINT message, WPARAM wParam, LPARAM lParam) {
  LRESULT result = 0;
  
  if(!initializing() && !destroying()) {
    switch(message) {
      case WM_NCACTIVATE: {
        if(Win32DocumentWindow::handle_ncactivate(result, hwnd(), wParam, lParam, document()->selectable()))
          return result;
        
        _active = wParam;
      } break;
      
      case WM_ACTIVATEAPP:
        if(Win32DocumentWindow::handle_activateapp(result, hwnd(), wParam, lParam, document()->selectable()))
          return result;
        
        if(!wParam) {
          auto conds = (RemovalConditionFlags)document()->get_own_style(RemovalConditions, 0);
          if(conds & RemovalConditionFlags::RemovalConditionFlagSelectionExit)
            close();
        }
        break;
      
      case WM_NCCALCSIZE: {
        result = base::callback(message, wParam, lParam);
        Impl(*this).on_after_nccalcsize(wParam, lParam, result);
        return result;
      } break;
      
      case WM_NCPAINT: {
        base::callback(message, wParam, lParam); // TODO: is this necessary at all?
        Impl(*this).on_ncpaint((HRGN)wParam);
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
  
  VolatileSelection anchor = self.source_range().get_all();
  if(!anchor)
    return false;
  
  bool has_target_rect = false;
  if(true /* content padding */) {
    if(auto seq = dynamic_cast<AbstractSequence*>(anchor.box)) {
      if(Box *box = anchor.contained_box()) {
        box->range_rect(0, box->length());
        has_target_rect = true;
      }
      
      if(!has_target_rect && seq->text_changed()) { // measure_range() just gives extents(). use cached rect ...
        if(self._last_target_rect != RectangleF(0,0,0,0)) {
          target_rect = self._last_target_rect;
          has_target_rect = true;
        }
      }
      
      if(!has_target_rect) {
        LineRangeMeasurement measurement = seq->measure_range(anchor.start, anchor.end);
        
        target_rect = measurement.bounds;
        float em = seq->get_em();
        float min_accent  = 0.75 * em;
        float min_descent = 0.25 * em;
        if(measurement.first_line_ascent < min_accent)
          target_rect.grow(Side::Top, min_accent - measurement.first_line_ascent);
        
        if(measurement.last_line_descent < min_descent)
          target_rect.grow(Side::Bottom, min_descent - measurement.last_line_descent);
        
        has_target_rect = true;
      }
    }
  }
  
  if(!has_target_rect) {
    target_rect = anchor.box->range_rect(anchor.start, anchor.end);
  }
  
  if(self._last_target_rect.size() != target_rect.size())
    self._size_observable.notify_all();
  
  self._last_target_rect = target_rect;
  
  if(!anchor.box->visible_rect(target_rect))
    return false;
  
  auto scale_factor = owner_wid->scale_factor();
  target_rect.x      *= scale_factor;
  target_rect.y      *= scale_factor;
  target_rect.width  *= scale_factor;
  target_rect.height *= scale_factor;
  
  target_rect.x -= GetScrollPos(owner_wid->hwnd(), SB_HORZ);
  target_rect.y -= GetScrollPos(owner_wid->hwnd(), SB_VERT);
  
//  pmath_debug_print("[anchor @ (%f, %f) - (%f, %f): %f x %f]\n",
//    target_rect.left(), target_rect.top(),
//    target_rect.right(), target_rect.bottom(),
//    target_rect.width, target_rect.height);
  
  RECT rc = discretize(target_rect);
  
  MapWindowPoints(owner_wid->hwnd(), nullptr, (POINT*)&rc, 2);
  
  target_rect.x = rc.left;
  target_rect.y = rc.top;
  target_rect.width  = rc.right - rc.left;
  target_rect.height = rc.bottom - rc.top;
  return true;
}

void Win32AttachedPopupWindow::Impl::adjust_target_rect(WindowFrameType wft, ControlPlacementKind cpk, const RECT &window_rect, RectangleF &target_rect) {
  switch(wft) {
    case WindowFrameThinCallout: 
      target_rect.grow(control_placement_side(cpk), -triangle_tip_size(window_rect) / 4);
      break;
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

void Win32AttachedPopupWindow::Impl::on_after_nccalcsize(WPARAM wParam, LPARAM lParam, LRESULT &res) {
  RECT &rect = *(RECT*)lParam; // also correct if wParam == TRUE, where lParam is (NCCALCSIZE_PARAMS*)
  
  switch(self.document()->get_own_style(WindowFrame)) {
    case WindowFrameThinCallout: {
      auto side = control_placement_side((ControlPlacementKind)self.document()->get_own_style(ControlPlacement, ControlPlacementKindBottom));
      switch(side) {
        case Side::Left:   rect.right-=  triangle_tip_size(rect); break;
        case Side::Right:  rect.left+=   triangle_tip_size(rect); break;
        case Side::Top:    rect.bottom-= triangle_tip_size(rect); break;
        case Side::Bottom: rect.top+=    triangle_tip_size(rect); break;
      }
      
      rect.top+=    2;
      rect.left+=   2;
      rect.right-=  2;
      rect.bottom-= 2;
      
      res = 0;
    } break;
  }
}

int Win32AttachedPopupWindow::Impl::triangle_tip_size(int window_width, int window_height) {
  int dpi = Win32HighDpi::get_dpi_for_window(self.hwnd());
  
  int size = MulDiv(20, dpi, 96);
  
  int max_size = std::min(window_width, window_height) / 2;
  if(size > max_size)
    size = max_size;
  
  return size;
}

void Win32AttachedPopupWindow::Impl::update_window_shape(WindowFrameType wft, ControlPlacementKind cpk, const RectangleF &window_rect, const RectangleF &target_rect) {
  switch(wft) {
    case WindowFrameThinCallout: {
      auto side = opposite_side(control_placement_side(cpk));
      auto tip_size = triangle_tip_size(window_rect);
      auto tri = CalloutTriangle::ForSideOfBasePointingToTarget(window_rect, side, target_rect, tip_size, true);
      
      auto main_rect = window_rect - Vector2F{window_rect.top_left()};
      main_rect.grow(side, -tip_size);
      
      // Discretize before obtaining triangle points to protect against different rounding directions
      RECT rect = discretize(main_rect);
      main_rect.x = rect.left;
      main_rect.y = rect.top;
      main_rect.width = rect.right - rect.left;
      main_rect.height = rect.bottom - rect.top;

      Point tri_points[3];
      tri.get_triangle_points(tri_points, main_rect, side);
      
      POINT triangle_points[3] = { discretize(tri_points[0]), discretize(tri_points[1]), discretize(tri_points[2]) };
      
      int frameradius = 0;
      if(Win32Version::is_windows_11_or_newer()) {
        // Since we inset the client rect by 2px, we can at most have radius 3 without touching the client rect
        frameradius = 3; 
      }
      
      HRGN triangle_rgn = CreatePolygonRgn(triangle_points, 3, WINDING);
      HRGN rgn = nullptr;
      if(frameradius > 0) {
        rgn = CreateRoundRectRgn(rect.left, rect.top, rect.right + 1, rect.bottom + 1, 2 * frameradius, 2 * frameradius);

        RECT base_rect = discretize(tri.get_basement(main_rect, side, frameradius));      
        if(HRGN base_rgn = CreateRectRgnIndirect(&base_rect)) {
          CombineRgn(rgn, rgn, base_rgn, RGN_OR);
          DeleteObject(base_rgn);
        }
      }
      else {
        rgn = CreateRectRgnIndirect(&rect);
      }
      CombineRgn(rgn, rgn, triangle_rgn, RGN_OR);
      DeleteObject(triangle_rgn);
      
      SetWindowRgn(self.hwnd(), rgn, TRUE);
      RedrawWindow(self.hwnd(), nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_NOERASE | RDW_UPDATENOW);
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
        
        RECT client_rect {};
        GetClientRect(self.hwnd(), &client_rect);
        MapWindowPoints(self.hwnd(), nullptr, (POINT*)&client_rect, 2);
        
        RECT win_rect {};
        GetWindowRect(self.hwnd(), &win_rect);
        OffsetRect(&client_rect, -win_rect.left, -win_rect.top);
        
        int dpi = Win32HighDpi::get_dpi_for_window(self.hwnd());
        
        SCROLLBARINFO sbi {};
        sbi.cbSize = sizeof(sbi);
        if(GetScrollBarInfo(self.hwnd(), OBJID_VSCROLL, &sbi)) {
          if(!(sbi.rgstate[0] & STATE_SYSTEM_INVISIBLE)) {
            ExcludeClipRect(hdc, 
              sbi.rcScrollBar.left   - win_rect.left,
              sbi.rcScrollBar.top    - win_rect.top,
              sbi.rcScrollBar.right  - win_rect.left,
              sbi.rcScrollBar.bottom - win_rect.top);
          }
        }
        if(GetScrollBarInfo(self.hwnd(), OBJID_HSCROLL, &sbi)) {
          if(!(sbi.rgstate[0] & STATE_SYSTEM_INVISIBLE)) {
            ExcludeClipRect(hdc, 
              sbi.rcScrollBar.left   - win_rect.left,
              sbi.rcScrollBar.top    - win_rect.top,
              sbi.rcScrollBar.right  - win_rect.left,
              sbi.rcScrollBar.bottom - win_rect.top);
          }
        }
        
        Color bg = self.document()->get_style(Background, Color::None);
        if(!bg)
          bg = Win32ControlPainter::win32_painter.win32_button_face_color(self.is_using_dark_mode());
        
        SetDCBrushColor(hdc, bg.to_bgr24());
        FillRgn(hdc, hrgn, (HBRUSH)GetStockObject(DC_BRUSH));
        
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
