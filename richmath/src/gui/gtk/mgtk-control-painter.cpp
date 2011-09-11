#include <gui/gtk/mgtk-control-painter.h>


using namespace richmath;

//{ class MathGtkControlPainter ...

MathGtkControlPainter::MathGtkControlPainter()
: ControlPainter()
{
}

MathGtkControlPainter::~MathGtkControlPainter() {
}

#if GTK_MAJOR_VERSION >= 3

void MathGtkControlPainter::calc_container_size(
  Canvas        *canvas,
  ContainerType  type,
  BoxSize       *extents
) {
  ControlPainter::calc_container_size(canvas, type, extents);
}

int MathGtkControlPainter::control_font_color(ContainerType type, ControlState state) {
  return ControlPainter::control_font_color(type, state);
}

bool MathGtkControlPainter::is_very_transparent(ContainerType type, ControlState state) {
  return ControlPainter::is_very_transparent(type, state);
}

void MathGtkControlPainter::draw_container(
  Canvas        *canvas,
  ContainerType  type,
  ControlState   state,
  float          x,
  float          y,
  float          width,
  float          height
) {
  ControlPainter::draw_container(canvas, type, state, x, y, width, height);
}
  
#endif

//} ... class MathGtkControlPainter
