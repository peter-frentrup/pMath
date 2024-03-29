#ifndef RICHMATH__GUI__WIN32__WIN32_CONTROL_PAINTER_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_CONTROL_PAINTER_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <windows.h>

#include <gui/control-painter.h>


namespace richmath {
  class Win32ControlPainter: public ControlPainter {
      class Impl;
    public:
      static Win32ControlPainter win32_painter;
      
      static void done();
      
    public:
      virtual void calc_container_size(
        ControlContext &context,
        Canvas         &canvas,
        ContainerType   type,
        BoxSize        *extents) override;
      
      virtual void calc_container_radii(
        ControlContext &context,
        ContainerType   type,
        BoxRadius      *radii) override;
      
      virtual Color control_font_color(ControlContext &context, ContainerType type, ControlState state) override;
      
      virtual bool is_very_transparent(ControlContext &context, ContainerType type, ControlState state) override;
      
      virtual void draw_container(
        ControlContext &context, 
        Canvas         &canvas,
        ContainerType   type,
        ControlState    state,
        RectangleF      rect) override;
      
      virtual bool enable_animations() override;
      
      virtual SharedPtr<BoxAnimation> control_transition(
        FrontEndReference  widget_id,
        Canvas            &canvas,
        ContainerType      type1,
        ContainerType      type2,
        ControlState       state1,
        ControlState       state2,
        RectangleF         rect) override;
        
      virtual Vector2F container_content_offset(
        ControlContext &control, 
        ContainerType   type,
        ControlState    state) override;
        
      virtual bool container_hover_repaint(ControlContext &context, ContainerType type) override;
      
      virtual bool control_glow_margins(
        ControlContext &control,
        ContainerType   type,
        ControlState    state,
        Margins<float> *outer,
        Margins<float> *inner) override;
      
      virtual void system_font_style(ControlContext &context, StyleData *style) override;
      
      virtual Color selection_color(ControlContext &context) override;
      Color win32_button_face_color(bool dark);
      
      virtual float scrollbar_width() override;
      
      virtual void paint_scrollbar_part(
        ControlContext     &context, 
        Canvas             &canvas,
        ScrollbarPart       part,
        ScrollbarDirection  dir,
        ControlState        state,
        RectangleF          rect) override;
    
    public: // win32 specific
      void draw_menubar(HDC dc, RECT *rect, bool dark_mode = false);
      void draw_menubar_itembg(HDC dc, RECT *rect, ControlState state, bool dark_mode = false);
      
      HANDLE get_control_theme( // do not close the theme
        ControlContext &context, 
        ContainerType   type,
        ControlState    state,
        int            *theme_part,
        int            *theme_state);
        
      void clear_cache();
      
    private:
      Win32ControlPainter();
      virtual ~Win32ControlPainter();
  };
}

#endif // RICHMATH__GUI__WIN32__WIN32_CONTROL_PAINTER_H__INCLUDED
