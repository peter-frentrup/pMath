#define WINVER 0x0600

#include <gui/win32/win32-control-painter.h>

#include <cmath>
#include <cstdio>
#include <cwchar>

#include <windows.h>

#include <cairo-win32.h>

#include <graphics/context.h>
#include <gui/win32/basic-win32-widget.h>
#include <gui/win32/win32-themes.h>
#include <util/array.h>
#include <util/style.h>

using namespace richmath;

static HWND hwnd_message = HWND_MESSAGE;

class Win32ControlPainterInfo: public BasicWin32Widget{
  public:
    Win32ControlPainterInfo()
    : BasicWin32Widget(0, 0, 0, 0, 0, 0, &hwnd_message)
    {
      init(); // total exception!!! normally not callable in constructor
    }
  
  protected:
    virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam){
      switch(message){
        case WM_DWMCOMPOSITIONCHANGED: 
        case WM_THEMECHANGED: {
          printf("D");
          Win32ControlPainter::win32_painter.clear_cache();
        } break;
      }
      
      return BasicWin32Widget::callback(message, wParam, lParam);
    }
};

static Win32ControlPainterInfo w32cpinfo;

Win32ControlPainter Win32ControlPainter::win32_painter;

Win32ControlPainter::Win32ControlPainter()
: ControlPainter(),
  blur_input_field(true),
  button_theme(0),
  edit_theme(0),
  scrollbar_theme(0),
  toolbar_theme(0)
{
  ControlPainter::std = this;
}

Win32ControlPainter::~Win32ControlPainter(){
  clear_cache();
}
      
void Win32ControlPainter::calc_container_size(
  Canvas        *canvas,
  ContainerType  type,
  BoxSize       *extents // in/out
){
  if(Win32Themes::GetThemeMargins
  && Win32Themes::GetThemePartSize){
    int theme_part, theme_state;
    HANDLE theme = get_control_theme(type, Normal, &theme_part, &theme_state);
    
    SIZE size = {0,0};
    Win32Themes::MARGINS mar = {0,0,0,0};
    // TMT_SIZINGMARGINS  = 3601
    // TMT_CONTENTMARGINS = 3602
    // TMT_CAPTIONMARGINS = 3603
    if(theme 
    && SUCCEEDED(Win32Themes::GetThemeMargins(
        theme, NULL, theme_part, theme_state, 3602, NULL, &mar))
    && (mar.cxLeftWidth > 0 
     || mar.cxRightWidth > 0 
     || mar.cyBottomHeight > 0 
     || mar.cyTopHeight > 0)
    ){
      extents->width+=   0.75f * (mar.cxLeftWidth + mar.cxRightWidth);
      extents->ascent+=  0.75f * mar.cyTopHeight;
      extents->descent+= 0.75f * mar.cyBottomHeight;
      
      Win32Themes::GetThemePartSize(
        theme, NULL, theme_part, theme_state, NULL, Win32Themes::TS_TRUE, &size);
      
      if(extents->width < 0.75 * size.cx)
         extents->width = 0.75 * size.cx;
      
      if(extents->height() < 0.75 * size.cy){
        float axis = 0.4 * canvas->get_font_size();
        
        extents->ascent  = size.cy * 0.75 / 2 + axis;
        extents->descent = size.cy * 0.75 - extents->ascent;
      }
      
      return;
    }
  }
  
  if(type == InputField){
    extents->width+=   3.75;
    extents->ascent+=  2.25;
    extents->descent+= 1.5;
    return;
  }
  
  ControlPainter::calc_container_size(canvas, type, extents);
}

