#define WINVER 0x0600

#include <gui/win32/win32-control-painter.h>

#include <cmath>
#include <cstdio>
#include <cwchar>

#include <windows.h>

#include <cairo-win32.h>

#include <boxes/box.h>
#include <eval/observable.h>
#include <graphics/context.h>
#include <gui/win32/basic-win32-widget.h>
#include <gui/win32/win32-highdpi.h>
#include <gui/win32/win32-themes.h>
#include <util/array.h>
#include <util/style.h>

#ifndef WM_DWMCOMPOSITIONCHANGED
#define WM_DWMCOMPOSITIONCHANGED   0x031E
#endif

using namespace richmath;

class Win32ControlPainterInfo: public BasicWin32Widget {
  public:
    Win32ControlPainterInfo()
      : BasicWin32Widget(0, 0, 0, 0, 0, 0, nullptr)
    {
      init(); // total exception!!! normally not callable in constructor
    }
    
  protected:
    virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override {
      switch(message) {
        case WM_DWMCOMPOSITIONCHANGED:
        case WM_THEMECHANGED: {
            Win32ControlPainter::win32_painter.clear_cache();
            style_observations.notify_all();
          } break;
      }
      
      return BasicWin32Widget::callback(message, wParam, lParam);
    }
  
  public:
    Observable style_observations;
};

static Win32ControlPainterInfo w32cpinfo;

static class Win32ControlPainterCache {
  public:
    Win32ControlPainterCache() {
    }
    
    ~Win32ControlPainterCache() {
      clear();
    }
  
  public:
    HANDLE button_theme(int dpi) {
      return get_theme_for_dpi(button_theme_for_dpi, L"BUTTON", dpi);
    }
    HANDLE edit_theme(int dpi) {
      return get_theme_for_dpi(edit_theme_for_dpi, L"EDIT", dpi);
    }
    HANDLE explorer_listview_theme(int dpi) {
      return get_theme_for_dpi(explorer_listview_theme_for_dpi, L"Explorer::LISTVIEW;LISTVIEW", dpi);
    }
    HANDLE explorer_treeview_theme(int dpi) {
      return get_theme_for_dpi(explorer_treeview_theme_for_dpi, L"Explorer::TREEVIEW;TREEVIEW", dpi);
    }
    HANDLE tooltip_theme(int dpi) {
      return get_theme_for_dpi(tooltip_theme_for_dpi, L"TOOLTIP", dpi);
    }
    HANDLE progress_theme(int dpi) {
      return get_theme_for_dpi(progress_theme_for_dpi, L"PROGRESS", dpi);
    }
    HANDLE scrollbar_theme(int dpi) {
      return get_theme_for_dpi(scrollbar_theme_for_dpi, L"SCROLLBAR", dpi);
    }
    HANDLE slider_theme(int dpi) {
      return get_theme_for_dpi(slider_theme_for_dpi, L"TRACKBAR", dpi);
    }
    HANDLE tab_theme(int dpi) {
      return get_theme_for_dpi(tab_theme_for_dpi, L"TAB", dpi);
    }
    HANDLE toolbar_theme(int dpi) {
      return get_theme_for_dpi(toolbar_theme_for_dpi, L"TOOLBAR", dpi);
    }
    
    void clear() {
      close_themes(button_theme_for_dpi);
      close_themes(edit_theme_for_dpi);
      close_themes(explorer_listview_theme_for_dpi);
      close_themes(explorer_treeview_theme_for_dpi);
      close_themes(tooltip_theme_for_dpi);
      close_themes(progress_theme_for_dpi);
      close_themes(scrollbar_theme_for_dpi);
      close_themes(slider_theme_for_dpi);
      close_themes(tab_theme_for_dpi);
      close_themes(toolbar_theme_for_dpi);
    }
  
  private:
    static void close_themes(Hashtable<int, HANDLE> &cache) {
      if(Win32Themes::CloseThemeData) {
        for(auto &e : cache.entries())
          Win32Themes::CloseThemeData(e.value);
        
        cache.clear();
      }
    }
    
    static HANDLE get_theme_for_dpi(Hashtable<int, HANDLE> &cache, const wchar_t *name, int dpi) {
      if(HANDLE *h = cache.search(dpi))
        return *h;
      
      if(Win32Themes::OpenThemeDataForDpi) {
        HANDLE h = Win32Themes::OpenThemeDataForDpi(nullptr, name, (UINT)dpi);
        if(h) 
          cache.set(dpi, h);
          
        return h;
      }
      
      if(Win32Themes::OpenThemeData) {
        HANDLE h = Win32Themes::OpenThemeData(nullptr, name);
        if(h) 
          cache.set(dpi, h);
          
        return h;
      }
      
      return nullptr;
    }
  
  private:
    Hashtable<int, HANDLE> button_theme_for_dpi;
    Hashtable<int, HANDLE> edit_theme_for_dpi;
    Hashtable<int, HANDLE> explorer_listview_theme_for_dpi;
    Hashtable<int, HANDLE> explorer_treeview_theme_for_dpi;
    Hashtable<int, HANDLE> tooltip_theme_for_dpi;
    Hashtable<int, HANDLE> progress_theme_for_dpi;
    Hashtable<int, HANDLE> scrollbar_theme_for_dpi;
    Hashtable<int, HANDLE> slider_theme_for_dpi;
    Hashtable<int, HANDLE> tab_theme_for_dpi;
    Hashtable<int, HANDLE> toolbar_theme_for_dpi;
} w32cp_cache;

Win32ControlPainter Win32ControlPainter::win32_painter;

