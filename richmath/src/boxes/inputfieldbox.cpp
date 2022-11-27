#include <boxes/inputfieldbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <eval/application.h>
#include <eval/eval-contexts.h>
#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;

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

enum class DynamicFunctions {
  Continue,
  Finish,
  OnlyPost,
};

namespace richmath {
  class InputFieldBox::Impl {
    public:
      Impl(InputFieldBox &self) : self{self} {}
      
      static ContainerType parse_appearance(Expr expr);

      static InputFieldType type_from_expr(Expr expr);
      static Expr expr_from_type(InputFieldType ty);
    
      bool assign_dynamic(DynamicFunctions funcs);
      
    private:
      void finish_assign_dynamic(Expr value, DynamicFunctions funcs);
      
    private:
      InputFieldBox &self;
  };
}

namespace {
  class ContinueAssignDynamicAfterEditEvent: public TimedEvent {
    public:
      explicit ContinueAssignDynamicAfterEditEvent(FrontEndReference id) 
        : TimedEvent(0.1), 
          _id(id) 
      {
      }
      
      virtual void execute_event() override {
        if(InputFieldBox *box = FrontEndObject::find_cast<InputFieldBox>(_id)) {
          box->continue_assign_dynamic();
        }
      }
      
    private:
      FrontEndReference _id;
  };
}

//{ class InputFieldBox ...