int Win32ControlPainter::control_font_color(ContainerType type, ControlState state){
  if(is_very_transparent(type, state))
    return -1;
  
  int theme_part, theme_state;
  HANDLE theme = get_control_theme(type, state, &theme_part, &theme_state);
  
  if(theme && Win32Themes::GetThemeColor){
    COLORREF col = 0;
    
    // TMT_TEXTCOLOR  = 3803
    // TMT_WINDOWTEXT	= 1609
    // TMT_BTNTEXT    = 1619
    if(SUCCEEDED(Win32Themes::GetThemeColor(
        theme, theme_part, theme_state, 3803, &col))
    || SUCCEEDED(Win32Themes::GetThemeColor(
        theme, theme_part, theme_state, 1609, &col))
    || SUCCEEDED(Win32Themes::GetThemeColor(
        theme, theme_part, theme_state, 1619, &col))
    ){
      return ((col & 0xFF0000) >> 16)
           |  (col & 0x00FF00)
           | ((col & 0x0000FF) << 16);
    }
    
  }
  
  switch(type){
    case FramelessButton:
    case GenericButton:
      return ControlPainter::control_font_color(type, state);
    
    case PushButton:
    case DefaultPushButton:
    case PaletteButton: {
      DWORD col = GetSysColor(COLOR_BTNTEXT);
      return ((col & 0xFF0000) >> 16)
           |  (col & 0x00FF00)
           | ((col & 0x0000FF) << 16);
    } break;
      
    case InputField: {
      DWORD col = GetSysColor(COLOR_WINDOWTEXT);
      return ((col & 0xFF0000) >> 16)
           |  (col & 0x00FF00)
           | ((col & 0x0000FF) << 16);
    } break;
  }
  return -1;
}

bool Win32ControlPainter::is_very_transparent(ContainerType type, ControlState state){
  switch(type){
    case FramelessButton:
    case GenericButton:
      return ControlPainter::is_very_transparent(type, state);
      
    case PaletteButton: {
      if(!Win32Themes::GetThemeBool)
        return false;
      
      int theme_part, theme_state;
      HANDLE theme = get_control_theme(type, state, &theme_part, &theme_state);
      
      // TMT_TRANSPARENT = 2201
      BOOL value;
      if(theme && SUCCEEDED(Win32Themes::GetThemeBool(
          theme, theme_part, theme_state, 2201, &value))
      ){
        return value;
      }
    } break;
    
    default: break;
  }
  
  return false;
}

  static bool rect_in_clip(
    Canvas *canvas,
    float   x,
    float   y,
    float   width,
    float   height
  ){
    cairo_rectangle_list_t *clip_rects = cairo_copy_clip_rectangle_list(canvas->cairo());
    
    if(clip_rects->status == CAIRO_STATUS_SUCCESS){
      for(int i = 0;i < clip_rects->num_rectangles;++i){
        cairo_rectangle_t *rect = &clip_rects->rectangles[i];
        if(x          >= rect->x 
        && y          >= rect->y 
        && x + width  <= rect->x + rect->width
        && y + height <= rect->y + rect->height){
          cairo_rectangle_list_destroy(clip_rects);
          return true;
        }
      }
    }
    
    cairo_rectangle_list_destroy(clip_rects);
    return false;
  }

