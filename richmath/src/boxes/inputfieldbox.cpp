#include <boxes/inputfieldbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <eval/application.h>
#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;

//{ class InputFieldBox ...

InputFieldBox::InputFieldBox(MathSequence *content)
  : ContainerWidgetBox(InputField, content),
    must_update(true),
    invalidated(false),
    transparent(false),
//  autoscroll(false),
    last_click_time(0),
    last_click_global_x(0.0),
    last_click_global_y(0.0),
    frame_x(0)
{
  dynamic.init(this, Expr());
  input_type = Symbol(PMATH_SYMBOL_EXPRESSION);
  reset_style();
  cx = 0;
}

bool InputFieldBox::try_load_from_object(Expr expr, BoxOptions opts) {
  if(expr[0] != PMATH_SYMBOL_INPUTFIELDBOX)
    return false;
    
  if(expr.expr_length() < 2)
    return false;
    
  Expr options(pmath_options_extract(expr.get(), 2));
  if(options.is_null())
    return false;
    
  /* now success is guaranteed */
  
  if(dynamic.expr() != expr[1] || input_type != expr[2]) {
    dynamic     = expr[1];
    input_type  = expr[2];
    must_update = true;
  }
  
  reset_style();
  style->add_pmath(options);
  
  return true;
}

ControlState InputFieldBox::calc_state(Context *context) {
  if(selection_inside) {
    if(mouse_inside)
      return PressedHovered;
      
    return Pressed;
  }
  
  return ContainerWidgetBox::calc_state(context);
}

bool InputFieldBox::expand(const BoxSize &size) {
  _extents = size;
  return true;
}

void InputFieldBox::resize(Context *context) {
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
    
  cx -= frame_x;
  frame_x = (_extents.width - w) / 2;
  cx += frame_x;
}