void Win32ControlPainter::done() {
  win32_painter.clear_cache();
  w32cpinfo.style_observations.notify_all(); // to clear the observers array
}

Win32ControlPainter::Win32ControlPainter()
  : ControlPainter(),
    blur_input_field(true)
{
  ControlPainter::std = this;
}

Win32ControlPainter::~Win32ControlPainter() {
  clear_cache();
}

void Win32ControlPainter::calc_container_size(
  ControlContext *context,
  Canvas         *canvas,
  ContainerType   type,
  BoxSize        *extents // in/out
) {
  int theme_part, theme_state;
  HANDLE theme = get_control_theme(context, type, Normal, &theme_part, &theme_state);
  
  switch(type) {
    case InputField: {
        if(Win32Themes::IsThemeActive && Win32Themes::IsThemeActive()) {
          extents->width +=   3;
          extents->ascent +=  1.5;
          extents->descent += 1.5;
          return;
        }
        
        extents->width +=   5.25;
        extents->ascent +=  3;
        extents->descent += 2.25;
      } return;
      
    case ListViewItem:
    case ListViewItemSelected:
      ControlPainter::calc_container_size(context, canvas, type, extents);
      return;
    
    case PanelControl:
      if(theme && Win32Themes::GetThemeMargins) {
        extents->width +=   9.0;
        extents->ascent +=  4.5;
        extents->descent += 4.5;
      }
      break;
      
    case ProgressIndicatorBackground: {
        if(theme && Win32Themes::GetThemePartSize) {
          SIZE size;
          if(SUCCEEDED(Win32Themes::GetThemePartSize(theme, nullptr, theme_part, theme_state, nullptr, Win32Themes::TS_TRUE, &size))) {
            extents->ascent = 0.75 * size.cy;
            extents->descent = 0;
            return;
          }
        }
      } break;
      
    case ProgressIndicatorBar: {
        if(!theme || theme_part != 5) {
          extents->width -=   4.5;
          extents->ascent -=  2.25;
          extents->descent -= 2.25;
        }
      } return;
      
    case SliderHorzChannel: {
        if(theme && Win32Themes::GetThemePartSize) {
          SIZE size;
          if(SUCCEEDED(Win32Themes::GetThemePartSize(theme, nullptr, theme_part, theme_state, nullptr, Win32Themes::TS_TRUE, &size))) {
            extents->ascent = 0.75 * size.cy;
            extents->descent = 0;
            return;
          }
        }
        float dx = 0;
        float dy = 4;
        canvas->device_to_user_dist(&dx, &dy);
        extents->ascent = max(dx, dy);
        extents->descent = 0;
      } return;
      
    case SliderHorzThumb: {
        if(theme && Win32Themes::GetThemePartSize) {
          SIZE size;
          if(SUCCEEDED(Win32Themes::GetThemePartSize(theme, nullptr, theme_part, theme_state, nullptr, Win32Themes::TS_TRUE, &size))) {
            extents->ascent = 0.75 * size.cy;
            extents->descent = 0;
            //extents->width = 0.75 * size.cx;
            extents->width = 0.5 * extents->height();
            return;
          }
        }
      } break;
      
    case TooltipWindow: {
        if(!theme) {
          extents->width +=   6;
          extents->ascent +=  3;
          extents->descent += 3;
          return;
        }
      } break;
      
    default: break;
  }
  
  if(Win32Themes::GetThemeMargins && Win32Themes::GetThemePartSize) {
    SIZE size = {0, 0};
    Win32Themes::MARGINS mar = {0, 0, 0, 0};
    // TMT_SIZINGMARGINS  = 3601
    // TMT_CONTENTMARGINS = 3602
    // TMT_CAPTIONMARGINS = 3603
    if(theme
        && SUCCEEDED(
          Win32Themes::GetThemeMargins(
            theme, nullptr, theme_part, theme_state, 3602, nullptr, &mar))
        && (mar.cxLeftWidth > 0
            || mar.cxRightWidth > 0
            || mar.cyBottomHeight > 0
            || mar.cyTopHeight > 0))
    {
      extents->width +=   0.75f * (mar.cxLeftWidth + mar.cxRightWidth);
      extents->ascent +=  0.75f * mar.cyTopHeight;
      extents->descent += 0.75f * mar.cyBottomHeight;
      
      Win32Themes::GetThemePartSize(
        theme, nullptr, theme_part, theme_state, nullptr, Win32Themes::TS_TRUE, &size);
        
      if(extents->width < 0.75 * size.cx)
        extents->width = 0.75 * size.cx;
        
      if(extents->height() < 0.75 * size.cy) {
        float axis = 0.4 * canvas->get_font_size();
        
        extents->ascent  = size.cy * 0.75 / 2 + axis;
        extents->descent = size.cy * 0.75 - extents->ascent;
      }
      
      return;
    }
  }
  
  ControlPainter::calc_container_size(context, canvas, type, extents);
}