void Win32ControlPainter::draw_container(
  Canvas        *canvas,
  ContainerType  type,
  ControlState   state,
  float          x,
  float          y,
  float          width,
  float          height
){
  if(width <= 0 || height <= 0)
    return;
  
  switch(type){
    case FramelessButton:
    case GenericButton:
      ControlPainter::generic_painter.draw_container(
        canvas, type, state, x, y, width, height);
      return;
      
    default: break;
  }
  
  if(canvas->pixel_device){
    canvas->user_to_device_dist(&width, &height);
    width  = floor(width  + 0.5);
    height = floor(height + 0.5);
    canvas->device_to_user_dist(&width, &height);
  }
  
  if(type == InputField
  && Win32Themes::IsThemeActive
  && Win32Themes::IsThemeActive()
  && Win32Themes::OpenThemeData 
  && Win32Themes::CloseThemeData 
  && Win32Themes::DrawThemeBackground){
    canvas->save();
    int c = canvas->get_color();
    
    if(canvas->glass_background){
      cairo_pattern_t *pat;
      
      float x1, x2, x3, x4, y1, y2, y3, y4;
      x1 = x4 = x;
      x2 = x3 = x + width;
      y1 = y2 = y;
      y3 = y4 = y + height;
      
      canvas->align_point(&x1, &y1, true);
      canvas->align_point(&x2, &y2, true);
      canvas->align_point(&x3, &y3, true);
      canvas->align_point(&x4, &y4, true);
      
      float r = 0.75;
      canvas->move_to(x1, y1 + r);
      canvas->arc(x1 + r, y1 + r, r,   M_PI,   3*M_PI/2, false);
      canvas->arc(x2 - r, y2 + r, r, 3*M_PI/2, 2*M_PI,   false);
      canvas->arc(x3 - r, y3 - r, r,        0,   M_PI/2, false);
      canvas->arc(x4 + r, y4 - r, r,   M_PI/2,   M_PI,   false);
      canvas->close_path();
      
      pat = cairo_pattern_create_linear(x4, y4, x1, y1);
      cairo_pattern_add_color_stop_rgba(pat, 0, 1, 1, 1, 0.7);
      cairo_pattern_add_color_stop_rgba(pat, 1, 1, 1, 1, 0.4);
      cairo_set_source(canvas->cairo(), pat);
      cairo_pattern_destroy(pat);
      
      cairo_set_line_width(canvas->cairo(), 2);
      canvas->stroke_preserve();
      
      if(state != Normal)
        cairo_set_source_rgba(canvas->cairo(), 1, 1, 1, 0.9);
      else
        cairo_set_source_rgba(canvas->cairo(), 1, 1, 1, 0.7);
      canvas->fill_preserve();
      
      pat = cairo_pattern_create_linear(x4, y4, x1, y1);
      cairo_pattern_add_color_stop_rgba(pat, 0, 0, 0, 0, 0.55);
      cairo_pattern_add_color_stop_rgba(pat, 1, 0, 0, 0, 0.75);
      cairo_set_source(canvas->cairo(), pat);
      cairo_pattern_destroy(pat);
      cairo_set_line_width(canvas->cairo(), 0.75);
      canvas->stroke();
      
    }
    else{
      canvas->pixrect(x, y, x + width, y + height, true);
      if(state == Pressed && blur_input_field){
        canvas->set_color(0x8080FF);
        canvas->show_blur_stroke(4, true);
      }
      
      DWORD col = GetSysColor(COLOR_WINDOW);
      col = ((col & 0xFF0000) >> 16)
          |  (col & 0x00FF00)
          | ((col & 0x0000FF) << 16);
           
      canvas->set_color(col);
      canvas->fill_preserve();
      
      canvas->set_color(0x808080);
      canvas->hair_stroke();
    }
    
    canvas->set_color(c);
    canvas->restore();
    return;
  }
  
  int dc_x = 0;
  int dc_y = 0;
  
  float uw, uh, a, b;
  a = width; b = 0;
  canvas->user_to_device_dist(&a, &b);
  uw = sqrt(a*a + b*b);
  int w = (int)ceil(uw);
  
  a = 0; b = height;
  canvas->user_to_device_dist(&a, &b);
  uh = sqrt(a*a + b*b);
  int h = (int)ceil(uh);
  
  if(w == 0 || h == 0)
    return;
  
  canvas->align_point(&x, &y, false);
  
  HDC dc = cairo_win32_surface_get_dc(canvas->target());
  cairo_surface_t *surface = 0;
  
  if(dc){
    cairo_matrix_t ctm;
    cairo_get_matrix(canvas->cairo(), &ctm);
    
    if((ctm.xx == 0) == (ctm.yy == 0) 
    && (ctm.xy == 0) == (ctm.yx == 0)
    && (ctm.xx == 0) != (ctm.xy == 0)
    && rect_in_clip(canvas, x, y, width, height)){
      float ux = x;
      float uy = y;
      canvas->user_to_device(&ux, &uy);
      dc_x = (int)floor(ux + 0.5);
      dc_y = (int)floor(uy + 0.5);
      cairo_surface_flush(cairo_get_target(canvas->cairo()));
    }
    else{
      dc = 0;
    }
  }
  
  if(!dc){
    if(Win32Themes::IsThemeActive
    && Win32Themes::IsThemeActive()
    && Win32Themes::IsCompositionActive /* XP not enough */
    && Win32Themes::OpenThemeData 
    && Win32Themes::CloseThemeData
    && Win32Themes::DrawThemeBackground){
      surface = cairo_win32_surface_create_with_dib(
        CAIRO_FORMAT_ARGB32,
        w, 
        h);
    }
    else{
      surface = cairo_win32_surface_create_with_dib(
        CAIRO_FORMAT_RGB24,
        w, 
        h);
    }
    
    dc = cairo_win32_surface_get_dc(surface);
  }
  
  RECT rect;
  rect.left   = dc_x;
  rect.top    = dc_y;
  rect.right  = dc_x + w;
  rect.bottom = dc_y + h;
  
  if(Win32Themes::OpenThemeData 
  && Win32Themes::CloseThemeData 
  && Win32Themes::DrawThemeBackground){
    int _part, _state;
    HANDLE theme = get_control_theme(type, state, &_part, &_state);
    if(!theme)
      goto FALLBACK;
    
    if(!Win32Themes::IsCompositionActive){ /* XP not enough */
      FillRect(dc, &rect, (HBRUSH)(COLOR_BTNFACE + 1));
    }
    
    Win32Themes::DrawThemeBackground(
      theme,
      dc,
      _part,
      _state,
      &rect,
      0);
  }
  else{ FALLBACK: ;
    UINT _state = 0;
    
    switch(type){
      case FramelessButton:
        break;
        
      case PaletteButton: {
        FillRect(dc, &rect, (HBRUSH)(COLOR_BTNFACE + 1));
        
        if(state == Pressed)
          DrawEdge(dc, &rect, BDR_SUNKENOUTER, BF_RECT);
        else if(state == Hot || state == Hovered)
          DrawEdge(dc, &rect, BDR_RAISEDINNER, BF_RECT);
        
      } break;
        
      case DefaultPushButton: 
      case GenericButton:
      case PushButton: {
        _state|= DFCS_BUTTONPUSH;
        
        switch(state){
          case Disabled: _state|= DFCS_INACTIVE; break;
          case Pressed:  _state|= DFCS_PUSHED;   break;
          case Hot:
          case Hovered:  _state|= DFCS_HOT;      break;
          default: ;
        }
        
        if(type == DefaultPushButton){
          FrameRect(dc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
          
          InflateRect(&rect, -1, -1);
        }
        
        DrawFrameControl(
          dc,
          &rect,
          DFC_BUTTON,
          _state);
      } break;
      
      case InputField: {
        FillRect(dc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));

        DrawEdge(
          dc,
          &rect,
          EDGE_SUNKEN,
          BF_RECT);
      } break;
    }
  }
  
  if(surface){
    cairo_surface_mark_dirty(surface);
    
    canvas->save();
    
    //cairo_reset_clip(canvas->cairo());
    
    cairo_set_source_surface(
      canvas->cairo(), 
      surface,
      x, y);
      
    if(w != width || h != height){
      cairo_matrix_t mat;
      mat.xx = w/width;
      mat.yy = h/height;
      mat.xy = mat.yx = 0;
      mat.x0 = -x * mat.xx;
      mat.y0 = -y * mat.yy;
      
      cairo_pattern_set_matrix(cairo_get_source(canvas->cairo()), &mat);
    }
    
    canvas->paint();
    
    canvas->restore();
    cairo_surface_destroy(surface);
  }
  else{
    cairo_surface_mark_dirty(cairo_get_target(canvas->cairo()));
  }
}

