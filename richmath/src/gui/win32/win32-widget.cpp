#define _WIN32_WINNT 0x600
// 0x501 for VK_OEM_XXX
// 0x600 for SPI_GETWHEELSCROLLCHARS

#include <gui/win32/win32-widget.h>
#include <gui/win32/win32-themes.h>

#include <climits>
#include <cmath>
#include <cstdio>

#include <cairo-win32.h>

#include <boxes/buttonbox.h>
#include <boxes/section.h>
#include <boxes/mathsequence.h>
#include <eval/binding.h>
#include <eval/application.h>
#include <eval/job.h>
#include <gui/control-painter.h>
#include <gui/win32/ole/dataobject.h>
#include <gui/win32/ole/dropsource.h>
#include <gui/win32/win32-clipboard.h>
#include <gui/win32/win32-menu.h>
#include <gui/win32/win32-tooltip-window.h>
#include <gui/win32/win32-touch.h>

#include <resources.h>

#ifndef WM_MOUSEHWHEEL
#  define WM_MOUSEHWHEEL  0x020E
#endif
#ifndef SPI_GETWHEELSCROLLCHARS
#  define SPI_GETWHEELSCROLLCHARS   0x006C
#endif

#define TID_SCROLL         1
#define TID_ANIMATE        2
#define TID_BLINKCURSOR    3

#define ANIMATION_DELAY  (50)

using namespace richmath;

SpecialKey richmath::win32_virtual_to_special_key(DWORD vkey) {
  switch(vkey) {
    case VK_LEFT:     return SpecialKey::Left;
    case VK_RIGHT:    return SpecialKey::Right;
    case VK_UP:       return SpecialKey::Up;
    case VK_DOWN:     return SpecialKey::Down;
    case VK_HOME:     return SpecialKey::Home;
    case VK_END:      return SpecialKey::End;
    case VK_PRIOR:    return SpecialKey::PageUp;
    case VK_NEXT:     return SpecialKey::PageDown;
    case VK_BACK:     return SpecialKey::Backspace;
    case VK_DELETE:   return SpecialKey::Delete;
    case VK_RETURN:   return SpecialKey::Return;
    case VK_ESCAPE:   return SpecialKey::Escape;
    case VK_TAB:      return SpecialKey::Tab;
    case VK_F1:       return SpecialKey::F1;
    case VK_F2:       return SpecialKey::F2;
    case VK_F3:       return SpecialKey::F3;
    case VK_F4:       return SpecialKey::F4;
    case VK_F5:       return SpecialKey::F5;
    case VK_F6:       return SpecialKey::F6;
    case VK_F7:       return SpecialKey::F7;
    case VK_F8:       return SpecialKey::F8;
    case VK_F9:       return SpecialKey::F9;
    case VK_F10:      return SpecialKey::F10;
    case VK_F11:      return SpecialKey::F11;
    case VK_F12:      return SpecialKey::F12;
    
    default: return SpecialKey::Unknown;
  }
}

//{ class Win32Widget ...

Win32Widget::Win32Widget(
  Document *doc,
  DWORD style_ex,
  DWORD style,
  int x,
  int y,
  int width,
  int height,
  HWND *parent)
  : NativeWidget(doc),
    BasicWin32Widget(style_ex, style, x, y, width, height, parent),
    _autohide_vertical_scrollbar(false),
    _image_format(CAIRO_FORMAT_RGB24),
    is_painting(false),
    scrolling(false),
    _width(0),
    _height(0),
    gesture_zoom_factor(1.0f),
    animation_running(false),
    is_dragging(false),
    is_drop_over(false)
{
  if (HDC hdc = GetDC(nullptr))
  {
    //int dpi_x = GetDeviceCaps(hdc, LOGPIXELSX);
    //int dpi_y = GetDeviceCaps(hdc, LOGPIXELSY);
    _dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);
  }
}

void Win32Widget::after_construction() {
  BasicWin32Widget::after_construction();
  
  if(_hwnd) {
    DWORD style_ex = GetWindowLong(_hwnd, GWL_EXSTYLE);
    if(style_ex & WS_EX_LAYOUTRTL) {
      style_ex &= ~WS_EX_LAYOUTRTL;
      SetWindowLong(_hwnd, GWL_EXSTYLE, style_ex);
    }
  }
  
  stylus = StylusUtil::create_stylus_for_window(_hwnd);
  if(stylus) {
    if(auto stylus3 = stylus.as<IRealTimeStylus3>()) {
      /* RealTimeStylus with MultiTouchEnabled disables WM_GESTURE */
      //HRbool(stylus3->put_MultiTouchEnabled(TRUE));
    }
    
//    GUID props[] = {
//      GUID_PACKETPROPERTY_GUID_X,
//      GUID_PACKETPROPERTY_GUID_Y,
//      GUID_PACKETPROPERTY_GUID_NORMAL_PRESSURE,
//
//      GUID_PACKETPROPERTY_GUID_Z,
//      GUID_PACKETPROPERTY_GUID_PACKET_STATUS,
//      GUID_PACKETPROPERTY_GUID_TIMER_TICK,
//      GUID_PACKETPROPERTY_GUID_SERIAL_NUMBER,
//      GUID_PACKETPROPERTY_GUID_TANGENT_PRESSURE,
//      GUID_PACKETPROPERTY_GUID_BUTTON_PRESSURE,
//      GUID_PACKETPROPERTY_GUID_X_TILT_ORIENTATION,
//      GUID_PACKETPROPERTY_GUID_Y_TILT_ORIENTATION,
//      GUID_PACKETPROPERTY_GUID_AZIMUTH_ORIENTATION,
//      GUID_PACKETPROPERTY_GUID_ALTITUDE_ORIENTATION,
//      GUID_PACKETPROPERTY_GUID_TWIST_ORIENTATION,
//      GUID_PACKETPROPERTY_GUID_PITCH_ROTATION,
//      GUID_PACKETPROPERTY_GUID_ROLL_ROTATION,
//      GUID_PACKETPROPERTY_GUID_YAW_ROTATION,
//      GUID_PACKETPROPERTY_GUID_WIDTH,
//      GUID_PACKETPROPERTY_GUID_HEIGHT,
//      GUID_PACKETPROPERTY_GUID_FINGERCONTACTCONFIDENCE,
//      GUID_PACKETPROPERTY_GUID_DEVICE_CONTACT_ID,
//    };
//    HRbool(stylus->SetDesiredPacketDescription(ARRAYSIZE(props), props));

    /* RealTimeStylus disables single-finger WM_GESTURE */
    HRbool(stylus->put_Enabled(TRUE));
    
    HRbool(stylus->AddStylusAsyncPlugin(
             StylusUtil::get_stylus_sync_plugin_count(stylus),
             this));
    fprintf(stderr, "[%lu RTS plugins]\n", StylusUtil::get_stylus_sync_plugin_count(stylus));
  }
  
  /* Enabling WM_TOUCH disables WM_GESTURE */
//  if(Win32Touch::RegisterTouchWindow)
//    Win32Touch::RegisterTouchWindow(_hwnd, 0);
}

Win32Widget::~Win32Widget() {
//  if(surface)
//    cairo_surface_destroy(surface);

}

