#ifndef __GUI__GTK__MGTK_CONTROL_PAINTER_H__
#define __GUI__GTK__MGTK_CONTROL_PAINTER_H__

#ifndef RICHMATH_USE_GTK_GUI
#error this header is gtk specific
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
#endif
    
    protected:
      MathGtkControlPainter();
      ~MathGtkControlPainter();
      
  };
}

#endif // __GUI__GTK__MGTK_CONTROL_PAINTER_H__
