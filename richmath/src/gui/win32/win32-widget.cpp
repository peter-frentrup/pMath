#define WINVER  0x603

#include <gui/win32/api/win32-themes.h>
#include <gui/win32/win32-widget.h>

#include <climits>
#include <cmath>
#include <cstdio>

#include <cairo-win32.h>

#include <boxes/buttonbox.h>
#include <boxes/gridbox.h>
#include <boxes/section.h>
#include <boxes/mathsequence.h>
#include <eval/binding.h>
#include <eval/application.h>
#include <eval/job.h>
#include <gui/control-painter.h>
#include <gui/win32/ole/dataobject.h>
#include <gui/win32/ole/dropsource.h>
#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-touch.h>
#include <gui/win32/menus/win32-automenuhook.h>
#include <gui/win32/menus/win32-menu.h>
#include <gui/win32/win32-attached-popup-window.h>
#include <gui/win32/win32-clipboard.h>
#include <gui/win32/win32-tooltip-window.h>
#include <util/autovaluereset.h>

#include <resources.h>

#ifndef DM_POINTERHITTEST
#  define DM_POINTERHITTEST   0x0250
#endif

#ifndef WM_MOUSEHWHEEL
#  define WM_MOUSEHWHEEL  0x020E
#endif
#ifndef SPI_GETWHEELSCROLLCHARS
#  define SPI_GETWHEELSCROLLCHARS   0x006C
#endif

const struct {} TimerIdScroll; 
const struct {} TimerIdAnimate; 
const struct {} TimerIdBlinkCursor; 

#define ANIMATION_DELAY  (16)

using namespace richmath;

namespace richmath { namespace strings {
  extern String EmptyString;
  extern String column;
  extern String columns;
  extern String Copy;
  extern String Image;
  extern String Insert;
  extern String Insert_placeholder;
  extern String items;
  extern String Message;
  extern String Move_placeholder;
  extern String Path;
  extern String Paths;
  extern String Popup;
  extern String Reorder_placeholder;
  extern String row;
  extern String rows;
}}

extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_RawBoxes;

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

extern pmath_symbol_t richmath_FE_Import_FileNamesDropDescription;
extern pmath_symbol_t richmath_System_Menu; 

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
    _image_format(CAIRO_FORMAT_RGB24),
    _autohide_vertical_scrollbar(false),
    _destination_has_alpha_channel(false),
    _old_pixels(nullptr),
    _old_pixels_with_alpha(nullptr),
    scrolling(false),
    already_scrolled(false),
    _has_dark_background(false),
    _focused(false),
    _width(0),
    _height(0),
    gesture_zoom_factor(1.0f),
    animation_running(false),
    is_dragging(false),
    is_drop_over(false)
{
  _dpi = Win32HighDpi::get_dpi_for_window(hwnd());
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
  
  hd_trackpad_handler.init(hwnd(), this);
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

    /* RealTimeStylus disables single-finger WM_GESTURE on Windows 7 and all WM_GESTUREs on Windows 10.
       RealTimeStylus also disables WM_POINTER */
    //HRbool(stylus->put_Enabled(TRUE));
    
    HRbool(stylus->AddStylusAsyncPlugin(
             StylusUtil::get_stylus_sync_plugin_count(stylus),
             this));
    fprintf(stderr, "[%lu RTS plugins]\n", StylusUtil::get_stylus_sync_plugin_count(stylus));
  }
  
//  /* Enabling WM_TOUCH disables WM_GESTURE */
//  if(Win32Touch::RegisterTouchWindow)
//    Win32Touch::RegisterTouchWindow(_hwnd, 0); // or TWF_FINETOUCH
}

Win32Widget::~Win32Widget() {
  if(_old_pixels)
    cairo_surface_destroy(_old_pixels);
  if(_old_pixels_with_alpha)
    cairo_surface_destroy(_old_pixels_with_alpha);
}

Vector2F Win32Widget::window_size() {
  RECT rect;
  GetClientRect(_hwnd, &rect);
  _width.register_observer();
  return Vector2F(rect.right, rect.bottom) / scale_factor();
}

Point Win32Widget::scroll_pos() {
  return Point(Vector2F(GetScrollPos(_hwnd, SB_HORZ), GetScrollPos(_hwnd, SB_VERT)) / scale_factor());
}

void Win32Widget::scroll_to(Point pos) {
  SCROLLINFO si;
  
  si.cbSize = sizeof(si);
  si.fMask  = SIF_ALL;
  
  int oldx, newx, oldy, newy;
  
  GetScrollInfo(_hwnd, SB_HORZ, &si);
  oldx = si.nPos;
  si.nPos = round(pos.x * scale_factor());
  SetScrollInfo(_hwnd, SB_HORZ, &si, TRUE);
  GetScrollInfo(_hwnd, SB_HORZ, &si);
  newx = si.nPos;
  
  GetScrollInfo(_hwnd, SB_VERT, &si);
  oldy = si.nPos;
  si.nPos = round(pos.y * scale_factor());
  SetScrollInfo(_hwnd, SB_VERT, &si, TRUE);
  GetScrollInfo(_hwnd, SB_VERT, &si);
  newy = si.nPos;
  
  if(oldx != newx || oldy != newy) {
    RECT norect = {0, 0, 0, 0};
    
    ScrollWindow(_hwnd, oldx - newx, oldy - newy, &norect, &norect);
    invalidate();
  }
}

void Win32Widget::show_tooltip(Box *source, Expr boxes) {
  Win32TooltipWindow::show_global_tooltip(source, boxes, document()->stylesheet());
}