void Win32Widget::window_size(float *w, float *h) {
  RECT rect;
  GetClientRect(_hwnd, &rect);
  *w = rect.right / scale_factor();
  *h = rect.bottom / scale_factor();
}

void Win32Widget::scroll_pos(float *x, float *y) {
  *x = GetScrollPos(_hwnd, SB_HORZ) / scale_factor();
  *y = GetScrollPos(_hwnd, SB_VERT) / scale_factor();
}

void Win32Widget::scroll_to(float x, float y) {
  SCROLLINFO si;
  
  si.cbSize = sizeof(si);
  si.fMask  = SIF_ALL;
  
  int oldx, newx, oldy, newy;
  
  GetScrollInfo(_hwnd, SB_HORZ, &si);
  oldx = si.nPos;
  si.nPos = floor(x * scale_factor() + 0.5);
  SetScrollInfo(_hwnd, SB_HORZ, &si, TRUE);
  GetScrollInfo(_hwnd, SB_HORZ, &si);
  newx = si.nPos;
  
  GetScrollInfo(_hwnd, SB_VERT, &si);
  oldy = si.nPos;
  si.nPos = floor(y * scale_factor() + 0.5);
  SetScrollInfo(_hwnd, SB_VERT, &si, TRUE);
  GetScrollInfo(_hwnd, SB_VERT, &si);
  newy = si.nPos;
  
  if(oldx != newx || oldy != newy) {
    RECT norect = {0, 0, 0, 0};
    
    ScrollWindow(_hwnd, oldx - newx, oldy - newy, &norect, &norect);
    invalidate();
  }
}

void Win32Widget::show_tooltip(Expr boxes) {
  Win32TooltipWindow::show_global_tooltip(boxes, document()->stylesheet());
}

void Win32Widget::hide_tooltip() {
  Win32TooltipWindow::hide_global_tooltip();
}

double Win32Widget::message_time() {
  return GetMessageTime() / 1000.0;
}

double Win32Widget::double_click_time() {
  return GetDoubleClickTime() / 1000.0;
}

void Win32Widget::double_click_dist(float *dx, float *dy) {
  *dx = GetSystemMetrics(SM_CXDOUBLECLK) / scale_factor();
  *dy = GetSystemMetrics(SM_CYDOUBLECLK) / scale_factor();
}

void Win32Widget::do_drag_drop(Box *src, int start, int end) {
  if(is_dragging || !src || start >= end)
    return;
    
  is_dragging = true;
  drag_source_reference().set(src, start, end);
  
  scrolling = false;
  
  DataObject *data_object = new DataObject;
  
  data_object->source = drag_source_reference();
  data_object->add_source_format(Win32Clipboard::mime_to_win32cbformat[Clipboard::PlainText]);
  data_object->add_source_format(Win32Clipboard::mime_to_win32cbformat[Clipboard::BoxesText]);
  
  DropSource *drop_source = new DropSource;
  
  DWORD effect = DROPEFFECT_COPY;
  if(src->get_style(Editable))
    effect |= DROPEFFECT_MOVE;
  
  if(Win32Themes::is_app_themed()) { 
    if(auto helper2 = _drag_source_helper.as<IDragSourceHelper2>()) {
      pmath_debug_print("[using drop source helper ...]\n");
      drop_source->description_data.copy(data_object);
    }
  }
  
  drop_source->set_drag_image_from_window(nullptr);
  
  HRESULT res = DoDragDrop(data_object, drop_source, effect, &effect);
  
  Document *doc = src->find_parent<Document>(true);
  
  if(res == DRAGDROP_S_DROP) {
    if(effect & DROPEFFECT_MOVE) {
      src   = drag_source_reference().get();
      start = drag_source_reference().start;
      end   = drag_source_reference().end;
      
      if(src && end <= src->length() && doc) {
        doc->select(src, start, end);
        if(!doc->remove_selection(false))
          beep();
      }
    }
  }
  
  data_object->Release();
  drop_source->Release();
  
  drag_source_reference().reset();
  is_dragging = false;
  
  if(doc)
    doc->reset_mouse(); // DoDragDrop eats the mouse-up message
}

bool Win32Widget::cursor_position(float *x, float *y) {
  POINT pt;
  if(GetCursorPos(&pt)) {
    ScreenToClient(_hwnd, &pt);
    *x = pt.x + GetScrollPos(_hwnd, SB_HORZ);
    *y = pt.y + GetScrollPos(_hwnd, SB_VERT);
    
    *x /= scale_factor();
    *y /= scale_factor();
    return true;
  }
  
  return false;
}

void Win32Widget::bring_to_front() {
  SetFocus(_hwnd);
}

void Win32Widget::invalidate() {
  is_painting = false; // if inside WM_PAINT, invalidate at end of event
  InvalidateRect(_hwnd, 0, FALSE);
}

void Win32Widget::invalidate_options() {
}

void Win32Widget::invalidate_rect(float x, float y, float w, float h) {
  is_painting = false; // if inside WM_PAINT, invalidate at end of event
  
  float sx, sy, sf;
  scroll_pos(&sx, &sy);
  sf = scale_factor();
  
  RECT rect;
  rect.left   = (int)floorf((x - sx) * sf) - 4;
  rect.top    = (int)floorf((y - sy) * sf) - 4;
  rect.right  = rect.left + (int)ceilf(w * sf) + 8;
  rect.bottom = rect.top  + (int)ceilf(h * sf) + 8;
  
  InvalidateRect(_hwnd, &rect, FALSE);
}

void Win32Widget::force_redraw() {
  RECT rect;
  GetClientRect(_hwnd, &rect);
  if(_width != rect.right || _height != rect.bottom) // called before WM_SIZE
    return;
    
  is_painting = true;
  {
    HDC dc = GetDC(_hwnd);
    on_paint(dc, true);
    ReleaseDC(_hwnd, dc);
  }
  
  if(!is_painting) {
    invalidate();
  }
  is_painting = false;
}

void Win32Widget::set_cursor(CursorType type) {
  if(mouse_moving) {
    cursor = type;
    return;
  }
  
  switch(type) {
    case FingerCursor:
      SetCursor(LoadCursor(0, IDC_HAND));
      return;
      
    case DefaultCursor:
      SetCursor(LoadCursor(0, IDC_ARROW));
      return;
      
    case CurrentCursor:
      break;
      
    case SizeNCursor:
    case SizeSCursor:
      SetCursor(LoadCursor(0, IDC_SIZENS));
      return;
      
    case SizeNWCursor:
    case SizeSECursor:
      SetCursor(LoadCursor(0, IDC_SIZENWSE));
      return;
      
    case SizeECursor:
    case SizeWCursor:
      SetCursor(LoadCursor(0, IDC_SIZEWE));
      return;
      
    case SizeNECursor:
    case SizeSWCursor:
      SetCursor(LoadCursor(0, IDC_SIZENESW));
      return;
      
    default:
      SetCursor(LoadCursor(GetModuleHandle(0), MAKEINTRESOURCE((int)type)));
  }
}

void Win32Widget::running_state_changed() {
}

