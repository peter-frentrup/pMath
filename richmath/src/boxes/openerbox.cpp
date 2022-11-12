#include <boxes/openerbox.h>
#include <eval/eval-contexts.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_OpenerBox;
extern pmath_symbol_t richmath_System_True;

namespace richmath {
  namespace strings {
    extern String DollarContext_namespace;
    extern String Opener;
  }
  
  class OpenerBox::Impl {
    public:
      Impl(OpenerBox &_self) : self(_self) {}
      
      void finish_update_value();
      Expr next_value_when_clicked();
      ContainerType calc_type(Expr result);
      Expr to_literal();
      
    private:
      OpenerBox &self;
  };
}

//{ class OpenerBox ...

OpenerBox::OpenerBox()
  : base(ContainerType::OpenerTriangleClosed),
    mouse_down_value(PMATH_UNDEFINED)
{
  dynamic.init(this, Expr());
}

bool OpenerBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_OpenerBox)
    return false;
  
  if(expr.expr_length() < 1)
    return false;
  
  Expr options(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;

  /* now success is guaranteed */

  reset_style();
  style->add_pmath(options);
  
  Expr dyn_expr = expr[1];
  if(dynamic.expr() != dyn_expr || has(opts, BoxInputFlags::ForceResetDynamic)) {
    dynamic = dyn_expr;
    must_update(true);
    is_initialized(false);
  }
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

void OpenerBox::paint(Context &context) {
  Impl(*this).finish_update_value();
  base::paint(context);
}

Expr OpenerBox::to_pmath_symbol() {
  return Symbol(richmath_System_OpenerBox); 
}

Expr OpenerBox::to_pmath_impl(BoxOutputFlags flags) {
  Gather gather;
  
  Expr val;
  if(has(flags, BoxOutputFlags::Literal))
    val = Impl(*this).to_literal();
  else
    val = dynamic.expr();
    
  Gather::emit(val);
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style->get(BaseStyleName, &s) && s == strings::Opener)
      with_inherited = false;
    
    style->emit_to_pmath(with_inherited);
  }
    
  Expr result = gather.end();
  result.set(0, Symbol(richmath_System_OpenerBox));
  return result;
}

void OpenerBox::reset_style() {
  Style::reset(style, strings::Opener);
}

void OpenerBox::dynamic_finished(Expr info, Expr result) {
  type = Impl(*this).calc_type(result);
  
  request_repaint_all();
}

VolatileSelection OpenerBox::dynamic_to_literal(int start, int end) {
  dynamic = Impl(*this).to_literal();
  return {this, start, end};
}

void OpenerBox::on_mouse_down(MouseEvent &event) {
  if(event.left && enabled()) {
    mouse_down_value = Impl(*this).next_value_when_clicked();
    dynamic.assign(mouse_down_value, true, false, false);
  }
  
  base::on_mouse_down(event);
}

void OpenerBox::on_mouse_up(MouseEvent &event) {
  base::on_mouse_up(event);
  
  mouse_down_value = Expr(PMATH_UNDEFINED);
}

void OpenerBox::on_mouse_cancel() {
  mouse_down_value = Expr(PMATH_UNDEFINED);
  
  base::on_mouse_cancel();
}

void OpenerBox::click() {
  if(!enabled())
    return;
  
  Expr value(PMATH_UNDEFINED);
  if(mouse_down_value == PMATH_UNDEFINED)
    value = Impl(*this).next_value_when_clicked();
  else
    swap(mouse_down_value, value);
    
  dynamic.assign(PMATH_CPP_MOVE(value), false, true, true);
}

//} ... class OpenerBox

//{ class OpenerBox::Impl ...

void OpenerBox::Impl::finish_update_value() {
  if(!self.must_update())
    return;
  
  self.must_update(false);
  
  bool was_initialized = self.is_initialized();
  self.is_initialized(true);
  
  Expr val;
  if(!self.dynamic.get_value(&val))
    return;
  
  if(!was_initialized && val.is_symbol() && self.dynamic.is_dynamic_of(val)) {
    val = Symbol(richmath_System_False);
    
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

Expr OpenerBox::Impl::next_value_when_clicked() {
  if(self.type == ContainerType::OpenerTriangleOpened) 
    return Symbol(richmath_System_False);
  else 
    return Symbol(richmath_System_True);
}

ContainerType OpenerBox::Impl::calc_type(Expr result) {
  if(result == richmath_System_True)
    return ContainerType::OpenerTriangleOpened;
  
  return ContainerType::OpenerTriangleClosed;
}

Expr OpenerBox::Impl::to_literal() {
  if(!self.dynamic.is_dynamic())
    return self.dynamic.expr();
  
  switch(self.type) {
    case ContainerType::CheckboxChecked:
    case ContainerType::OpenerTriangleOpened:
    case ContainerType::RadioButtonChecked:
      return Symbol(richmath_System_True);
      
    case ContainerType::CheckboxUnchecked:
    case ContainerType::OpenerTriangleClosed:
    case ContainerType::RadioButtonUnchecked:
      return Symbol(richmath_System_False);
    
    default:
      break;
  }
  
  Expr val = self.dynamic.get_value_now();
  val = EvaluationContexts::replace_symbol_namespace(
          PMATH_CPP_MOVE(val),
          EvaluationContexts::resolve_context(&self),
          strings::DollarContext_namespace);
  return val;
}

//} ... class OpenerBox::Impl