SharedPtr<BoxAnimation> Win32ControlPainter::control_transition(
  int            widget_id,
  Canvas        *canvas,
  ContainerType  type,
  ControlState   state1,
  ControlState   state2,
  float          x,
  float          y,
  float          width,
  float          height
){
  if(!Win32Themes::GetThemeTransitionDuration 
  || widget_id == 0) 
    return 0;
  
//  if(state2 == Pressed/* || state1 == Normal*/)
//    return 0;
  
  bool repeat = false;
  if(type == DefaultPushButton && state1 == Normal && state2 == Normal){
    state2 = Hot;
    repeat = true;
  }
  
  int theme_part, theme_state1, theme_state2;
  HANDLE theme = get_control_theme(type, state1, &theme_part, &theme_state1);
                 get_control_theme(type, state2, &theme_part, &theme_state2);
  if(!theme)
    return 0;
  
  DWORD duration = 0;
  /* TMT_TRANSITIONDURATIONS = 6000 */
  Win32Themes::GetThemeTransitionDuration(
    theme, theme_part, theme_state1, theme_state2, 6000, &duration);
    
  if(duration > 0){
    float x0, y0;
    canvas->current_pos(&x0, &y0);
    
    float x1 = x0;
    float y1 = y;
    float w1 = width;
    float h1 = height;
    
    if(type == InputField){ // bigger buffer for glow rectangle
      x1-= 4.5;
      y1-= 4.5;
      w1+= 9;
      h1+= 9;
    }
    
    SharedPtr<LinearTransition> anim = new LinearTransition(
      widget_id, 
      canvas,
      x1, y1, w1, h1,
      duration / 1000.0);
    
    if(anim->box_id == 0 
    || !anim->buf1 
    || !anim->buf2)
      return 0;
    
    anim->repeat = repeat;
    
    draw_container(
      anim->buf1->canvas(),
      type,
      state1,
      x - x0,
      y - y0,
      width,
      height);
    
    draw_container(
      anim->buf2->canvas(),
      type,
      state2,
      x - x0,
      y - y0,
      width,
      height);
    
    return anim;
  }
  
  return 0;
}