InputFieldBox::InputFieldBox(AbstractSequence *content)
  : base(ContainerType::InputField, content),
    frame_x(0)
{
  must_update(true);
  dynamic.init(this, Expr());
  input_type(InputFieldType::Expression);
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
  
  InputFieldType new_type = Impl::type_from_expr(expr[2]);
  
  if(dynamic.expr() != expr[1] || input_type() != new_type || has(opts, BoxInputFlags::ForceResetDynamic)) {
    dynamic = expr[1];
    input_type(new_type);
    must_update(true);
    _assigned_result = Expr(PMATH_UNDEFINED);
  }
  
  _continue_assign_dynamic_event = nullptr;
  
  reset_style();
  style->add_pmath(options);
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

ControlState InputFieldBox::calc_state(Context &context) {
  if(!enabled())
    return ControlState::Disabled;
  
  if(selection_inside()) {
    if(mouse_inside())
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
  
  type = Impl::parse_appearance(get_own_style(Appearance));
  
  float old_cx = cx;
  AbstractStyleBox::resize_default_baseline(context); // not ContainerWidgetBox::resize_default_baseline() !
  cx = old_cx;
  
  context.math_spacing = old_math_spacing;
  context.width = old_width;
  
  if(_content->var_extents().ascent < 0.75 * _content->get_em())
    _content->var_extents().ascent = 0.75 * _content->get_em();
    
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
  if(!context.selection.get() && enabled()) {
    if(auto doc = find_parent<Document>(false)) {
      doc->select(content(), content()->length(), content()->length());
    }
  }
  
  if(must_update()) {
    must_update(false);
    
    Expr result;
    if( dynamic.get_value(&result) &&
        !_continue_assign_dynamic_event &&
        (!selection_inside() || _assigned_result != result)
    ) {
      _assigned_result = Expr(PMATH_UNDEFINED);
      
      BoxInputFlags opt = BoxInputFlags::Default;
      if(get_style(AutoNumberFormating))
        opt |= BoxInputFlags::FormatNumbers;
        
      invalidated(true);
      
      switch(input_type()) {
        case InputFieldType::HeldExpression:
            if(result.expr_length() == 1 && result[0] == richmath_System_Hold) {
              result = result[1];
            }
            // Fall through
        case InputFieldType::Expression: {
            if(result.is_null()) {
              result = strings::EmptyString;
            }
            else {
              result = Call(Symbol(richmath_System_MakeBoxes), PMATH_CPP_MOVE(result));
              result = prepare_dynamic(PMATH_CPP_MOVE(result));
              result = EvaluationContexts::prepare_namespace_for(PMATH_CPP_MOVE(result), this);
              result = EvaluationContexts::make_context_block(PMATH_CPP_MOVE(result), EvaluationContexts::resolve_context(this));
              result = Application::interrupt_wait_for(PMATH_CPP_MOVE(result), this, Application::dynamic_timeout);
            }
          } break;
        
        case InputFieldType::RawBoxes: break;
        
        case InputFieldType::String: {
            if(!result.is_string())
              result = strings::EmptyString;
          } break;
          
        case InputFieldType::Number: {
            if(result.is_number()) {
              result = Call(Symbol(richmath_System_MakeBoxes), result);
              result = Application::interrupt_wait_for(result, this, Application::dynamic_timeout);
            }
            else
              result = strings::EmptyString;
          } break;
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
      
      if(was_parent && !context.selection.get()) { // old selection got lost
        context.selection = SelectionReference();
        if(auto doc = find_parent<Document>(false))
          doc->select(content(), content()->length(), content()->length());
      }
      
      invalidate();
      invalidated(false);
    }
  }
  
  float x, y;
  context.canvas().current_pos(&x, &y);
  
  if(invalidated() && !_continue_assign_dynamic_event) {
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
  
  bool  old_math_spacing = context.math_spacing;
  context.math_spacing = false; // same as during resize()
  
  base::paint_content(context);
  
  context.math_spacing = old_math_spacing;
  
  context.canvas().restore();
}

void InputFieldBox::reset_style() {
  Style::reset(style, strings::InputField);
}

bool InputFieldBox::scroll_to(const RectangleF &rect) {
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
    
  if(cx != old_cx) {
    request_repaint_all();
    return true;
  }
  return false;
}

bool InputFieldBox::scroll_to(Canvas &canvas, const VolatileSelection &child_sel) {
  return default_scroll_to(canvas, _content, child_sel);
}

Box *InputFieldBox::remove(int *index) {
  *index = 0;
  _content->remove(0, _content->length());
  return _content;
}

Expr InputFieldBox::to_pmath_symbol() {
  return Symbol(richmath_System_InputFieldBox);
}

Expr InputFieldBox::to_pmath_impl(BoxOutputFlags flags) {
  if(invalidated())
    Impl(*this).assign_dynamic(DynamicFunctions::Continue); // TODO: DynamicFunctions::Finish ?
    
  Gather g;
  Gather::emit(dynamic.expr());
  Gather::emit(Impl::expr_from_type(input_type()));
  
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
  if(must_update())
    return;
    
  must_update(true);
  request_repaint_all();
}

void InputFieldBox::dynamic_finished(Expr info, Expr result) {
  BoxInputFlags opt = BoxInputFlags::Default;
  if(get_style(AutoNumberFormating))
    opt |= BoxInputFlags::FormatNumbers;
    
  content()->load_from_object(result, opt);
  invalidate();
  invalidated(false);
}

VolatileSelection InputFieldBox::dynamic_to_literal(int start, int end) {
  if(dynamic.is_dynamic()) {
    dynamic = Expr();
    Impl(*this).assign_dynamic(DynamicFunctions::Finish);
  }
  
  return {this, start, end};
}

void InputFieldBox::invalidate() {
  base::invalidate();
  
  if(invalidated())
    return;
    
  invalidated(true);
  if(must_update()) {
    if(!_continue_assign_dynamic_event)
      return; // Dynamic changed due to someone else
  }
  
  if(get_own_style(ContinuousAction, false)) {
    if(_continue_assign_dynamic_event) {
      _continue_assign_dynamic_event->reset_timer();
    }
    else {
      _continue_assign_dynamic_event = new ContinueAssignDynamicAfterEditEvent(id());
      if(!_continue_assign_dynamic_event->register_for(id())) {
        continue_assign_dynamic();
      }
    }
    //Impl(*this).assign_dynamic(DynamicFunctions::Continue);
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
  
  if(invalidated() && enabled()) {
    Impl(*this).assign_dynamic(DynamicFunctions::Finish);
  }
  else if(did_continuous_updates() && dynamic.has_pre_or_post_assignment()) {
    Impl(*this).assign_dynamic(DynamicFunctions::OnlyPost);
  }
}

void InputFieldBox::on_finish_editing() {
  if(invalidated() && enabled()) {
    Impl(*this).assign_dynamic(DynamicFunctions::Finish);
  }
  else if(did_continuous_updates() && dynamic.has_pre_or_post_assignment()) {
    Impl(*this).assign_dynamic(DynamicFunctions::OnlyPost);
  }
    
  base::on_finish_editing();
}

void InputFieldBox::continue_assign_dynamic() {
  _continue_assign_dynamic_event = nullptr;
  Impl(*this).assign_dynamic(DynamicFunctions::Continue);
}

void InputFieldBox::on_key_down(SpecialKeyEvent &event) {
  if(!enabled())
    return;
    
  switch(event.key) {
    case SpecialKey::Return:
      if(!invalidated())
        dynamic_updated();
        
      if(!Impl(*this).assign_dynamic(DynamicFunctions::Finish)) {
        if(auto doc = find_parent<Document>(false))
          doc->native()->beep();
          
        must_update(true);
      }
      break;
     
//    case SpecialKey::Up:
//      event.key = SpecialKey::Left;
//      break;
//      
//    case SpecialKey::Down:
//      event.key = SpecialKey::Right;
//      break;
      
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

//} ... class InputFieldBox

//{ class InputFieldBox::Impl ...

ContainerType InputFieldBox::Impl::parse_appearance(Expr expr) {
  if(expr.is_string()) {
    String s = PMATH_CPP_MOVE(expr);
    
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

InputFieldType InputFieldBox::Impl::type_from_expr(Expr expr) {
  if(expr == richmath_System_Expression) return InputFieldType::Expression;
  if(expr == richmath_System_RawBoxes)   return InputFieldType::RawBoxes;
  if(expr == richmath_System_String)     return InputFieldType::String;
  if(expr == richmath_System_Number)     return InputFieldType::Number;
  
  // Hold(Expression)
  if(expr[0] == richmath_System_Hold)    return InputFieldType::HeldExpression;
  
  return InputFieldType::Expression;
}

Expr InputFieldBox::Impl::expr_from_type(InputFieldType ty) {
  switch(ty) {
    case InputFieldType::Expression:     return Symbol(richmath_System_Expression);
    case InputFieldType::HeldExpression: return Call(Symbol(richmath_System_Hold), Symbol(richmath_System_Expression));
    case InputFieldType::RawBoxes:       return Symbol(richmath_System_RawBoxes);
    case InputFieldType::String:         return Symbol(richmath_System_String);
    case InputFieldType::Number:         return Symbol(richmath_System_Number);
  }
  
  return Expr();
}

bool InputFieldBox::Impl::assign_dynamic(DynamicFunctions funcs) {
  self.invalidated(false);
  
  switch(self.input_type()) {
    case InputFieldType::Expression:
    case InputFieldType::HeldExpression: {
        Expr boxes = self._content->to_pmath(BoxOutputFlags::Parseable | BoxOutputFlags::WithDebugMetadata);
        
        boxes = EvaluationContexts::prepare_namespace_for(PMATH_CPP_MOVE(boxes), &self);
        
        Expr value = Call(Symbol(richmath_System_Try),
                          Call(Symbol(richmath_System_MakeExpression), boxes),
                          Call(Symbol(richmath_System_RawBoxes), boxes));
        value = EvaluationContexts::make_context_block(PMATH_CPP_MOVE(value), EvaluationContexts::resolve_context(&self));
        value = Evaluate(PMATH_CPP_MOVE(value));
        
        if(value[0] == richmath_System_HoldComplete) {
          if(self.input_type() == InputFieldType::HeldExpression) {
            value.set(0, Symbol(richmath_System_Hold));
          }
          else {
            if(value.expr_length() == 1)
              value = value[1];
            else
              value.set(0, Symbol(richmath_System_Sequence));
          }
        }
        
        finish_assign_dynamic(PMATH_CPP_MOVE(value), funcs);
        return true;
      }
      
    case InputFieldType::RawBoxes: {
        Expr boxes = self._content->to_pmath(BoxOutputFlags::WithDebugMetadata);
        
        finish_assign_dynamic(PMATH_CPP_MOVE(boxes), funcs);
        return true;
      }
      
    case InputFieldType::String: {
        if(self._content->count() > 0) {
          Expr boxes = self._content->to_pmath(BoxOutputFlags::Parseable);
          
          Expr value = Call(Symbol(richmath_System_ToString),
                            Call(Symbol(richmath_System_RawBoxes), boxes));
                            
          value = Evaluate(PMATH_CPP_MOVE(value)); // TODO: evaluate this in the kernel thread or use Expr::to_string() directly ?
          
          finish_assign_dynamic(PMATH_CPP_MOVE(value), funcs);
          if(!self.dynamic.is_dynamic())
            self.must_update(true);
        }
        else {
          finish_assign_dynamic(self._content->text(), funcs);
        }
        
        return true;
      }
    
    case InputFieldType::Number:  {
        Expr boxes = self._content->to_pmath(BoxOutputFlags::Parseable);
        
        Expr value = Call(Symbol(richmath_System_Try),
                          Call(Symbol(richmath_System_MakeExpression), boxes));
                          
        value = Evaluate(value);
        
        if( value[0] == richmath_System_HoldComplete &&
            value.expr_length() == 1                 &&
            value[1].is_number())
        {
          finish_assign_dynamic(value[1], funcs);
          return true;
        }
        
        return false;
      }
  }
  
  return false;
}

void InputFieldBox::Impl::finish_assign_dynamic(Expr value, DynamicFunctions funcs) {
  bool need_start = !self.did_continuous_updates();
  
  switch(funcs) {
    case DynamicFunctions::Continue:
      self.did_continuous_updates(true);
      self._assigned_result = value;
      self.dynamic.assign(PMATH_CPP_MOVE(value), need_start, true, false);
      break;
    
    case DynamicFunctions::Finish:
      self.did_continuous_updates(false);
      self._assigned_result = value;
      self.dynamic.assign(PMATH_CPP_MOVE(value), need_start, true, true);
      break;
    
    case DynamicFunctions::OnlyPost:
      self.did_continuous_updates(false);
      self._assigned_result = value;
      self.dynamic.assign(PMATH_CPP_MOVE(value), false, false, true);
      break;
  }
}

//} ... class InputFieldBox::Impl