void InputFieldBox::paint_content(Context *context) {
  if(must_update) {
    must_update = false;
    
    Expr result;
    if(dynamic.get_value(&result)) {
      BoxOptions opt = BoxOptions::Default;
      if(get_style(AutoNumberFormating))
        opt |= BoxOptions::FormatNumbers;
        
      invalidated = true;
      
      if(input_type == PMATH_SYMBOL_NUMBER) {
        if(result.is_number()) {
          result = Call(Symbol(PMATH_SYMBOL_MAKEBOXES), result);
          result = Application::interrupt(result, Application::dynamic_timeout);
        }
        else
          result = String("");
      }
      else if(input_type == PMATH_SYMBOL_STRING) {
        if(!result.is_string())
          result = String("");
      }
      else if(input_type[0] == PMATH_SYMBOL_HOLD) { // Hold(Expression)
        if(result.expr_length() == 1 && result[0] == PMATH_SYMBOL_HOLD)
          result.set(0, Symbol(PMATH_SYMBOL_MAKEBOXES));
        else
          result = Call(Symbol(PMATH_SYMBOL_MAKEBOXES), result);
          
        result = Application::interrupt(result, Application::dynamic_timeout);
      }
      else if(input_type != PMATH_SYMBOL_RAWBOXES) {
        result = Call(Symbol(PMATH_SYMBOL_MAKEBOXES), result);
        result = Application::interrupt(result, Application::dynamic_timeout);
      }
      
      if(result.is_null())
        result = String("");
      else if(result == PMATH_UNDEFINED || result == PMATH_SYMBOL_ABORTED)
        result = String("$Aborted");
        
      bool was_parent = is_parent_of(context->selection.get());
      
      content()->load_from_object(result, opt);
      context->canvas->save();
      {
        float w = _extents.width;
        resize(context);
        _extents.width = w;
      }
      context->canvas->restore();
      
      if(was_parent) {
        context->selection = SelectionReference();
        if(auto doc = find_parent<Document>(false))
          doc->select(content(), content()->length(), content()->length());
      }
      
      invalidate();
      invalidated = false;
    }
  }
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  if(invalidated) {
    context->canvas->save();
    
    float cx = x + _extents.width;
    float cy = y - _extents.ascent;
    float r  = _extents.height();
    cairo_pattern_t *pat;
    
    pat = cairo_pattern_create_radial(cx, cy, 0, cx, cy, r);
    cairo_pattern_add_color_stop_rgba(pat, 0,  1.0, 0.5, 0.0, 0.8);
    cairo_pattern_add_color_stop_rgba(pat, 1,  1.0, 0.5, 0.0, 0.0);
    
    cairo_set_source(context->canvas->cairo(), pat);
    cairo_pattern_destroy(pat);
    
    context->canvas->move_to(cx, cy);
    context->canvas->arc(cx, cy, r, M_PI / 2, M_PI, false);
    context->canvas->fill();
    
    context->canvas->restore();
  }
  
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

void InputFieldBox::reset_style() {
  if(style)
    style->clear();
  else
    style = new Style;
    
  style->set(BaseStyleName, String("InputField"));
}

void InputFieldBox::scroll_to(float x, float y, float w, float h) {
  float old_cx = cx;
  
  if(x + cx < frame_x) {
    cx = frame_x - x;
    
    float extra = (_extents.width - 2 * frame_x) * 0.2;
    if(extra + w > _extents.width - 2 * frame_x)
      extra = _extents.width - 2 * frame_x - w;
      
    cx += extra;
    if(cx > frame_x)
      cx = frame_x;
  }
  else if(x + w > -cx + _extents.width - 2 * frame_x) {
    cx = _extents.width - frame_x - x - w;
    
    float extra = (_extents.width - 2 * frame_x) * 0.2;
    if(extra + w > _extents.width - 2 * frame_x)
      extra = _extents.width - 2 * frame_x - w;
      
    cx -= extra;
  }
  else if(x + w < _extents.width - 2 * frame_x)
    cx = frame_x;
    
  if(cx != old_cx)
    request_repaint_all();
}

void InputFieldBox::scroll_to(Canvas *canvas, Box *child, int start, int end) {
  default_scroll_to(canvas, _content, child, start, end);
}

Box *InputFieldBox::remove(int *index) {
  *index = 0;
  _content->remove(0, _content->length());
  return _content;
}

Expr InputFieldBox::to_pmath(BoxFlags flags) {
  if(invalidated)
    assign_dynamic();
    
  Gather g;
  Gather::emit(dynamic.expr());
  Gather::emit(input_type);
  
  if(style)
    style->emit_to_pmath(false);
    
  Expr result = g.end();
  result.set(0, Symbol(PMATH_SYMBOL_INPUTFIELDBOX));
  return result;
}

void InputFieldBox::dynamic_updated() {
  if(must_update)
    return;
    
  must_update = true;
  request_repaint_all();
}

void InputFieldBox::dynamic_finished(Expr info, Expr result) {
  BoxOptions opt = BoxOptions::Default;
  if(get_style(AutoNumberFormating))
    opt |= BoxOptions::FormatNumbers;
    
  content()->load_from_object(result, opt);
  invalidate();
  invalidated = false;
}

Box *InputFieldBox::dynamic_to_literal(int *start, int *end) {
  if(dynamic.is_dynamic()) {
    dynamic = Expr();
    assign_dynamic();
  }
  
  return this;
}

void InputFieldBox::invalidate() {
  ContainerWidgetBox::invalidate();
  
  if(invalidated)
    return;
    
  invalidated = true;
  if(get_own_style(ContinuousAction, false)) {
    assign_dynamic();
  }
}

bool InputFieldBox::exitable() {
  return false;//_parent && _parent->selectable();
}

bool InputFieldBox::selectable(int i) {
  return i >= 0;
}

void InputFieldBox::on_mouse_down(MouseEvent &event) {
  if(auto doc = find_parent<Document>(false)) {
    if(event.left) {
      event.set_origin(0);
      float gx = event.x;
      float gy = event.y;
      
      gx *= doc->native()->scale_factor();
      gy *= doc->native()->scale_factor();
      
      float ddx, ddy;
      doc->native()->double_click_dist(&ddx, &ddy);
      
      if( abs(doc->native()->message_time() - last_click_time) <= doc->native()->double_click_time() &&
          fabs(gx - last_click_global_x) <= ddx &&
          fabs(gy - last_click_global_y) <= ddy)
      {
        Box *box  = doc->selection_box();
        int start = doc->selection_start();
        int end   = doc->selection_end();
        
        box = expand_selection(box, &start, &end);
        
        doc->select(box, start, end);
      }
      else {
        event.set_origin(this);
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

void InputFieldBox::on_mouse_move(MouseEvent &event) {
  if(auto doc = find_parent<Document>(false)) {
    event.set_origin(this);
    
    int start, end;
    bool was_inside_start;
    Box *box = mouse_selection(event.x, event.y, &start, &end, &was_inside_start);
    
    doc->native()->set_cursor(NativeWidget::text_cursor(box, start));
    
    if(event.left && mouse_left_down) {
      doc->select_to(box, start, end);
    }
  }
  
  ContainerWidgetBox::on_mouse_move(event);
}

void InputFieldBox::on_enter() {
  request_repaint_all();
  
  ContainerWidgetBox::on_enter();
}

void InputFieldBox::on_exit() {
  request_repaint_all();
  
  ContainerWidgetBox::on_exit();
  
  if(invalidated)
    assign_dynamic();
}

void InputFieldBox::on_finish_editing() {
  if(invalidated)
    assign_dynamic();
    
  ContainerWidgetBox::on_finish_editing();
}

void InputFieldBox::on_key_down(SpecialKeyEvent &event) {
  switch(event.key) {
    case SpecialKey::Return:
    
      if(!invalidated)
        dynamic_updated();
        
      if(!assign_dynamic()) {
        if(auto doc = find_parent<Document>(false))
          doc->native()->beep();
          
        must_update = true;
      }
      
      event.key = SpecialKey::Unknown;
      return;
      
//    case SpecialKey::Tab:
//      event.key = SpecialKey::Unknown;
//      return;
      
    case SpecialKey::Up: {
        Document *doc = find_parent<Document>(false);
        if(doc && doc->selection_box() == _content && doc->selection_start() == 0)
          break;
          
        event.key = SpecialKey::Left;
      } break;
      
    case SpecialKey::Down: {
        Document *doc = find_parent<Document>(false);
        if(doc && doc->selection_box() == _content && doc->selection_start() == _content->length())
          break;
          
        event.key = SpecialKey::Right;
      } break;
      
    default:
      break;
  }
  
  ContainerWidgetBox::on_key_down(event);
}

void InputFieldBox::on_key_press(uint32_t unichar) {
  if(unichar != '\n' && unichar != '\t') {
    ContainerWidgetBox::on_key_press(unichar);
  }
}

bool InputFieldBox::assign_dynamic() {
  invalidated = false;
  
  if(input_type == PMATH_SYMBOL_EXPRESSION || input_type[0] == PMATH_SYMBOL_HOLD) { // Expression or Hold(Expression)
    Expr boxes = _content->to_pmath(BoxFlags::Parseable);
    
    Expr value = Call(Symbol(PMATH_SYMBOL_TRY),
                      Call(Symbol(PMATH_SYMBOL_MAKEEXPRESSION), boxes),
                      Call(Symbol(PMATH_SYMBOL_RAWBOXES), boxes));
                      
    value = Evaluate(value);
    
    if(value[0] == PMATH_SYMBOL_HOLDCOMPLETE) {
      if(input_type[0] == PMATH_SYMBOL_HOLD) {
        value.set(0, Symbol(PMATH_SYMBOL_HOLD));
      }
      else {
        if(value.expr_length() == 1)
          value = value[1];
        else
          value.set(0, Symbol(PMATH_SYMBOL_SEQUENCE));
      }
    }
    
    dynamic.assign(value);
    return true;
  }
  
  if(input_type == PMATH_SYMBOL_NUMBER) {
    Expr boxes = _content->to_pmath(BoxFlags::Parseable);
    
    Expr value = Call(Symbol(PMATH_SYMBOL_TRY),
                      Call(Symbol(PMATH_SYMBOL_MAKEEXPRESSION), boxes));
                      
    value = Evaluate(value);
    
    if( value[0] == PMATH_SYMBOL_HOLDCOMPLETE &&
        value.expr_length() == 1              &&
        value[1].is_number())
    {
      dynamic.assign(value[1]);
      return true;
    }
    
    return false;
  }
  
  if(input_type == PMATH_SYMBOL_RAWBOXES) {
    Expr boxes = _content->to_pmath(BoxFlags::Default);
    
    dynamic.assign(boxes);
    return true;
  }
  
  if(input_type == PMATH_SYMBOL_STRING) {
    if(_content->count() > 0) {
      Expr boxes = _content->to_pmath(BoxFlags::Parseable);
      
      Expr value = Call(Symbol(PMATH_SYMBOL_TOSTRING),
                        Call(Symbol(PMATH_SYMBOL_RAWBOXES), boxes));
                        
      value = Evaluate(value);
      
      dynamic.assign(value);
      if(!dynamic.is_dynamic())
        must_update = true;
    }
    else {
      dynamic.assign(_content->text());
    }
    
    return true;
  }
  
  return false;
}

//} ... class InputFieldBox