void Win32ControlPainter::container_content_move(
  ContainerType  type,
  ControlState   state,
  float         *x,
  float         *y
){
//  if(Win32Themes::GetThemePosition){
//    int theme_part, theme_state;
//    HANDLE theme = get_control_theme(type, state, &theme_part, &theme_state);
//    
//    POINT off;
//    // TMT_OFFSET	= 3401
//    if(theme
//    && SUCCEEDED(Win32Themes::GetThemePosition(
//        theme, theme_part, theme_state, 3401, &off))
//    ){
//      *x+= 0.75f * off.x;
//      *y+= 0.75f * off.y;
//      return;
//    }
//    
//    if(theme)
//      return;
//  }
  
  ControlPainter::container_content_move(type, state, x, y);
}

bool Win32ControlPainter::container_hover_repaint(ContainerType type){
  return Win32Themes::OpenThemeData 
      && Win32Themes::CloseThemeData 
      && Win32Themes::DrawThemeBackground;
}

void Win32ControlPainter::system_font_style(Style *style){
  NONCLIENTMETRICSW nonclientmetrics;
  LOGFONTW *logfont = 0;
  
  if(Win32Themes::GetThemeSysFont){
    logfont = &nonclientmetrics.lfMessageFont;
    if(!Win32Themes::GetThemeSysFont(NULL, 0x0325/*TMT_MSGBOXFONT*/, logfont))
      logfont = 0;
  }
  
  if(!logfont){
    nonclientmetrics.cbSize = sizeof(nonclientmetrics);
    SystemParametersInfoW(
      SPI_GETNONCLIENTMETRICS, 
      sizeof(nonclientmetrics),
      &nonclientmetrics,
      FALSE);
      
    logfont = &nonclientmetrics.lfMessageFont;
  }
    
  style->set(FontFamily, String::FromUcs2((const uint16_t*)logfont->lfFaceName));
  style->set(FontSize, abs(logfont->lfHeight) * 3/4.f);
  
  if(logfont->lfWeight > FW_NORMAL)
    style->set(FontWeight, FontWeightBold);
  else
    style->set(FontWeight, FontWeightPlain);
  
  style->set(FontSlant, logfont->lfItalic ? FontSlantItalic: FontSlantPlain);
}

int Win32ControlPainter::selection_color(){
  DWORD col = GetSysColor(COLOR_HIGHLIGHT);
  return ((col & 0xFF0000) >> 16)
       |  (col & 0x00FF00)
       | ((col & 0x0000FF) << 16);
}

