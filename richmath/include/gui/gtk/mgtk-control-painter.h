#ifndef __GUI__GTK__MGTK_CONTROL_PAINTER_H__
#define __GUI__GTK__MGTK_CONTROL_PAINTER_H__

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <gtk/gtk.h>

#include <gui/control-painter.h>


namespace richmath {
  class MathGtkControlPainter: public ControlPainter {
    public:
      static MathGtkControlPainter gtk_painter;
      
    public:
#if GTK_MAJOR_VERSION >= 3
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
    
      virtual void container_content_move(
        ContainerType  type,
        ControlState   state,
        float         *x,
        float         *y) override;
        
      virtual bool container_hover_repaint(ContainerType type) override;
      
      virtual void system_font_style(Style *style) override;
      
    public:
      GtkStyleContext *get_control_theme(ContainerType type);
      GtkStateFlags    get_state_flags(ContainerType type, ControlState state);
      
      void clear_cache();
      
    protected:
      GtkStyleContext *button_context;
      GtkStyleContext *tool_button_context;
      GtkStyleContext *input_field_context;
      GtkStyleContext *slider_context;
      GtkStyleContext *progress_bar_trough_context;
      GtkStyleContext *progress_bar_context;
      GtkStyleContext *checkbox_context;
      GtkStyleContext *radio_button_context;
      
#endif
    
    protected:
      MathGtkControlPainter();
      ~MathGtkControlPainter();
      
  };
}

#endif // __GUI__GTK__MGTK_CONTROL_PAINTER_H__
