#include <boxes/inputfieldbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;

//{ class InputFieldBox ...

InputFieldBox::InputFieldBox(MathSequence *content)
: ContainerWidgetBox(InputField, content),
  transparent(false),
//  autoscroll(false),
  last_click_time(0),
  last_click_global_x(0.0),
  last_click_global_y(0.0),
  frame_x(0)
{
  style = new Style(String("ControlStyle"));
  cx = 0;
}

ControlState InputFieldBox::calc_state(Context *context){
  if(selection_inside)
    return Pressed;
  
  return ContainerWidgetBox::calc_state(context);
}

bool InputFieldBox::expand(const BoxSize &size){
  _extents = size;
  return true;
}

void InputFieldBox::resize(Context *context){
  bool  old_math_spacing = context->math_spacing;
  float old_width        = context->width;
  context->math_spacing = false;
  context->width = HUGE_VAL;
  
  float old_cx = cx;
  AbstractStyleBox::resize(context); // not ContainerWidgetBox::resize() !
  cx = old_cx;
  
  context->math_spacing = old_math_spacing;
  context->width = old_width;
  
  if(_content->var_extents().ascent < 0.95 * _content->get_em())
     _content->var_extents().ascent = 0.95 * _content->get_em();
  
  if(_content->var_extents().descent < 0.25 * _content->get_em())
     _content->var_extents().descent = 0.25 * _content->get_em();

  _extents = _content->extents();

  float w = 10 * context->canvas->get_font_size();
  _extents.width = w;
  
  ControlPainter::std->calc_container_size(
    context->canvas,
    type,
    &_extents);
  
  cx-= frame_x;
  frame_x = (_extents.width - w) / 2;
  cx+= frame_x;
}

void InputFieldBox::paint_content(Context *context){
  float x, y;
  context->canvas->current_pos(&x, &y);
  
//  if(autoscroll){
//    autoscroll = false;
//    
//    Box *box = context->selection.get();
//    while(box && box != this)
//      box = box->parent();
//    
//    if(box == this){
//      double x1, y1, x2, y2;
//      Box *sel = context->selection.get();
//      
//      cairo_matrix_t mat;
//      cairo_matrix_init_identity(&mat);
//      sel->transformation(_content, &mat);
//      
//      context->canvas->save();
//      {
//        context->canvas->transform(mat);
//        context->canvas->move_to(0, 0);
//        
//        sel->selection_path(
//          context->canvas, 
//          context->selection.start, 
//          context->selection.end);
//        
//        cairo_path_extents(context->canvas->cairo(), &x1, &y1, &x2, &y2);
//        
//        context->canvas->new_path();
//      }
//      context->canvas->restore();
//      
//      float x = x1;
//      float y = y1;
//      float w = x2 - x1;
//      float h = y2 - y1;
//      cairo_matrix_invert(&mat);
//      Canvas::transform_rect(mat, &x, &y, &w, &h);
//      scroll_to(x, y, w, h);
//    }
//  }
  
  float dx = frame_x - 0.75f;
  float dy = frame_x;
  
  context->canvas->save();
  context->canvas->pixrect(
    x + dx, 
    y - _extents.ascent + dy, 
    x + _extents.width - dx, 
    y + _extents.descent - dy, 
    false);
  context->canvas->clip();
  context->canvas->move_to(x, y);
  
  ContainerWidgetBox::paint_content(context);
  
  context->canvas->restore();
}

void InputFieldBox::scroll_to(float x, float y, float w, float h){
  if(x + cx < frame_x){
    cx = frame_x - x;
    
    float extra = (_extents.width - 2 * frame_x) * 0.2;
    if(extra + w > _extents.width - 2 * frame_x)
      extra = _extents.width - 2 * frame_x - w;
    
    cx+= extra;
    if(cx > frame_x)
      cx = frame_x;
  }
  else if(x + w > -cx + _extents.width - 2 * frame_x){
    cx = _extents.width - frame_x - x - w;
    
    float extra = (_extents.width - 2 * frame_x) * 0.2;
    if(extra + w > _extents.width - 2 * frame_x)
      extra = _extents.width - 2 * frame_x - w;
    
    cx-= extra;
  }
  else if(x + w < _extents.width - 2 * frame_x)
    cx = frame_x;
}

