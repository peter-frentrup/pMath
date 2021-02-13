#include <boxes/inputfieldbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <eval/application.h>
#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;

static ContainerType parse_inputfield_appearance(Expr expr);

namespace richmath {namespace strings {
  extern String AddressBand;
  extern String EmptyString;
  extern String Framed;
  extern String Frameless;
  extern String DollarAborted;
  extern String InputField;
}}

extern pmath_symbol_t richmath_System_DollarAborted;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_Expression;
extern pmath_symbol_t richmath_System_Hold;
extern pmath_symbol_t richmath_System_HoldComplete;
extern pmath_symbol_t richmath_System_InputFieldBox;
extern pmath_symbol_t richmath_System_MakeBoxes;
extern pmath_symbol_t richmath_System_MakeExpression;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_Number;
extern pmath_symbol_t richmath_System_RawBoxes;
extern pmath_symbol_t richmath_System_Sequence;
extern pmath_symbol_t richmath_System_String;
extern pmath_symbol_t richmath_System_ToString;
extern pmath_symbol_t richmath_System_Try;

//{ class InputFieldBox ...

InputFieldBox::InputFieldBox(MathSequence *content)
  : base(ContainerType::InputField, content),
    must_update(true),
    invalidated(false),
    frame_x(0)
{
  dynamic.init(this, Expr());
  input_type = Symbol(richmath_System_Expression);
  reset_style();
  cx = 0;
}

bool InputFieldBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_InputFieldBox)
    return false;
    
  if(expr.expr_length() < 2)
    return false;
    
  Expr options(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
    
  /* now success is guaranteed */
  
  if(dynamic.expr() != expr[1] || input_type != expr[2] || has(opts, BoxInputFlags::ForceResetDynamic)) {
    dynamic     = expr[1];
    input_type  = expr[2];
    must_update = true;
  }
  
  reset_style();
  style->add_pmath(options);
  
  finish_load_from_object(std::move(expr));
  return true;
}

ControlState InputFieldBox::calc_state(Context &context) {
  if(!enabled())
    return ControlState::Disabled;
  
  if(selection_inside) {
    if(mouse_inside)
      return ControlState::PressedHovered;
      
    return ControlState::Pressed;
  }
  
  return base::calc_state(context);
}

bool InputFieldBox::expand(const BoxSize &size) {
  _extents = size;
  return true;
}

void InputFieldBox::resize_default_baseline(Context &context) {
  bool  old_math_spacing = context.math_spacing;
  float old_width        = context.width;
  context.math_spacing = false;
  context.width = HUGE_VAL;
  
  type = parse_inputfield_appearance(get_own_style(Appearance));
  
  float old_cx = cx;
  AbstractStyleBox::resize_default_baseline(context); // not ContainerWidgetBox::resize_default_baseline() !
  cx = old_cx;
  
  context.math_spacing = old_math_spacing;
  context.width = old_width;
  
  if(_content->var_extents().ascent < 0.95 * _content->get_em())
    _content->var_extents().ascent = 0.95 * _content->get_em();
    
  if(_content->var_extents().descent < 0.25 * _content->get_em())
    _content->var_extents().descent = 0.25 * _content->get_em();
    
  _extents = _content->extents();
  
  float w = 10 * context.canvas().get_font_size();
  _extents.width = w;
  
  ControlPainter::std->calc_container_size(
    *this,
    context.canvas(),
    type,
    &_extents);
    
  cx -= frame_x;
  frame_x = (_extents.width - w) / 2;
  cx += frame_x;
}