int Win32ControlPainter::control_font_color(ControlContext *context, ContainerType type, ControlState state) {
  if(is_very_transparent(context, type, state))
    return -1;
    
  int theme_part, theme_state;
  HANDLE theme = get_control_theme(context, type, state, &theme_part, &theme_state);
  
  if(theme && Win32Themes::GetThemeColor) {
    COLORREF col = 0;
    
    // TMT_TEXTCOLOR  = 3803
    // TMT_WINDOWTEXT  = 1609
    // TMT_BTNTEXT    = 1619
    if( SUCCEEDED(Win32Themes::GetThemeColor(
                    theme, theme_part, theme_state, 3803, &col)) ||
        SUCCEEDED(Win32Themes::GetThemeColor(
                    theme, theme_part, theme_state, 1609, &col)) ||
        SUCCEEDED(Win32Themes::GetThemeColor(
                    theme, theme_part, theme_state, 1619, &col)))
    {
      return ((col & 0xFF0000) >> 16) |
             ( col & 0x00FF00)        |
             ((col & 0x0000FF) << 16);
    }
    
    return -1;
  }
  
  switch(type) {
    case NoContainerType:
    case FramelessButton:
    case GenericButton:
      return ControlPainter::control_font_color(context, type, state);
      
    case PushButton:
    case DefaultPushButton:
    case PaletteButton:
    case PanelControl: {
        DWORD col = GetSysColor(COLOR_BTNTEXT);
        return ((col & 0xFF0000) >> 16) |
               ( col & 0x00FF00)        |
               ((col & 0x0000FF) << 16);
      } break;
      
    case InputField: {
        DWORD col = GetSysColor(COLOR_WINDOWTEXT);
        return ((col & 0xFF0000) >> 16) |
               ( col & 0x00FF00)        |
               ((col & 0x0000FF) << 16);
      } break;
      
    case ListViewItem: {
        DWORD col = GetSysColor(COLOR_WINDOWTEXT);
        return ((col & 0xFF0000) >> 16) |
               ( col & 0x00FF00)        |
               ((col & 0x0000FF) << 16);
      } break;
      
    case ListViewItemSelected: {
        DWORD col = GetSysColor(COLOR_HIGHLIGHTTEXT);
        return ((col & 0xFF0000) >> 16) |
               ( col & 0x00FF00)        |
               ((col & 0x0000FF) << 16);
      } break;
      
    case TooltipWindow: {
        DWORD col = GetSysColor(COLOR_INFOTEXT);
        return ((col & 0xFF0000) >> 16) |
               ( col & 0x00FF00)        |
               ((col & 0x0000FF) << 16);
      } break;
      
    case SliderHorzChannel:
    case SliderHorzThumb:
    case ProgressIndicatorBackground:
    case ProgressIndicatorBar:
    case CheckboxUnchecked:
    case CheckboxChecked:
    case CheckboxIndeterminate:
    case RadioButtonUnchecked:
    case RadioButtonChecked:
      break;
  }
  
  return -1;
}