bool Win32Widget::is_mouse_down() {
  return GetKeyState(VK_LBUTTON)  < 0 ||
         GetKeyState(VK_RBUTTON)  < 0 ||
         GetKeyState(VK_MBUTTON)  < 0 ||
         GetKeyState(VK_XBUTTON1) < 0 ||
         GetKeyState(VK_XBUTTON2) < 0;
}

void Win32Widget::beep() {
  MessageBeep(0);
}

bool Win32Widget::register_timed_event(SharedPtr<TimedEvent> event) {
  if(!_hwnd)
    return false;
    
  animations.add(event);
  if(!animation_running) {
    animation_running = 0 != SetTimer(_hwnd, TID_ANIMATE, ANIMATION_DELAY, nullptr);
    
    if(!animation_running) {
      animations.remove(event);
      return false;
    }
  }
  
  return true;
}

STDMETHODIMP Win32Widget::DragEnter(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect) {
  if(!is_dragging) {
    drag_source_reference().set(
      document()->selection_box(),
      document()->selection_start(),
      document()->selection_end());
  }
  
  is_drop_over = true;
  
  return BasicWin32Widget::DragEnter(data_object, key_state, pt, effect);
}

STDMETHODIMP Win32Widget::DragLeave(void) {
  if(drag_source_reference().get()) {
    document()->select(
      drag_source_reference().get(),
      drag_source_reference().start,
      drag_source_reference().end);
  }
  
  if(!is_dragging)
    drag_source_reference().reset();
    
  is_drop_over = false;
  return BasicWin32Widget::DragLeave();
}

STDMETHODIMP Win32Widget::DataInterest(RealTimeStylusDataInterest* pEventInterest) {
  *pEventInterest = (RealTimeStylusDataInterest)(
                      RTSDI_StylusDown |
                      RTSDI_StylusUp);
  return S_OK;
}

STDMETHODIMP Win32Widget::StylusDown(IRealTimeStylus *piRtsSrc, const StylusInfo *pStylusInfo, ULONG cPropCountPerPkt, LONG *pPacket, LONG **ppInOutPkt) {
  ComBase<IInkTablet> tablet;
  HR(piRtsSrc->GetTabletFromTabletContextId(pStylusInfo->tcid, tablet.get_address_of()));
  
  auto kind = (TabletDeviceKind) - 1;
  if(auto tablet2 = tablet.as<IInkTablet2>()) {
    HR(tablet2->get_DeviceKind(&kind));
  }
  
  // ink space is in 0.01mm. 72pt = 1inch = 25.4mm So 0.01mm = 72/2540 pt
  float x_pt = pPacket[0] * 72.0f / 2540.0f;
  float y_pt = pPacket[1] * 72.0f / 2540.0f;
  
  fprintf(
    stderr,
    "[%u StylusDown tablet %u (%s), cid %u %sat (%f,%f)]\n",
    GetCurrentThreadId(),
    pStylusInfo->tcid,
    kind == TDK_Mouse ? "mouse" : (kind == TDK_Pen ? "pen" : (kind == TDK_Touch ? "touch" : "???")),
    pStylusInfo->cid,
    pStylusInfo->bIsInvertedCursor ? "(inverted) " : " ",
    (double)x_pt,
    (double)y_pt);
    
  //StylusUtil::debug_describe_packet_data_definition(piRtsSrc, pStylusInfo->tcid);
  StylusUtil::debug_describe_packet_data(piRtsSrc, pStylusInfo->tcid, pPacket);
  return S_OK;
}

STDMETHODIMP Win32Widget::StylusUp(IRealTimeStylus *piRtsSrc, const StylusInfo *pStylusInfo, ULONG cPropCountPerPkt, LONG *pPacket, LONG **ppInOutPkt) {
  fprintf(
    stderr,
    "[%u StylusUp tablet %u, stylus %u %s]\n",
    GetCurrentThreadId(),
    pStylusInfo->tcid,
    pStylusInfo->cid,
    pStylusInfo->bIsInvertedCursor ? "inverted" : "");
  return S_OK;
}

static const char *describe_system_event(SYSTEM_EVENT event) {
  switch(event) {
    case ISG_Tap:        return "ISG_Tap";
    case ISG_DoubleTap:  return "ISG_DoubleTap";
    case ISG_RightTap:   return "ISG_RightTap";
    case ISG_Drag:       return "ISG_Drag";
    case ISG_RightDrag:  return "ISG_RightDrag";
    case ISG_HoldEnter:  return "ISG_HoldEnter";
    case ISG_HoldLeave:  return "ISG_HoldLeave";
    case ISG_HoverEnter: return "ISG_HoverEnter";
    case ISG_HoverLeave: return "ISG_HoverLeave";
    case ISG_Flick:      return "ISG_Flick";
  }
  return "???";
}

void Win32Widget::paint_background(Canvas *canvas) {
  canvas->set_color(0xffffff);
  canvas->paint();
}

void Win32Widget::paint_canvas(Canvas *canvas, bool resize_only) {
  cairo_set_line_width(canvas->cairo(), 1);
  cairo_set_line_cap(canvas->cairo(), CAIRO_LINE_CAP_SQUARE);
  canvas->set_font_size(10);// 10 * 4/3.
  
  if(!resize_only) {
    int color = document()->get_style(Background, -1);
    if(color >= 0) {
      canvas->set_color(color);
      canvas->paint();
    }
    else
      paint_background(canvas);
  }
  
  canvas->scale(scale_factor(), scale_factor());
  canvas->set_color(document()->get_style(FontColor, 0));
  
  document()->paint_resize(canvas, resize_only);
  if( _hwnd &&
      _hwnd == GetFocus() &&
      document()->selection_box() &&
      document()->selection_length() == 0 &&
      GetCaretBlinkTime() != INFINITE)
  {
    SetTimer(_hwnd, TID_BLINKCURSOR, GetCaretBlinkTime(), nullptr);
  }
  
  canvas->scale(1 / scale_factor(), 1 / scale_factor());
  
  float w, h;
  window_size(&w, &h);
  
  if(scrolling && mouse_down_event.middle) {
    SCROLLINFO si;
    
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_RANGE;
    
    GetScrollInfo(_hwnd, SB_VERT, &si);
    bool vert = si.nMax - si.nMin > (int)si.nPage;
    
    GetScrollInfo(_hwnd, SB_HORZ, &si);
    bool horz = si.nMax - si.nMin > (int)si.nPage;
    
    canvas->new_path();
    ControlPainter::std->paint_scroll_indicator(
      canvas,
      mouse_down_event.x,
      mouse_down_event.y,
      horz,//w < document()->extents().width,
      vert);
  }
  
  if(is_scrollable()) {
    RECT outer;
    GetWindowRect(_hwnd, &outer);
    
    int w_page = floorf(scale_factor() * w + 0.5f);
    int h_page = floorf(scale_factor() * h + 0.5f);
    
    int w_max = floorf(scale_factor() * document()->extents().width + 0.5f);
    int h_max;
    
    if(autohide_vertical_scrollbar())
      h_max = floorf(document()->extents().height()            * scale_factor() + 0.5f);
    else
      h_max = floorf((document()->extents().height() + h * 0.8) * scale_factor() + 0.5f);
      
    if(outer.bottom - outer.top  >= h_max)
      h_page = h_max + 1;
      
    if(outer.right  - outer.left >= w_max)
      w_page = w_max + 1;
      
    SCROLLINFO si;
    
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_RANGE;
    si.nMin = 0;
    
    si.nMax = h_max;
    si.nPage = h_page;
    
    SetScrollInfo(_hwnd, SB_VERT, &si, TRUE);
    
    si.nMax = w_max;
    si.nPage = w_page;
    
    SetScrollInfo(_hwnd, SB_HORZ, &si, TRUE);
    ShowScrollBar(_hwnd, SB_HORZ, (int)si.nPage < si.nMax);
  }
}