void InputFieldBox::scroll_to(Canvas *canvas, Box *child, int start, int end){
  default_scroll_to(canvas, _content, child, start, end);
}

Box *InputFieldBox::remove(int *index){
  *index = 0;
  _content->remove(0, _content->length());
  return _content;
}

Expr InputFieldBox::to_pmath(bool parseable){
  return Call(
    Symbol(PMATH_SYMBOL_INPUTFIELDBOX),
    _content->to_pmath(parseable));
}

bool InputFieldBox::exitable(){
  return false;//_parent && _parent->selectable();
}
      
bool InputFieldBox::selectable(int i){
  return i >= 0;
}

void InputFieldBox::on_mouse_down(MouseEvent &event){
  Document *doc = find_parent<Document>(false);
  if(doc){
    if(event.left){
      event.set_source(0);
      float gx = event.x;
      float gy = event.y;
      
      gx*= doc->native()->scale_factor();
      gy*= doc->native()->scale_factor();
      
      float ddx, ddy;
      doc->native()->double_click_dist(&ddx, &ddy);
      
      if(NativeWidget::time_diff(
          last_click_time, 
          doc->native()->message_time()) <= doc->native()->double_click_time()
      && fabs(gx - last_click_global_x) <= ddx
      && fabs(gy - last_click_global_y) <= ddy)
      {
        Box *box  = doc->selection_box();
        int start = doc->selection_start();
        int end   = doc->selection_end();
        
        box = expand_selection(box, &start, &end);
        
        doc->select(box, start, end);
      }
      else{
        event.set_source(this);
        int start, end; 
        bool was_inside_start;
        Box *box = mouse_selection(event.x, event.y, &start, &end, &was_inside_start);
        doc->select(box, start, end);
      }
      
      last_click_time = doc->native()->message_time();
      last_click_global_x = gx;
      last_click_global_y = gy;
    }
  }
  
  ContainerWidgetBox::on_mouse_down(event);
}

void InputFieldBox::on_mouse_move(MouseEvent &event){
  Document *doc = find_parent<Document>(false);
  
  if(doc){
    event.set_source(this);
    
    int start, end; 
    bool was_inside_start;
    Box *box = mouse_selection(event.x, event.y, &start, &end, &was_inside_start);
    
    doc->native()->set_cursor(NativeWidget::text_cursor(box, start));
    
    if(event.left && mouse_left_down){
      doc->select_to(box, start, end);
    }
  }
  
  ContainerWidgetBox::on_mouse_move(event);
}

void InputFieldBox::on_enter(){
  if(transparent){
    request_repaint_all();
  }
  
  ContainerWidgetBox::on_enter();
}

void InputFieldBox::on_exit(){
  if(transparent){
    request_repaint_all();
  }
  
  ContainerWidgetBox::on_exit();
}

void InputFieldBox::on_key_down(SpecialKeyEvent &event){
  switch(event.key){
    case KeyReturn:
    case KeyTab:
      event.key = KeyUnknown;
      return;
    
    case KeyUp:   event.key = KeyLeft;  break;
    case KeyDown: event.key = KeyRight; break;
    
    default:
      break;
  }
  
//  autoscroll = true;
  ContainerWidgetBox::on_key_down(event);
}

void InputFieldBox::on_key_press(uint32_t unichar){
  if(unichar != '\n' || unichar != '\t'){
//    autoscroll = true;
    ContainerWidgetBox::on_key_press(unichar);
  }
}

//} ... class InputFieldBox
