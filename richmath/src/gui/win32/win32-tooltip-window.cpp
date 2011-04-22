#define _WIN32_WINNT 0x0600
#define WINVER       0x0600

#include <gui/win32/win32-tooltip-window.h>

#include <boxes/mathsequence.h>
#include <boxes/section.h>

#include <gui/win32/win32-themes.h>

#include <cmath>


#define TID_UPDATETOOLPOS    101


using namespace richmath;

static Win32TooltipWindow *tooltip_window = 0;

//{ class Win32TooltipWindow ...

Win32TooltipWindow::Win32TooltipWindow()
: Win32Widget(
    new Document(), 
    WS_EX_TOOLWINDOW, 
    WS_POPUP | WS_CLIPCHILDREN | WS_DISABLED, 
    0, 
    0, 
    100, 
    100, 
    NULL),
  _align_left(true)
{
}

void Win32TooltipWindow::after_construction(){
  Win32Widget::after_construction();
  
  if(!document()->style)
    document()->style = new Style;
  document()->style->set(Editable,            false);
  document()->style->set(Selectable,          false);
  document()->select(0,0,0);
  
  document()->border_visible = false;
}

Win32TooltipWindow::~Win32TooltipWindow(){
  if(this == tooltip_window)
    tooltip_window = 0;
}

void Win32TooltipWindow::show_global_tooltip(int x, int y, Expr boxes){
  if(!tooltip_window){
    tooltip_window = new Win32TooltipWindow();
    tooltip_window->init();
  }
  
  if(tooltip_window->_content_expr != boxes){
    tooltip_window->_content_expr = boxes;
    
    Document *doc = tooltip_window->document();
    doc->remove(0, doc->length());
    
    Style *style = new Style;
    style->set(BaseStyleName,       "ControlStyle");
    style->set(SectionMarginLeft,   0);
    style->set(SectionMarginTop,    0);
    style->set(SectionMarginRight,  0);
    style->set(SectionMarginBottom, 0);
    
    boxes = Call(Symbol(PMATH_SYMBOL_BUTTONBOX), 
      boxes, 
      Rule(Symbol(PMATH_SYMBOL_BUTTONFRAME),
      String("TooltipWindow")));
    
    MathSection *section = new MathSection(style);
    section->content()->load_from_object(boxes, BoxOptionFormatNumbers);
    doc->insert(0, section);
  }
  
  if(!IsWindowVisible(tooltip_window->_hwnd))
    ShowWindow(tooltip_window->_hwnd, SW_SHOWNA);
  
  SetWindowPos(tooltip_window->_hwnd, HWND_TOPMOST, x + 10, y + 10, 0, 0, 
    SWP_NOACTIVATE | SWP_NOSIZE);
}

void Win32TooltipWindow::hide_global_tooltip(){
  if(tooltip_window){
    ShowWindow(tooltip_window->_hwnd, SW_HIDE);
    tooltip_window->_content_expr = Expr(PMATH_UNDEFINED);
  }
}

void Win32TooltipWindow::delete_global_tooltip(){
  if(tooltip_window)
    delete tooltip_window;
}

void Win32TooltipWindow::page_size(float *w, float *h){
  Win32Widget::page_size(w, h);
  *w = HUGE_VAL;
}

void Win32TooltipWindow::resize(){
  if(Win32Themes::IsThemeActive
  && Win32Themes::IsThemeActive()){
    SetWindowRgn(_hwnd, CreateRoundRectRgn(0, 0, best_width+1, best_height+1, 4, 4), FALSE);
  }
  else
    SetWindowRgn(_hwnd, NULL, FALSE);
  
  SetWindowPos(_hwnd, NULL, 0, 0, best_width, best_height,
    SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
}

void Win32TooltipWindow::paint_canvas(Canvas *canvas, bool resize_only){
  Win32Widget::paint_canvas(canvas, resize_only);
  
  int old_bh = best_height;
  int old_bw = best_width;
  
  best_height = (int)ceilf(document()->extents().height() * scale_factor());
  best_width  = (int)ceilf(document()->unfilled_width     * scale_factor());
  
  if(best_height < 1)
    best_height = 1;
    
  if(best_width < 1)
    best_width = 1;
    
  RECT outer, inner;
  GetWindowRect(_hwnd, &outer);
  GetClientRect(_hwnd, &inner);
  
  best_width+=  outer.right  - outer.left - inner.right  + inner.left;
  best_height+= outer.bottom - outer.top  - inner.bottom + inner.top;
  
  if(old_bw != best_width || old_bh != best_height){
    resize();
  }
}

LRESULT Win32TooltipWindow::callback(UINT message, WPARAM wParam, LPARAM lParam){
  if(!initializing()){
    switch(message){
      case WM_NCHITTEST:
        return HTNOWHERE;
      
      case WM_MOUSEACTIVATE: 
        return MA_NOACTIVATE;
      
      case WM_ACTIVATEAPP: 
        if(!wParam)
          ShowWindow(_hwnd, SW_HIDE);
        break;
      
      case WM_SHOWWINDOW:{
        if(wParam){
          SetTimer(_hwnd, TID_UPDATETOOLPOS, 500, NULL);
        }
        else
          KillTimer(_hwnd, TID_UPDATETOOLPOS);
      } break;
      
      case WM_TIMER:
        if(wParam == TID_UPDATETOOLPOS){
          POINT pt;
          RECT rect;
          
          if(GetCursorPos(&pt)
          && GetWindowRect(_hwnd, &rect)){
            SetWindowPos(_hwnd, NULL, pt.x + 10, pt.y + 10, 0, 0, 
              SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
          }
        }
        break;
    }
  }
  
  return Win32Widget::callback(message, wParam, lParam);
}

//} ... class Win32TooltipWindow