float Win32ControlPainter::scrollbar_width(){
  return GetSystemMetrics(SM_CXHTHUMB) * 3/4.f;
}

void Win32ControlPainter::paint_scrollbar_part(
  Canvas             *canvas,
  ScrollbarPart       part,
  ScrollbarDirection  dir,
  ControlState        state,
  float               x,
  float               y,
  float               width,
  float               height
){
  if(part == ScrollbarNowhere)
    return;
  
  int dc_x = 0;
  int dc_y = 0;
  
  float uw, uh;
  uw = width;
  uh = height;
  canvas->user_to_device_dist(&uw, &uh);
  uw = fabs(uw);
  uh = fabs(uh);
  int w = (int)ceil(uw);
  int h = (int)ceil(uh);
  
  if(w == 0 || h == 0)
    return;
  
  canvas->align_point(&x, &y, true);
  
  HDC dc = cairo_win32_surface_get_dc(cairo_get_target(canvas->cairo()));
  cairo_surface_t *surface = 0;

  if(dc){
    cairo_matrix_t ctm;
    cairo_get_matrix(canvas->cairo(), &ctm);
    
    if(ctm.xx > 0 && ctm.yy > 0 && ctm.xy == 0 && ctm.yx == 0){
      float ux = x;
      float uy = y;
      canvas->user_to_device(&ux, &uy);
      dc_x = (int)ux;
      dc_y = (int)uy;
      
      cairo_surface_flush(cairo_get_target(canvas->cairo()));
    }
    else
      dc = 0;
  }
  
  if(!dc){
    if(Win32Themes::OpenThemeData 
    && Win32Themes::CloseThemeData 
    && Win32Themes::DrawThemeBackground){
      surface = cairo_win32_surface_create_with_dib(
        CAIRO_FORMAT_ARGB32,
        w, 
        h);
    }
    else{
      surface = cairo_win32_surface_create_with_dib(
        CAIRO_FORMAT_RGB24,
        w, 
        h);
    }
    
    dc = cairo_win32_surface_get_dc(surface);
  }
  
  RECT rect;
  rect.left   = dc_x;
  rect.top    = dc_y;
  rect.right  = dc_x + w;
  rect.bottom = dc_y + h;
  
  if(Win32Themes::OpenThemeData 
  && Win32Themes::CloseThemeData 
  && Win32Themes::DrawThemeBackground){
    if(!scrollbar_theme)
      scrollbar_theme = Win32Themes::OpenThemeData(0, L"SCROLLBAR");
    
    if(!scrollbar_theme)
      goto FALLBACK;
    
    int _part = 0;
    int _state = 0;
    
    switch(part){
      case ScrollbarUpLeft:
      case ScrollbarDownRight: 
        _part = 1; // SBP_ARROWBTN
        break;
      
      case ScrollbarThumb: {
        if(dir == ScrollbarHorizontal) 
          _part = 2; 
        else
          _part = 3;
      } break;
      
      case ScrollbarLowerRange: {
        if(dir == ScrollbarHorizontal) 
          _part = 4; 
        else
          _part = 6;
      } break;
      
      case ScrollbarUpperRange: {
        if(dir == ScrollbarHorizontal) 
          _part = 5; 
        else
          _part = 7;
      } break;
      
      case ScrollbarNowhere:
      case ScrollbarSizeGrip: _part = 10; break;
    }
    
    if(_part == 1){ // arrow button
      switch(state){
        case Disabled: _state = 4; break;
        case Pressed:  _state = 3; break;
        case Hovered:
        case Hot:      _state = 2; break;
        case Normal:   _state = 1; break;
        
        default: ;
      }
      
      if(_state){
        if(dir == ScrollbarHorizontal){
          if(part == ScrollbarDownRight)
            _state+= 12;
          else
            _state+= 8;
        }
        else{
          if(part == ScrollbarDownRight)
            _state+= 4;
        }
      }
      else{
        if(dir == ScrollbarHorizontal){
          if(part == ScrollbarDownRight)
            _state = 20;
          else
            _state = 19;
        }
        else{
          if(part == ScrollbarDownRight)
            _state = 18;
          else
            _state = 17;
        }
      }
    }
    else if(_part == 10){ // size box
      _state = 1;
    }
    else{ // thumbs, ranges
      switch(state){
        case Normal:   _state = 1; break;
        case Hot:      _state = 2; break;
        case Pressed:  _state = 3; break;
        case Disabled: _state = 4; break;
        case Hovered:  _state = 5; break;
      }
    }
    
    Win32Themes::DrawThemeBackground(
      scrollbar_theme,
      dc,
      _part,
      _state,
      &rect,
      0);
    
    if(part == ScrollbarThumb){
      if(dir == ScrollbarHorizontal){
        if(width >= height){
          Win32Themes::DrawThemeBackground(
            scrollbar_theme,
            dc,
            8,
            _state,
            &rect,
            0);
        }
      }
      else if(height >= width){
        if(height >= width){
          Win32Themes::DrawThemeBackground(
            scrollbar_theme,
            dc,
            9,
            _state,
            &rect,
            0);
        }
      }
    }
    
//    Win32Themes::CloseThemeData(theme);
  }
  else{ FALLBACK: ;
    UINT _type = DFC_SCROLL;
    UINT _state = 0;
    
    bool bg = false;
    
    switch(part){
      case ScrollbarUpLeft: {
        if(dir == ScrollbarHorizontal)
          _state = DFCS_SCROLLLEFT;
        else
          _state = DFCS_SCROLLUP;
      } break;
      
      case ScrollbarDownRight: {
        if(dir == ScrollbarHorizontal)
          _state = DFCS_SCROLLRIGHT;
        else
          _state = DFCS_SCROLLDOWN;
      } break;
      
      case ScrollbarSizeGrip: _state = DFCS_SCROLLSIZEGRIP; break;
      
      case ScrollbarThumb:
        _type = DFC_BUTTON;
        _state = DFCS_BUTTONPUSH;
        break;
      
      default: bg = true;
    }
    
    if(!bg){
      switch(state){
        case Pressed:  _state|= DFCS_PUSHED;   break;
        case Hot:
        case Hovered:  _state|= DFCS_HOT;      break;
        case Disabled: _state|= DFCS_INACTIVE; break;
        default: ;
      }
      
      DrawFrameControl(
        dc,
        &rect,
        _type,
        _state);
    }
  }
  
  if(surface){
    cairo_surface_mark_dirty(surface);
    
    canvas->save();
    
    cairo_reset_clip(canvas->cairo());
    
    cairo_set_source_surface(
      canvas->cairo(), 
      surface,
      x, y);
      
    if(w != width || h != height){
      cairo_matrix_t mat;
      mat.xx = w/width;
      mat.yy = h/height;
      mat.xy = mat.yx = 0;
      mat.x0 = -x * mat.xx;
      mat.y0 = -y * mat.yy;
      
      cairo_pattern_set_matrix(cairo_get_source(canvas->cairo()), &mat);
    }
    
    canvas->paint();
    
    canvas->restore();
    cairo_surface_destroy(surface);
  }
  else{
    cairo_surface_mark_dirty(cairo_get_target(canvas->cairo()));
  }
}

