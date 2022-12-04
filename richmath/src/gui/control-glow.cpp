#include <gui/control-glow.h>

#include <boxes/box.h>
#include <graphics/context.h>


using namespace richmath;

//{ class ControlGlowHook ...

bool ControlGlowHook::all_disabled = false;

ControlGlowHook::ControlGlowHook(Box *destination, ContainerType type, ControlState state)
: PaintHook{},
  destination{destination},
  type{type},
  state{state},
  outside{0.0f},
  inside{0.0f}
{
}

void ControlGlowHook::run(Box *box, Context &context) {
  ControlContext *cc = dynamic_cast<ControlContext*>(destination);
  if(!cc)
    return;
  
  if(all_disabled)
    return;
    
  all_disabled = true;
  
  Point p0 = context.canvas().current_pos();
  
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  destination->transformation(box, &mat);
  
  context.canvas().save();
  {
    context.canvas().rel_move_to(mat.x0, mat.y0);
    context.canvas().transform(mat);
    
    RectangleF rect = destination->extents().to_rectangle(p0);
    RectangleF outer = rect + outside;
    outer.pixel_align(context.canvas(), false, 0 /* 1 */);
    outer.add_rect_path(context.canvas(), false);
    
    rect-= inside;
    rect.pixel_align(context.canvas(), false, -1);
    if(rect.is_positive())
      rect.add_rect_path(context.canvas(), true);
    
    context.canvas().clip();
    ControlPainter::std->draw_container(*cc, context.canvas(), type, state, outer);
  }
  context.canvas().restore();
  
  all_disabled = false;
}

//} ... class ControlGlowHook
