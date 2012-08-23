#include <boxes/containerwidgetbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;

//{ class ContainerWidgetBox ...

ContainerWidgetBox::ContainerWidgetBox(ContainerType _type, MathSequence *content)
  : AbstractStyleBox(content),
    type(_type),
    old_state(Normal),
    mouse_inside(false),
    mouse_left_down(false),
    mouse_middle_down(false),
    mouse_right_down(false),
    selection_inside(false)
{
  if(!style)
    style = new Style();
    
  style->set(BaseStyleName, "ControlStyle");
}

ControlState ContainerWidgetBox::calc_state(Context *context) {
  if(context->selection.id == id() && context->active)
    return Pressed;
    
  if(context->clicked_box_id == id() && mouse_left_down) {
    if(mouse_inside)
      return PressedHovered;
      
    return Pressed;
  }
  
  Box *mo = FrontEndObject::find_cast<Box>(context->mouseover_box_id);
  if(mo && mo->mouse_sensitive() == this)
    return Hovered;
    
  return Normal;
}

void ContainerWidgetBox::resize(Context *context) {
  AbstractStyleBox::resize(context);
  
  ControlPainter::std->calc_container_size(
    context->canvas,
    type,
    &_extents);
    
  cx = (_extents.width - _content->extents().width) / 2;
}

void ContainerWidgetBox::paint(Context *context) {
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  Rectangle rect = _extents.to_rectangle(Point(x, y));
  ControlState state = calc_state(context);
  
  if(state != old_state || !animation) {
    animation = ControlPainter::std->control_transition(
                  id(),
                  context->canvas,
                  type,
                  type,
                  old_state,
                  state,
                  rect.x,
                  rect.y,
                  rect.width,
                  rect.height);
                  
    old_state = state;
  }
  
  bool need_bg = true;
  if(animation) {
    if(animation->paint(context->canvas)) {
      need_bg = false;
    }
    else {
      animation = 0;
      
      animation = ControlPainter::std->control_transition(
                    id(),
                    context->canvas,
                    type,
                    type,
                    old_state,
                    old_state,
                    rect.x,
                    rect.y,
                    rect.width,
                    rect.height);
    }
  }
  
  bool very_transparent = ControlPainter::std->is_very_transparent(type, state);
  
  if(need_bg) {
    ControlPainter::std->draw_container(
      context->canvas,
      type,
      state,
      rect.x,
      rect.y,
      rect.width,
      rect.height);
  }
  
  float x2 = x;
  float y2 = y;
  ControlPainter::std->container_content_move(
    type, state, &x2, &y2);
    
  context->canvas->move_to(x2, y2);
  
  int old_cursor_color = context->cursor_color;
  int old_color        = context->canvas->get_color();
  int c = ControlPainter::std->control_font_color(type, state);
  if(c >= 0) {
    context->canvas->set_color(c);
    context->cursor_color = c;
  }
  
  if(very_transparent || !context->canvas->show_only_text)
    AbstractStyleBox::paint(context);
    
  if(type == FramelessButton && state == PressedHovered) {
    context->canvas->save();
    {
    /* Workaround for Cairo/win32 1.10.0 bug (fixed in 1.12.0?):
       Fill with CAIRO_OPERATOR_DIFFERENCE crshes the simple rectangle fast path
     */
      rect.add_round_rect_path(*context->canvas, BoxRadius(0.75));
      //rect.add_rect_path(*context->canvas);
      
      cairo_set_operator(context->canvas->cairo(), CAIRO_OPERATOR_DIFFERENCE);
      context->canvas->set_color(0xffffff);
      context->canvas->fill();
    }
    context->canvas->restore();
  }
  
  context->canvas->set_color(old_color);
  context->cursor_color = old_cursor_color;
}

void ContainerWidgetBox::on_mouse_enter() {
  if(!mouse_inside && ControlPainter::std->container_hover_repaint(type))
    request_repaint_all();
    
  mouse_inside = true;
}

void ContainerWidgetBox::on_mouse_exit() {
  if(/*mouse_inside && */ControlPainter::std->container_hover_repaint(type))
    request_repaint_all();
    
  mouse_inside = false;
}

void ContainerWidgetBox::on_mouse_down(MouseEvent &event) {
  event.set_source(this);
  
  mouse_left_down   = mouse_left_down   || event.left;
  mouse_middle_down = mouse_middle_down || event.middle;
  mouse_right_down  = mouse_right_down  || event.right;
  mouse_inside = event.x >= 0 &&
                 event.x <= _extents.width &&
                 event.y >= -_extents.ascent &&
                 event.y <= _extents.descent;
                 
  request_repaint_all();
}

void ContainerWidgetBox::on_mouse_move(MouseEvent &event) {
  event.set_source(this);
  
  bool mi = event.x >= 0 &&
            event.x <= _extents.width &&
            event.y >= -_extents.ascent &&
            event.y <= _extents.descent;
            
  if(mi != mouse_inside)
    request_repaint_all();
    
  mouse_inside = mi;
}

void ContainerWidgetBox::on_mouse_up(MouseEvent &event) {
  request_repaint_all();
  
  mouse_left_down   = mouse_left_down   && !event.left;
  mouse_middle_down = mouse_middle_down && !event.middle;
  mouse_right_down  = mouse_right_down  && !event.right;
}

void ContainerWidgetBox::on_mouse_cancel() {
  request_repaint_all();
  
  mouse_left_down = mouse_middle_down = mouse_right_down = false;
}

void ContainerWidgetBox::on_enter() {
  selection_inside = true;
  AbstractStyleBox::on_enter();
}

void ContainerWidgetBox::on_exit() {
  selection_inside = false;
  AbstractStyleBox::on_exit();
}

//} ... class ContainerWidgetBox
