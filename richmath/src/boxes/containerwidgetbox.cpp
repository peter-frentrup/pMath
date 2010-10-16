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

ControlState ContainerWidgetBox::calc_state(Context *context){
  if(context->selection.id == id() && context->active)
    return Pressed;
  
  if(context->clicked_box_id == id() && mouse_left_down){
    if(mouse_inside)
      return Pressed;
    
    return Hot;
  }
  
  if(context->mouseover_box_id == id())
    return Hovered;
  
  return Normal;
}

void ContainerWidgetBox::resize(Context *context){
  AbstractStyleBox::resize(context);
  
  ControlPainter::std->calc_container_size(
    context->canvas,
    type,
    &_extents);
      
  cx = (_extents.width - _content->extents().width) / 2;
}

void ContainerWidgetBox::paint(Context *context){
  float x,y;
  context->canvas->current_pos(&x, &y);
  
  ControlState state = calc_state(context);
  
  if(state != old_state || !animation){
    animation = ControlPainter::std->control_transition(
      id(),
      context->canvas,
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
  if(animation){
    if(animation->paint(context->canvas)){
      need_bg = false;
    }
    else{
      animation = 0;
      
      animation = ControlPainter::std->control_transition(
        id(),
        context->canvas,
        type,
        old_state,
        old_state,
        x,
        y - _extents.ascent,
        _extents.width,
        _extents.height());
    }
  }

  bool very_transparent = ControlPainter::std->is_very_transparent(type, state);
  
  if(need_bg){
    ControlPainter::std->draw_container(
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
  
  int old_color = context->canvas->get_color();
  int c = ControlPainter::std->control_font_color(type, state);
  if(c >= 0){
    context->canvas->set_color(c);
  }
  
  if(very_transparent || !context->canvas->show_only_text)
    AbstractStyleBox::paint(context);
  
  context->canvas->set_color(old_color);
  
  if(type == FramelessButton && state == Pressed){
    context->canvas->pixrect(
      x,
      y - _extents.ascent, 
      x + _extents.width,
      y + _extents.descent,
      false);
    
    context->canvas->bitop_fill(BitOpDn, 0);
  }
}

void ContainerWidgetBox::on_mouse_enter(){
  if(!mouse_inside && ControlPainter::std->container_hover_repaint(type))
    request_repaint_all();
    
  mouse_inside = true;
}

void ContainerWidgetBox::on_mouse_exit(){
  if(/*mouse_inside && */ControlPainter::std->container_hover_repaint(type))
    request_repaint_all();
  
  mouse_inside = false;
}

void ContainerWidgetBox::on_mouse_down(MouseEvent &event){
  event.set_source(this);
  
  mouse_left_down   = mouse_left_down   || event.left;
  mouse_middle_down = mouse_middle_down || event.middle;
  mouse_right_down  = mouse_right_down  || event.right;
  mouse_inside = event.x >= 0 
              && event.x <= _extents.width
              && event.y >= -_extents.ascent
              && event.y <= _extents.descent;
  
  request_repaint_all();
}

void ContainerWidgetBox::on_mouse_move(MouseEvent &event){
  event.set_source(this);
  
  bool mi = event.x >= 0 
         && event.x <= _extents.width
         && event.y >= -_extents.ascent
         && event.y <= _extents.descent;
  
  if(mi != mouse_inside)
    request_repaint_all();
  
  mouse_inside = mi;
}

void ContainerWidgetBox::on_mouse_up(MouseEvent &event){
  request_repaint_all();
  
  mouse_left_down   = mouse_left_down   && !event.left;
  mouse_middle_down = mouse_middle_down && !event.middle;
  mouse_right_down  = mouse_right_down  && !event.right;
}

void ContainerWidgetBox::on_mouse_cancel(){
  request_repaint_all();
  
  mouse_left_down = mouse_middle_down = mouse_right_down = false;
}

void ContainerWidgetBox::on_enter(){
  selection_inside = true;
  AbstractStyleBox::on_enter();
}

void ContainerWidgetBox::on_exit(){
  selection_inside = false;
  AbstractStyleBox::on_exit();
}

//} ... class ContainerWidgetBox
