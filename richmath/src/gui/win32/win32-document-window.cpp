#define _WIN32_WINNT 0x0600
#define WINVER       0x0600

#include <gui/win32/win32-document-window.h>

#include <cmath>
#include <cstdio>
#include <cairo-win32.h>

#include <boxes/section.h>
#include <eval/binding.h>
#include <eval/client.h>
#include <gui/control-painter.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/win32-menubar.h>
#include <gui/win32/win32-themes.h>
#include <resources.h>

#ifndef WM_MOUSEHWHEEL
  #define WM_MOUSEHWHEEL  0x020E
#endif

using namespace richmath;

//{ FOREACH_WINDOW ...

#define FOREACH_WINDOW(NAME, PROC) \
  do{ \
    BasicWin32Window *_FOREACH_WINDOW_FIRST = BasicWin32Window::first_window(); \
     \
    if(_FOREACH_WINDOW_FIRST){ \
      bool _FOREACH_WINDOW_FIRST_TIME = true; \
       \
      for( \
          BasicWin32Window *_FOREACH_WINDOW_NEXT = _FOREACH_WINDOW_FIRST; \
          _FOREACH_WINDOW_FIRST_TIME || _FOREACH_WINDOW_NEXT != _FOREACH_WINDOW_FIRST; \
          _FOREACH_WINDOW_NEXT = _FOREACH_WINDOW_NEXT->next_window() \
      ){ \
        _FOREACH_WINDOW_FIRST_TIME = false; \
        Win32DocumentWindow *NAME = dynamic_cast<Win32DocumentWindow*>(_FOREACH_WINDOW_NEXT); \
         \
        if(NAME){ \
          PROC \
        } \
      } \
    } \
  }while(0)

//}

class richmath::WorkingArea: public Win32Widget{
  friend class Win32DocumentWindow;
  public:
    WorkingArea(
      Document *doc,
      DWORD style_ex, 
      DWORD style, 
      int x, 
      int y, 
      int width,
      int height,
      Win32DocumentWindow *_parent)
    : Win32Widget(doc, style_ex, style, x, y, width, height, &_parent->hwnd()),
      parent(_parent),
      auto_size(false),
      best_width(1),
      best_height(1)
    {
    }
    
    virtual void page_size(float *w, float *h){
      Win32Widget::page_size(w, h);
      
      if(auto_size)
        *w = HUGE_VAL;
    }
      
    virtual void running_state_changed(){
      if(parent)
        parent->title(parent->title());
    }
    
    virtual void close(){
      SendMessageW(parent->hwnd(), WM_CLOSE, 0, 0);
    }

  protected:
    Win32DocumentWindow *parent;
    
  public:
    bool auto_size;
    
    int best_width;
    int best_height;
    
  protected:
    void rearrange(){
      if(auto_size){
        RECT rect;
        GetClientRect(_hwnd, &rect);
        if(best_width  != rect.right - rect.left
        || best_height != rect.bottom - rect.top)
          parent->rearrange();
      }
    }
    
    virtual void set_cursor(CursorType type){
      if(auto_size && document()->count() == 0)
        return;
        
      Win32Widget::set_cursor(type);
    }
    
    virtual void paint_background(Canvas *canvas){
      if((auto_size && document()->count() == 0)
      || parent->is_palette()){
         parent->paint_background(canvas, _hwnd);
      }
      else{
        canvas->set_color(0xffffff);
        canvas->paint();
      }
    }
    
    virtual void paint_canvas(Canvas *canvas, bool resize_only){
      Win32Widget::paint_canvas(canvas, resize_only);
      
      if(auto_size){
        int old_bh = best_height;
        int old_bw = best_width;
        
        best_height = (int)ceilf(document()->extents().height() * scale_factor());
        best_width  = (int)ceilf(document()->unfilled_width     * scale_factor());
        
        if(best_height < 1)
          best_height = 1;
          
        RECT outer, inner;
        GetWindowRect(_hwnd, &outer);
        GetClientRect(_hwnd, &inner);
        
        best_width+=  outer.right  - outer.left - inner.right  + inner.left;
        best_height+= outer.bottom - outer.top  - inner.bottom + inner.top;
        
        if(old_bw != best_width || old_bh != best_height)
          parent->rearrange();
      }
      else
        best_width = best_height = 1;
    }
    