void Win32Widget::on_paint(HDC dc, bool from_wmpaint) {
  RECT rect;
  GetClientRect(_hwnd, &rect);
  
  _dpi = GetDeviceCaps(dc, LOGPIXELSY);
  
  cairo_surface_t *target = cairo_win32_surface_create_with_dib(
                              _image_format,
                              rect.right,
                              rect.bottom);
                              
  cairo_t *cr = cairo_create(target);
  {
    Canvas canvas(cr);
    
//    if(_image_format == CAIRO_FORMAT_ARGB32){
//      cairo_font_options_t *opts = cairo_font_options_create();
//      cairo_font_options_set_antialias(opts, CAIRO_ANTIALIAS_GRAY);
//      cairo_set_font_options(cr, opts);
//      cairo_font_options_destroy(opts);
//    }

//  {
//    cairo_font_options_t *opts = cairo_font_options_create();
//    cairo_font_options_set_hint_metrics(opts, CAIRO_HINT_METRICS_OFF);
//    cairo_set_font_options(cr, opts);
//    cairo_surface_get_font_options(target, opts);
//    cairo_font_options_destroy(opts);
//  }

    if(!from_wmpaint) {
      canvas.clip();
    }
    else {
      canvas.move_to(rect.left,  rect.top);
      canvas.line_to(rect.right, rect.top);
      canvas.line_to(rect.right, rect.bottom);
      canvas.line_to(rect.left,  rect.bottom);
      canvas.close_path();
      canvas.clip();
    }
    
    paint_canvas(&canvas, !from_wmpaint);
  }
  cairo_destroy(cr);
  cairo_surface_flush(target);
  
  if(from_wmpaint) {
    BitBlt(dc, 0, 0, rect.right, rect.bottom,
           cairo_win32_surface_get_dc(target), 0, 0, SRCCOPY);
  }
  
  cairo_surface_destroy(target);
}

void Win32Widget::on_hscroll(WORD kind, WORD thumbPos) {
  SCROLLINFO si;
  
  si.cbSize = sizeof(si);
  si.fMask  = SIF_ALL;
  GetScrollInfo(_hwnd, SB_HORZ, &si);
  
  int xPos = si.nPos;
  
  switch(kind) {
    case SB_LEFT:
      si.nPos = si.nMin;
      break;
      
    case SB_RIGHT:
      si.nPos = si.nMax;
      break;
      
    case SB_LINELEFT:
      si.nPos -= 20;
      break;
      
    case SB_LINERIGHT:
      si.nPos += 20;
      break;
      
    case SB_PAGELEFT:
      si.nPos -= si.nPage;
      break;
      
    case SB_PAGERIGHT:
      si.nPos += si.nPage;
      break;
      
    case SB_THUMBTRACK:
      si.nPos = si.nTrackPos;
      break;
      
    case SB_THUMBPOSITION:
      si.nPos = thumbPos;
      break;
  }
  
  si.fMask = SIF_POS;
  SetScrollInfo(_hwnd, SB_HORZ, &si, TRUE);
  GetScrollInfo(_hwnd, SB_HORZ, &si);
  
  if(si.nPos != xPos) {
    RECT norect = {0, 0, 0, 0};
    ScrollWindow(_hwnd, xPos - si.nPos, 0, &norect, &norect);
    invalidate();
  }
}

void Win32Widget::on_vscroll(WORD kind, WORD thumbPos) {
  SCROLLINFO si;
  
  si.cbSize = sizeof(si);
  si.fMask  = SIF_ALL;
  GetScrollInfo(_hwnd, SB_VERT, &si);
  
  int yPos = si.nPos;
  
  switch(kind) {
    case SB_TOP:
      si.nPos = si.nMin;
      break;
      
    case SB_BOTTOM:
      si.nPos = si.nMax;
      break;
      
    case SB_LINEUP:
      si.nPos -= 20;
      break;
      
    case SB_LINEDOWN:
      si.nPos += 20;
      break;
      
    case SB_PAGEUP:
      si.nPos -= si.nPage;
      break;
      
    case SB_PAGEDOWN:
      si.nPos += si.nPage;
      break;
      
    case SB_THUMBTRACK:
      si.nPos = si.nTrackPos;
      break;
      
    case SB_THUMBPOSITION:
      si.nPos = thumbPos;
      break;
  }
  
  si.fMask = SIF_POS;
  SetScrollInfo(_hwnd, SB_VERT, &si, TRUE);
  GetScrollInfo(_hwnd, SB_VERT, &si);
  
  if(si.nPos != yPos) {
    RECT norect = {0, 0, 0, 0};
    ScrollWindow(_hwnd, 0, yPos - si.nPos, &norect, &norect);
    invalidate();
  }
}

void Win32Widget::on_mousedown(MouseEvent &event) {
  if(event.left)
    SetCapture(_hwnd);
    
  bool may_start_scrolling = !scrolling;
  if(scrolling) {
    KillTimer(_hwnd, TID_SCROLL);
    scrolling = false;
    invalidate();
  }
  
  document()->mouse_down(event);
  
  if(may_start_scrolling && is_scrollable()) {
    if(event.middle || event.left) {
      SCROLLINFO si;
      
      si.cbSize = sizeof(si);
      si.fMask = SIF_PAGE | SIF_RANGE;
      
      GetScrollInfo(_hwnd, SB_VERT, &si);
      bool vert = si.nMax - si.nMin > (int)si.nPage;
      
      GetScrollInfo(_hwnd, SB_HORZ, &si);
      bool horz = si.nMax - si.nMin > (int)si.nPage;
      
      if(vert || horz) {
        scrolling = true;
        already_scrolled = !event.middle;
        float sx, sy;
        scroll_pos(&sx, &sy);
        mouse_down_event = event;
        mouse_down_event.x = (event.x - sx) * scale_factor();
        mouse_down_event.y = (event.y - sy) * scale_factor();
        invalidate();
        SetTimer(_hwnd, TID_SCROLL, 20, 0);
      }
    }
  }
  
  if(document()->selection_box()) {
    SetFocus(_hwnd);
  }
  else {
    SetWindowPos(GetAncestor(_hwnd, GA_ROOT), HWND_TOP, 0, 0, 1, 1, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
  }
//  else {
//    Document *cur = get_current_document();
//    if(cur && cur != document()) {
//      Win32Widget *wig = dynamic_cast<Win32Widget *>(cur->native());
//
//      if(wig && wig->hwnd() != GetFocus()) {
//        SetFocus(wig->hwnd());
//      }
//    }
//  }
}

void Win32Widget::on_mouseup(MouseEvent &event) {
  ReleaseCapture();
  
  document()->mouse_up(event);
  
  if(scrolling && already_scrolled) {
    scrolling = false;
    KillTimer(_hwnd, TID_SCROLL);
    invalidate();
  }
}

void Win32Widget::on_mousemove(MouseEvent &event) {
  mouse_moving = true;
  cursor = DefaultCursor;
  
  Win32TooltipWindow::move_global_tooltip();
  document()->mouse_move(event);
  
  if(scrolling && mouse_down_event.middle)
    already_scrolled = false;
    
  mouse_moving = false;
  set_cursor(cursor);
}

void Win32Widget::on_keydown(DWORD virtkey, bool ctrl, bool alt, bool shift) {
  SpecialKeyEvent event;
  event.key = win32_virtual_to_special_key(virtkey);
  
  if(event.key != SpecialKey::Unknown) {
    event.ctrl  = ctrl;
    event.alt   = alt;
    event.shift = shift;
    document()->key_down(event);
  }
  
  switch(virtkey) {
    case VK_CAPITAL: {
        if(GetKeyState(VK_CAPITAL) & 0x1) {
          keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY, 0);
          keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
        }
        else
          document()->key_press(PMATH_CHAR_ALIASDELIMITER);
      } break;
  }
}

