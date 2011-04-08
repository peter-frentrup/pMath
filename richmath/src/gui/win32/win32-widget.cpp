#define _WIN32_WINNT 0x600 
// 0x501 for VK_OEM_XXX
// 0x600 for SPI_GETWHEELSCROLLCHARS

#include <gui/win32/win32-widget.h>

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

#include <resources.h>

#ifndef WM_MOUSEHWHEEL
  #define WM_MOUSEHWHEEL  0x020E
#endif
#ifndef SPI_GETWHEELSCROLLCHARS
  #define SPI_GETWHEELSCROLLCHARS   0x006C
#endif

#define TID_SCROLL         1
#define TID_ANIMATE        2
#define TID_BLINKCURSOR    3

#define ANIMATION_DELAY  (50)

using namespace richmath;

static Hashtable<int, String, cast_hash> menucommands;

static void add_remove_window(int count){
  static int window_count = 0;
  
  if(window_count == 0){
    menucommands.set( SC_CLOSE                    , "Close");
    
    menucommands.set( IDM_EDITBOXES               , "EditBoxes");
    menucommands.set( IDM_EXPANDSELECTION         , "ExpandSelection");
    menucommands.set( IDM_FINDMATCHINGFENCE       , "FindMatchingFence");
    menucommands.set( IDM_SELECTALL               , "SelectAll");
    menucommands.set( IDM_CUT                     , "Cut");
    menucommands.set( IDM_COPY                    , "Copy");
    menucommands.set( IDM_PASTE                   , "Paste");
    menucommands.set( IDM_OPENCLOSEGROUP          , "OpenCloseGroup");

    menucommands.set( IDM_INSERTCOLUMN            , "InsertColumn");
    menucommands.set( IDM_INSERTFRACTION          , "InsertFraction");
    menucommands.set( IDM_INSERTOPPOSITE          , "InsertOpposite");
    menucommands.set( IDM_INSERTOVERSCRIPT        , "InsertOverscript");
    menucommands.set( IDM_INSERTRADICAL           , "InsertRadical");
    menucommands.set( IDM_INSERTROW               , "InsertRow");
    menucommands.set( IDM_INSERTSUBSCRIPT         , "InsertSubscript");
    menucommands.set( IDM_INSERTSUPERSCRIPT       , "InsertSuperscript");
    menucommands.set( IDM_INSERTUNDERSCRIPT       , "InsertUnderscript");
    menucommands.set( IDM_DUPLICATEPREVIOUSINPUT  , "DuplicatePreviousInput");
    menucommands.set( IDM_DUPLICATEPREVIOUSOUTPUT , "DuplicatePreviousOutput");
    menucommands.set( IDM_SIMILARSECTIONBELOW     , "SimilarSectionBelow");

    menucommands.set( IDM_EVALUATORABORT              , "EvaluatorAbort");
    menucommands.set( IDM_EVALUATEINPLACE             , "EvaluateInPlace");
    menucommands.set( IDM_EVALUATESECTIONS            , "EvaluateSections");
    menucommands.set( IDM_EVALUATORSUBSESSION         , "EvaluatorSubsession");
    menucommands.set( IDM_SUBSESSIONEVALUATESECTIONS  , "SubsessionEvaluateSections");
  }
  
  window_count+= count;
  
  if(window_count <= 0){
    menucommands.clear();
//    PostQuitMessage(0);
  }
}
  
SpecialKey richmath::win32_virtual_to_special_key(DWORD vkey){
  switch(vkey){
    case VK_LEFT:     return KeyLeft;
    case VK_RIGHT:    return KeyRight;
    case VK_UP:       return KeyUp;
    case VK_DOWN:     return KeyDown;
    case VK_HOME:     return KeyHome;
    case VK_END:      return KeyEnd;
    case VK_PRIOR:    return KeyPageUp;
    case VK_NEXT:     return KeyPageDown;
    case VK_BACK:     return KeyBackspace;
    case VK_DELETE:   return KeyDelete;
    case VK_RETURN:   return KeyReturn;
    case VK_ESCAPE:   return KeyEscape;
    case VK_TAB:      return KeyTab;
    case VK_F1:       return KeyF1;
    case VK_F2:       return KeyF2;
    case VK_F3:       return KeyF3;
    case VK_F4:       return KeyF4;
    case VK_F5:       return KeyF5;
    case VK_F6:       return KeyF6;
    case VK_F7:       return KeyF7;
    case VK_F8:       return KeyF8;
    case VK_F9:       return KeyF9;
    case VK_F10:      return KeyF10;
    case VK_F11:      return KeyF11;
    case VK_F12:      return KeyF12;
    
    default: return KeyUnknown;
  }
}