    virtual void on_paint(HDC dc, bool from_wmpaint){
      Win32Widget::on_paint(dc, from_wmpaint);
      rearrange();
    }
};

class richmath::Dock: public Win32Widget {
  friend class Win32DocumentWindow;
  protected:
    virtual void after_construction(){
      Win32Widget::after_construction();
      
      assert(document()->style.is_valid());
      
      document()->style->set(Selectable, false);
      
      document()->select(0,0,0);
      document()->border_visible = false;
    }
    
  public:
    Dock(Win32DocumentWindow *_parent)
    : Win32Widget(
        new Document(),
        0,
        WS_CHILD | WS_VISIBLE,
        0,0,10,10,
        &_parent->hwnd()),
      parent(_parent)
    {
    }
    
    virtual bool is_scrollable(){ return false; }
    virtual void scroll_pos(float *x, float *y){ *x = *y = 0; }
    virtual void scroll_to(float x, float y){}
    
    virtual void close(){
      SendMessageW(parent->hwnd(), WM_CLOSE, 0, 0);
    }

    virtual int height(){
      return (int)(document()->extents().height() * scale_factor() + 0.5f);
    }
    
    virtual int min_width(){
      return (int)(document()->unfilled_width * scale_factor() + 0.5f);
    }
     
    virtual void running_state_changed(){
      if(parent)
        parent->title(parent->title());
    }
    
    void resize(){
      HDC dc = GetDC(_hwnd);
      on_paint(dc, false);
      ReleaseDC(_hwnd, dc);
    }
    
    bool is_parent_bottom(){
      RECT self_rect;
      RECT parent_rect;
      GetClientRect(_hwnd, &self_rect);
      parent->get_client_rect(&parent_rect);
      
      POINT pt;
      pt.x = 0;
      pt.y = self_rect.bottom;
      
      MapWindowPoints(_hwnd, parent->hwnd(), &pt, 1);
      return pt.y == parent_rect.bottom;
    }
    
    bool have_size_grip(){
      if(!is_parent_bottom())
        return false;
      
      int last = document()->count() - 1;
      if(last < 0)
        return true;
      
      if(document()->section(last)->get_style(SectionMarginRight) < 12.0)
        return false;
      
      return true;
    }
  
  protected:
    Win32DocumentWindow *parent;
    
  protected:
    void paint_size_grip(Canvas *canvas){
      if(have_size_grip()){
        canvas->save();
        canvas->scale(scale_factor(), scale_factor());
        
        float w,h,b;
        window_size(&w, &h);
        b = ControlPainter::std->scrollbar_width();
        
        canvas->glass_background = true;
        ControlPainter::std->paint_scrollbar_part(
          canvas,
          ScrollbarSizeGrip,
          ScrollbarHorizontal,
          Normal,
          w - b,
          h - b,
          b,
          b);
        canvas->restore();
      }
    }
    
    virtual void paint_background(Canvas *canvas){
      parent->paint_background(canvas, _hwnd);
    }
    
    virtual void paint_canvas(Canvas *canvas, bool resize_only){
      bool bif = Win32ControlPainter::win32_painter.blur_input_field;
      Win32ControlPainter::win32_painter.blur_input_field = false;
        
      Win32Widget::paint_canvas(canvas, resize_only);
        
      Win32ControlPainter::win32_painter.blur_input_field = bif;
      paint_size_grip(canvas);
    }
    
    void rearrange(){
      RECT rect;
      GetClientRect(_hwnd, &rect);
      if(height() != rect.bottom)
        parent->rearrange();
    }
    
    virtual void on_paint(HDC dc, bool from_wmpaint){
      Win32Widget::on_paint(dc, from_wmpaint);
      rearrange();
    }

    virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam){
      if(!initializing()){
        switch(message){
          case WM_MOUSEMOVE: {
            Win32Widget::callback(message, wParam, lParam);
            
            if(have_size_grip()){
              int x = (int16_t) (lParam & 0xFFFF)            + GetScrollPos(_hwnd, SB_HORZ);
              int y = (int16_t)((lParam & 0xFFFF0000) >> 16) + GetScrollPos(_hwnd, SB_VERT);
              
              float w, h;
              window_size(&w, &h);
              
              float b = ControlPainter::std->scrollbar_width();
              
              if(x / scale_factor() >= w - b
              && y / scale_factor() >= h - b){
                SetCursor(LoadCursor(0, IDC_SIZENWSE));
              }
            }
          } return 0;
          
          case WM_LBUTTONDOWN: {
            Win32Widget::callback(message, wParam, lParam);
            
            if(have_size_grip()){
              int x = (int16_t) (lParam & 0xFFFF)            + GetScrollPos(_hwnd, SB_HORZ);
              int y = (int16_t)((lParam & 0xFFFF0000) >> 16) + GetScrollPos(_hwnd, SB_VERT);
              
              float w, h;
              window_size(&w, &h);
              
              float b = ControlPainter::std->scrollbar_width();
              
              if(x / scale_factor() >= w - b
              && y / scale_factor() >= h - b){
                SendMessageW(_hwnd, WM_LBUTTONUP, wParam, lParam);
                SendMessageW(parent->hwnd(), WM_NCLBUTTONDOWN, HTBOTTOMRIGHT, 0);
                return 1;
              }
            }
          } return 0;
        }
      }
      
      return Win32Widget::callback(message, wParam, lParam);
    }
};

class richmath::GlassDock: public richmath::Dock {
  protected:
    virtual void after_construction(){
      Dock::after_construction();
      
      document()->style->set(Background, -1);
      
      shadows = Evaluate(Parse(
          "{{-`1`,-`2`, GrayLevel(1), `3`},"
           "{ `1`,-`2`, GrayLevel(1), `3`},"
           "{-`1`, `2`, GrayLevel(1), `3`},"
           "{ `1`, `2`, GrayLevel(1), `3`}}",
        0.75, 0.75, 4.0));
      
      set_textshadows();
    }
    
  public:
    GlassDock(Win32DocumentWindow *_parent)
    : Dock(_parent)
    {
    }
    
  protected:
    Expr shadows;
  
  protected:
    void set_textshadows(){
      if(!document()->style)
        document()->style = new Style();
      
      document()->style->set(TextShadow, shadows);
    }
    
    void remove_textshadows(){
      if(document()->style)
        document()->style->remove(TextShadow);
    }
    
    virtual void paint_background(Canvas *canvas){
      if(Win32Themes::IsCompositionActive
      && Win32Themes::IsCompositionActive()){
        set_textshadows();
        canvas->glass_background = true;
        
//        /* Workaround a Cairo (1.8.8) Bug:
//            Platform: Windows, Cleartype on, ARGB32 image or HDC
//            
//            The last (cleartype-blured) pixel column of the last glyph and the zero-th 
//            column (also cleartype-blured) of the first pixel in a glyph-string wont 
//            be drawn. That looks ugly.
//            
//            To see the difference, comment out the following lines.
//         */
//        BOOL haveFontSmoothing = FALSE;
//        UINT fontSmoothingType = FE_FONTSMOOTHINGSTANDARD; 
//        
//        SystemParametersInfo(SPI_GETFONTSMOOTHING,     0, &haveFontSmoothing, 0);
//        SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &fontSmoothingType, 0);
//        
//        if(haveFontSmoothing && fontSmoothingType == FE_FONTSMOOTHINGCLEARTYPE){
//          //canvas->native_show_glyphs = false;
//          
//          cairo_font_options_t *opt = cairo_font_options_create();
//          cairo_font_options_set_antialias(opt, CAIRO_ANTIALIAS_GRAY);
//          cairo_set_font_options(canvas->cairo(), opt);
//          cairo_font_options_destroy(opt);
//        }
      }
      else{
        remove_textshadows();
        
        //canvas->native_show_glyphs = true;
        
        if(parent->glass_enabled())
          canvas->glass_background = true;
      }
      
      Dock::paint_background(canvas);
    }
    
    virtual void on_paint(HDC dc, bool from_wmpaint){
      if(Win32Themes::IsCompositionActive
      && Win32Themes::IsCompositionActive())
        _image_format = CAIRO_FORMAT_ARGB32;
      else
        _image_format = CAIRO_FORMAT_RGB24;
        
      Dock::on_paint(dc, from_wmpaint);
    }

    virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam){
      if(!initializing()){
        switch(message){
          case WM_LBUTTONDOWN: {
            if(Dock::callback(message, wParam, lParam) == 0
            && document()->clicked_box_id() == document()->id()
            && parent->glass_enabled()){
              SendMessageW(_hwnd, WM_LBUTTONUP, wParam, lParam);
              SendMessageW(parent->hwnd(), WM_NCLBUTTONDOWN, HTCAPTION, lParam);
            }
          } return 0;
        }
      }
      
      return Dock::callback(message, wParam, lParam);
    }
};

//{ class Win32DocumentWindow ...

Win32DocumentWindow::Win32DocumentWindow(
  Document *doc,
  DWORD style_ex, 
  DWORD style, 
  int x, 
  int y, 
  int width,
  int height)
: BasicWin32Window(
    style_ex, 
    style | WS_CLIPCHILDREN, 
    x, 
    y, 
    width, 
    height),
  _top_glass_area(0),
  _top_area(0),
  _working_area(0),
  _bottom_area(0),
  _bottom_glass_area(0),
  menubar(0),
  creation(true),
  _is_palette(false)
{
  _working_area = new WorkingArea(
    doc,
    0,
    WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_VISIBLE,
    0,0,0,0,
    this);
  
  _top_glass_area    = new GlassDock(this);
  _top_area          = new Dock(this);
  _bottom_area       = new Dock(this);
  _bottom_glass_area = new GlassDock(this);
}

void Win32DocumentWindow::after_construction(){
  BasicWin32Window::after_construction();
  
  _working_area->init();
  _top_glass_area->init();
  _top_area->init();
  _bottom_area->init();
  _bottom_glass_area->init();
  
  top_glass()->main_document    = document();
  top()->main_document          = document();
  bottom()->main_document       = document();
  bottom_glass()->main_document = document();
  
  menubar = new Win32Menubar(
    this, _hwnd,
    LoadMenuW(GetModuleHandle(0), MAKEINTRESOURCEW(MENU_MAIN)));
  
  HMENU sysmenu = GetSystemMenu(_hwnd, FALSE);
  AppendMenuW(sysmenu, MF_SEPARATOR, 0, L"");
  
  for(int i = 0;i < GetMenuItemCount(menubar->menu());++i){
    wchar_t data[100];
    
    MENUITEMINFOW info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_SUBMENU;
    info.dwTypeData = data;
    info.cch = 99;
    
    GetMenuItemInfoW(menubar->menu(), i, TRUE, &info);
    InsertMenuItemW(sysmenu, GetMenuItemCount(sysmenu), TRUE, &info);
  }
  
  all_document_ids.set(document()->id(), Void());
  if(get_current_document() == 0){
    set_current_document(document());
  }
  
  creation = false;
  
  on_theme_changed();
  
  title("untitled");
}

Win32DocumentWindow::~Win32DocumentWindow(){
  all_document_ids.remove(_working_area->document()->id());
  
  delete _top_glass_area;
  delete _top_area;
  delete _bottom_area;
  delete _bottom_glass_area;
  delete _working_area;
  delete menubar;
  
  static bool deleting_all = false;
  
  if(!deleting_all){
    bool have_only_palettes = true;
    FOREACH_WINDOW(win,
      if(win != this && !win->_is_palette){
        have_only_palettes = false;
        break;
      }
    );
    
    if(have_only_palettes){
      static Array<Win32DocumentWindow*> all_windows;
      
      all_windows.length(0);
      FOREACH_WINDOW(win,
        if(win != this)
          all_windows.add(win);
      );
      
      deleting_all = true;
      for(int i = 0;i < all_windows.length();++i){
        delete all_windows[i];
      }
      deleting_all = false;
      
      PostQuitMessage(0);
    }
  }
}

bool Win32DocumentWindow::is_all_glass(){
  return _top_area->document()->count()     == 0
      && _working_area->document()->count() == 0
      && _bottom_area->document()->count()  == 0;
}