void InputFieldBox::paint_content(Context &context) {
  if(must_update) {
    must_update = false;
    
    Expr result;
    if(dynamic.get_value(&result)) {
      BoxInputFlags opt = BoxInputFlags::Default;
      if(get_style(AutoNumberFormating))
        opt |= BoxInputFlags::FormatNumbers;
        
      invalidated = true;
      
      if(input_type == richmath_System_Number) {
        if(result.is_number()) {
          result = Call(Symbol(richmath_System_MakeBoxes), result);
          result = Application::interrupt_wait(result, Application::dynamic_timeout);
        }
        else
          result = strings::EmptyString;
      }
      else if(input_type == richmath_System_String) {
        if(!result.is_string())
          result = strings::EmptyString;
      }
      else if(input_type[0] == richmath_System_Hold) { // Hold(Expression)
        if(result.expr_length() == 1 && result[0] == richmath_System_Hold)
          result.set(0, Symbol(richmath_System_MakeBoxes));
        else
          result = Call(Symbol(richmath_System_MakeBoxes), result);
          
        result = Application::interrupt_wait(result, Application::dynamic_timeout);
      }
      else if(input_type != richmath_System_RawBoxes) {
        result = Call(Symbol(richmath_System_MakeBoxes), result);
        result = Application::interrupt_wait(result, Application::dynamic_timeout);
      }
      
      if(result.is_null())
        result = strings::EmptyString;
      else if(result == PMATH_UNDEFINED || result == richmath_System_DollarAborted)
        result = strings::DollarAborted;
        
      bool was_parent = is_parent_of(context.selection.get());
      
      content()->load_from_object(result, opt);
      context.canvas().save();
      {
        float w = _extents.width;
        resize(context);
        _extents.width = w;
      }
      context.canvas().restore();
      
      if(was_parent) {
        context.selection = SelectionReference();
        if(auto doc = find_parent<Document>(false))
          doc->select(content(), content()->length(), content()->length());
      }
      
      invalidate();
      invalidated = false;
    }
  }
  
  float x, y;
  context.canvas().current_pos(&x, &y);
  
  if(invalidated) {
    context.canvas().save();
    
    float cx = x + _extents.width;
    float cy = y - _extents.ascent;
    float r  = _extents.height();
    cairo_pattern_t *pat;
    
    pat = cairo_pattern_create_radial(cx, cy, 0, cx, cy, r);
    cairo_pattern_add_color_stop_rgba(pat, 0,  1.0, 0.5, 0.0, 0.8);
    cairo_pattern_add_color_stop_rgba(pat, 1,  1.0, 0.5, 0.0, 0.0);
    
    cairo_set_source(context.canvas().cairo(), pat);
    cairo_pattern_destroy(pat);
    
    context.canvas().move_to(cx, cy);
    context.canvas().arc(cx, cy, r, M_PI / 2, M_PI, false);
    context.canvas().fill();
    
    context.canvas().restore();
  }
  
  float dx = frame_x - 0.75f;
  float dy = 0;
  
  context.canvas().save();
  
  context.canvas().pixrect(
    x + dx,
    y - _extents.ascent + dy,
    x + _extents.width - dx,
    y + _extents.descent - dy,
    false);
  context.canvas().clip();
  context.canvas().move_to(x, y);
  base::paint_content(context);
  
  context.canvas().restore();
}

void InputFieldBox::reset_style() {
  Style::reset(style, strings::InputField);
}

void InputFieldBox::scroll_to(const RectangleF &rect) {
  float old_cx = cx;
  
  if(rect.left() < -cx + frame_x) {
    cx = frame_x - rect.left();
    
    float extra = (_extents.width - 2 * frame_x) * 0.2;
    if(extra + rect.width > _extents.width - 2 * frame_x)
      extra = _extents.width - 2 * frame_x - rect.width;
      
    cx += extra;
    if(cx > frame_x)
      cx = frame_x;
  }
  else if(rect.right() > -cx + _extents.width - 2 * frame_x) {
    cx = _extents.width - frame_x - rect.right();
    
    float extra = (_extents.width - 2 * frame_x) * 0.2;
    if(extra + rect.width > _extents.width - 2 * frame_x)
      extra = _extents.width - 2 * frame_x - rect.width;
      
    cx -= extra;
  }
  else if(rect.right() < _extents.width - 2 * frame_x)
    cx = frame_x;
    
  if(cx != old_cx)
    request_repaint_all();
}

void InputFieldBox::scroll_to(Canvas &canvas, const VolatileSelection &child_sel) {
  default_scroll_to(canvas, _content, child_sel);
}

Box *InputFieldBox::remove(int *index) {
  *index = 0;
  _content->remove(0, _content->length());
  return _content;
}

Expr InputFieldBox::to_pmath_symbol() {
  return Symbol(richmath_System_InputFieldBox);
}

Expr InputFieldBox::to_pmath(BoxOutputFlags flags) {
  if(invalidated)
    assign_dynamic();
    
  Gather g;
  Gather::emit(dynamic.expr());
  Gather::emit(input_type);
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style->get(BaseStyleName, &s) && s == strings::InputField)
      with_inherited = false;
    
    style->emit_to_pmath(with_inherited);
  }
  
  Expr result = g.end();
  result.set(0, Symbol(richmath_System_InputFieldBox));
  return result;
}

void InputFieldBox::dynamic_updated() {
  if(must_update)
    return;
    
  must_update = true;
  request_repaint_all();
}

void InputFieldBox::dynamic_finished(Expr info, Expr result) {
  BoxInputFlags opt = BoxInputFlags::Default;
  if(get_style(AutoNumberFormating))
    opt |= BoxInputFlags::FormatNumbers;
    
  content()->load_from_object(result, opt);
  invalidate();
  invalidated = false;
}

VolatileSelection InputFieldBox::dynamic_to_literal(int start, int end) {
  if(dynamic.is_dynamic()) {
    dynamic = Expr();
    assign_dynamic();
  }
  
  return {this, start, end};
}

