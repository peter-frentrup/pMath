#ifndef __GUI__WIN32__WIN32_CONTROL_PAINTER_H__
#define __GUI__WIN32__WIN32_CONTROL_PAINTER_H__

#include <windows.h>

#include <gui/control-painter.h>

namespace richmath{
  class Win32ControlPainter: public ControlPainter {
    public:
      static Win32ControlPainter win32_painter;
    
    public:
      virtual void calc_container_size(
        Canvas        *canvas,
        ContainerType  type,
        BoxSize       *extents);
      
      virtual int control_font_color(ContainerType type, ControlState state);
      
      virtual bool is_very_transparent(ContainerType type, ControlState state);
      
      virtual void draw_container(
        Canvas        *canvas,
        ContainerType  type,
        ControlState   state,
        float          x,
        float          y,
        float          width,
        float          height);
      
      virtual SharedPtr<BoxAnimation> control_transition(
        int                          widget_id,
        Canvas                      *canvas,
        ContainerType                type,
        ControlState                 state1,
        ControlState                 state2,
        float                        x,
        float                        y,
        float                        width,
        float                        height);
      
      virtual void container_content_move(
        ContainerType  type,
        ControlState   state,
        float         *x,
        float         *y);
      
      virtual bool container_hover_repaint(ContainerType type);
      
      virtual void system_font_style(Style *style);
      
      virtual int selection_color();
      
      virtual float scrollbar_width();
      
      virtual void paint_scrollbar_part(
        Canvas             *canvas,
        ScrollbarPart       part,
        ScrollbarDirection  dir,
        ControlState        state,
        float               x,
        float               y,
        float               width,
        float               height);
    
    public: // win32 specific
      bool blur_input_field;
      
    public: // win32 specific
      void draw_menubar(HDC dc, RECT *rect);
      bool draw_menubar_itembg(HDC dc, RECT *rect, ControlState state);
      
      HANDLE get_control_theme( // do not close the theme
        ContainerType  type, 
        ControlState   state,
        int           *theme_part,
        int           *theme_state);
      
      void clear_cache();
      
    private:
      Win32ControlPainter();
      virtual ~Win32ControlPainter();
      
      HANDLE button_theme;
      HANDLE edit_theme;
      HANDLE scrollbar_theme;
      HANDLE toolbar_theme;
  };
}

#endif // __GUI__WIN32__WIN32_CONTROL_PAINTER_H__