void Win32Widget::on_popupmenu(POINT screen_pt) {
  UINT flags = 0;
  
  if(GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0)
    flags |= TPM_LEFTALIGN;
  else
    flags |= TPM_RIGHTALIGN;
    
  TrackPopupMenuEx(
    Win32Menu::popup_menu->hmenu(),
    flags,
    screen_pt.x,
    screen_pt.y,
    _hwnd,
    nullptr);
}

LRESULT Win32Widget::callback(UINT message, WPARAM wParam, LPARAM lParam) {

  switch(message) {
    case WM_SIZE: {
        AutoMemorySuspension ams;
        RECT rect;
        GetClientRect(_hwnd, &rect);
        _width  = rect.right;
        _height = rect.bottom;
        
        if(!initializing()) {
          if(_width * scale_factor() != document()->extents().width)
            document()->invalidate_all();
          else
            document()->invalidate();
        }
      } return 0;
  }
  
  if(!initializing()) {
    AutoMemorySuspension ams;
    switch(message) {
      case WM_ERASEBKGND:
        return 1;
        
      case WM_PRINT:
      case WM_PRINTCLIENT: {
          on_paint((HDC)wParam, true);
        } return 0;
        
      case WM_PAINT: {
          RECT rect;
          GetClientRect(_hwnd, &rect);
          if(_width != rect.right || _height != rect.bottom) // called before WM_SIZE
            return 0;
            
          is_painting = true;
          {
            PAINTSTRUCT paintStruct;
            HDC dc = BeginPaint(_hwnd, &paintStruct);
            SetLayout(dc, 0);
            on_paint(dc, true);
            EndPaint(_hwnd, &paintStruct);
          }
          
          if(!is_painting) {
            invalidate();
          }
          is_painting = false;
        } return 0;
        
      case WM_CONTEXTMENU:  {
          POINT pt;
          if(lParam == -1) {
          
            if(GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0) {
              pt.x = 0;
              pt.y = 0;
            }
            else {
              RECT rect;
              
              GetClientRect(_hwnd, &rect);
              pt.x = rect.right;
              pt.y = 0;
            }
            
            ClientToScreen(_hwnd, &pt);
          }
          else {
            pt.x = (int16_t)( lParam & 0xFFFF);
            pt.y = (int16_t)((lParam & 0xFFFF0000) >> 16);
          }
          on_popupmenu(pt);
        } return 0;
        
      case WM_INITMENUPOPUP: {
          HMENU sub = (HMENU)wParam;
          
          Win32Menu::init_popupmenu(sub);
        } return 0;
        
      case WM_HSCROLL: {
          on_hscroll(LOWORD(wParam), HIWORD(wParam));
        } return 0;
        
      case WM_VSCROLL: {
          on_vscroll(LOWORD(wParam), HIWORD(wParam));
        } return 0;
        
      case WM_MOUSEHWHEEL: {
          POINT pt;
          if(GetCursorPos(&pt)) {
            RECT rect;
            GetClientRect(_hwnd, &rect);
            ScreenToClient(_hwnd, &pt);
            
            if(!PtInRect(&rect, pt)) {
              HWND parent = (HWND)GetWindowLongPtr(_hwnd, GWLP_HWNDPARENT);
              return SendMessageW(parent, message, wParam, lParam);
            }
          }
          
          unsigned int num_chars;
          if(!SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &num_chars, 0))
            num_chars = 3;
            
          if(num_chars == 0)
            return 0;
            
          int rel_wheel = (int16_t)HIWORD(wParam);
          float delta = (float)num_chars * -10 * rel_wheel / (float)WHEEL_DELTA;
          
          SCROLLINFO si;
          si.cbSize = sizeof(si);
          si.fMask  = SIF_ALL;
          GetScrollInfo(_hwnd, SB_HORZ, &si);
          float max_scroll = si.nPage / scale_factor();
          if(delta > max_scroll)
            delta = max_scroll;
          if(delta < -max_scroll)
            delta = -max_scroll;
            
          scroll_by(delta, 0);
        } return 0;
        
      case WM_MOUSEWHEEL: {
          int rel_wheel = (int16_t)HIWORD(wParam);
          
          if(GetKeyState(VK_CONTROL) & ~1) {
            scale_by(pow(2, 0.5 * rel_wheel / (float)WHEEL_DELTA));
          }
          else {
            POINT pt;
            if(GetCursorPos(&pt)) {
              RECT rect;
              GetClientRect(_hwnd, &rect);
              ScreenToClient(_hwnd, &pt);
              
              if(!PtInRect(&rect, pt)) {
                HWND parent = (HWND)GetWindowLongPtr(_hwnd, GWLP_HWNDPARENT);
                return SendMessageW(parent, message, wParam, lParam);
              }
            }
            
            unsigned int num_lines;
            if(!SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &num_lines, 0))
              num_lines = 3;
              
            if(num_lines == 0)
              return 0;
              
            float delta = (float)num_lines * -20 * rel_wheel / (float)WHEEL_DELTA;
            
            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask  = SIF_ALL;
            GetScrollInfo(_hwnd, SB_VERT, &si);
            float max_scroll = si.nPage / scale_factor();
            if(delta > max_scroll)
              delta = max_scroll;
            if(delta < -max_scroll)
              delta = -max_scroll;
              
            scroll_by(0, delta);
          }
        } return 0;
        
      case WM_LBUTTONDOWN:
      case WM_MBUTTONDOWN:
      case WM_RBUTTONDOWN: {
          MouseEvent event;
          
          event.left   = message == WM_LBUTTONDOWN;
          event.middle = message == WM_MBUTTONDOWN;
          event.right  = message == WM_RBUTTONDOWN;
          
          event.x = (int16_t)( lParam & 0xFFFF)            + GetScrollPos(_hwnd, SB_HORZ);
          event.y = (int16_t)((lParam & 0xFFFF0000) >> 16) + GetScrollPos(_hwnd, SB_VERT);
          
          event.x /= scale_factor();
          event.y /= scale_factor();
          
          event.device = Win32Touch::get_mouse_message_source(&event.id);
          fprintf(
            stderr,
            "[WM_%sBUTTONDOWN: %s id %d at (%f,%f)]\n",
            message == WM_LBUTTONDOWN ? "L" : (message == WM_MBUTTONDOWN ? "M" : "R"),
            event.device == DeviceKind::Mouse ? "mouse" : (event.device == DeviceKind::Pen ? "pen" : "touch"),
            event.id,
            (double)event.x,
            (double)event.y);
            
          on_mousedown(event);
        } return 0;
        
      case WM_LBUTTONUP:
      case WM_MBUTTONUP:
      case WM_RBUTTONUP: {
          MouseEvent event;
          
          event.device = Win32Touch::get_mouse_message_source(&event.id);
          fprintf(
            stderr,
            "[WM_%sBUTTONUP: %s id %d]\n",
            message == WM_LBUTTONUP ? "L" : (message == WM_MBUTTONUP ? "M" : "R"),
            event.device == DeviceKind::Mouse ? "mouse" : (event.device == DeviceKind::Pen ? "pen" : "touch"),
            event.id);
            
          event.left   = message == WM_LBUTTONUP;
          event.middle = message == WM_MBUTTONUP;
          event.right  = message == WM_RBUTTONUP;
          
          POINT pt;
          pt.x = (int16_t)( lParam & 0xFFFF);
          pt.y = (int16_t)((lParam & 0xFFFF0000) >> 16);
          
          event.x = pt.x + GetScrollPos(_hwnd, SB_HORZ);
          event.y = pt.y + GetScrollPos(_hwnd, SB_VERT);
          
          event.x /= scale_factor();
          event.y /= scale_factor();
          
          on_mouseup(event);
          
          if(message == WM_RBUTTONUP) {
            ClientToScreen(_hwnd, &pt);
            on_popupmenu(pt);
          }
        } return 0;
        
      case WM_MOUSEMOVE: {
          MouseEvent event;
          event.device = Win32Touch::get_mouse_message_source(&event.id);
//          int cursorId;
//          PointerEventSource source = Win32Touch::get_mouse_message_source(&cursorId);
//          pmath_debug_print(
//            "[WM_MOUSEMOVE: %s id %d]\n",
//            source == PointerEventSource::Mouse ? "mouse" : (source == PointerEventSource::Pen ? "pen" : "touch"),
//            cursorId);

          event.left   = (wParam & MK_LBUTTON) != 0;
          event.middle = (wParam & MK_MBUTTON) != 0;
          event.right  = (wParam & MK_RBUTTON) != 0;
          
          event.x = (int16_t)(lParam & 0xFFFF)            + GetScrollPos(_hwnd, SB_HORZ);
          event.y = (int16_t)((lParam & 0xFFFF0000) >> 16) + GetScrollPos(_hwnd, SB_VERT);
          
          event.x /= scale_factor();
          event.y /= scale_factor();
          
          on_mousemove(event);
          
          
          TRACKMOUSEEVENT tme;
          memset(&tme, 0, sizeof(tme));
          tme.cbSize = sizeof(tme);
          tme.dwFlags = TME_LEAVE;
          tme.hwndTrack = _hwnd;
          tme.dwHoverTime = HOVER_DEFAULT;
          
          TrackMouseEvent(&tme);
          
          // NOTE: The internet says, WM_MOUSEMOVE messages must be passed on to
          // DefWindowProc for WM_TOUCH messages to be emitted.
          // This does not seem to be necessary on Windows 7, but maybe later?
        } return 0;
        
      case WM_MOUSELEAVE: {
          document()->mouse_exit();
        } return 0;
        
      case WM_GESTURENOTIFY: {
          //Win32Touch::GESTURENOTIFYSTRUCT *note = (Win32Touch::GESTURENOTIFYSTRUCT*)lParam;
          if(Win32Touch::SetGestureConfig) {
            Win32Touch::GESTURECONFIG gc[] = {
              { GID_ZOOM,
                GC_ZOOM, 0
              },
              { GID_PAN,
                GC_PAN | GC_PAN_WITH_INERTIA | GC_PAN_WITH_GUTTER | GC_PAN_WITH_SINGLE_FINGER_VERTICALLY,
                GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY
              },
              { GID_TWOFINGERTAP,
                GC_TWOFINGERTAP, 0
              },
              { GID_PRESSANDTAP,
                GC_PRESSANDTAP, 0
              }
            };
            Win32Touch::SetGestureConfig(_hwnd, 0, ARRAYSIZE(gc), gc, sizeof(gc[0]));
          }
        } break;
        
      case WM_GESTURE:
        if(Win32Touch::GetGestureInfo) {
          Win32Touch::GESTUREINFO gi;
          
          memset(&gi, 0, sizeof(gi));
          gi.cbSize = sizeof(gi);
          
          if(Win32Touch::GetGestureInfo((HANDLE)lParam, &gi)) {
            bool handled = false;
            
            switch(gi.dwID) {
              case GID_ZOOM: {
                  handled = true;
                  
                  if(gi.dwFlags & GF_BEGIN) {
                    gesture_zoom_factor = custom_scale_factor() / (double)(uint32_t)(gi.ullArguments);
                  }
                  else {
                    double relzoom = (double)(uint32_t)(gi.ullArguments) * gesture_zoom_factor;
//                    pmath_debug_print("[%s%s%szoom %f gesture %d at (%d,%d)]\n",
//                                      (gi.dwFlags & GF_BEGIN)   ? "begin " : "",
//                                      (gi.dwFlags & GF_INERTIA) ? "inerta " : "",
//                                      (gi.dwFlags & GF_END)     ? "end " : "",
//                                      relzoom,
//                                      (int)gi.ullArguments,
//                                      (int)gi.ptsLocation.x,
//                                      (int)gi.ptsLocation.y);

                    set_custom_scale(relzoom);
                  }
                } break;
                
              case GID_TWOFINGERTAP: {
                  handled = true;
                  set_custom_scale(1.0f);
//                  pmath_debug_print(
//                    "[%s%s%s2-finger tap gesture (finger distance %llu) at (%d,%d)]\n",
//                    (gi.dwFlags & GF_BEGIN)   ? "begin " : "",
//                    (gi.dwFlags & GF_INERTIA) ? "inerta " : "",
//                    (gi.dwFlags & GF_END)     ? "end " : "",
//                    gi.ullArguments,
//                    (int)gi.ptsLocation.x,
//                    (int)gi.ptsLocation.y);
                } break;
            }
            
            if(handled) {
              Win32Touch::CloseGestureInfoHandle((HANDLE)lParam);
              return 0;
            }
          }
        }
        break;
        
//      case WM_TOUCH:
//        if(Win32Touch::GetTouchInputInfo) {
//          UINT cInputs = LOWORD(wParam);
//          Array<Win32Touch::TOUCHINPUT> inputs((int)cInputs);
//
//          if(Win32Touch::GetTouchInputInfo((HANDLE)lParam, cInputs, inputs.items(), sizeof(Win32Touch::TOUCHINPUT))) {
//            bool handled = false;
//
//            bool allMoveOnly = true;
//            for(int i = 0;i < inputs.length();++i) {
//              Win32Touch::TOUCHINPUT &ti = inputs[i];
//              if((ti.dwFlags & (TOUCHEVENTF_UP | TOUCHEVENTF_DOWN)) != 0) {
//                allMoveOnly = false;
//              }
//            }
//
//            if(!allMoveOnly) {
//              pmath_debug_print("[WM_TOUCH: %u inputs]\n", cInputs);
//              for(int i = 0;i < inputs.length();++i) {
//                Win32Touch::TOUCHINPUT &ti = inputs[i];
//
//                pmath_debug_print(
//                  "  [id %u %s%s%s%s%s%s%s%sat (%.2f,%.2f) %s%.2fx%.2f]\n",
//                  ti.dwID,
//                  (ti.dwFlags & TOUCHEVENTF_PEN)        ? "pen "           : "", // does not happen?
//                  (ti.dwFlags & TOUCHEVENTF_PRIMARY)    ? "primary "       : "",
//                  (ti.dwFlags & TOUCHEVENTF_MOVE)       ? "move "          : "",
//                  (ti.dwFlags & TOUCHEVENTF_DOWN)       ? "down "          : "",
//                  (ti.dwFlags & TOUCHEVENTF_UP)         ? "up "            : "",
//                  (ti.dwFlags & TOUCHEVENTF_INRANGE)    ? "inrange "       : "",
//                  (ti.dwFlags & TOUCHEVENTF_NOCOALESCE) ? "non-coalesced " : "coalesced ",
//                  (ti.dwFlags & TOUCHEVENTF_PALM)       ? "palm "          : "",
//                  ti.x / 100.0f,
//                  ti.y / 100.0f,
//                  (ti.dwMask & TOUCHINPUTMASKF_CONTACTAREA) ? "contact area ": "ignore area ",
//                  ti.cxContact / 100.0f,
//                  ti.cyContact / 100.0f);
//              }
//            }
//
//            if(handled) {
//              Win32Touch::CloseTouchInputHandle((HANDLE)lParam);
//              return 0;
//            }
//          }
//        }
//        break;

      case WM_TIMER: {
          switch(wParam) {
            case TID_SCROLL: {
                if(scrolling) {
                  POINT mouse;
                  GetCursorPos(&mouse);
                  ScreenToClient(_hwnd, &mouse);
                  
                  float relx = 0;
                  float rely = 0;
                  if(mouse_down_event.middle) {
                    if(abs(mouse.x - mouse_down_event.x) > 10)
                      relx = abs(mouse.x - mouse_down_event.x) / 4;
                      
                    if(abs(mouse.y - mouse_down_event.y) > 10)
                      rely = abs(mouse.y - mouse_down_event.y) / 4;
                      
                    relx /= scale_factor();
                    rely /= scale_factor();
                    
                    if(mouse.x < mouse_down_event.x)
                      relx = -relx;
                      
                    if(mouse.y < mouse_down_event.y)
                      rely = -rely;
                  }
                  else if(mouse_down_event.left) {
                    RECT rect;
                    GetClientRect(_hwnd, &rect);
                    
                    if(mouse.x < 0)
                      relx = mouse.x / 4;
                    else if(mouse.x > rect.right)
                      relx = (mouse.x - rect.right) / 4;
                      
                    if(mouse.y < 0)
                      rely = mouse.y / 4;
                    else if(mouse.y > rect.bottom)
                      rely = (mouse.y - rect.bottom) / 4;
                  }
                  
                  if(relx != 0 || rely != 0) {
                    scroll_by(relx, rely);
                    
                    MouseEvent event;
                    
                    event.left   = (GetKeyState(VK_LBUTTON) & ~1);
                    event.middle = (GetKeyState(VK_MBUTTON) & ~1);
                    event.right  = (GetKeyState(VK_RBUTTON) & ~1);
                    
                    event.x = mouse.x + GetScrollPos(_hwnd, SB_HORZ);
                    event.y = mouse.y + GetScrollPos(_hwnd, SB_VERT);
                    
                    event.x /= scale_factor();
                    event.y /= scale_factor();
                    
                    on_mousemove(event);
                  }
                }
                else
                  KillTimer(_hwnd, TID_SCROLL);
              } break;
              
            case TID_ANIMATE: {
                KillTimer(_hwnd, TID_ANIMATE);
                animation_running = 0;
                
                for(auto e : animations.deletable_entries()) {
                  if(e.key->min_wait_seconds <= e.key->timer()) {
                    auto anim = e.key;
                    e.delete_self();
                    anim->execute_event();
                  }
                  else if(!animation_running) {
                    animation_running = 0 != SetTimer(_hwnd, TID_ANIMATE, ANIMATION_DELAY, nullptr);
                    
                    if(!animation_running) {
                      auto anim = e.key;
                      e.delete_self();
                      anim->execute_event();
                    }
                  }
                }
              } break;
              
            case TID_BLINKCURSOR: {
                KillTimer(_hwnd, TID_BLINKCURSOR);
                
                Context *ctx = document_context();
                if( ctx->old_selection == ctx->selection ||
                    _hwnd != GetFocus() ||
                    is_mouse_down())
                {
                  ctx->old_selection.id = FrontEndReference::None;
                }
                else
                  ctx->old_selection = ctx->selection;
                  
                if(Box *box = ctx->selection.get())
                  box->request_repaint_range(ctx->selection.start, ctx->selection.end);
              } break;
          }
        } return 0;
        
      case WM_KEYDOWN: if(!is_drop_over) {
          on_keydown(
            wParam,
            GetKeyState(VK_CONTROL) & ~1,
            GetKeyState(VK_MENU)    & ~1,
            GetKeyState(VK_SHIFT)   & ~1);
        } return 0;
        
      case WM_KEYUP: if(!is_drop_over) {
          SpecialKeyEvent event;
          event.key = win32_virtual_to_special_key(wParam);
          if(event.key != SpecialKey::Unknown) {
            event.ctrl  = GetKeyState(VK_CONTROL) & ~1;
            event.alt   = GetKeyState(VK_MENU)    & ~1;
            event.shift = GetKeyState(VK_SHIFT)   & ~1;
            document()->key_up(event);
          }
        } return 0;
        
      case WM_CHAR: if(!is_drop_over) {
          if(wParam == 0xFFFF)
            return 1;
            
          if((wParam == ' ' ||
              wParam == '\r' ||
              wParam == '\n') &&
              ((GetKeyState(VK_CONTROL) & ~1) ||
               (GetKeyState(VK_MENU) & ~1) ||
               (GetKeyState(VK_SHIFT) & ~1)))
          {
            return 0;
          }
          
          if(wParam == '\t')
            return 0;
            
          document()->key_press(wParam);
        } return 0;
        
      case WM_ACTIVATE: {
          if(LOWORD(wParam) == WA_INACTIVE) {
            document()->reset_mouse();
          }
        } break;
        
      case WM_SETFOCUS: {
          Box *box = document()->selection_box();
          if(!box)
            box = document();
            
          if(box->selectable()) {
            set_current_document(document());
          }
          
          if( document()->selection_box() &&
              document()->selection_length() == 0 &&
              GetCaretBlinkTime() != INFINITE)
          {
            SetTimer(_hwnd, TID_BLINKCURSOR, GetCaretBlinkTime(), nullptr);
          }
        } return 0;
        
      case WM_SYSKEYUP: {
          if( wParam == VK_F10                &&
              ( GetKeyState(VK_SHIFT)   & ~1) &&
              !(GetKeyState(VK_MENU)    & ~1) &&
              !(GetKeyState(VK_CONTROL) & ~1))
          {
            SendMessage(_hwnd, WM_CONTEXTMENU, 0, -1);
            return 0;
          }
        }
      /* fall through */
      case WM_SYSKEYDOWN: {
          if(HWND parent = (HWND)GetWindowLongPtr(_hwnd, GWLP_HWNDPARENT)) {
            return SendMessageW(parent, message, wParam, lParam);
          }
        } break;
        
      case WM_COMMAND: {
          Expr cmd = Win32Menu::id_to_command(LOWORD(wParam));
          if(cmd.is_null())
            break;
            
          Application::run_menucommand(cmd);
        } return 0;
    }
  }
  
  return BasicWin32Widget::callback(message, wParam, lParam);
}

