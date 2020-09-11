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

ControlState EmptyWidgetBox::calc_state(Context &context) {
  if(!enabled())
    return Disabled;
  
  if(context.selection.id == id() && context.active)
    return Pressed;
    
  if(context.clicked_box_id == id() && mouse_left_down) {
    if(mouse_inside)
      return PressedHovered;
      
    return Pressed;
  }
  
  Box *mo = FrontEndObject::find_cast<Box>(context.mouseover_box_id);
  if(mo && mo->mouse_sensitive() == this)
    return Hovered;
    
  return Normal;
}

void EmptyWidgetBox::resize(Context &context) {
  ControlPainter::std->calc_container_size(
    *this,
    context.canvas(),
    type,
    &_extents);
  
  float h = _extents.height();
  float em = context.canvas().get_font_size();
  _extents.ascent = 0.25 * em + 0.5 * h;
  _extents.descent = h - _extents.ascent;
}

void EmptyWidgetBox::paint(Context &context) {
  Point pos = context.canvas().current_pos();
  
  ControlState state = calc_state(context);
  
  if(animation && !animation->is_compatible(context.canvas(), _extents.width, _extents.height())) 
    animation = nullptr;
  
  if(state != old_state || old_type != type || !animation) {
    animation = ControlPainter::std->control_transition(
                  id(),
                  context.canvas(),
                  old_type,
                  type,
                  old_state,
                  state,
                  _extents.to_rectangle(pos));
    
    old_state = state;
  }
  
  bool need_bg = true;
  if(animation) {
    animation->update(this);
    if(animation->paint(context.canvas())) {
      need_bg = false;
    }
    else {
      animation = nullptr;
      
      animation = ControlPainter::std->control_transition(
                    id(),
                    context.canvas(),
                    old_type,
                    type,
                    old_state,
                    old_state,
                    _extents.to_rectangle(pos));
    }
  }
  
  if(need_bg) {
    ControlPainter::std->draw_container(
      *this,
      context.canvas(),
      type,
      state,
      _extents.to_rectangle(pos));
  }
  
  pos+= ControlPainter::std->container_content_offset(*this, type, state);
    
  context.canvas().move_to(pos);
  
  if(type == FramelessButton && state == PressedHovered) {
    context.canvas().save();
    {
      RectangleF rect = _extents.to_rectangle(pos);
      
      rect.pixel_align(context.canvas(), false, 0);
      rect.add_rect_path(context.canvas());
      
      cairo_set_operator(context.canvas().cairo(), CAIRO_OPERATOR_DIFFERENCE);
      context.canvas().set_color(Color::White);
      context.canvas().fill();
    }
    context.canvas().restore();
  }
  
  old_type = type;
}

void EmptyWidgetBox::dynamic_updated() {
  if(must_update)
    return;
    
  must_update = true;
  request_repaint_all();
}

VolatileSelection EmptyWidgetBox::mouse_selection(Point pos, bool *was_inside_start) {
  *was_inside_start = true;
  return { this, 0, 0 };
}

void EmptyWidgetBox::on_mouse_enter() {
  if(!mouse_inside && ControlPainter::std->container_hover_repaint(*this, type) && enabled())
    request_repaint_all();
    
  mouse_inside = true;
  base::on_mouse_enter();
}

void EmptyWidgetBox::on_mouse_exit() {
  if(/*mouse_inside && */ControlPainter::std->container_hover_repaint(*this, type) && enabled())
    request_repaint_all();
    
  mouse_inside = false;
  base::on_mouse_exit();
}

void EmptyWidgetBox::on_mouse_down(MouseEvent &event) {
  if(!enabled())
    return;
  
  event.set_origin(this);
  
  mouse_left_down   = mouse_left_down   || event.left;
  mouse_middle_down = mouse_middle_down || event.middle;
  mouse_right_down  = mouse_right_down  || event.right;
  mouse_inside      = _extents.to_rectangle().contains(event.position);
  
  if(event.left)
    request_repaint_all();
}

void EmptyWidgetBox::on_mouse_move(MouseEvent &event) {
  if(mouse_inside) {
    if(Document *doc = find_parent<Document>(false))
      doc->native()->set_cursor(CursorType::Default);
  }
  
  if(!enabled())
    return;
  
  event.set_origin(this);
  
  bool mi = _extents.to_rectangle().contains(event.position);
  
  if(mi != mouse_inside)
    request_repaint_all();
    
  mouse_inside = mi;
}

void EmptyWidgetBox::on_mouse_up(MouseEvent &event) {
  if(event.left && enabled()) {
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
  
  AutoResetCurrentObserver guard;
  return doc->native()->is_foreground_window();
}

bool EmptyWidgetBox::is_focused_widget() {
  Document *doc = find_parent<Document>(false);
  if(!doc)
    return false;
  
  AutoResetCurrentObserver guard;
  return doc->native()->is_focused_widget();
}

bool EmptyWidgetBox::is_using_dark_mode() {
  Document *doc = find_parent<Document>(false);
  if(!doc)
    return false;
  
  AutoResetCurrentObserver guard;
  return doc->native()->is_using_dark_mode();
}

int EmptyWidgetBox::dpi() {
  Document *doc = find_parent<Document>(false);
  if(!doc)
    return 96;
  
  AutoResetCurrentObserver guard;
  return doc->native()->dpi();
}

//} ... class EmptyWidgetBox