String richmath::win32_command_id_to_command_string(DWORD id){
  return menucommands[id];
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
  BasicWin32Widget(style_ex,style,x,y,width,height,parent),
  _autohide_vertical_scrollbar(false),
  _image_format(CAIRO_FORMAT_RGB24),
  is_painting(false),
  scrolling(false),
  _width(0),
  _height(0),
  animation_running(false),
  is_dragging(false),
  is_drop_over(false)
{
  add_remove_window(1);
}

void Win32Widget::after_construction(){
  BasicWin32Widget::after_construction();
}

Win32Widget::~Win32Widget(){
//  if(surface)
//    cairo_surface_destroy(surface);
  
  add_remove_window(-1);
}

void Win32Widget::window_size(float *w, float *h){
  RECT rect;
  GetClientRect(_hwnd, &rect);
  *w = rect.right / scale_factor();
  *h = rect.bottom / scale_factor();
}

void Win32Widget::scroll_pos(float *x, float *y){
  *x = GetScrollPos(_hwnd, SB_HORZ) / scale_factor();
  *y = GetScrollPos(_hwnd, SB_VERT) / scale_factor();
}

void Win32Widget::scroll_to(float x, float y){
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
  
  if(oldx != newx || oldy != newy){
    RECT norect = {0,0,0,0};
    ScrollWindow(_hwnd, oldx - newx, oldy - newy, &norect, &norect);
    invalidate();
  }
}

double Win32Widget::message_time(){
  return GetMessageTime() / 1000.0;
}

double Win32Widget::double_click_time(){
  return GetDoubleClickTime() / 1000.0;
}

void Win32Widget::double_click_dist(float *dx, float *dy){
  *dx = GetSystemMetrics(SM_CXDOUBLECLK) / scale_factor();
  *dy = GetSystemMetrics(SM_CYDOUBLECLK) / scale_factor();
}

void Win32Widget::do_drag_drop(Box *src, int start, int end){
  if(is_dragging || !src || start >= end)
    return;
  
  is_dragging = true;
  drag_source_reference().set(src, start, end);
  
  scrolling = false;
  
  DataObject *data_object = new DataObject;
  
  FORMATETC fmt;
  memset(&fmt, 0, sizeof(fmt));
  fmt.dwAspect = DVASPECT_CONTENT;
  fmt.lindex   = -1;
  fmt.tymed    = TYMED_HGLOBAL;
  
  data_object->source = drag_source_reference();
  
  data_object->mimetypes.add(                          Clipboard::PlainText);
  fmt.cfFormat = Win32Clipboard::mime_to_win32cbformat[Clipboard::PlainText];
  data_object->formats.add(fmt);
  
  data_object->mimetypes.add(                          Clipboard::BoxesText);
  fmt.cfFormat = Win32Clipboard::mime_to_win32cbformat[Clipboard::BoxesText];
  data_object->formats.add(fmt);
  
  DropSource *drop_source = new DropSource;
  
  DWORD effect = DROPEFFECT_COPY;
  if(src->get_style(Editable))
    effect|= DROPEFFECT_MOVE;
  
  HRESULT res = DoDragDrop(data_object, drop_source, effect, &effect);
  
  if(res == DRAGDROP_S_DROP){
    if(effect & DROPEFFECT_MOVE){
      src   = drag_source_reference().get();
      start = drag_source_reference().start;
      end   = drag_source_reference().end;
      
      if(src && end <= src->length()){
        Document *doc = src->find_parent<Document>(true);
        
        if(doc){
          doc->select(src, start, end);
          if(!doc->remove_selection(false))
            beep();
        }
      }
    }
  }
  
  data_object->Release();
  drop_source->Release();
  
  drag_source_reference().reset();
  is_dragging = false;
}

void Win32Widget::invalidate(){
  is_painting = false; // if inside WM_PAINT; invalidate at end of event
  InvalidateRect(_hwnd, 0, FALSE);
}

void Win32Widget::force_redraw(){
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
  
  if(!is_painting){
    invalidate();
  }
  is_painting = false;
}

void Win32Widget::set_cursor(CursorType type){
  if(mouse_moving){
    cursor = type;
    return;
  }
  
  switch(type){
    case FingerCursor: 
      SetCursor(LoadCursor(0, IDC_HAND));
      return;
    
    case DefaultCursor: 
      SetCursor(LoadCursor(0, IDC_ARROW));
      return;
    
    case CurrentCursor:
      break;
    
    default:
      SetCursor(LoadCursor(GetModuleHandle(0), MAKEINTRESOURCE((int)type)));
  }
}

void Win32Widget::running_state_changed(){
}

bool Win32Widget::is_mouse_down(){
  return GetKeyState(VK_LBUTTON) < 0
      || GetKeyState(VK_RBUTTON) < 0
      || GetKeyState(VK_MBUTTON) < 0
      || GetKeyState(VK_XBUTTON1) < 0
      || GetKeyState(VK_XBUTTON2) < 0;
}

void Win32Widget::beep(){
  MessageBeep(0);
}

bool Win32Widget::register_timed_event(SharedPtr<TimedEvent> event){
  if(!_hwnd)
    return false;
    
  animations.set(event, Void());
  if(!animation_running){
    animation_running = 0 != SetTimer(_hwnd, TID_ANIMATE, ANIMATION_DELAY, NULL);
    
    if(!animation_running){
      animations.remove(event);
      return false;
    }
  }
  
  return true;
}

STDMETHODIMP Win32Widget::DragEnter(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect){
  if(!is_dragging){
    drag_source_reference().set(
      document()->selection_box(), 
      document()->selection_start(), 
      document()->selection_end());
  }
  
  is_drop_over = true;
  
  return BasicWin32Widget::DragEnter(data_object, key_state, pt, effect);
}

STDMETHODIMP Win32Widget::DragLeave(void){
  if(drag_source_reference().get()){
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
      
void Win32Widget::paint_background(Canvas *canvas){
  canvas->set_color(0xffffff);
  canvas->paint();
}

void Win32Widget::paint_canvas(Canvas *canvas, bool resize_only){
  cairo_set_line_width(canvas->cairo(), 1);
  cairo_set_line_cap(canvas->cairo(), CAIRO_LINE_CAP_SQUARE);
  canvas->set_font_size(10);// 10 * 4/3.
  
  if(!resize_only){
    int color = document()->get_style(Background, -1);
    if(color >= 0){
      canvas->set_color(color);
      canvas->paint();
    }
    else
      paint_background(canvas);
  }
  
  canvas->scale(scale_factor(), scale_factor());
  canvas->set_color(document()->get_style(FontColor, 0));
  
  document()->paint_resize(canvas, resize_only);
  if(_hwnd 
  && _hwnd == GetFocus()
  && document()->selection_box()
  && document()->selection_length() == 0
  && GetCaretBlinkTime() != INFINITE){
    SetTimer(_hwnd, TID_BLINKCURSOR, GetCaretBlinkTime(), NULL);
  }
  
  canvas->scale(1/scale_factor(), 1/scale_factor());
  
  float w, h;
  window_size(&w, &h);
  
  if(scrolling && mouse_down_event.middle){
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
  
  if(is_scrollable()){
    RECT outer;
    GetWindowRect(_hwnd, &outer);
    
    int w_page = floorf(scale_factor() * w + 0.5f);
    int h_page = floorf(scale_factor() * h + 0.5f);
    
    int w_max = floorf(scale_factor() * document()->extents().width + 0.5f);
    int h_max;
    
    if(autohide_vertical_scrollbar())
      h_max = floorf( document()->extents().height()            * scale_factor() + 0.5f);
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

void Win32Widget::on_paint(HDC dc, bool from_wmpaint){
  RECT rect;
  GetClientRect(_hwnd, &rect);
  
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
   
    if(!from_wmpaint){
      canvas.clip();
    }
    else{
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
  
  if(from_wmpaint){
    BitBlt(dc, 0, 0, rect.right, rect.bottom, 
      cairo_win32_surface_get_dc(target), 0, 0, SRCCOPY); 
  }
  
  cairo_surface_destroy(target);
}

void Win32Widget::on_hscroll(WORD kind){
  SCROLLINFO si;
  
  si.cbSize = sizeof(si);
  si.fMask  = SIF_ALL;
  GetScrollInfo(_hwnd, SB_HORZ, &si);
  
  int xPos = si.nPos;
  
  switch(kind){
    case SB_LEFT:
      si.nPos = si.nMin;
      break;
      
    case SB_RIGHT:
      si.nPos = si.nMax;
      break;
    
    case SB_LINELEFT:
      si.nPos-= 20;
      break;
    
    case SB_LINERIGHT:
      si.nPos+= 20;
      break;
    
    case SB_PAGELEFT:
      si.nPos-= si.nPage;
      break;
    
    case SB_PAGERIGHT:
      si.nPos+= si.nPage;
      break;
    
    case SB_THUMBTRACK:
      si.nPos = si.nTrackPos;
      break;
  }
  
  si.fMask = SIF_POS;
  SetScrollInfo(_hwnd, SB_HORZ, &si, TRUE);
  GetScrollInfo(_hwnd, SB_HORZ, &si);
  
  if(si.nPos != xPos){
    RECT norect = {0,0,0,0};
    ScrollWindow(_hwnd, xPos - si.nPos, 0, &norect, &norect);
    invalidate();
  }
}

void Win32Widget::on_vscroll(WORD kind){
  SCROLLINFO si;
  
  si.cbSize = sizeof(si);
  si.fMask  = SIF_ALL;
  GetScrollInfo(_hwnd, SB_VERT, &si);
  
  int yPos = si.nPos;
  
  switch(kind){
    case SB_TOP:
      si.nPos = si.nMin;
      break;
      
    case SB_BOTTOM:
      si.nPos = si.nMax;
      break;
    
    case SB_LINEUP:
      si.nPos-= 20;
      break;
    
    case SB_LINEDOWN:
      si.nPos+= 20;
      break;
    
    case SB_PAGEUP:
      si.nPos-= si.nPage;
      break;
    
    case SB_PAGEDOWN:
      si.nPos+= si.nPage;
      break;
    
    case SB_THUMBTRACK:
      si.nPos = si.nTrackPos;
      break;
  }
  
  si.fMask = SIF_POS;
  SetScrollInfo(_hwnd, SB_VERT, &si, TRUE);
  GetScrollInfo(_hwnd, SB_VERT, &si);
  
  if(si.nPos != yPos){
    RECT norect = {0,0,0,0};
    ScrollWindow(_hwnd, 0, yPos - si.nPos, &norect, &norect);
    invalidate();
  }
}

void Win32Widget::on_mousedown(MouseEvent &event){
  if(event.left)
    SetCapture(_hwnd);
  
  bool may_start_scrolling = !scrolling;
  if(scrolling){
    KillTimer(_hwnd, TID_SCROLL);
    scrolling = false;
    invalidate();
  }
  
  document()->mouse_down(event);
  
  if(may_start_scrolling && is_scrollable()){
    if(event.middle || event.left){
      SCROLLINFO si;
    
      si.cbSize = sizeof(si);
      si.fMask = SIF_PAGE | SIF_RANGE;
      
      GetScrollInfo(_hwnd, SB_VERT, &si);
      bool vert = si.nMax - si.nMin > (int)si.nPage;
      
      GetScrollInfo(_hwnd, SB_HORZ, &si);
      bool horz = si.nMax - si.nMin > (int)si.nPage;
      
      if(vert || horz){
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
  
  if(document()->selection_box()){
    SetFocus(_hwnd);
  }
  else{
    Document *cur = get_current_document();
    if(cur && cur != document()){
      Win32Widget *wig = dynamic_cast<Win32Widget*>(cur->native());
      
      if(wig && wig->hwnd() != GetFocus()){
        SetFocus(wig->hwnd());
      }
    }
  }
}

void Win32Widget::on_mouseup(MouseEvent &event){
  ReleaseCapture();
  
  document()->mouse_up(event);
  
  if(scrolling && already_scrolled){
    scrolling = false;
    KillTimer(_hwnd, TID_SCROLL);
    invalidate();
  }
}

void Win32Widget::on_mousemove(MouseEvent &event){
  mouse_moving = true;
  cursor = DefaultCursor;
  
  document()->mouse_move(event);
  
  if(scrolling && mouse_down_event.middle)
    already_scrolled = false;
  
  mouse_moving = false;
  set_cursor(cursor);
}

void Win32Widget::on_keydown(DWORD virtkey, bool ctrl, bool alt, bool shift){
  SpecialKeyEvent event;
  event.key = win32_virtual_to_special_key(virtkey);
  
  if(event.key){
    event.ctrl  = ctrl;
    event.alt   = alt;
    event.shift = shift;
    document()->key_down(event);
  }
  
  switch(virtkey){
    case VK_CAPITAL: {
      if(GetKeyState(VK_CAPITAL) & 0x1){
        keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY, 0);
        keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
      }
      else 
        document()->key_press(PMATH_CHAR_ALIASDELIMITER);
    } break;
  }
}

LRESULT Win32Widget::callback(UINT message, WPARAM wParam, LPARAM lParam){
  if(!initializing()){
    switch(message){
      case WM_SIZE: {
        RECT rect;
        GetClientRect(_hwnd, &rect);
        _width  = rect.right;
        _height = rect.bottom;
        
        if(_width * scale_factor() != document()->extents().width)
          document()->invalidate_all();
        else
          document()->invalidate();
      } return 0;
        
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
          on_paint(dc, true);
          EndPaint(_hwnd, &paintStruct);
        }
        
        if(!is_painting){
          invalidate();
        }
        is_painting = false;
      } return 0;
      
      case WM_HSCROLL: {
        on_hscroll(LOWORD(wParam));
      } return 0;
      
      case WM_VSCROLL: {
        on_vscroll(LOWORD(wParam));
      } return 0;
      
      case WM_MOUSEHWHEEL: {
        POINT pt;
        if(GetCursorPos(&pt)){
          RECT rect;
          GetClientRect(_hwnd, &rect);
          ScreenToClient(_hwnd, &pt);
          
          if(!PtInRect(&rect, pt)){
            HWND parent = (HWND)GetWindowLongPtr(_hwnd, GWL_HWNDPARENT);
            return SendMessageW(parent, message, wParam, lParam);
          }
        }
        
        unsigned int num_chars;
        if(!SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &num_chars, 0))
          num_chars = 3;

        if(num_chars == 0)
          return 0;
        
        int rel_wheel = (int16_t)HIWORD(wParam);
        
        scroll_by((float)num_chars * -10 * rel_wheel / (float)WHEEL_DELTA, 0);
      } return 0;
      
      case WM_MOUSEWHEEL: {
        int rel_wheel = (int16_t)HIWORD(wParam);
        
        if(GetKeyState(VK_CONTROL) & ~1){
          scale_by(pow(2, 0.5 * rel_wheel / (float)WHEEL_DELTA));
        }
        else{
          POINT pt;
          if(GetCursorPos(&pt)){
            RECT rect;
            GetClientRect(_hwnd, &rect);
            ScreenToClient(_hwnd, &pt);
            
            if(!PtInRect(&rect, pt)){
              HWND parent = (HWND)GetWindowLongPtr(_hwnd, GWL_HWNDPARENT);
              return SendMessageW(parent, message, wParam, lParam);
            }
          }
          
          unsigned int num_lines;
          if(!SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &num_lines, 0))
            num_lines = 3;
          
          if(num_lines == 0)
            return 0;
          
          scroll_by(0, (float)num_lines * -20 * rel_wheel / (float)WHEEL_DELTA);
        }
      } return 0;
      
      case WM_LBUTTONDOWN: 
      case WM_MBUTTONDOWN: 
      case WM_RBUTTONDOWN: {
        MouseEvent event;
        
        event.left   = message == WM_LBUTTONDOWN;
        event.middle = message == WM_MBUTTONDOWN;
        event.right  = message == WM_RBUTTONDOWN;
        
        event.x = (int16_t) (lParam & 0xFFFF)            + GetScrollPos(_hwnd, SB_HORZ);
        event.y = (int16_t)((lParam & 0xFFFF0000) >> 16) + GetScrollPos(_hwnd, SB_VERT);
        
        event.x/= scale_factor();
        event.y/= scale_factor();
        
        on_mousedown(event);
      } return 0;
      
      case WM_LBUTTONUP: 
      case WM_MBUTTONUP: 
      case WM_RBUTTONUP: {
        MouseEvent event;
        
        event.left   = message == WM_LBUTTONUP;
        event.middle = message == WM_MBUTTONUP;
        event.right  = message == WM_RBUTTONUP;
        
        event.x = (int16_t) (lParam & 0xFFFF)            + GetScrollPos(_hwnd, SB_HORZ);
        event.y = (int16_t)((lParam & 0xFFFF0000) >> 16) + GetScrollPos(_hwnd, SB_VERT);
        
        event.x/= scale_factor();
        event.y/= scale_factor();
        
        on_mouseup(event);
      } return 0;
      
      case WM_MOUSEMOVE: {
        MouseEvent event;
        
        event.left   = (wParam & MK_LBUTTON) != 0;
        event.middle = (wParam & MK_MBUTTON) != 0;
        event.right  = (wParam & MK_RBUTTON) != 0;
        
        event.x = (int16_t) (lParam & 0xFFFF)            + GetScrollPos(_hwnd, SB_HORZ);
        event.y = (int16_t)((lParam & 0xFFFF0000) >> 16) + GetScrollPos(_hwnd, SB_VERT);
        
        event.x/= scale_factor();
        event.y/= scale_factor();
        
        on_mousemove(event);
        
        
        TRACKMOUSEEVENT tme;
        memset(&tme, 0, sizeof(tme));
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = _hwnd;
        tme.dwHoverTime = HOVER_DEFAULT;
        
        TrackMouseEvent(&tme);
      } return 0;
      
      case WM_MOUSELEAVE: {
        document()->mouse_exit();
      } return 0;
      
      case WM_TIMER: {
        switch(wParam){
          case TID_SCROLL: {
            if(scrolling){
              POINT mouse;
              GetCursorPos(&mouse);
              ScreenToClient(_hwnd, &mouse);
              
              float relx = 0;
              float rely = 0;
              if(mouse_down_event.middle){
                if(abs(mouse.x - mouse_down_event.x) > 10)
                  relx = abs(mouse.x - mouse_down_event.x) / 4;
                  
                if(abs(mouse.y - mouse_down_event.y) > 10)
                  rely = abs(mouse.y - mouse_down_event.y) / 4;
                  
                relx/= scale_factor();
                rely/= scale_factor();
                
                if(mouse.x < mouse_down_event.x)
                  relx = -relx;
                
                if(mouse.y < mouse_down_event.y)
                  rely = -rely;
              }
              else if(mouse_down_event.left){
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
              
              if(relx != 0 || rely != 0){
                scroll_by(relx, rely);
                
                MouseEvent event;
          
                event.left   = (GetKeyState(VK_LBUTTON) & ~1);
                event.middle = (GetKeyState(VK_MBUTTON) & ~1);
                event.right  = (GetKeyState(VK_RBUTTON) & ~1);
                
                event.x = mouse.x + GetScrollPos(_hwnd, SB_HORZ);
                event.y = mouse.y + GetScrollPos(_hwnd, SB_VERT);
                
                event.x/= scale_factor();
                event.y/= scale_factor();
                
                on_mousemove(event);
              }
            }
            else
              KillTimer(_hwnd, TID_SCROLL);
          } break;
        
          case TID_ANIMATE: {
            KillTimer(_hwnd, TID_ANIMATE);
            animation_running = 0;
            
            unsigned int count, i;
            for(count = 0,i = 0;count < animations.size();++i){
              Entry<SharedPtr<TimedEvent>,Void> *e = animations.entry(i);
              
              if(e){
                ++count;
                
                SharedPtr<TimedEvent> te = e->key;
                if(te->min_wait_seconds <= te->timer()){
                  animations.remove(te);
                  
                  te->execute_event();
                }
                else if(!animation_running){
                  animation_running = 0 != SetTimer(_hwnd, TID_ANIMATE, ANIMATION_DELAY, NULL);
      
                  if(!animation_running){
                    animations.remove(te);
                    
                    te->execute_event();
                  }
                }
              }
            }
          } break;
        
          case TID_BLINKCURSOR: {
            KillTimer(_hwnd, TID_BLINKCURSOR);
            
            Context *ctx = document_context();
            if(ctx->old_selection == ctx->selection 
            || _hwnd != GetFocus()
            || is_mouse_down())
              ctx->old_selection.id = 0;
            else
              ctx->old_selection = ctx->selection;
            
            Box *box = ctx->selection.get();
            if(box)
              box->request_repaint_all();
          } break;
        }
      } return 0;
      
      case WM_KEYDOWN: if(!is_drop_over){
        on_keydown(
          wParam, 
          GetKeyState(VK_CONTROL) & ~1,
          GetKeyState(VK_MENU)    & ~1, 
          GetKeyState(VK_SHIFT)   & ~1);
      } return 0;
      
      case WM_KEYUP: if(!is_drop_over){
        SpecialKeyEvent event;
        event.key = win32_virtual_to_special_key(wParam);
        if(event.key){
          event.ctrl  = GetKeyState(VK_CONTROL) & ~1;
          event.alt   = GetKeyState(VK_MENU)    & ~1;
          event.shift = GetKeyState(VK_SHIFT)   & ~1;
          document()->key_up(event);
        }
      } return 0;
      
      case WM_CHAR: if(!is_drop_over){
        if(wParam == 0xFFFF)
          return 1;
        
        if((wParam == ' ' 
         || wParam == '\r' 
         || wParam == '\n')
        && ((GetKeyState(VK_CONTROL) & ~1) 
         || (GetKeyState(VK_MENU) & ~1) 
         || (GetKeyState(VK_SHIFT) & ~1)))
          return 0;
        
        if(wParam == '\t')
          return 0;
        
        document()->key_press(wParam);
      } return 0;
      
      case WM_ACTIVATE: {
        if(LOWORD(wParam) == WA_INACTIVE){
          document()->reset_mouse();
        }
      } break;
      
      case WM_SETFOCUS: {
        Box *box = document()->selection_box();
        if(!box)
          box = document();
        
        if(box->selectable()){
          set_current_document(document());
        }
        
        if(document()->selection_box()
        && document()->selection_length() == 0
        && GetCaretBlinkTime() != INFINITE){
          SetTimer(_hwnd, TID_BLINKCURSOR, GetCaretBlinkTime(), NULL);
        }
      } return 0;
      
      case WM_SYSKEYDOWN:
      case WM_SYSKEYUP: {
        HWND parent = (HWND)GetWindowLongPtr(_hwnd, GWL_HWNDPARENT);
        if(parent){
          return SendMessageW(parent, message, wParam, lParam);
        }
      } break;
      
      case WM_COMMAND: {
        pmath_debug_print("C");
        String cmd = win32_command_id_to_command_string(LOWORD(wParam));
        if(cmd.is_null())
          break;
          
        Application::run_menucommand(cmd);
      } return 0;
    }
  }
  
  return BasicWin32Widget::callback(message, wParam, lParam);
}

bool Win32Widget::is_data_droppable(IDataObject *data_object){
  FORMATETC fmt;
  memset(&fmt, 0, sizeof(fmt));
  
  fmt.dwAspect = DVASPECT_CONTENT;
  fmt.lindex   = -1;
  fmt.ptd      = NULL;
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

DWORD Win32Widget::drop_effect(DWORD key_state, POINTL ptl, DWORD allowed_effects){
  POINT pt = {(int) ptl.x, (int)ptl.y };
  ScreenToClient(_hwnd, &pt);
  
  float x = (pt.x + GetScrollPos(_hwnd, SB_HORZ)) / scale_factor();
  float y = (pt.y + GetScrollPos(_hwnd, SB_VERT)) / scale_factor();
  
  int start, end;
  bool was_inside_start;
  Box *dst = document()->mouse_selection(x, y, &start, &end, &was_inside_start);
  
  if(!dst || !dst->get_style(Editable) || !dst->selectable(start))
    return DROPEFFECT_NONE;
  
  if(is_dragging){
    Box *src = drag_source_reference().get();
    
    if(src){
      Box *box = Box::common_parent(src, dst);
      
      if(box == src){
        int s = start;
        int e = end;
        box = dst;
        
        if(box == src
        && s <= drag_source_reference().end && e >= drag_source_reference().start)
          return DROPEFFECT_NONE;
        
        while(box != src){
          s = box->index();
          e = s + 1;
          box = box->parent();
        }
        
        if(s < drag_source_reference().end && e > drag_source_reference().start)
          return DROPEFFECT_NONE;
      }
      else if(box == dst){
        int s = drag_source_reference().start;
        int e = drag_source_reference().end;
        box = src;
        
        while(box != dst){
          s = box->index();
          e = s + 1;
          box = box->parent();
        }
        
        if(s < end && e > start)
          return DROPEFFECT_NONE;
      }
    }
  }
  
  return BasicWin32Widget::drop_effect(key_state, ptl, allowed_effects);
}

void Win32Widget::do_drop_data(IDataObject *data_object, DWORD effect){
  String mimetype;
  String text_data;
  
	STGMEDIUM stgmed;
	memset(&stgmed, 0, sizeof(stgmed));
	
	FORMATETC fmt;
  memset(&fmt, 0, sizeof(fmt));
  
  fmt.dwAspect = DVASPECT_CONTENT;
  fmt.lindex   = -1;
  fmt.ptd      = NULL;
  fmt.tymed    = TYMED_HGLOBAL;
  
  do{
    mimetype = Clipboard::BoxesText;
    fmt.cfFormat = Win32Clipboard::mime_to_win32cbformat[mimetype];
    if(data_object->QueryGetData(&fmt) == S_OK
    && data_object->GetData(&fmt, &stgmed) == S_OK){
      const uint16_t *data = (const uint16_t*)GlobalLock(stgmed.hGlobal);
      
      text_data = String::FromUcs2(data, -1);
      
      GlobalUnlock(stgmed.hGlobal);
      ReleaseStgMedium(&stgmed);
      break;
    }
    
    
    mimetype = Clipboard::PlainText;
    fmt.cfFormat = Win32Clipboard::mime_to_win32cbformat[Clipboard::PlainText];
    if(data_object->QueryGetData(&fmt) == S_OK
    && data_object->GetData(&fmt, &stgmed) == S_OK){
      const uint16_t *data = (const uint16_t*)GlobalLock(stgmed.hGlobal);
      
      text_data = String::FromUcs2(data, -1);
      
      GlobalUnlock(stgmed.hGlobal);
      ReleaseStgMedium(&stgmed);
      break;
    }
    
    fmt.cfFormat = CF_TEXT;
    if(data_object->QueryGetData(&fmt) == S_OK
    && data_object->GetData(&fmt, &stgmed) == S_OK){
      const char *data = (const char*)GlobalLock(stgmed.hGlobal);
      
      text_data = String(pmath_string_from_native(data, -1));
      
      GlobalUnlock(stgmed.hGlobal);
      ReleaseStgMedium(&stgmed);
      break;
    }
  }while(false);
  
  if(!text_data.is_null()){
    Box *oldbox  = document()->selection_box();
    int oldstart = document()->selection_start();
    int oldend   = document()->selection_start();
    
    if(effect & DROPEFFECT_MOVE && is_dragging){
      
      Box *src = drag_source_reference().get();
      if(src){
        int s = drag_source_reference().start;
        int e = drag_source_reference().end;
        
        drag_source_reference().reset();
        
        document()->select(src, s, e);
        document()->remove_selection(false);
        
        if(src == oldbox){
          if(oldstart >= e)
            oldstart-= e-s;
          if(oldend >= e)
            oldend-= e-s;
        }
        
        document()->select(oldbox, oldstart, oldend);
      }
    }
    
    document()->paste_from_text(mimetype, text_data);
    
    Box *newbox  = document()->selection_box();
    int newend   = document()->selection_start();
    
    if(oldbox == newbox){
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

void Win32Widget::position_drop_cursor(POINTL ptl){
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