void Win32ControlPainter::draw_menubar(HDC dc, RECT *rect){
  if(Win32Themes::OpenThemeData 
  && Win32Themes::CloseThemeData 
  && Win32Themes::DrawThemeBackground){
    HANDLE theme = Win32Themes::OpenThemeData(0, L"MENU");
    if(!theme)
      goto FALLBACK;
    
    Win32Themes::DrawThemeBackground(
      theme,
      dc,
      7,
      0,
      rect,
      0);
    
    Win32Themes::CloseThemeData(theme);
  }
  else{ FALLBACK: ;
    FillRect(dc, rect, GetSysColorBrush(COLOR_BTNFACE));
  }
}

bool Win32ControlPainter::draw_menubar_itembg(HDC dc, RECT *rect, ControlState state){
  if(Win32Themes::OpenThemeData
  && Win32Themes::CloseThemeData 
  && Win32Themes::DrawThemeBackground){
    HANDLE theme = Win32Themes::OpenThemeData(0, L"MENU");
      
    if(!theme)
      goto FALLBACK;
    
    int _state;
    switch(state){
      case Hot:     _state = 2; break;
      case Pressed: _state = 3; break;
      default: _state = 1;
    }
    
    Win32Themes::DrawThemeBackground(
      theme,
      dc,
      8,
      _state,
      rect,
      0);
    
    Win32Themes::CloseThemeData(theme);
    return true;
  }
  
 FALLBACK:
  if(state != Normal){
    FillRect(dc, rect, GetSysColorBrush(COLOR_HIGHLIGHT));
    return false;
  }
  
  return true;
}