bool Win32ControlPainter::is_very_transparent(ControlContext *context, ContainerType type, ControlState state) {
  switch(type) {
    case NoContainerType:
    case FramelessButton:
    case GenericButton:
      return ControlPainter::is_very_transparent(context, type, state);
      
    case PaletteButton: {
        if(!Win32Themes::GetThemeBool)
          return false;
          
        int theme_part, theme_state;
        HANDLE theme = get_control_theme(context, type, state, &theme_part, &theme_state);
        
        // TMT_TRANSPARENT = 2201
        BOOL value;
        if(theme && SUCCEEDED(Win32Themes::GetThemeBool(
                                theme, theme_part, theme_state, 2201, &value))
          ) {
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
) {
  cairo_rectangle_list_t *clip_rects = cairo_copy_clip_rectangle_list(canvas->cairo());
  
  if(clip_rects->status == CAIRO_STATUS_SUCCESS) {
    for(int i = 0; i < clip_rects->num_rectangles; ++i) {
      cairo_rectangle_t *rect = &clip_rects->rectangles[i];
      if(x          >= rect->x
          && y          >= rect->y
          && x + width  <= rect->x + rect->width
          && y + height <= rect->y + rect->height) {
        cairo_rectangle_list_destroy(clip_rects);
        return true;
      }
    }
  }
  
  cairo_rectangle_list_destroy(clip_rects);
  return false;
}

void Win32ControlPainter::draw_container(
  ControlContext *context, 
  Canvas         *canvas,
  ContainerType   type,
  ControlState    state,
  float           x,
  float           y,
  float           width,
  float           height
) {
  if(width <= 0 || height <= 0)
    return;
    
  switch(type) {
    case NoContainerType:
    case FramelessButton:
    case GenericButton:
      ControlPainter::generic_painter.draw_container(
        context, canvas, type, state, x, y, width, height);
      return;
      
    default: break;
  }
  
  if(canvas->pixel_device) {
    canvas->user_to_device_dist(&width, &height);
    width  = floor(width  + 0.5);
    height = floor(height + 0.5);
    canvas->device_to_user_dist(&width, &height);
  }
  
  if( type == InputField           &&
      Win32Themes::IsThemeActive   &&
      Win32Themes::IsThemeActive() &&
      Win32Themes::OpenThemeData   &&
      Win32Themes::CloseThemeData  &&
      Win32Themes::DrawThemeBackground)
  {
    canvas->save();
    int c = canvas->get_color();
    
    if(canvas->glass_background) {
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
      canvas->arc(x1 + r, y1 + r, r,   M_PI,   3 * M_PI / 2, false);
      canvas->arc(x2 - r, y2 + r, r, 3 * M_PI / 2, 2 * M_PI,   false);
      canvas->arc(x3 - r, y3 - r, r,        0,   M_PI / 2, false);
      canvas->arc(x4 + r, y4 - r, r,   M_PI / 2,   M_PI,   false);
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
    else {
      canvas->pixrect(x, y, x + width, y + height, true);
      if(blur_input_field && (state == PressedHovered || state == Pressed)) {
        canvas->set_color(0x8080FF);
        canvas->show_blur_stroke(4, true);
      }
      
      DWORD col = GetSysColor(COLOR_WINDOW);
      col = (  (col & 0xFF0000) >> 16)
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
  uw = sqrt(a * a + b * b);
  int w = (int)ceil(uw);
  
  a = 0; b = height;
  canvas->user_to_device_dist(&a, &b);
  uh = sqrt(a * a + b * b);
  int h = (int)ceil(uh);
  
  if(w == 0 || h == 0)
    return;
    
  canvas->align_point(&x, &y, false);
  
  HDC dc = cairo_win32_surface_get_dc(canvas->target());
  cairo_surface_t *surface = nullptr;
  
  if(dc) {
    cairo_matrix_t ctm = canvas->get_matrix();
    
    if( (ctm.xx == 0) == (ctm.yy == 0) &&
        (ctm.xy == 0) == (ctm.yx == 0) &&
        (ctm.xx == 0) != (ctm.xy == 0) &&
        rect_in_clip(canvas, x, y, width, height))
    {
      float ux = x;
      float uy = y;
      canvas->user_to_device(&ux, &uy);
      dc_x = (int)floor(ux + 0.5);
      dc_y = (int)floor(uy + 0.5);
      cairo_surface_flush(cairo_get_target(canvas->cairo()));
    }
    else {
      dc = 0;
    }
  }
  
  if(!dc) {
    if( Win32Themes::IsThemeActive       &&
        Win32Themes::IsThemeActive()     &&
        Win32Themes::IsCompositionActive && /* XP not enough */
        Win32Themes::OpenThemeData       &&
        Win32Themes::CloseThemeData      &&
        Win32Themes::DrawThemeBackground)
    {
      surface = cairo_win32_surface_create_with_dib(
                  CAIRO_FORMAT_ARGB32,
                  w,
                  h);
    }
    else {
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
      && Win32Themes::DrawThemeBackground) {
      
    bool two_times = false;
    if(canvas->glass_background && type == PaletteButton) {
      if(state == Normal)
        state = Hovered;
      else if(state == Hovered)
        two_times = true;
    }
    
    int _part, _state;
    HANDLE theme = get_control_theme(context, type, state, &_part, &_state);
    if(!theme)
      goto FALLBACK;
      
    if(!Win32Themes::IsCompositionActive) { /* XP not enough */
      FillRect(dc, &rect, (HBRUSH)(COLOR_BTNFACE + 1));
    }
    
    Win32Themes::DrawThemeBackground(
      theme,
      dc,
      _part,
      _state,
      &rect,
      0);
      
    if(two_times) {
      Win32Themes::DrawThemeBackground(
        theme,
        dc,
        _part,
        _state,
        &rect,
        0);
      Win32Themes::DrawThemeBackground(
        theme,
        dc,
        _part,
        _state,
        &rect,
        0);
    }
  }
  else {
  FALLBACK: ;
    switch(type) {
      case NoContainerType:
      case FramelessButton:
        break;
        
      case PaletteButton: {
          FillRect(dc, &rect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          if(state == PressedHovered)
            DrawEdge(dc, &rect, BDR_SUNKENOUTER, BF_RECT);
          else if(state == Hovered)
            DrawEdge(dc, &rect, BDR_RAISEDINNER, BF_RECT);
            
        } break;
        
      case DefaultPushButton:
      case GenericButton:
      case PushButton: {
          if(type == DefaultPushButton) {
            FrameRect(dc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            
            InflateRect(&rect, -1, -1);
          }
          
          FillRect(dc, &rect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          if(state == PressedHovered) {
            DrawEdge(dc, &rect, EDGE_SUNKEN, BF_RECT);
          }
          else {
            DrawEdge(dc, &rect, EDGE_RAISED, BF_RECT);
          }
          
//        DrawFrameControl(
//          dc,
//          &rect,
//          DFC_BUTTON,
//          _state);
        } break;
        
      case InputField: {
          FillRect(dc, &rect, (HBRUSH)(COLOR_WINDOW + 1));
          
          DrawEdge(
            dc,
            &rect,
            EDGE_SUNKEN,
            BF_RECT);
        } break;
        
      case TooltipWindow: {
          FrameRect(dc, &rect, GetSysColorBrush(COLOR_WINDOWFRAME));
          
          InflateRect(&rect, -1, -1);
          
          FillRect(dc, &rect, (HBRUSH)(COLOR_INFOBK + 1));
        } break;
        
      case ListViewItem:
        FillRect(dc, &rect, (HBRUSH)(COLOR_WINDOW + 1));
        break;
        
      case ListViewItemSelected:
        FillRect(dc, &rect, (HBRUSH)(COLOR_HIGHLIGHT + 1));
        break;
      
      case PanelControl:
        FillRect(dc, &rect, (HBRUSH)(COLOR_BTNFACE + 1));
        DrawEdge(dc, &rect, BDR_RAISEDOUTER, BF_RECT);
        break;
        
      case ProgressIndicatorBackground:
      case SliderHorzChannel: {
          FillRect(dc, &rect, (HBRUSH)(COLOR_BTNFACE + 1));
          
          DrawEdge(
            dc,
            &rect,
            EDGE_SUNKEN,
            BF_RECT);
        } break;
        
      case ProgressIndicatorBar: {
          int chunk = (rect.bottom - rect.top) / 2;
          RECT chunk_rect = rect;
          
          if(chunk < 2)
            chunk = 2;
            
          int x;
          for(x = rect.left; x + chunk <= rect.right; x += chunk + 2) {
            chunk_rect.left = x;
            chunk_rect.right = x + chunk;
            
            FillRect(dc, &chunk_rect, (HBRUSH)(COLOR_HIGHLIGHT + 1));
          }
          
          if(x < rect.right) {
            chunk_rect.left = x;
            chunk_rect.right = rect.right;
            
            FillRect(dc, &chunk_rect, (HBRUSH)(COLOR_HIGHLIGHT + 1));
          }
        } break;
        
      case SliderHorzThumb: {
          DrawFrameControl(
            dc,
            &rect,
            DFC_BUTTON,
            DFCS_BUTTONPUSH);
        } break;
        
      case CheckboxUnchecked:
      case CheckboxChecked:
      case CheckboxIndeterminate: {
          UINT _state = DFCS_BUTTONCHECK;
          
          if(type == CheckboxChecked)       _state |= DFCS_CHECKED;
          if(type == CheckboxIndeterminate) _state = DFCS_BUTTON3STATE | DFCS_CHECKED;
          
          switch(state) {
            case Disabled:       _state |= DFCS_INACTIVE; break;
            case PressedHovered: _state |= DFCS_PUSHED;   break;
            default: break;
          }
          
          DrawFrameControl(
            dc,
            &rect,
            DFC_BUTTON,
            _state);
        } break;
        
      case RadioButtonUnchecked:
      case RadioButtonChecked: {
          UINT _state = DFCS_BUTTONRADIO;
          
          if(type == RadioButtonChecked)    _state |= DFCS_CHECKED;
          
          switch(state) {
            case Disabled:       _state |= DFCS_INACTIVE; break;
            case PressedHovered: _state |= DFCS_PUSHED;   break;
            default: break;
          }
          
          if(rect.right - rect.left > 0) {
            double w = (rect.right - rect.left) * 0.75;
            double c = (rect.right + rect.left) * 0.5;
            rect.left  = (int)(c - w / 2);
            rect.right = (int)(c + w / 2);
          }
          
          if(rect.bottom - rect.top > 12) {
            double w = (rect.bottom - rect.top) * 0.75;
            double c = (rect.bottom + rect.top) * 0.5;
            rect.top    = (int)(c - w / 2);
            rect.bottom = (int)(c + w / 2);
          }
          
          DrawFrameControl(
            dc,
            &rect,
            DFC_BUTTON,
            _state);
        } break;
      
      case OpenerTriangleClosed:
      case OpenerTriangleOpened:
        ControlPainter::draw_container(context, canvas, type, state, x, y, width, height);
        return;
    }
  }
  
  if(surface) {
    cairo_surface_mark_dirty(surface);
    
    canvas->save();
    
    //cairo_reset_clip(canvas->cairo());
    
    cairo_set_source_surface(
      canvas->cairo(),
      surface,
      x, y);
      
    if(w != width || h != height) {
      cairo_matrix_t mat;
      mat.xx = w / width;
      mat.yy = h / height;
      mat.xy = mat.yx = 0;
      mat.x0 = -x * mat.xx;
      mat.y0 = -y * mat.yy;
      
      cairo_pattern_set_matrix(cairo_get_source(canvas->cairo()), &mat);
    }
    
    canvas->paint();
    
    canvas->restore();
    cairo_surface_destroy(surface);
  }
  else {
    cairo_surface_mark_dirty(cairo_get_target(canvas->cairo()));
  }
}

SharedPtr<BoxAnimation> Win32ControlPainter::control_transition(
  FrontEndReference  widget_id,
  Canvas            *canvas,
  ContainerType      type1,
  ContainerType      type2,
  ControlState       state1,
  ControlState       state2,
  float              x,
  float              y,
  float              width,
  float              height
) {
  if(!Win32Themes::GetThemeTransitionDuration || !widget_id.is_valid())
    return nullptr;
  
  ControlContext *context = ControlContext::find(FrontEndObject::find_cast<Box>(widget_id));
  
  bool repeat = false;
  if(type2 == DefaultPushButton && state1 == Normal && state2 == Normal) {
    if(context->is_foreground_window()) {
      state2 = Hot;
      repeat = true;
    }
  }
  
  if( state1 == Normal                      &&
      (state2 == Hot || state2 == Hovered)  &&
      type2 == PaletteButton)
  {
    return nullptr;
  }
  
  if(type1 == type2 && state1 == state2)
    return nullptr;
  
  int theme_part, theme_state1, theme_state2;
  HANDLE theme = get_control_theme(context, type1, state1, &theme_part, &theme_state1);
  get_control_theme(context, type2, state2, &theme_part, &theme_state2);
  if(!theme)
    return nullptr;
  
  if(theme_state1 == theme_state2)
    return nullptr;
    
  if( type2 == PushButton        ||
      type2 == DefaultPushButton ||
      type2 == PaletteButton)
  {
    if(state2 == PressedHovered/* || state1 == Normal*/)
      return nullptr;
  }
  
  DWORD duration = 0;
  /* TMT_TRANSITIONDURATIONS = 6000 */
  Win32Themes::GetThemeTransitionDuration(
    theme, theme_part, theme_state1, theme_state2, 6000, &duration);
    
  if(duration > 0) {
    float x0, y0;
    canvas->current_pos(&x0, &y0);
    
    float x1 = x0;
    float y1 = y;
    float w1 = width;
    float h1 = height;
    
    if(type2 == InputField) { // bigger buffer for glow rectangle
      x1 -= 4.5;
      y1 -= 4.5;
      w1 += 9;
      h1 += 9;
    }
    
    SharedPtr<LinearTransition> anim = new LinearTransition(
      widget_id,
      canvas,
      x1, y1, w1, h1,
      duration / 1000.0);
      
    if( !anim->box_id.is_valid() || 
        !anim->buf1       || 
        !anim->buf2)
    {
      return 0;
    }
    
    anim->repeat = repeat;
    
    draw_container(
      context,
      anim->buf1->canvas(),
      type1,
      state1,
      x - x0,
      y - y0,
      width,
      height);
      
    draw_container(
      context,
      anim->buf2->canvas(),
      type2,
      state2,
      x - x0,
      y - y0,
      width,
      height);
      
    return anim;
  }
  
  return nullptr;
}

void Win32ControlPainter::container_content_move(
  ContainerType  type,
  ControlState   state,
  float         *x,
  float         *y
) {
//  if(Win32Themes::GetThemePosition){
//    int theme_part, theme_state;
//    HANDLE theme = get_control_theme(???, type, state, &theme_part, &theme_state);
//
//    POINT off;
//    // TMT_OFFSET  = 3401
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

bool Win32ControlPainter::container_hover_repaint(ControlContext *context, ContainerType type) {
  switch(type) {
    case NoContainerType:
    case GenericButton:
    case TooltipWindow:
    case PanelControl:
      return false;
  }
  
  return Win32Themes::OpenThemeData && 
         Win32Themes::CloseThemeData && 
         Win32Themes::DrawThemeBackground;
}

void Win32ControlPainter::system_font_style(ControlContext *context, Style *style) {
  NONCLIENTMETRICSW nonclientmetrics;
  LOGFONTW *logfont = nullptr;
  
  float dpi_scale = 1.0f;
  
  if(Win32Themes::GetThemeSysFont) {
    logfont = &nonclientmetrics.lfMessageFont;
    if(!Win32Themes::GetThemeSysFont(nullptr, 0x0325/*TMT_MSGBOXFONT*/, logfont))
      logfont = nullptr;
    
    if(logfont) {
      // GetThemeSysFont gets metrics in dots per inch for the current logical screen
        
      HDC hdc = GetDC(nullptr);
      dpi_scale = GetDeviceCaps(hdc, LOGPIXELSX) / (float)context->dpi();
      ReleaseDC(nullptr, hdc);
    }
  }
  
  if(!logfont) {
    nonclientmetrics.cbSize = sizeof(nonclientmetrics);
    Win32HighDpi::get_nonclient_metrics_for_dpi(&nonclientmetrics, context->dpi());
    dpi_scale = 1.0f;
    logfont = &nonclientmetrics.lfMessageFont;
  }
  
  style->set(FontFamilies, String::FromUcs2((const uint16_t *)logfont->lfFaceName));
  style->set(FontSize, abs(logfont->lfHeight) * dpi_scale * 3 / 4.f);
  
  if(logfont->lfWeight > FW_NORMAL)
    style->set(FontWeight, FontWeightBold);
  else
    style->set(FontWeight, FontWeightPlain);
    
  style->set(FontSlant, logfont->lfItalic ? FontSlantItalic : FontSlantPlain);
}

int Win32ControlPainter::selection_color(ControlContext *context) {
  DWORD col = GetSysColor(COLOR_HIGHLIGHT);
  return (  (col & 0xFF0000) >> 16)
         |  (col & 0x00FF00)
         | ((col & 0x0000FF) << 16);
}

float Win32ControlPainter::scrollbar_width() {
  return GetSystemMetrics(SM_CXHTHUMB) * 3 / 4.f;
}

void Win32ControlPainter::paint_scrollbar_part(
  ControlContext     *context, 
  Canvas             *canvas,
  ScrollbarPart       part,
  ScrollbarDirection  dir,
  ControlState        state,
  float               x,
  float               y,
  float               width,
  float               height
) {
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
  cairo_surface_t *surface = nullptr;
  
  if(dc) {
    cairo_matrix_t ctm = canvas->get_matrix();
    
    if(ctm.xx > 0 && ctm.yy > 0 && ctm.xy == 0 && ctm.yx == 0) {
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
  
  if(!dc) {
    if( Win32Themes::OpenThemeData && 
        Win32Themes::CloseThemeData && 
        Win32Themes::DrawThemeBackground
    ) {
      surface = cairo_win32_surface_create_with_dib(
                  CAIRO_FORMAT_ARGB32,
                  w,
                  h);
    }
    else {
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
  
  if( Win32Themes::OpenThemeData  &&
      Win32Themes::CloseThemeData &&
      Win32Themes::DrawThemeBackground)
  {
    HANDLE scrollbar_theme = w32cp_cache.scrollbar_theme(context->dpi());
    if(!scrollbar_theme)
      goto FALLBACK;
      
    int _part = 0;
    int _state = 0;
    
    switch(part) {
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
    
    if(_part == 1) { // arrow button
      switch(state) {
        case Disabled:        _state = 4; break;
        case PressedHovered:  _state = 3; break;
        case Hovered:         _state = 2; break;
        case Hot:
        case Pressed:
        case Normal:          _state = 1; break;
        
        default: ;
      }
      
      if(_state) {
        if(dir == ScrollbarHorizontal) {
          if(part == ScrollbarDownRight)
            _state += 12;
          else
            _state += 8;
        }
        else {
          if(part == ScrollbarDownRight)
            _state += 4;
        }
      }
      else {
        if(dir == ScrollbarHorizontal) {
          if(part == ScrollbarDownRight)
            _state = 20;
          else
            _state = 19;
        }
        else {
          if(part == ScrollbarDownRight)
            _state = 18;
          else
            _state = 17;
        }
      }
    }
    else if(_part == 10) { // size box
      _state = 1;
    }
    else { // thumbs, ranges
      switch(state) {
        case Pressed:
        case Normal:         _state = 1; break;
        case Hot:            _state = 2; break;
        case PressedHovered: _state = 3; break;
        case Disabled:       _state = 4; break;
        case Hovered:        _state = 5; break;
      }
    }
    
    Win32Themes::DrawThemeBackground(
      scrollbar_theme,
      dc,
      _part,
      _state,
      &rect,
      0);
      
    if(part == ScrollbarThumb) {
      if(dir == ScrollbarHorizontal) {
        if(width >= height) {
          Win32Themes::DrawThemeBackground(
            scrollbar_theme,
            dc,
            8,
            _state,
            &rect,
            0);
        }
      }
      else if(height >= width) {
        if(height >= width) {
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
  else {
  FALLBACK: ;
    UINT _type = DFC_SCROLL;
    UINT _state = 0;
    
    bool bg = false;
    
    switch(part) {
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
    
    if(!bg) {
      switch(state) {
        case PressedHovered:  _state |= DFCS_PUSHED;   break;
        case Hot:
        case Hovered:         _state |= DFCS_HOT;      break;
        case Disabled:        _state |= DFCS_INACTIVE; break;
        default: break;
      }
      
      DrawFrameControl(
        dc,
        &rect,
        _type,
        _state);
    }
  }
  
  if(surface) {
    cairo_surface_mark_dirty(surface);
    
    canvas->save();
    
    cairo_reset_clip(canvas->cairo());
    
    cairo_set_source_surface(
      canvas->cairo(),
      surface,
      x, y);
      
    if(w != width || h != height) {
      cairo_matrix_t mat;
      mat.xx = w / width;
      mat.yy = h / height;
      mat.xy = mat.yx = 0;
      mat.x0 = -x * mat.xx;
      mat.y0 = -y * mat.yy;
      
      cairo_pattern_set_matrix(cairo_get_source(canvas->cairo()), &mat);
    }
    
    canvas->paint();
    
    canvas->restore();
    cairo_surface_destroy(surface);
  }
  else {
    cairo_surface_mark_dirty(cairo_get_target(canvas->cairo()));
  }
}

void Win32ControlPainter::draw_menubar(HDC dc, RECT *rect) {
  if( Win32Themes::OpenThemeData  &&
      Win32Themes::CloseThemeData &&
      Win32Themes::DrawThemeBackground)
  {
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
  else {
  FALLBACK: 
    FillRect(dc, rect, GetSysColorBrush(COLOR_BTNFACE));
  }
}

void Win32ControlPainter::draw_menubar_itembg(HDC dc, RECT *rect, ControlState state) {
  if( Win32Themes::OpenThemeData  && 
      Win32Themes::CloseThemeData && 
      Win32Themes::DrawThemeBackground) 
  {
    HANDLE theme = Win32Themes::OpenThemeData(0, L"MENU");
    
    if(!theme)
      goto FALLBACK;
      
    int _state;
    switch(state) {
      case Hovered: _state = 2; break;
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
    return;
  }
  
FALLBACK:
  if(state != Normal) {
    UINT edge;
    switch(state) {
      case Hovered: edge = BDR_RAISEDINNER; break;
      case Pressed: edge = BDR_SUNKENOUTER; break;
      default:      edge = 0; break;
    }
    RECT edge_rect = *rect;
    edge_rect.bottom-= 1;
    edge_rect.right-= 1;
    DrawEdge(dc, &edge_rect, edge, BF_RECT);
  }
}

HANDLE Win32ControlPainter::get_control_theme(
  ControlContext *context, 
  ContainerType   type,
  ControlState    state,
  int            *theme_part,
  int            *theme_state
) {
  w32cpinfo.style_observations.register_observer();

  if(!theme_part) {
    static int dummy_part;
    theme_part = &dummy_part;
  }
  
  if(!theme_state) {
    static int dummy_state;
    theme_state = &dummy_state;
  }
  
  *theme_part = 0;
  *theme_state = 0;
  if(!Win32Themes::OpenThemeData)
    return 0;
    
  HANDLE theme = nullptr;
  
  switch(type) {
    case PushButton:
    case DefaultPushButton:
    case CheckboxUnchecked:
    case CheckboxChecked:
    case CheckboxIndeterminate:
    case RadioButtonUnchecked:
    case RadioButtonChecked: 
      theme = w32cp_cache.button_theme(context->dpi());
      break;
      
    case PaletteButton: 
      theme = w32cp_cache.toolbar_theme(context->dpi());
      break;
      
    case InputField: 
      theme = w32cp_cache.edit_theme(context->dpi());
      break;
      
    case ListViewItem:
    case ListViewItemSelected:
      theme = w32cp_cache.explorer_listview_theme(context->dpi());
      break;
    
    case OpenerTriangleClosed:
    case OpenerTriangleOpened:
      theme = w32cp_cache.explorer_treeview_theme(context->dpi());
      break;
      
    case PanelControl: 
      theme = w32cp_cache.tab_theme(context->dpi());
      break;
    
    case ProgressIndicatorBackground:
    case ProgressIndicatorBar:
      theme = w32cp_cache.progress_theme(context->dpi());
      break;
    
    case SliderHorzChannel:
    case SliderHorzThumb: 
      theme = w32cp_cache.slider_theme(context->dpi());
      break;
      
    case TooltipWindow: 
      theme = w32cp_cache.tooltip_theme(context->dpi());
      break;
      
    default: return nullptr;
  }
  
  if(!theme)
    return nullptr;
    
  switch(type) {
    case GenericButton:
    case PushButton:
    case DefaultPushButton:
    case PaletteButton: {
        *theme_part = 1;//BP_PUSHBUTTON / TP_BUTTON
        
        switch(state) {
          case Disabled:        *theme_state = 4; break;
          case PressedHovered:  *theme_state = 3; break;
          case Hovered:         *theme_state = 2; break;
          case Hot: {
              if(type == DefaultPushButton) {
                *theme_state = 6;
              }
              else
                *theme_state = 2;
            } break;
            
          case Pressed:
          case Normal:
            if(type == DefaultPushButton) {
              *theme_state = 5;
            }
            else
              *theme_state = 1;
        }
      } break;
      
    case InputField: {
        *theme_part = 6;//EP_EDITBORDER_NOSCROLL
        
        switch(state) {
          case Normal:         *theme_state = 1; break;
          case Hot:
          case Hovered:        *theme_state = 2; break;
          case Pressed:
          case PressedHovered: *theme_state = 3; break; // = focused
          case Disabled:       *theme_state = 4; break;
        }
      } break;
      
    case ListViewItem: {
        *theme_part = 1;//LVP_LISTITEM
        
        switch(state) {
          case Normal:          theme = nullptr; break;
          case Hot:
          case Hovered:        *theme_state = 2; break;
          case Pressed:
          case PressedHovered: *theme_state = 3; break;
          case Disabled:       *theme_state = 4; break;
        }
      } break;
      
    case ListViewItemSelected: {
        *theme_part = 1;//LVP_LISTITEM
        
        switch(state) {
          case Pressed:
          case Normal:         *theme_state = 3; break;
          case Hot:
          case Hovered:        *theme_state = 6; break;
          case PressedHovered: *theme_state = 6; break;
          case Disabled:       *theme_state = 5; break;
        }
      } break;
      
    case TooltipWindow: {
        *theme_part  = 1; // TTP_STANDARD
        *theme_state = 1;
      } break;
    
    case PanelControl: {
        *theme_part  = 9; // TABP_PANE
        *theme_state = 0;
      } break;
    
    case ProgressIndicatorBackground: {
        if(Win32Themes::IsThemePartDefined(theme, 11, 0))
          *theme_part = 11; // PP_TRANSPARENTBAR
        else
          *theme_part = 1;  // PP_BAR
          
        *theme_state = 1;
      } break;
      
    case ProgressIndicatorBar: {
        if(Win32Themes::IsThemePartDefined(theme, 5, 0))
          *theme_part = 5; // PP_FILL
        else
          *theme_part = 3; // PP_CHUNK
          
        *theme_state = 1;
      } break;
      
    case SliderHorzChannel: {
        *theme_part  = 1; // TKP_TRACK
        *theme_state = 1;
      } break;
      
    case SliderHorzThumb: {
        *theme_part = 3; // TKP_THUMB
        switch(state) {
          case Normal:         *theme_state = 1; break;
          case Pressed:
          case Hot:
          case Hovered:        *theme_state = 2; break;
          case PressedHovered: *theme_state = 3; break;
          case Disabled:       *theme_state = 4; break;
        }
      } break;
      
    case CheckboxUnchecked: {
        *theme_part = 3; // BP_CHECKBOX
        switch(state) {
          case Normal:         *theme_state = 1; break;
          case Pressed:
          case Hot:
          case Hovered:        *theme_state = 2; break;
          case PressedHovered: *theme_state = 3; break;
          case Disabled:       *theme_state = 4; break;
        }
      } break;
      
    case CheckboxChecked: {
        *theme_part = 3; // BP_CHECKBOX
        switch(state) {
          case Normal:         *theme_state = 5; break;
          case Pressed:
          case Hot:
          case Hovered:        *theme_state = 6; break;
          case PressedHovered: *theme_state = 7; break;
          case Disabled:       *theme_state = 8; break;
        }
      } break;
      
    case CheckboxIndeterminate: {
        *theme_part = 3; // BP_CHECKBOX
        switch(state) {
          case Normal:         *theme_state = 9; break;
          case Pressed:
          case Hot:
          case Hovered:        *theme_state = 10; break;
          case PressedHovered: *theme_state = 11; break;
          case Disabled:       *theme_state = 12; break;
        }
      } break;
      
    case RadioButtonUnchecked: {
        *theme_part = 2; // BP_RADIOBUTTON
        switch(state) {
          case Normal:         *theme_state = 1; break;
          case Pressed:
          case Hot:
          case Hovered:        *theme_state = 2; break;
          case PressedHovered: *theme_state = 3; break;
          case Disabled:       *theme_state = 4; break;
        }
      } break;
      
    case RadioButtonChecked: {
        *theme_part = 2; // BP_RADIOBUTTON
        switch(state) {
          case Normal:         *theme_state = 5; break;
          case Pressed:
          case Hot:
          case Hovered:        *theme_state = 6; break;
          case PressedHovered: *theme_state = 7; break;
          case Disabled:       *theme_state = 8; break;
        }
      } break;
    
    case OpenerTriangleClosed: {
        *theme_state = 1; // GLPS_CLOSED / HGLPS_CLOSED
        switch(state) {
          case Disabled:
          case Normal:         *theme_part = 2; break; // TVP_GLYPH
          case Pressed:
          case Hot:
          case Hovered:        
          case PressedHovered: *theme_part = 4; break; // TVP_HOTGLYPH
        }
      } break;
      
    case OpenerTriangleOpened: {
        *theme_state = 2; // GLPS_OPENED / HGLPS_OPENED
        switch(state) {
          case Disabled:
          case Normal:         *theme_part = 2; break; // TVP_GLYPH
          case Pressed:
          case Hot:
          case Hovered:        
          case PressedHovered: *theme_part = 4; break; // TVP_HOTGLYPH
        }
      } break;
      
    default: break;
  }
  
  return theme;
}

void Win32ControlPainter::clear_cache() {
  w32cp_cache.clear();
}