void Win32Widget::hide_tooltip() {
  Win32TooltipWindow::hide_global_tooltip();
}

Document *Win32Widget::try_create_popup_window(const SelectionReference &anchor) {
  Box *anchor_box = FrontEndObject::find_cast<Box>(anchor.id);
  if(!document()->is_parent_of(anchor_box))
    return nullptr;
  
  auto *popup = new Win32AttachedPopupWindow(document(), anchor_box);
  popup->init();
  return popup->document();
}

double Win32Widget::message_time() {
  return GetMessageTime() / 1000.0;
}

double Win32Widget::double_click_time() {
  return GetDoubleClickTime() / 1000.0;
}

Vector2F Win32Widget::double_click_dist() {
  return Vector2F(GetSystemMetrics(SM_CXDOUBLECLK), GetSystemMetrics(SM_CYDOUBLECLK)) / scale_factor();
}

void Win32Widget::do_drag_drop(const VolatileSelection &src, MouseEvent &event) {
  if(is_dragging || src.is_empty())
    return;
    
  is_dragging = true;
  drag_source_reference().set(src);
  
  scrolling = false;
  
  DataObject *data_object = new DataObject;
  
  data_object->source = drag_source_reference();
  data_object->add_source_format(Win32Clipboard::AtomBoxesText);
  data_object->add_source_format(Win32Clipboard::AtomSvgImage, TYMED_HGLOBAL);
  data_object->add_source_format(CF_DIB);
  data_object->add_source_format(CF_BITMAP, TYMED_GDI);
  data_object->add_source_format(CF_UNICODETEXT);
  data_object->add_source_format(CF_TEXT);
  
  DropSource *drop_source = new DropSource;
  drop_source->description_data.copy(data_object);
  
  DWORD effect = DROPEFFECT_COPY;
  if(src.box->get_style(Editable))
    effect |= DROPEFFECT_MOVE;
  
  if(Win32Themes::is_app_themed()) {
    drop_source->set_flags(DSH_ALLOWDROPDESCRIPTIONTEXT);
  }
  
  event.set_origin(document());
  Point pos{(event.position - scroll_pos()) * scale_factor()};
  
  /* Unlike set_drag_image_from_window() i.e. IDropSourceHelper::InitializeFromWindow, 
     set_drag_image_from_document() , i.e. IDropSourceHelper::InitializeFromBitmap,
     we lose default drag description texts with the latter.
   */
  if(FAILED(drop_source->set_drag_image_from_document(pos, data_object->source)))
    drop_source->set_drag_image_from_window(nullptr);
  
  GridBox::selection_strategy = GridBox::best_selection_strategy_for_drag_source(src);
  HRESULT res = data_object->do_drag_drop(drop_source, effect, &effect);
  GridBox::selection_strategy = GridSelectionStrategy::ContentsOnly;
  
  // TODO: is it clear that src was not deleted during do_drag_drop?
  Document *doc = src.box->find_parent<Document>(true);
  
  if(res == DRAGDROP_S_DROP) {
    if(effect & DROPEFFECT_MOVE) {
      VolatileSelection new_src = drag_source_reference().get_all();
      
      if(new_src && new_src.end <= new_src.box->length() && doc) {
        doc->select(new_src);
        if(!doc->remove_selection())
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

void Win32Widget::bring_to_front() {
  SetFocus(_hwnd);
}

void Win32Widget::invalidate() {
  InvalidateRect(_hwnd, 0, FALSE);
}

void Win32Widget::invalidate_options() {
}

void Win32Widget::invalidate_rect(const RectangleF &rect) {
  Point sp = scroll_pos();
  float sf = scale_factor();
  
  RECT irect;
  irect.left   = (int)floorf((rect.x - sp.x) * sf) - 4;
  irect.top    = (int)floorf((rect.y - sp.y) * sf) - 4;
  irect.right  = irect.left + (int)ceilf(rect.width  * sf) + 8;
  irect.bottom = irect.top  + (int)ceilf(rect.height * sf) + 8;
  
  InvalidateRect(_hwnd, &irect, FALSE);
}

void Win32Widget::force_redraw() {
  RECT rect;
  GetClientRect(_hwnd, &rect);
  if(!_width.unobserved_equals(rect.right) || _height != rect.bottom) // called before WM_SIZE
    return;
  
  HDC dc = GetDC(_hwnd);
  on_paint(dc, true);
  ReleaseDC(_hwnd, dc);
}

void Win32Widget::set_cursor(CursorType type) {
  if(mouse_moving) {
    cursor = type;
    return;
  }
  
  switch(type) {
    case CursorType::Finger:
      SetCursor(LoadCursor(0, IDC_HAND));
      return;
      
    case CursorType::Default:
      SetCursor(LoadCursor(0, IDC_ARROW));
      return;
      
    case CursorType::Current:
      break;
      
    case CursorType::SizeN:
    case CursorType::SizeS:
      SetCursor(LoadCursor(0, IDC_SIZENS));
      return;
      
    case CursorType::SizeNW:
    case CursorType::SizeSE:
      SetCursor(LoadCursor(0, IDC_SIZENWSE));
      return;
      
    case CursorType::SizeE:
    case CursorType::SizeW:
      SetCursor(LoadCursor(0, IDC_SIZEWE));
      return;
      
    case CursorType::SizeNE:
    case CursorType::SizeSW:
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
    animation_running = 0 != SetTimer(_hwnd, (UINT_PTR)&TimerIdAnimate, ANIMATION_DELAY, nullptr);
    
    if(!animation_running) {
      animations.remove(event);
      return false;
    }
  }
  
  return true;
}

STDMETHODIMP Win32Widget::DragEnter(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect) {
//  if(!is_dragging) {
//    drag_source_reference().set(
//      document()->selection_box(),
//      document()->selection_start(),
//      document()->selection_end());
//  }
  
  is_drop_over = true;
  
  return BasicWin32Widget::DragEnter(data_object, key_state, pt, effect);
}

STDMETHODIMP Win32Widget::DragLeave(void) {
  if(VolatileSelection src = drag_source_reference().get_all()) 
    document()->select(src);
  
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

void Win32Widget::paint_background(Canvas &canvas) {
  canvas.set_color(Color::White);
  canvas.paint();
}

void Win32Widget::paint_canvas(Canvas &canvas, bool resize_only) {
  cairo_set_line_width(canvas.cairo(), 1);
  cairo_set_line_cap(canvas.cairo(), CAIRO_LINE_CAP_SQUARE);
  canvas.set_font_size(10);// 10 * 4/3.
  
  if(!resize_only) {
    Color color = document()->get_style(Background);
    if(color.is_valid()) {
      canvas.set_color(color);
      canvas.paint();
    }
    else
      paint_background(canvas);
    
    bool old_has_dark_background  = _has_dark_background;
    _has_dark_background = color.is_dark();
    if(old_has_dark_background != _has_dark_background)
      on_changed_dark_mode();
  }
  
  canvas.scale(scale_factor(), scale_factor());
  canvas.set_color(document()->get_style(FontColor, Color::Black));
  
  document()->paint_resize(canvas, resize_only);
  if( _hwnd &&
      _hwnd == GetFocus() &&
      document()->selection_box() &&
      document()->selection_length() == 0 &&
      GetCaretBlinkTime() != INFINITE)
  {
    SetTimer(_hwnd, (UINT_PTR)&TimerIdBlinkCursor, GetCaretBlinkTime(), nullptr);
  }
  
  canvas.scale(1 / scale_factor(), 1 / scale_factor());
  
  Vector2F win_size = window_size();
  
  if(scrolling && mouse_down_event.middle) {
    SCROLLINFO si;
    
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_RANGE;
    
    GetScrollInfo(_hwnd, SB_VERT, &si);
    bool vert = si.nMax - si.nMin > (int)si.nPage;
    
    GetScrollInfo(_hwnd, SB_HORZ, &si);
    bool horz = si.nMax - si.nMin > (int)si.nPage;
    
    canvas.new_path();
    ControlPainter::std->paint_scroll_indicator(canvas, mouse_down_event.position, horz, vert);
  }
  
  if(is_scrollable()) {
    RECT outer;
    GetWindowRect(_hwnd, &outer);
    
    int w_page = round(scale_factor() * win_size.x);
    int h_page = round(scale_factor() * win_size.y);
    
    int w_max = round(scale_factor() * document()->extents().width);
    int h_max;
    
    if(autohide_vertical_scrollbar())
      h_max = round(document()->extents().height()                      * scale_factor());
    else
      h_max = round((document()->extents().height() + win_size.y * 0.8) * scale_factor());
      
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

void Win32Widget::on_changed_dark_mode() {
  if(!Win32Themes::SetWindowTheme)
    return;
  
  if(is_using_dark_mode())
    Win32Themes::SetWindowTheme(_hwnd, L"DarkMode_Explorer", nullptr);
  else
    Win32Themes::SetWindowTheme(_hwnd, L"Explorer", nullptr);
}

static int best_image_size(int width) {
  if(width > 0x10000)
    return width;
  int w = 1;
  while(w < width)
    w*= 2;
  return w;
}

void Win32Widget::on_paint(HDC dc, bool from_wmpaint) {
  RECT rect;
  GetClientRect(_hwnd, &rect);
  
  _dpi = Win32HighDpi::get_dpi_for_window(hwnd());
  //pmath_debug_print("[get_dpi_for_window = %d]\n", Win32HighDpi::get_dpi_for_window(hwnd()));
  
  int width = rect.right;
  int height = rect.bottom;
  
  int best_width  = best_image_size(width);
  int best_height = best_image_size(height);
  
  cairo_surface_t *target = nullptr;
  if(_old_pixels && cairo_surface_status(_old_pixels) == CAIRO_STATUS_SUCCESS) {
    cairo_surface_t *old_img = cairo_win32_surface_get_image(_old_pixels);
    if( old_img && 
        cairo_image_surface_get_format(old_img) == _image_format &&
        cairo_image_surface_get_width(old_img)  == best_width &&
        cairo_image_surface_get_height(old_img) == best_height)
    {
      target = _old_pixels;
    }
  }
  if(!target) {
    if(_old_pixels)
      cairo_surface_destroy(_old_pixels);
    
    _old_pixels = cairo_win32_surface_create_with_dib(_image_format, best_width, best_height);
    
    if(cairo_surface_status(_old_pixels) != CAIRO_STATUS_SUCCESS) {
      cairo_surface_destroy(_old_pixels);
      _old_pixels = nullptr;
      return;
    }
    
    target = _old_pixels;
  }
  
  cairo_t *cr = cairo_create(target);
  {
    Canvas canvas(cr);
    
//    if(_image_format == CAIRO_FORMAT_ARGB32){
//      cairo_font_options_t *opts = cairo_font_options_create();
//      cairo_font_options_set_antialias(opts, CAIRO_ANTIALIAS_GRAY);
//      cairo_set_font_options(cr, opts);
//      cairo_font_options_destroy(opts);
//    }

//    {
//      cairo_font_options_t *opts = cairo_font_options_create();
////      cairo_font_options_set_hint_metrics(opts, CAIRO_HINT_METRICS_OFF);
//      cairo_font_options_set_antialias(opts, CAIRO_ANTIALIAS_SUBPIXEL);
//      cairo_set_font_options(cr, opts);
//      cairo_font_options_destroy(opts);
//    }
  
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
    
    paint_canvas(canvas, !from_wmpaint);
  }
  cairo_destroy(cr);
  cairo_surface_flush(target);
  
  if(from_wmpaint) {
    if(_destination_has_alpha_channel && _image_format != CAIRO_FORMAT_ARGB32) {
      if(_old_pixels_with_alpha) {
        cairo_surface_t *old_img = cairo_win32_surface_get_image(_old_pixels_with_alpha);
        if( !old_img || 
            cairo_image_surface_get_width(old_img) != best_width ||
            cairo_image_surface_get_height(old_img) != best_height)
        {
          cairo_surface_destroy(_old_pixels_with_alpha);
          _old_pixels_with_alpha = nullptr;
        }
      }
      if(!_old_pixels_with_alpha) {
        _old_pixels_with_alpha = cairo_win32_surface_create_with_dib(CAIRO_FORMAT_ARGB32, best_width, best_height);
      }
      
      if(cairo_surface_status(_old_pixels) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(_old_pixels_with_alpha);
        _old_pixels_with_alpha = nullptr;
      }
      else {
        cairo_t *tmp_cr = cairo_create(_old_pixels_with_alpha);
        cairo_set_source_surface(tmp_cr, target, 0.0, 0.0);
        cairo_paint(tmp_cr);
        cairo_destroy(tmp_cr);
        
        BitBlt(dc, 0, 0, rect.right, rect.bottom,
               cairo_win32_surface_get_dc(_old_pixels_with_alpha), 0, 0, SRCCOPY);
      }
    }
    else {
      BitBlt(dc, 0, 0, rect.right, rect.bottom,
             cairo_win32_surface_get_dc(target), 0, 0, SRCCOPY);
    }
  }
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
  else {
    /*  During WM_GESTURE, Windows 10 will *post* instead of *send* WM_HSCROLL messages
        (with SB_THUMBTRACK and with SB_THUMBPOSITION).
        That way, GetScrollInfo above will already get the updated new position, so that 
        si.nPos == yPos. But we still need to redraw the window.
     */
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
  else {
    /*  During WM_GESTURE, Windows 10 will *post* instead of *send* WM_VSCROLL messages
        (with SB_THUMBTRACK and with SB_THUMBPOSITION).
        That way, GetScrollInfo above will already get the updated new position, so that 
        si.nPos == yPos. But we still need to redraw the window.
     */
    invalidate();
  }
}

void Win32Widget::on_mousedown(MouseEvent &event) {
  if(event.left)
    SetCapture(_hwnd);
    
  bool may_start_scrolling = !scrolling;
  if(scrolling) {
    KillTimer(_hwnd, (UINT_PTR)&TimerIdScroll);
    scrolling = false;
    invalidate();
  }
  
  document()->mouse_down(event);
  
  if(may_start_scrolling && is_scrollable()) {
    if(event.middle || event.left) {
      SCROLLINFO si;
      
      si.cbSize = sizeof(si);
      si.fMask = SIF_PAGE | SIF_RANGE;
      
      bool vert = false;
      if(GetScrollInfo(_hwnd, SB_VERT, &si))
        vert = si.nMax - si.nMin > (int)si.nPage;
      
      bool horz = false;
      if(GetScrollInfo(_hwnd, SB_HORZ, &si))
        horz = si.nMax - si.nMin > (int)si.nPage;
      
      if(vert || horz) {
        scrolling = true;
        already_scrolled = !event.middle;
        Point sp = scroll_pos();
        mouse_down_event = event;
        mouse_down_event.position = Point{(event.position - scroll_pos()) * scale_factor()};
        SetTimer(_hwnd, (UINT_PTR)&TimerIdScroll, 20, 0);
        if(event.middle)
          invalidate();
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
//    Document *cur = Documents::current();
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
    KillTimer(_hwnd, (UINT_PTR)&TimerIdScroll);
    if(mouse_down_event.middle)
      invalidate();
  }
}

void Win32Widget::on_mousemove(MouseEvent &event) {
  mouse_moving = true;
  cursor = CursorType::Default;
  
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

void Win32Widget::on_popupmenu(VolatileSelection src, POINT screen_pt) {
  UINT flags = TPM_RETURNCMD;
  
  if(GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0)
    flags |= TPM_LEFTALIGN;
  else
    flags |= TPM_RIGHTALIGN;
  
  if(!src.box)
    src = VolatileSelection(document(), 0);
  
  SharedPtr<Win32Menu> popup_menu;
  Expr context_menu = src.box->get_finished_flatlist_style(ContextMenu);
  if(context_menu[0] == richmath_System_List && context_menu.expr_length() > 0) {
    popup_menu = new Win32Menu(Call(Symbol(richmath_System_Menu), strings::Popup, std::move(context_menu)), true);
  }
  if(!popup_menu)
    return;
  
  HMENU menu = popup_menu->hmenu();
  
  MenuExitInfo exit_info;
  DWORD cmd;
  {
    Win32AutoMenuHook menu_hook(menu, _hwnd, nullptr, false, false);
    Win32Menu::use_dark_mode = is_using_dark_mode();
    cmd = TrackPopupMenuEx(
            menu,
            flags,
            screen_pt.x,
            screen_pt.y,
            _hwnd,
            nullptr);
    
    exit_info = menu_hook.exit_info;
  }
  
  if(!cmd && !exit_info.handle_after_exit()) {
    if(exit_info.reason == MenuExitReason::ExplicitCmd)
      cmd = exit_info.cmd;
  }
  
  if(cmd) {
    Application::with_evaluation_box(src.box, [&](){ callback(WM_COMMAND, cmd, 0); });
  }
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
          if(!_width.unobserved_equals(rect.right) || _height != rect.bottom) // called before WM_SIZE
            return 0;
          
          {
            PAINTSTRUCT paintStruct;
            HDC dc = BeginPaint(_hwnd, &paintStruct);
            SetLayout(dc, 0);
            on_paint(dc, true);
            EndPaint(_hwnd, &paintStruct);
          }
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
          on_popupmenu(document()->selection_now(), pt);
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
          
          event.position.x = (int16_t)( lParam & 0xFFFF)            + GetScrollPos(_hwnd, SB_HORZ);
          event.position.y = (int16_t)((lParam & 0xFFFF0000) >> 16) + GetScrollPos(_hwnd, SB_VERT);
          
          event.position.x /= scale_factor();
          event.position.y /= scale_factor();
          
          event.device = Win32Touch::get_mouse_message_source(&event.id);
          fprintf(
            stderr,
            "[WM_%sBUTTONDOWN: %s id %d at (%f,%f)]\n",
            message == WM_LBUTTONDOWN ? "L" : (message == WM_MBUTTONDOWN ? "M" : "R"),
            event.device == DeviceKind::Mouse ? "mouse" : (event.device == DeviceKind::Pen ? "pen" : "touch"),
            event.id,
            (double)event.position.x,
            (double)event.position.y);
            
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
          
          event.position.x = pt.x + GetScrollPos(_hwnd, SB_HORZ);
          event.position.y = pt.y + GetScrollPos(_hwnd, SB_VERT);
          
          event.position.x /= scale_factor();
          event.position.y /= scale_factor();
          
          on_mouseup(event);
          
          if(message == WM_RBUTTONUP) {
            bool dummy;
            if(auto src = document()->mouse_selection(event.position, &dummy)) {
              ClientToScreen(_hwnd, &pt);
              on_popupmenu(src, pt);
            }
          }
        } return 0;
        
      case WM_MOUSEMOVE: {
          MouseEvent event;
          event.device = Win32Touch::get_mouse_message_source(&event.id);
          
          event.left   = (wParam & MK_LBUTTON) != 0;
          event.middle = (wParam & MK_MBUTTON) != 0;
          event.right  = (wParam & MK_RBUTTON) != 0;
          
          event.position.x = (int16_t)(lParam & 0xFFFF)            + GetScrollPos(_hwnd, SB_HORZ);
          event.position.y = (int16_t)((lParam & 0xFFFF0000) >> 16) + GetScrollPos(_hwnd, SB_VERT);
          
          event.position.x /= scale_factor();
          event.position.y /= scale_factor();
          
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
        
      case WM_GESTURE: pmath_debug_print("[WM_GESTURE]");
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
      
      case WM_POINTERWHEEL: {
          pmath_debug_print("[WM_POINTERWHEEL]");
        } break;
        
      case DM_POINTERHITTEST: hd_trackpad_handler.on_pointer_hit_test(wParam); break;
      
      case WM_POINTERDOWN: {
          if(Win32Touch::GetCurrentInputMessageSource) {
            Win32Touch::INPUT_MESSAGE_SOURCE ims = {};
            if(Win32Touch::GetCurrentInputMessageSource(&ims)) {
              pmath_debug_print("[WM_POINTERDOWN device %x from %x]\n", ims.deviceType, ims.originId);
              break;
            }
          }
          
          pmath_debug_print("[WM_POINTERDOWN]");
        } break;
      case WM_POINTERUP: {
          pmath_debug_print("[WM_POINTERUP]");
        } break;
      
      case WM_TIMER: {
          if(wParam == (UINT_PTR)&TimerIdScroll) {
            if(scrolling) {
              POINT mouse;
              GetCursorPos(&mouse);
              ScreenToClient(_hwnd, &mouse);
              
              Vector2F delta;
              if(mouse_down_event.middle) {
                if(abs(mouse.x - mouse_down_event.position.x) > 10)
                  delta.x = abs(mouse.x - mouse_down_event.position.x) / 4;
                  
                if(abs(mouse.y - mouse_down_event.position.y) > 10)
                  delta.y = abs(mouse.y - mouse_down_event.position.y) / 4;
                  
                delta.x /= scale_factor();
                delta.y /= scale_factor();
                
                if(mouse.x < mouse_down_event.position.x)
                  delta.x = -delta.x;
                  
                if(mouse.y < mouse_down_event.position.y)
                  delta.y = -delta.y;
              }
              else if(mouse_down_event.left) {
                RECT rect;
                GetClientRect(_hwnd, &rect);
                
                if(mouse.x < 0)
                  delta.x = mouse.x / 4;
                else if(mouse.x > rect.right)
                  delta.x = (mouse.x - rect.right) / 4;
                  
                if(mouse.y < 0)
                  delta.y = mouse.y / 4;
                else if(mouse.y > rect.bottom)
                  delta.y = (mouse.y - rect.bottom) / 4;
              }
              
              if(delta != Vector2F(0, 0)) {
                scroll_by(delta);
                
                MouseEvent event;
                
                event.left   = (GetKeyState(VK_LBUTTON) & ~1);
                event.middle = (GetKeyState(VK_MBUTTON) & ~1);
                event.right  = (GetKeyState(VK_RBUTTON) & ~1);
                
                event.position.x = mouse.x + GetScrollPos(_hwnd, SB_HORZ);
                event.position.y = mouse.y + GetScrollPos(_hwnd, SB_VERT);
                
                event.position.x /= scale_factor();
                event.position.y /= scale_factor();
                
                on_mousemove(event);
              }
            }
            else
              KillTimer(_hwnd, (UINT_PTR)&TimerIdScroll);
          }
          else if(wParam == (UINT_PTR)&TimerIdAnimate) {
            KillTimer(_hwnd, (UINT_PTR)&TimerIdAnimate);
            animation_running = 0;
            
            for(auto e : animations.deletable_entries()) {
              if(e.key->min_wait_seconds <= e.key->timer()) {
                auto anim = e.key;
                e.delete_self();
                anim->execute_event();
              }
              else if(!animation_running) {
                animation_running = 0 != SetTimer(_hwnd, (UINT_PTR)&TimerIdAnimate, ANIMATION_DELAY, nullptr);
                
                if(!animation_running) {
                  auto anim = e.key;
                  e.delete_self();
                  anim->execute_event();
                }
              }
            }
          }
          else if(wParam == (UINT_PTR)&TimerIdBlinkCursor) {
            KillTimer(_hwnd, (UINT_PTR)&TimerIdBlinkCursor);
            
            Context *ctx = document_context();
            if(_hwnd != GetFocus())
              ctx->old_selection = ctx->selection;
            else if(ctx->old_selection == ctx->selection || is_mouse_down())
              ctx->old_selection.id = FrontEndReference::None;
            else
              ctx->old_selection = ctx->selection;
              
            if(Box *box = ctx->selection.get())
              box->request_repaint_range(ctx->selection.start, ctx->selection.end);
          }
          else if(wParam == (UINT_PTR)&hd_trackpad_handler.TimerId) {
            hd_trackpad_handler.on_timer();
          }
        } return 0;
        
      case WM_KEYDOWN: if(!is_drop_over) {
          // TODO: NumpadEnter is VK_RETURN with "extended bit" 24 set in lParam. 
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
        
      case WM_CHAR:
      case WM_UNICHAR: if(!is_drop_over) {
          if(wParam == UNICODE_NOCHAR)
            return 1;
          
          static uint32_t current_high = 0;
          
          if((wParam == ' ' ||
              wParam == '\r' ||
              wParam == '\n') &&
              ((GetKeyState(VK_CONTROL) & ~1) ||
               (GetKeyState(VK_MENU) & ~1) ||
               (GetKeyState(VK_SHIFT) & ~1)))
          {
            current_high = 0;
            return 0;
          }
          
          if(wParam == '\t') {
            current_high = 0;
            return 0;
          }

          if(current_high && wParam <= 0xFFFFU && is_utf16_low(wParam)) {
            uint32_t unichar = 0x10000 | ((current_high & 0x03FF) << 10) | (wParam & 0x03FF);
            current_high = 0;
            document()->key_press(unichar);
            return 0;
          }

          if(current_high) {
            document()->key_press(current_high);
            current_high = 0;
          }
          
          if(wParam <= 0xFFFFU && is_utf16_high(wParam)) {
            current_high = wParam;
            return 0;
          }

          document()->key_press(wParam);
        } return 0;
        
      case WM_ACTIVATE: {
          if(LOWORD(wParam) == WA_INACTIVE) {
            document()->reset_mouse();
          }
        } break;
        
      case WM_SETFOCUS: {
          _focused = true;
          document()->focus_set();
          
          if(document()->selectable())
            do_set_current_document();
          
          Box *sel_box = document()->selection_box();
          if(sel_box && document()->selection_length() == 0) {
            auto ctx = document_context();
            ctx->old_selection.id = FrontEndReference::None;
            sel_box->request_repaint_range(ctx->selection.start, ctx->selection.end);
            
            if(GetCaretBlinkTime() != INFINITE)
              SetTimer(_hwnd, (UINT_PTR)&TimerIdBlinkCursor, GetCaretBlinkTime(), nullptr);
          }
        } return 0;
      
      case WM_KILLFOCUS: {
          _focused = false;
          document()->focus_killed();
        } break;
      
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
            
          Menus::run_command_now(cmd);
        } return 0;
      
      case WM_MENUDRAG:      return Win32Menu::on_menudrag(     wParam, lParam);
      case WM_MENUGETOBJECT: return Win32Menu::on_menugetobject(wParam, lParam);
      
      case WM_MENUSELECT: {
          Win32Menu::on_menuselect(wParam, lParam);
        } break;
    }
  }
  
  return BasicWin32Widget::callback(message, wParam, lParam);
}

DWORD Win32Widget::preferred_drop_effect(IDataObject *data_object) {
  FORMATETC fmt;
  memset(&fmt, 0, sizeof(fmt));
  
  fmt.dwAspect = DVASPECT_CONTENT;
  fmt.lindex   = -1;
  fmt.ptd      = nullptr;
  fmt.tymed    = TYMED_HGLOBAL;
  
  // dynamic_cast<DataObject*>(data_object) will throw a std::__non_rtti_object exception
  // when data_object coemes from elsewhere.
  DataObject *local_data_object = DataObject::as_current_data_object(data_object);
  
  DWORD ok_drop_effect = DROPEFFECT_COPY;
  if(is_dragging) { // if(local_data_object && doc->is_parent_of(local_data_object->source.get()))
    ok_drop_effect = DROPEFFECT_MOVE;
  }
  
  fmt.cfFormat = _preferred_drop_format = Win32Clipboard::mime_to_win32cbformat[Clipboard::BoxesText];
  if(data_object->QueryGetData(&fmt) == S_OK)
    return ok_drop_effect;
    
  fmt.cfFormat = _preferred_drop_format = Win32Clipboard::mime_to_win32cbformat[Clipboard::PlainText];
  if(data_object->QueryGetData(&fmt) == S_OK)
    return ok_drop_effect;
    
  fmt.cfFormat = _preferred_drop_format = CF_TEXT;
  if(data_object->QueryGetData(&fmt) == S_OK)
    return ok_drop_effect;
  
  fmt.cfFormat = _preferred_drop_format = CF_HDROP;
  if(data_object->QueryGetData(&fmt) == S_OK) 
    return ok_drop_effect;//DROPEFFECT_LINK;
    
  _preferred_drop_format = 0;
  return DROPEFFECT_NONE;
}

DWORD Win32Widget::drop_effect(DWORD key_state, POINTL ptl, DWORD allowed_effects) {
  POINT pt = {(int) ptl.x, (int)ptl.y };
  ScreenToClient(_hwnd, &pt);
  
  Point pos{
    (pt.x + GetScrollPos(_hwnd, SB_HORZ)) / scale_factor(),
    (pt.y + GetScrollPos(_hwnd, SB_VERT)) / scale_factor()};
  
  bool was_inside_start;
  if(!may_drop_into(document()->mouse_selection(pos, &was_inside_start), is_dragging))
    return DROPEFFECT_NONE;
    
  return BasicWin32Widget::drop_effect(key_state, ptl, allowed_effects);
}

void Win32Widget::apply_drop_description(DWORD effect, DWORD key_state, POINTL pt) {
  DROPIMAGETYPE drop_image = DROPIMAGE_INVALID;
  
  switch(effect & ~DROPEFFECT_SCROLL) {
    case DROPEFFECT_NONE: drop_image = DROPIMAGE_NOIMAGE; break; // DROPIMAGE_NONE
    case DROPEFFECT_COPY: drop_image = DROPIMAGE_COPY; break;
    case DROPEFFECT_MOVE: drop_image = DROPIMAGE_MOVE; break;
    case DROPEFFECT_LINK: drop_image = DROPIMAGE_LINK; break;
  }
  
  String desc_message;
  String desc_param;
  
//  desc_message = "at %1";
//  desc_param = List(pt.x, pt.y).to_string();
//  
//  set_drop_description(drop_image, desc_message, desc_param);
//  return;

  if(VolatileSelection drag_src = drag_source_reference().get_all()) {
    if(GridBox *src_grid = dynamic_cast<GridBox*>(drag_src.box)) {
      VolatileSelection drop_dst = document()->selection_now();
      if(GridBox *dst_grid = dynamic_cast<GridBox*>(drop_dst.box)) {
        GridIndexRect src_rect = src_grid->get_enclosing_range(drag_src.start, drag_src.end);
        GridIndexRect dst_rect = dst_grid->get_enclosing_range(drop_dst.start, drop_dst.end);
        
        bool may_be_singular = true;
        if(effect == DROPEFFECT_COPY) {
          desc_message = strings::Insert_placeholder;
        }
        else if(effect == DROPEFFECT_MOVE) {
          if(drag_src.box == dst_grid && dst_rect.cols() == 0 || dst_rect.rows() == 0) {
            if(src_rect.cols() == dst_grid->cols() || src_rect.rows() == dst_grid->rows()) {
              desc_message = strings::Reorder_placeholder;
              may_be_singular = false;
            }
            else
              desc_message = strings::Move_placeholder;
          }
          else
            desc_message = strings::Move_placeholder;
        }
        
        if(src_rect.rows() == src_grid->rows()) {
          if(src_rect.cols() == 1 && may_be_singular)
            desc_param = strings::column;
          else
            desc_param = strings::columns;
        }
        else if(src_rect.cols() == src_grid->cols()) {
          if(src_rect.rows() == 1 && may_be_singular)
            desc_param = strings::row;
          else
            desc_param = strings::rows;
        }
        else
          desc_param = strings::items;
        
        if(desc_message) {
          set_drop_description(drop_image, desc_message, desc_param);
          return;
        }
      }
    }
  }
  
  if(_preferred_drop_format == CF_HDROP) {
    Expr paths = DataObject::get_global_data_dropfiles(_dragging.get());
    
    if(effect == DROPEFFECT_LINK) {
      desc_message = strings::Insert_placeholder;
      desc_param   = paths.expr_length() == 1 ? strings::Path : strings::Paths;
    }
    else {
      Expr desc = Application::interrupt_wait(
        Call(Symbol(richmath_FE_Import_FileNamesDropDescription), paths),
        Application::edit_interrupt_timeout);
      
      if(desc.is_list_of_rules()) {
        if(Expr image = desc.lookup(strings::Image, {})) {
          if(image == richmath_System_Automatic) {
            //drop_image = DROPIMAGE_INVALID;
          }
          else if(image == strings::Copy)      drop_image = DROPIMAGE_COPY;
          else if(String image_s = image) {
            if(     image_s.equals("Move"))    drop_image = DROPIMAGE_MOVE;
            else if(image_s.equals("Link"))    drop_image = DROPIMAGE_LINK;
            else if(image_s.equals("Label"))   drop_image = DROPIMAGE_LABEL;
            else if(image_s.equals("Warning")) drop_image = DROPIMAGE_WARNING;
            else if(image_s.equals("No"))      drop_image = DROPIMAGE_NONE;
            else if(image_s.equals("NoImage")) drop_image = DROPIMAGE_NOIMAGE;
          }
        }
        
        Expr msg;
        if(desc.try_lookup(strings::Message, msg))
          desc_message = msg.to_string();
        
        if(desc.try_lookup(strings::Insert, msg))
          desc_param = msg.to_string();
      }
    }
    
    set_drop_description(drop_image, desc_message, desc_param);
    return;
  }
  
  //set_drop_description(DROPIMAGE_INVALID, strings::EmptyString, strings::EmptyString);
}

void Win32Widget::do_drop_data(IDataObject *data_object, DWORD effect) {
  AutoMemorySuspension ams;
  SetFocus(_hwnd);
  
  String mimetype;
  String text_data;
  Expr box_data;
  Expr files_data;
  
  STGMEDIUM stgmed;
  memset(&stgmed, 0, sizeof(stgmed));
  
  FORMATETC fmt;
  memset(&fmt, 0, sizeof(fmt));
  
  fmt.dwAspect = DVASPECT_CONTENT;
  fmt.lindex   = -1;
  fmt.ptd      = nullptr;
  fmt.tymed    = TYMED_HGLOBAL;
  
  do {
    if(DataObject *local_data_object = DataObject::as_current_data_object(data_object)) {
      if(VolatileSelection src = local_data_object->source.get_all()) {
        box_data = src.to_pmath(BoxOutputFlags::Default);
        break;
      }
      
      if(local_data_object->source_content[0] == richmath_System_RawBoxes && local_data_object->source_content.expr_length() == 1) {
        box_data = local_data_object->source_content[1];
        break;
      }
    }
    
    fmt.cfFormat = Win32Clipboard::AtomBoxesText;
    if(data_object->QueryGetData(&fmt) == S_OK && data_object->GetData(&fmt, &stgmed) == S_OK) {
      mimetype = Clipboard::BoxesText;
      size_t size = GlobalSize(stgmed.hGlobal) / 2;
      if(size < INT_MAX) {
        const uint16_t *data = (const uint16_t *)GlobalLock(stgmed.hGlobal);
        
        int len = 0;
        const uint16_t *s = data;
        while(len < (int)size && *s) {
          ++len;
          ++s;
        }
        text_data = String::FromUcs2(data, len);
        
        GlobalUnlock(stgmed.hGlobal);
        ReleaseStgMedium(&stgmed);
        break;
      }
    }
    
    mimetype = Clipboard::PlainText;
    fmt.cfFormat = Win32Clipboard::mime_to_win32cbformat[Clipboard::PlainText];
    if(data_object->QueryGetData(&fmt) == S_OK && data_object->GetData(&fmt, &stgmed) == S_OK) {
      size_t size = GlobalSize(stgmed.hGlobal) / 2;
      if(size < INT_MAX) {
        const uint16_t *data = (const uint16_t *)GlobalLock(stgmed.hGlobal);
        
        int len = 0;
        const uint16_t *s = data;
        while(len < (int)size && *s) {
          ++len;
          ++s;
        }
        text_data = String::FromUcs2(data, len);
        
        GlobalUnlock(stgmed.hGlobal);
        ReleaseStgMedium(&stgmed);
        break;
      }
    }
    
    fmt.cfFormat = CF_TEXT;
    if(data_object->QueryGetData(&fmt) == S_OK && data_object->GetData(&fmt, &stgmed) == S_OK) {
      size_t size = GlobalSize(stgmed.hGlobal);
      if(size < INT_MAX) {
        const char *data = (const char *)GlobalLock(stgmed.hGlobal);
        int len = 0;
        const char *s = data;
        while(len < (int)size && *s) {
          ++len;
          ++s;
        }
        
        text_data = String(pmath_string_from_native(data, len));
        
        GlobalUnlock(stgmed.hGlobal);
        ReleaseStgMedium(&stgmed);
        break;
      }
    }
  
    fmt.cfFormat = CF_HDROP;
    if(data_object->QueryGetData(&fmt) == S_OK) {
      files_data = DataObject::get_global_data_dropfiles(data_object);
      break;
    }
  } while(false);
  
  if(!box_data.is_null() || !text_data.is_null() || !files_data.is_null()) {
    VolatileSelection sel = document()->selection_now();
    
    if(effect & DROPEFFECT_MOVE && is_dragging) {
      if(SelectionReference drag_src = drag_source_reference()) {
        drag_source_reference().reset();
        
        document()->remove_selection(drag_src);
      }
    }
    
    if(!box_data.is_null())
      document()->paste_from_boxes(box_data);
    else if(!files_data.is_null())
      document()->paste_from_filenames(files_data, effect != DROPEFFECT_LINK);
    else
      document()->paste_from_text(mimetype, text_data);
    
    if(sel.box == document()->selection_box()) 
      document()->select(sel.box, sel.start, document()->selection_end());
  }
  
  DragLeave();
}

void Win32Widget::position_drop_cursor(POINTL ptl) {
  POINT pt = {(int) ptl.x, (int)ptl.y };
  ScreenToClient(_hwnd, &pt);
  Point pos{
    (pt.x + GetScrollPos(_hwnd, SB_HORZ)) / scale_factor(),
    (pt.y + GetScrollPos(_hwnd, SB_VERT)) / scale_factor()};
  
  bool was_inside_start;
  document()->select(document()->mouse_selection(pos, &was_inside_start));
}

//} ... class Win32Widget