void Win32DocumentWindow::rearrange(){
  if(creation)
    return;
  
  RECT rect;
  get_client_rect(&rect);
  
  #define PARTCOUNT  6
  HWND widgets[PARTCOUNT];
  widgets[0] = menubar->hwnd();
  widgets[1] = _top_glass_area->hwnd();
  widgets[2] = _top_area->hwnd();
  widgets[3] = _working_area->hwnd();
  widgets[4] = _bottom_area->hwnd();
  widgets[5] = _bottom_glass_area->hwnd();
  
  int new_ys[PARTCOUNT + 1] = {0};
  new_ys[0] = rect.top;
  new_ys[1] = new_ys[0] + menubar->height();
  new_ys[2] = new_ys[1] + _top_glass_area->height();
  new_ys[3] = new_ys[2] + _top_area->height();
  
  new_ys[6] = rect.bottom;
  new_ys[5] = new_ys[6] - _bottom_glass_area->height();
  new_ys[4] = new_ys[5] - _bottom_area->height();
  
  int work_height = new_ys[4] - new_ys[3];
  if(_working_area->auto_size){
    if(work_height            != _working_area->best_height
    || rect.right - rect.left != _working_area->best_width){
      RECT outer;
      GetWindowRect(_hwnd, &outer);
      
      int h = _working_area->best_height;
      int w = _working_area->best_width;
      
      if(w < _top_glass_area->min_width())
         w = _top_glass_area->min_width();
      
      if(w < _top_area->min_width())
         w = _top_area->min_width();
      
      if(w < _bottom_area->min_width())
         w = _bottom_area->min_width();
      
      if(w < _bottom_glass_area->min_width())
         w = _bottom_glass_area->min_width();
      
      int oldh = outer.bottom - outer.top;
      int oldw = outer.right  - outer.left;
      int newh = oldh - work_height + h;
      int neww = oldw - (rect.right - rect.left)  + w;
      
      MONITORINFO monitor_info;
      memset(&monitor_info, 0, sizeof(monitor_info));
      monitor_info.cbSize = sizeof(monitor_info);
      HMONITOR hmon = MonitorFromWindow(_hwnd, MONITOR_DEFAULTTONEAREST);
      if(GetMonitorInfo(hmon, &monitor_info)){
        if(newh > monitor_info.rcWork.bottom - monitor_info.rcWork.top)
           newh = monitor_info.rcWork.bottom - monitor_info.rcWork.top;
          
        if(neww > monitor_info.rcWork.right - monitor_info.rcWork.left)
           neww = monitor_info.rcWork.right - monitor_info.rcWork.left;
      }
      
      if(newh != oldh || neww != oldw){
        bool align_right;
        bool align_bottom;
        get_snap_alignment(&align_right, &align_bottom);
        
        if(align_right)
          outer.left = outer.right - neww;
          
        if(align_bottom)
          outer.top = outer.bottom - newh;
        
        UINT flags = SWP_NOZORDER;
        if(!align_right && !align_bottom)
          flags|= SWP_NOMOVE;
        
        SetWindowPos(
          _hwnd, NULL,
          outer.left, outer.top, 
          neww, newh,
          flags);
        
        rect.right+=  neww - oldw;
        rect.bottom+= newh - oldh;
        
        for(int i = 4;i <= PARTCOUNT;++i)
          new_ys[i]+= newh - oldh;
      }
    }
  }
  
  if(glass_enabled()){
    widgets[0] = _top_glass_area->hwnd();
    widgets[1] = menubar->hwnd();
    
    new_ys[1] = new_ys[0] + _top_glass_area->height();
    
    Win32Themes::MARGINS mar = {
      0, 0, 
      _top_glass_area->height(), 
      _bottom_glass_area->height()};
    
    if(_working_area->document()->count() == 0){
      if(rect.bottom - rect.top <= mar.cyTopHeight + mar.cyBottomHeight + 1
      || _working_area->auto_size){
        mar.cxLeftWidth = mar.cxRightWidth = mar.cyTopHeight = mar.cyBottomHeight = -1;
        
        SetWindowPos(_working_area->hwnd(), 0, 0, 0, 0, 0, 
          SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
      }
    }
    
    extend_glass(&mar);
  }
  
  int order[PARTCOUNT] = {0,1,2,5,4,3};
  
  /*
    When the bottom-glass-area moves up, we must move the (non-glass) 
    bottom-area above it before to avoid artifacts.
    When the bottom-glass-area moves down, it must be moved before for the same 
    reason.
  */
  POINT topleft = {0,0};
  MapWindowPoints(widgets[5], _hwnd, &topleft, 1);
  if(topleft.y > new_ys[5]){ 
    order[3] = 4;
    order[4] = 5;
  }
  
  for(int i = 0;i < PARTCOUNT;++i){
    int j = order[i];
    
    RECT oldrect;
    GetWindowRect(widgets[j], &oldrect);
    
    if(oldrect.left   != rect.left
    || oldrect.top    != new_ys[j]
    || oldrect.right  != rect.right - rect.left
    || oldrect.bottom != new_ys[j+1] - new_ys[j]){
      int w = rect.right - rect.left;
      int h = new_ys[j+1] - new_ys[j];
      
      SetWindowPos(
        widgets[j], NULL,
        rect.left, new_ys[j], w, h,
        SWP_NOZORDER);
    }
  }
  
  min_client_height = rect.bottom - (new_ys[4] - new_ys[3]);
  
  if(_working_area->auto_size){
    min_client_height+= work_height;
    max_client_height = min_client_height;
    
    min_client_width = rect.right - rect.left;
    max_client_width = min_client_width;
  }
  else{
    min_client_height+= GetSystemMetrics(SM_CYHSCROLL);
    max_client_height = -1;
    min_client_width = 0;
    max_client_width = -1;
  }

  if(has_themed_frame()){
    _top_glass_area->invalidate();
    _top_area->invalidate();
    _bottom_area->invalidate();
    _bottom_glass_area->invalidate();
  }
}

void Win32DocumentWindow::title(String text){
  _title = text;
  
  if(!Client::is_idle(document()->id()))
    text = String("Running... ") + text;
  
  String tmp = text + String::FromChar(0);
  
  SetWindowTextW(_hwnd, (const WCHAR*)tmp.buffer());
}

void Win32DocumentWindow::is_palette(bool value){
  if(_is_palette == value)
    return;
  
  _is_palette = value;
  
  if(_is_palette){
    // tool window caption:
    SetWindowLongW(_hwnd, GWL_EXSTYLE, 
      GetWindowLongW(_hwnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
    
    // in front of non-palettes:
    zorder_level = 1;
    
    // also enable Minimize/Maximize in system menu:
    SetWindowLongW(_hwnd, GWL_STYLE,
      GetWindowLongW(_hwnd, GWL_STYLE) & ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX));
    
    menubar->appearence(MaNeverShow);
  }
  else{
    // normal window caption:
    SetWindowLongW(_hwnd, GWL_EXSTYLE, 
      GetWindowLongW(_hwnd, GWL_EXSTYLE) & ~(WS_EX_TOOLWINDOW));
    
    // behind palettes:
    zorder_level = 0;
    
    // also enable Minimize/Maximize in system menu:
    SetWindowLongW(_hwnd, GWL_STYLE,
      GetWindowLongW(_hwnd, GWL_STYLE) | (WS_MAXIMIZEBOX | WS_MINIMIZEBOX));
    
    if(glass_enabled())
      menubar->appearence(MaAutoShow);
    else
      menubar->appearence(MaAllwaysShow);
  }
  
  _working_area->auto_size = _is_palette;
  _working_area->document()->border_visible = !_is_palette;
  _working_area->_autohide_vertical_scrollbar = _is_palette;
  
  on_theme_changed();
}

void Win32DocumentWindow::on_theme_changed(){
  BasicWin32Window::on_theme_changed();
  
  if(_is_palette)
    menubar->appearence(MaNeverShow);
  else if(glass_enabled())
    menubar->appearence(MaAutoShow);
  else
    menubar->appearence(MaAllwaysShow);
  
  DWORD style_ex = GetWindowLongW(_working_area->hwnd(), GWL_EXSTYLE);
  if((Win32Themes::IsCompositionActive
   && Win32Themes::IsCompositionActive())
  || _is_palette){
    style_ex = style_ex & ~WS_EX_STATICEDGE;
  }
  else{
    style_ex = style_ex | WS_EX_STATICEDGE;
  }
  SetWindowLongW(_working_area->hwnd(), GWL_EXSTYLE, style_ex);
  
  rearrange();
}

LRESULT Win32DocumentWindow::callback(UINT message, WPARAM wParam, LPARAM lParam){
  LRESULT result = 0;
  
  if(!initializing()){
    if(menubar->callback(&result, message, wParam, lParam))
      return result;
      
    switch(message){
      case WM_SIZE: {
        _top_glass_area->resize();
        _top_area->resize();
        _bottom_area->resize();
        _bottom_glass_area->resize();
        rearrange();
      } break;
      
      case WM_MOUSEHWHEEL:
      case WM_MOUSEWHEEL: {
        POINT pt;
        if(GetCursorPos(&pt)){
          RECT rect;
          GetClientRect(_working_area->hwnd(), &rect);
          ScreenToClient(_working_area->hwnd(), &pt);
          
          if(PtInRect(&rect, pt)){
            return SendMessageW(_working_area->hwnd(), message, wParam, lParam);
          }
        }
      } break;
      
      case WM_SETFOCUS: {
        SetFocus(_working_area->hwnd());
        if(_working_area->document()->selectable()){
          set_current_document(document());
        }
      } break;
      
      case WM_MOUSEACTIVATE: {
        if(LOWORD(lParam) == HTCLIENT)
          return MA_NOACTIVATE;
      } break;
      
      case WM_ACTIVATE: {
        if(HIWORD(wParam)){ // minimizing
          bool have_only_palettes = true;
          
          FOREACH_WINDOW(wnd,
            if(wnd != this 
            && !wnd->_is_palette
            && IsWindowVisible(wnd->hwnd())
            && (GetWindowLongW(wnd->hwnd(), GWL_STYLE) & WS_MINIMIZE) == 0){
              have_only_palettes = false;
              break;
            }
          );
          
          if(have_only_palettes){
            FOREACH_WINDOW(tool,
              if(tool->_is_palette){
                ShowWindow(tool->hwnd(), SW_HIDE);
              }
            );
          }
        }
      } break;
      
      case WM_ACTIVATEAPP: {
        static bool already_activated = false;
        
        if(wParam){ // activate
          if(!already_activated){            
            FOREACH_WINDOW(wnd,
              if(!wnd->_is_palette){
                SetWindowPos(wnd->hwnd(), HWND_TOP, 0, 0, 0, 0, 
                  SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
              }
            );
          }
          already_activated = true;
        }
        else{
          already_activated = false;
        }
      } break;
      
      case WM_COMMAND: {
        SharedPtr<MathShaper> ms = _working_area->document_context()->math_shaper;
        switch(LOWORD(wParam)){
          case IDM_CAMBRIAMATHSHAPER:     ms = MathShaper::available_shapers["Cambria Math"];    break;
          case IDM_MATHEMATICASHAPER:     ms = MathShaper::available_shapers["Mathematica Serif"];     break;
          case IDM_MATHEMATICAMONOSHAPER: ms = MathShaper::available_shapers["Mathematica Mono"]; break;
          case IDM_ASANAMATHSHAPER:       ms = MathShaper::available_shapers["Asana Math"];      break;
          case IDM_NEOEULERSHAPER:        ms = MathShaper::available_shapers["Neo Euler"];      break;
          case IDM_XITSMATHSHAPER:        ms = MathShaper::available_shapers["XITS Math"];      break;
          default:
            Client::run_menucommand(win32_command_id_to_command_string(LOWORD(wParam)));
            return 0;
        }
        
        _top_glass_area->document_context()->math_shaper = ms;
        _top_area->document_context()->math_shaper = ms;
        _bottom_area->document_context()->math_shaper = ms;
        _bottom_glass_area->document_context()->math_shaper = ms;
        _working_area->document_context()->math_shaper = ms;
        
        _top_glass_area->document()->invalidate_all();
        _top_area->document()->invalidate_all();
        _bottom_area->document()->invalidate_all();
        _bottom_glass_area->document()->invalidate_all();
        _working_area->document()->invalidate_all();
      } return 0;
      
      case WM_SYSCOMMAND: {
        if(wParam < 0xF000){
          String cmd = win32_command_id_to_command_string(LOWORD(wParam));
          if(cmd.is_valid())
            Client::run_menucommand(cmd);
          else
            SendMessageW(_hwnd, WM_COMMAND, wParam, lParam);
        }
      } break;
      
      case WM_KEYDOWN:
      case WM_CHAR: 
      case WM_KEYUP:
        return SendMessageW(_working_area->hwnd(), message, wParam, lParam);
    }
  }
  
  return BasicWin32Window::callback(message, wParam, lParam);
}

//} ... class Win32DocumentWindow