HANDLE Win32ControlPainter::get_control_theme(
  ContainerType  type, 
  ControlState   state,
  int           *theme_part,
  int           *theme_state
){
  if(!theme_part){
    static int dummy_part;
    theme_part = &dummy_part;
  }
  
  if(!theme_state){
    static int dummy_state;
    theme_state = &dummy_state;
  }
  
  *theme_part = 0;
  *theme_state = 0;
  if(!Win32Themes::OpenThemeData)
    return 0;
  
  HANDLE theme = 0;
  
  switch(type){
    case PushButton:
    case DefaultPushButton: {
      if(!button_theme)
        button_theme = Win32Themes::OpenThemeData(0, L"BUTTON");
      
      theme = button_theme;
    } break;
      
    case PaletteButton: {
      if(!toolbar_theme)
        toolbar_theme = Win32Themes::OpenThemeData(0, L"TOOLBAR");
      
      theme = toolbar_theme;
//      very_transparent = true;
    } break;
    
    case InputField: {
      if(!edit_theme)
        edit_theme = Win32Themes::OpenThemeData(0, L"EDIT");
      
      theme = edit_theme;
    } break;
    
    default: return 0;
  }
  
  if(!theme)
    return 0;
    
  switch(type){
    case GenericButton:
    case PushButton:
    case DefaultPushButton:
    case PaletteButton: {
      *theme_part = 1;//BP_PUSHBUTTON / TP_BUTTON
       
      switch(state){
        case Disabled: *theme_state = 4; break;
        case Pressed:  *theme_state = 3; break;
        case Hovered:  *theme_state = 2; break; // Hot
        case Hot: {
          if(type == DefaultPushButton){
//            if(Win32Themes::IsThemePartDefined
//            && Win32Themes::IsThemePartDefined(theme, *theme_part, 6))
              *theme_state = 6;
//            else
//              *theme_state = 5;
          }
          else
            *theme_state = 2;
        } break;
        case Normal:
          if(type == DefaultPushButton){
//            if(state == Hot
//            && Win32Themes::IsThemePartDefined
//            && Win32Themes::IsThemePartDefined(theme, *theme_part, 6)){
//              *theme_state = 6;
//            }
//            else
            *theme_state = 5;
          }
          else
            *theme_state = 1;
      }
    } break;
    
    case InputField: {
      *theme_part = 6;//EP_EDITBORDER_NOSCROLL
      
      switch(state){
        case Normal:   *theme_state = 1; break;
        case Hot: 
        case Hovered:  *theme_state = 2; break;
        case Pressed:  *theme_state = 3; break; // = focused
        case Disabled: *theme_state = 4; break;
      }
    } break;
    
    default: break;
  }
  
  return theme;
}

void Win32ControlPainter::clear_cache(){
  if(Win32Themes::CloseThemeData){
    if(button_theme)
      Win32Themes::CloseThemeData(button_theme);
    
    if(edit_theme)
      Win32Themes::CloseThemeData(edit_theme);
    
    if(scrollbar_theme)
      Win32Themes::CloseThemeData(scrollbar_theme);
      
    if(toolbar_theme)
      Win32Themes::CloseThemeData(toolbar_theme);
  }
  
  button_theme = edit_theme = scrollbar_theme = toolbar_theme = 0;
}