void InputFieldBox::invalidate() {
  base::invalidate();
  
  if(invalidated)
    return;
    
  invalidated = true;
  if(get_own_style(ContinuousAction, false)) {
    assign_dynamic();
  }
}

bool InputFieldBox::selectable(int i) {
  return i >= 0 && enabled();
}

void InputFieldBox::on_mouse_down(MouseEvent &event) {
  if(enabled()) {
    if(auto doc = find_parent<Document>(false)) {
      doc->on_mouse_down(event);
      return;
    }
  }
  
  base::on_mouse_down(event);
}

void InputFieldBox::on_mouse_move(MouseEvent &event) {
  if(enabled()) {
    if(auto doc = find_parent<Document>(false)) {
      doc->on_mouse_move(event);
      return;
    }
  }
  
  base::on_mouse_move(event);
}

void InputFieldBox::on_mouse_up(MouseEvent &event) {
  if(enabled()) {
    if(auto doc = find_parent<Document>(false)) {
      doc->on_mouse_up(event);
      return;
    }
  }
  
  base::on_mouse_up(event);
}

void InputFieldBox::on_enter() {
  base::on_enter();
}

void InputFieldBox::on_exit() {
  base::on_exit();
  
  if(invalidated && enabled())
    assign_dynamic();
}

void InputFieldBox::on_finish_editing() {
  if(invalidated && enabled())
    assign_dynamic();
    
  base::on_finish_editing();
}

void InputFieldBox::on_key_down(SpecialKeyEvent &event) {
  if(!enabled())
    return;
    
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
//        Document *doc = find_parent<Document>(false);
//        if(doc && doc->selection_box() == _content && doc->selection_start() == 0)
//          break;
          
        event.key = SpecialKey::Left;
      } break;
      
    case SpecialKey::Down: {
//        Document *doc = find_parent<Document>(false);
//        if(doc && doc->selection_box() == _content && doc->selection_start() == _content->length())
//          break;
          
        event.key = SpecialKey::Right;
      } break;
      
    default:
      break;
  }
  
  base::on_key_down(event);
}

void InputFieldBox::on_key_press(uint32_t unichar) {
  if(unichar != '\n' && unichar != '\t') {
    base::on_key_press(unichar);
  }
}

bool InputFieldBox::assign_dynamic() {
  invalidated = false;
  
  if(input_type == richmath_System_Expression || input_type[0] == richmath_System_Hold) { // Expression or Hold(Expression)
    Expr boxes = _content->to_pmath(BoxOutputFlags::Parseable);
    
    Expr value = Call(Symbol(richmath_System_Try),
                      Call(Symbol(richmath_System_MakeExpression), boxes),
                      Call(Symbol(richmath_System_RawBoxes), boxes));
                      
    value = Evaluate(value);
    
    if(value[0] == richmath_System_HoldComplete) {
      if(input_type[0] == richmath_System_Hold) {
        value.set(0, Symbol(richmath_System_Hold));
      }
      else {
        if(value.expr_length() == 1)
          value = value[1];
        else
          value.set(0, Symbol(richmath_System_Sequence));
      }
    }
    
    dynamic.assign(value);
    return true;
  }
  
  if(input_type == richmath_System_Number) {
    Expr boxes = _content->to_pmath(BoxOutputFlags::Parseable);
    
    Expr value = Call(Symbol(richmath_System_Try),
                      Call(Symbol(richmath_System_MakeExpression), boxes));
                      
    value = Evaluate(value);
    
    if( value[0] == richmath_System_HoldComplete &&
        value.expr_length() == 1                 &&
        value[1].is_number())
    {
      dynamic.assign(value[1]);
      return true;
    }
    
    return false;
  }
  
  if(input_type == richmath_System_RawBoxes) {
    Expr boxes = _content->to_pmath(BoxOutputFlags::Default);
    
    dynamic.assign(boxes);
    return true;
  }
  
  if(input_type == richmath_System_String) {
    if(_content->count() > 0) {
      Expr boxes = _content->to_pmath(BoxOutputFlags::Parseable);
      
      Expr value = Call(Symbol(richmath_System_ToString),
                        Call(Symbol(richmath_System_RawBoxes), boxes));
                        
      value = Evaluate(value); // TODO: evaluate this in the kernel thread or use Expr::to_string() directly
      
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

static ContainerType parse_inputfield_appearance(Expr expr) {
  if(expr.is_string()) {
    String s = std::move(expr);
    
    if(s == strings::Frameless)
      return ContainerType::None;
    
    if(s == strings::Framed)
      return ContainerType::InputField;
    
    if(s == strings::AddressBand)
      return ContainerType::AddressBandInputField;
    
    return ContainerType::InputField;
  }
  
  if(expr == richmath_System_None)
    return ContainerType::None;
    
  if(expr == richmath_System_Automatic)
    return ContainerType::InputField;
  
  return ContainerType::InputField;
}
