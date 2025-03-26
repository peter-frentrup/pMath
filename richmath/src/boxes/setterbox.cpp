#include <boxes/setterbox.h>

#include <boxes/mathsequence.h>
#include <eval/eval-contexts.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_SetterBox;

namespace richmath { 
  class SetterBox::Impl {
    public:
      Impl(SetterBox &self) : self{self} {}
      
      void finish_update_value();
      Expr to_literal();
    
    private:
      SetterBox &self;
  };

  namespace strings {
    extern String DollarContext_namespace;
    extern String Setter;
  }
}

//{ class SetterBox ...

SetterBox::SetterBox(AbstractSequence *content)
  : base(content, ContainerType::PaletteButton)
{
  dynamic.init(this, Expr());
  must_update(true);
}

bool SetterBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_SetterBox))
    return false;
    
  if(expr.expr_length() < 3)
    return false;
    
  Expr options(pmath_options_extract_ex(expr.get(), 3, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  
  if(options.is_null())
    return false;
    
  /* now success is guaranteed */
  
  if(dynamic.expr() != expr[1] || has(opts, BoxInputFlags::ForceResetDynamic)) {
    dynamic = expr[1];
    must_update(true);
    is_initialized(false);
  }
  
  if(value != expr[2]) {
    value = expr[2];
    must_update(true);
  }
  
  if(must_update()) {
    is_down(!dynamic.is_dynamic() && dynamic.expr() == value);
  }
  
  _content->load_from_object(expr[3], opts);
  
  reset_style();
  style.add_pmath(options);
    
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

ControlState SetterBox::calc_state(Context &context) {
  if(is_down()) {
    //if(mouse_inside()
    return ControlState::PressedHovered;
    
    //return ControlState::Pressed;
  }
  
  ControlState state = base::calc_state(context);
  //if(state == ControlState::Normal)
  //  return ControlState::Hovered;
  return state;
}

void SetterBox::paint(Context &context) {
  Impl(*this).finish_update_value();
  base::paint(context);
}

Expr SetterBox::to_pmath_symbol() {
  return Symbol(richmath_System_SetterBox);
}

Expr SetterBox::to_pmath_impl(BoxOutputFlags flags) {
  Gather g;
  
  if(has(flags, BoxOutputFlags::Literal))
    g.emit(Impl(*this).to_literal());
  else
    g.emit(dynamic.expr());
  
  g.emit(value);
  g.emit(_content->to_pmath(flags - BoxOutputFlags::Parseable));
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style.get(BaseStyleName, &s) && s == strings::Setter)
      with_inherited = false;
    
    style.emit_to_pmath(with_inherited);
  }
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_SetterBox));
  return e;
}

void SetterBox::reset_style() {
  style.reset(strings::Setter);
}

void SetterBox::on_mouse_down(MouseEvent &event) {
  if(event.left && enabled())
    dynamic.assign(value, true, false, false);
  
  base::on_mouse_down(event);
}

void SetterBox::click() {
  if(!enabled())
    return;
    
  dynamic.assign(value, false, true, true);
  is_down(true); // TODO: reset if assignment fails
  request_repaint_all();
}

void SetterBox::dynamic_updated() {
  if(must_update())
    return;
    
  must_update(true);
  request_repaint_all();
}

void SetterBox::dynamic_finished(Expr info, Expr result) {
  bool new_is_down = result == value;
  
  if(is_down() != new_is_down) {
    is_down(new_is_down);
    request_repaint_all();
  }
}

VolatileSelection SetterBox::dynamic_to_literal(int start, int end) {
  dynamic = Impl(*this).to_literal();
  return {this, start, end};
}

//} ... class SetterBox

//{ class SetterBox::Impl ...

void SetterBox::Impl::finish_update_value() {
  if(!self.must_update())
    return;
    
  self.must_update(false);
  
  bool was_initialized = self.is_initialized();
  self.is_initialized(true);
  
  Expr val;
  if(!self.dynamic.get_value(&val))
    return;
    
  if(!was_initialized && val.is_symbol() && self.dynamic.is_dynamic_of(val)) {
    val = self.value;
    self.dynamic.assign(val, true, true, true);
    
    if(!self.dynamic.get_value(&val))
      return;
  }
  
  val = EvaluationContexts::replace_symbol_namespace(
          PMATH_CPP_MOVE(val), 
          EvaluationContexts::resolve_context(&self), 
          strings::DollarContext_namespace);

  self.is_down(val == self.value);
}

Expr SetterBox::Impl::to_literal() {
  if(!self.dynamic.is_dynamic())
    return self.dynamic.expr();
  
  if(self.is_down())
    return self.value;
  
  if(self.value == richmath_System_False)
    return Symbol(richmath_System_None);
  
  return Symbol(richmath_System_False);
}

//} ... class SetterBox::Impl
