#ifndef RICHMATH__GUI__GTK__MGTK_CONTROL_PAINTER_H__INCLUDED
#define RICHMATH__GUI__GTK__MGTK_CONTROL_PAINTER_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <gtk/gtk.h>

#include <gui/control-painter.h>


namespace richmath {
  class MathGtkControlPainter: public ControlPainter {
    public:
      static MathGtkControlPainter gtk_painter;
      
      static void done();
      
    public:
#if GTK_MAJOR_VERSION >= 3
      virtual void calc_container_size(
        ControlContext *context,
        Canvas         *canvas,
        ContainerType   type,
        BoxSize        *extents) override;
      
      virtual Color control_font_color(ControlContext *context, ContainerType type, ControlState state) override;
      
      virtual bool is_very_transparent(ControlContext *context, ContainerType type, ControlState state) override;
      
      virtual void draw_container(
        ControlContext *context, 
        Canvas         *canvas,
        ContainerType   type,
        ControlState    state,
        float           x,
        float           y,
        float           width,
        float           height) override;
    
      virtual void container_content_move(
        ControlContext *context, 
        ContainerType   type,
        ControlState    state,
        float          *x,
        float          *y) override;
        
      virtual bool container_hover_repaint(ControlContext *context, ContainerType type) override;
      
      virtual void system_font_style(ControlContext *context, Style *style) override;
      
    public:
      GtkStyleContext *get_control_theme(ControlContext *context, ContainerType type, bool foreground = false);
      GtkStateFlags    get_state_flags(ControlContext *context, ContainerType type, ControlState state);
      
      GtkStyleProvider *current_theme_light();
      GtkStyleProvider *current_theme_dark();
      
      void clear_cache();
      
#endif
    
    protected:
      MathGtkControlPainter();
      ~MathGtkControlPainter();
      
  };
}

#endif // RICHMATH__GUI__GTK__MGTK_CONTROL_PAINTER_H__INCLUDED