bool Win32Widget::is_data_droppable(IDataObject *data_object) {
  FORMATETC fmt;
  memset(&fmt, 0, sizeof(fmt));
  
  fmt.dwAspect = DVASPECT_CONTENT;
  fmt.lindex   = -1;
  fmt.ptd      = nullptr;
  fmt.tymed    = TYMED_HGLOBAL;
  
  fmt.cfFormat = Win32Clipboard::mime_to_win32cbformat[Clipboard::BoxesText];
  if(data_object->QueryGetData(&fmt) == S_OK)
    return true;
    
  fmt.cfFormat = Win32Clipboard::mime_to_win32cbformat[Clipboard::PlainText];
  if(data_object->QueryGetData(&fmt) == S_OK)
    return true;
    
  fmt.cfFormat = CF_TEXT;
  if(data_object->QueryGetData(&fmt) == S_OK)
    return true;
    
  return false;
}

DWORD Win32Widget::drop_effect(DWORD key_state, POINTL ptl, DWORD allowed_effects) {
  POINT pt = {(int) ptl.x, (int)ptl.y };
  ScreenToClient(_hwnd, &pt);
  
  float x = (pt.x + GetScrollPos(_hwnd, SB_HORZ)) / scale_factor();
  float y = (pt.y + GetScrollPos(_hwnd, SB_VERT)) / scale_factor();
  
  int start, end;
  bool was_inside_start;
  Box *dst = document()->mouse_selection(x, y, &start, &end, &was_inside_start);
  
  if(!may_drop_into(dst, start, end, is_dragging))
    return DROPEFFECT_NONE;
    
  return BasicWin32Widget::drop_effect(key_state, ptl, allowed_effects);
}

