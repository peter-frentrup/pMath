#include <boxes/emptywidgetbox.h>

#include <eval/dynamic.h>

#include <graphics/context.h>
#include <graphics/rectangle.h>

#include <gui/document.h>
#include <gui/native-widget.h>


using namespace richmath;

//{ class EmptyWidgetBox ...

EmptyWidgetBox::EmptyWidgetBox(ContainerType _type)
  : Box(),
    type(_type),
    old_type(_type),
    old_state(Normal),
    mouse_inside(false),
    mouse_left_down(false),
    mouse_middle_down(false),
    mouse_right_down(false),
    must_update(true)
{
}

ControlState EmptyWidgetBox::calc_state(Context *context) {
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

void EmptyWidgetBox::resize(Context *context) {
  ControlPainter::std->calc_container_size(
    context->canvas,
    type,
    &_extents);
}

void EmptyWidgetBox::paint(Context *context) {
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  ControlState state = calc_state(context);
  
  if(state != old_state || old_type != type || !animation) {
    animation = ControlPainter::std->control_transition(
                  id(),
                  context->canvas,
                  old_type,
                  type,
                  old_state,
                  state,
                  x,
                  y - _extents.ascent,
                  _extents.width,
                  _extents.height());
                  
    old_state = state;
  }
  
  bool need_bg = true;
  if(animation) {
    animation->update(this);
    if(animation->paint(context->canvas)) {
      need_bg = false;
    }
    else {
      animation = 0;
      
      animation = ControlPainter::std->control_transition(
                    id(),
                    context->canvas,
                    old_type,
                    type,
                    old_state,
                    old_state,
                    x,
                    y - _extents.ascent,
                    _extents.width,
                    _extents.height());
    }
  }
  
  if(need_bg) {
    ControlPainter::std->draw_container(
      this,
      context->canvas,
      type,
      state,
      x,
      y - _extents.ascent,
      _extents.width,
      _extents.height());
  }
  
  ControlPainter::std->container_content_move(
    type, state, &x, &y);
    
  context->canvas->move_to(x, y);
  
  if(type == FramelessButton && state == PressedHovered) {
    context->canvas->save();
    {
      Rectangle rect(x, y - _extents.ascent, _extents.width, _extents.height());
      
      rect.pixel_align(*context->canvas, false, 0);
      rect.add_rect_path(*context->canvas, false);
      
      cairo_set_operator(context->canvas->cairo(), CAIRO_OPERATOR_DIFFERENCE);
      context->canvas->set_color(0xffffff);
      context->canvas->fill();
    }
    context->canvas->restore();
  }
  
  old_type = type;
}

void EmptyWidgetBox::dynamic_updated() {
  if(must_update)
    return;
    
  must_update = true;
  request_repaint_all();
}

Box *EmptyWidgetBox::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  *was_inside_start = true;
  *start = *end = 0;
  return this;
}

void EmptyWidgetBox::on_mouse_enter() {
  if(!mouse_inside && ControlPainter::std->container_hover_repaint(this, type))
    request_repaint_all();
    
  mouse_inside = true;
  Box::on_mouse_enter();
}

void EmptyWidgetBox::on_mouse_exit() {
  if(/*mouse_inside && */ControlPainter::std->container_hover_repaint(this, type))
    request_repaint_all();
    
  mouse_inside = false;
  Box::on_mouse_exit();
}

void EmptyWidgetBox::on_mouse_down(MouseEvent &event) {
  event.set_origin(this);
  
  mouse_left_down   = mouse_left_down   || event.left;
  mouse_middle_down = mouse_middle_down || event.middle;
  mouse_right_down  = mouse_right_down  || event.right;
  mouse_inside      = _extents.to_rectangle().contains(event.x, event.y);
  
  if(event.left)
    request_repaint_all();
}

void EmptyWidgetBox::on_mouse_move(MouseEvent &event) {
  Document *doc = find_parent<Document>(false);
  event.set_origin(this);
  
  if(mouse_inside && doc)
    doc->native()->set_cursor(DefaultCursor);
    
  bool mi = _extents.to_rectangle().contains(event.x, event.y);
  
  if(mi != mouse_inside)
    request_repaint_all();
    
  mouse_inside = mi;
}

void EmptyWidgetBox::on_mouse_up(MouseEvent &event) {
  if(event.left) {
    request_repaint_all();
    
    if(mouse_inside && mouse_left_down)
      click();
  }
  
  mouse_left_down   = mouse_left_down   && !event.left;
  mouse_middle_down = mouse_middle_down && !event.middle;
  mouse_right_down  = mouse_right_down  && !event.right;
}

void EmptyWidgetBox::on_mouse_cancel() {
  request_repaint_all();
  
  mouse_left_down = mouse_middle_down = mouse_right_down = false;
}

void EmptyWidgetBox::click() {
}

bool EmptyWidgetBox::is_foreground_window() {
  Document *doc = find_parent<Document>(false);
  if(!doc)
    return false;
  
  FrontEndReference old_dyn_box_id = Dynamic::current_evaluation_box_id;
  Dynamic::current_evaluation_box_id = id();
  
  auto result = doc->native()->is_foreground_window();
  
  Dynamic::current_evaluation_box_id = old_dyn_box_id;
  return result;
}

int EmptyWidgetBox::dpi() {
  Document *doc = find_parent<Document>(false);
  if(!doc)
    return 96;
  
  FrontEndReference old_dyn_box_id = Dynamic::current_evaluation_box_id;
  Dynamic::current_evaluation_box_id = id();
  
  auto result = doc->native()->dpi();
  
  Dynamic::current_evaluation_box_id = old_dyn_box_id;
  return result;
}

//} ... class EmptyWidgetBox
