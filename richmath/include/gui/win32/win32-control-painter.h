#ifndef RICHMATH__GUI__WIN32__WIN32_CONTROL_PAINTER_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_CONTROL_PAINTER_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <windows.h>

#include <gui/control-painter.h>


namespace richmath {
  class Win32ControlPainter: public ControlPainter {
    public:
      static Win32ControlPainter win32_painter;
      
    public:
      virtual void calc_container_size(
        Canvas        *canvas,
        ContainerType  type,
        BoxSize       *extents) override;
        
      virtual int control_font_color(ContainerType type, ControlState state) override;
      
      virtual bool is_very_transparent(ContainerType type, ControlState state) override;
      
      virtual void draw_container(
        Canvas        *canvas,
        ContainerType  type,
        ControlState   state,
        float          x,
        float          y,
        float          width,
        float          height) override;
        
      virtual SharedPtr<BoxAnimation> control_transition(
        FrontEndReference  widget_id,
        Canvas            *canvas,
        ContainerType      type1,
        ContainerType      type2,
        ControlState       state1,
        ControlState       state2,
        float              x,
        float              y,
        float              width,
        float              height) override;
        
      virtual void container_content_move(
        ContainerType  type,
        ControlState   state,
        float         *x,
        float         *y) override;
        
      virtual bool container_hover_repaint(ContainerType type) override;
      
      virtual void system_font_style(Style *style) override;
      
      virtual int selection_color() override;
      
      virtual float scrollbar_width() override;
      
      virtual void paint_scrollbar_part(
        Canvas             *canvas,
        ScrollbarPart       part,
        ScrollbarDirection  dir,
        ControlState        state,
        float               x,
        float               y,
        float               width,
        float               height) override;
        
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
      HANDLE explorer_listview_theme;
      HANDLE tooltip_theme;
      HANDLE progress_theme;
      HANDLE scrollbar_theme;
      HANDLE slider_theme;
      HANDLE tab_theme;
      HANDLE toolbar_theme;
  };
}

#endif // RICHMATH__GUI__WIN32__WIN32_CONTROL_PAINTER_H__INCLUDED
