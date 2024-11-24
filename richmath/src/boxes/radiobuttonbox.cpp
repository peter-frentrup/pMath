#include <boxes/radiobuttonbox.h>
#include <eval/eval-contexts.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_RadioButtonBox;
extern pmath_symbol_t richmath_System_True;

namespace richmath { 
  class RadioButtonBox::Impl {
    public:
      Impl(RadioButtonBox &self) : self{self} {}
      
      void finish_update_value();
      ContainerType calc_type(Expr result);
      Expr to_literal();
    
    private:
      RadioButtonBox &self;
  };

  namespace strings {
    extern String DollarContext_namespace;
    extern String RadioButton;
  }
}

//{ class RadioButtonBox ...

RadioButtonBox::RadioButtonBox()
  : base(ContainerType::RadioButtonUnchecked)
{
  first_paint(true);
  dynamic.init(this, Expr());
}

bool RadioButtonBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_RadioButtonBox))
    return false;
    
  Expr options(PMATH_UNDEFINED);
  Expr new_value;
  
  if(expr.expr_length() >= 2) {
    options = Expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
    
    if(options.is_null())
      return false;
      
    new_value = expr[2];
  }
  else {
    new_value = Symbol(richmath_System_True);
  }
  
  /* now success is guaranteed */
  
  value = new_value;
  
  if(expr.expr_length() >= 1) {
    Expr dyn_expr = expr[1];
    if(dynamic.expr() != dyn_expr || has(opts, BoxInputFlags::ForceResetDynamic)) {
      dynamic = dyn_expr;
      must_update(true);
      is_initialized(false);
    }
  }
  else
    dynamic = Symbol(richmath_System_False);
    
  reset_style();
  style.add_pmath(options);
    
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

void RadioButtonBox::paint(Context &context) {
  Impl(*this).finish_update_value();
  
  if(type == ContainerType::RadioButtonChecked || first_paint()) {
    old_type = type;
    animation = nullptr;
  }
  
  base::paint(context);
  first_paint(false);
}

Expr RadioButtonBox::to_pmath_symbol() {
  return Symbol(richmath_System_RadioButtonBox);
}

Expr RadioButtonBox::to_pmath_impl(BoxOutputFlags flags) {
  Gather gather;
  
  Expr val;
  if(has(flags, BoxOutputFlags::Literal))
    val = Impl(*this).to_literal();
  else
    val = dynamic.expr();
    
  Gather::emit(val);
  Gather::emit(value);
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style.get(BaseStyleName, &s) && s == strings::RadioButton)
      with_inherited = false;
    
    style.emit_to_pmath(with_inherited);
  }
  
  Expr result = gather.end();
  if(value == richmath_System_True && result.expr_length() == 2) {
    return Call(Symbol(richmath_System_RadioButtonBox), dynamic.expr());
  }
  
  result.set(0, Symbol(richmath_System_RadioButtonBox));
  return result;
}

void RadioButtonBox::reset_style() {
  style.reset(strings::RadioButton);
}

void RadioButtonBox::dynamic_finished(Expr info, Expr result) {
  type = Impl(*this).calc_type(result);
  
  request_repaint_all();
}

VolatileSelection RadioButtonBox::dynamic_to_literal(int start, int end) {
  dynamic = Impl(*this).to_literal();
  return {this, start, end};
}

void RadioButtonBox::on_mouse_down(MouseEvent &event) {
  if(event.left && enabled())
    dynamic.assign(value, true, false, false);
  
  base::on_mouse_down(event);
}

void RadioButtonBox::click() {
  if(!enabled())
    return;
  
  dynamic.assign(value, false, true, true);
}

//} ... class RadioButtonBox

//{ class RadioButtonBox::Impl ...

void RadioButtonBox::Impl::finish_update_value() {
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

  self.type = calc_type(val);
}

ContainerType RadioButtonBox::Impl::calc_type(Expr result) {
  if(result == self.value)
    return ContainerType::RadioButtonChecked;
    
  return ContainerType::RadioButtonUnchecked;
}

Expr RadioButtonBox::Impl::to_literal() {
  if(!self.dynamic.is_dynamic())
    return self.dynamic.expr();
  
  switch(self.type) {
    case ContainerType::CheckboxChecked:
    case ContainerType::OpenerTriangleOpened:
    case ContainerType::RadioButtonChecked:
      return self.value;
    
    default:
      break;
  }
  
  if(self.value == richmath_System_False)
    return Symbol(richmath_System_None);
  
  return Symbol(richmath_System_False);
}

//} ... class RadioButtonBox::Impl