void Win32Widget::do_drop_data(IDataObject *data_object, DWORD effect) {
  String mimetype;
  String text_data;
  
  STGMEDIUM stgmed;
  memset(&stgmed, 0, sizeof(stgmed));
  
  FORMATETC fmt;
  memset(&fmt, 0, sizeof(fmt));
  
  fmt.dwAspect = DVASPECT_CONTENT;
  fmt.lindex   = -1;
  fmt.ptd      = nullptr;
  fmt.tymed    = TYMED_HGLOBAL;
  
  do {
    mimetype = Clipboard::BoxesText;
    fmt.cfFormat = Win32Clipboard::mime_to_win32cbformat[mimetype];
    if( data_object->QueryGetData(&fmt) == S_OK &&
        data_object->GetData(&fmt, &stgmed) == S_OK)
    {
      const uint16_t *data = (const uint16_t *)GlobalLock(stgmed.hGlobal);
      
      text_data = String::FromUcs2(data, -1);
      
      GlobalUnlock(stgmed.hGlobal);
      ReleaseStgMedium(&stgmed);
      break;
    }
    
    
    mimetype = Clipboard::PlainText;
    fmt.cfFormat = Win32Clipboard::mime_to_win32cbformat[Clipboard::PlainText];
    if( data_object->QueryGetData(&fmt) == S_OK &&
        data_object->GetData(&fmt, &stgmed) == S_OK)
    {
      const uint16_t *data = (const uint16_t *)GlobalLock(stgmed.hGlobal);
      
      text_data = String::FromUcs2(data, -1);
      
      GlobalUnlock(stgmed.hGlobal);
      ReleaseStgMedium(&stgmed);
      break;
    }
    
    fmt.cfFormat = CF_TEXT;
    if( data_object->QueryGetData(&fmt) == S_OK &&
        data_object->GetData(&fmt, &stgmed) == S_OK)
    {
      const char *data = (const char *)GlobalLock(stgmed.hGlobal);
      
      text_data = String(pmath_string_from_native(data, -1));
      
      GlobalUnlock(stgmed.hGlobal);
      ReleaseStgMedium(&stgmed);
      break;
    }
  } while(false);
  
  if(!text_data.is_null()) {
    Box *oldbox  = document()->selection_box();
    int oldstart = document()->selection_start();
    int oldend   = document()->selection_start();
    
    if(effect & DROPEFFECT_MOVE && is_dragging) {
      if(Box *src = drag_source_reference().get()) {
        int s = drag_source_reference().start;
        int e = drag_source_reference().end;
        
        drag_source_reference().reset();
        
        document()->select(src, s, e);
        document()->remove_selection(false);
        
        if(src == oldbox) {
          if(oldstart >= e)
            oldstart -= e - s;
          if(oldend >= e)
            oldend -= e - s;
        }
        
        document()->select(oldbox, oldstart, oldend);
      }
    }
    
    document()->paste_from_text(mimetype, text_data);
    
    Box *newbox  = document()->selection_box();
    int newend   = document()->selection_start();
    
    if(oldbox == newbox) {
//      int inslen = newend - oldstart;
//
//      if(is_dragging
//      && drag_source_reference().get() == oldbox
//      && drag_source_reference().start >= oldend){
//        drag_source_reference().start+= inslen;
//        drag_source_reference().end  += inslen;
//      }

      document()->select(newbox, oldstart, newend);
    }
  }
  
  DragLeave();
}

void Win32Widget::position_drop_cursor(POINTL ptl) {
  POINT pt = {(int) ptl.x, (int)ptl.y };
  ScreenToClient(_hwnd, &pt);
  float x = (pt.x + GetScrollPos(_hwnd, SB_HORZ)) / scale_factor();
  float y = (pt.y + GetScrollPos(_hwnd, SB_VERT)) / scale_factor();
  
  int start, end;
  bool was_inside_start;
  Box *box = document()->mouse_selection(x, y, &start, &end, &was_inside_start);
  
  document()->select(box, start, end);
}

//} ... class Win32Widget
