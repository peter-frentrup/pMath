#include <boxes/checkboxbox.h>
#include <eval/eval-contexts.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_CheckboxBox;
extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_True;

namespace richmath {
  namespace strings {
    extern String DollarContext_namespace;
    extern String Checkbox;
  }

  class CheckboxBox::Impl {
    public:
      Impl(CheckboxBox &_self) : self(_self) {}
      
      void finish_update_value();
      Expr next_value_when_clicked();
      ContainerType calc_type(Expr result);
      Expr to_literal();
      
    private:
      CheckboxBox &self;
  };
}

//{ class CheckboxBox ...

CheckboxBox::CheckboxBox()
  : base(CheckboxIndeterminate),
    mouse_down_value(PMATH_UNDEFINED),
    is_initialized(false)
{
  dynamic.init(this, Expr());
}

bool CheckboxBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_CheckboxBox)
    return false;
  
  Expr options(PMATH_UNDEFINED);
  
  if(expr.expr_length() >= 2) {
    Expr _values = expr[2];
    
    if(_values.expr_length() != 2 || _values[0] != richmath_System_List) {
      _values = Expr();
      options = Expr(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
    }
    else 
      options = Expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
    
    if(options.is_null())
      return false;
      
    values = _values;
  }
  else {
    values = Expr();
  }

  /* now success is guaranteed */

  if(style) {
    reset_style();
    style->add_pmath(options);
  }
  else if(options != PMATH_UNDEFINED)
    style = new Style(options);
  
  if(expr.expr_length() >= 1) {
    Expr dyn_expr = expr[1];
    if(dynamic.expr() != dyn_expr || has(opts, BoxInputFlags::ForceResetDynamic)) {
      dynamic        = dyn_expr;
      must_update    = true;
      is_initialized = false;
    }
  }
  else {
    dynamic = Symbol(richmath_System_False);
  }
  
  finish_load_from_object(std::move(expr));
  return true;
}

void CheckboxBox::paint(Context &context) {
  Impl(*this).finish_update_value();
  base::paint(context);
}

Expr CheckboxBox::to_pmath_symbol() {
  return Symbol(richmath_System_CheckboxBox); 
}

Expr CheckboxBox::to_pmath(BoxOutputFlags flags) {
  Gather gather;
  
  Expr val;
  if(has(flags, BoxOutputFlags::Literal))
    val = Impl(*this).to_literal();
  else
    val = dynamic.expr();
    
  Gather::emit(val);
  
  if(values.is_null())
    Gather::emit(List(Symbol(richmath_System_False), Symbol(richmath_System_True)));
  else
    Gather::emit(values);
    
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style->get(BaseStyleName, &s) && s == strings::Checkbox)
      with_inherited = false;
    
    style->emit_to_pmath(with_inherited);
  }
  
  Expr result = gather.end();
  if(values.is_null() && result.expr_length() == 2) {
    return Call(Symbol(richmath_System_CheckboxBox), val);
  }
  
  result.set(0, Symbol(richmath_System_CheckboxBox));
  return result;
}

void CheckboxBox::reset_style() {
  Style::reset(style, strings::Checkbox);
}

void CheckboxBox::dynamic_finished(Expr info, Expr result) {
  type = Impl(*this).calc_type(result);
  
  request_repaint_all();
}

VolatileSelection CheckboxBox::dynamic_to_literal(int start, int end) {
  dynamic = Impl(*this).to_literal();
  return {this, start, end};
}

void CheckboxBox::on_mouse_down(MouseEvent &event) {
  if(event.left && enabled()) {
    mouse_down_value = Impl(*this).next_value_when_clicked();
    dynamic.assign(mouse_down_value, true, false, false);
  }
  
  base::on_mouse_down(event);
}

void CheckboxBox::on_mouse_up(MouseEvent &event) {
  base::on_mouse_up(event);
  
  mouse_down_value = Expr(PMATH_UNDEFINED);
}

void CheckboxBox::on_mouse_cancel() {
  mouse_down_value = Expr(PMATH_UNDEFINED);
  
  base::on_mouse_cancel();
}

void CheckboxBox::click() {
  if(!enabled())
    return;
  
  Expr value(PMATH_UNDEFINED);
  if(mouse_down_value == PMATH_UNDEFINED)
    value = Impl(*this).next_value_when_clicked();
  else
    swap(mouse_down_value, value);
    
  dynamic.assign(std::move(value), false, true, true);
}

//} ... class CheckboxBox

//{ class CheckboxBox::Impl ...

void CheckboxBox::Impl::finish_update_value() {
  if(!self.must_update)
    return;
  
  self.must_update = false;
  
  bool was_initialized = self.is_initialized;
  self.is_initialized = true;
  
  Expr val;
  if(self.dynamic.get_value(&val)) {
    if(!was_initialized && val.is_symbol() && self.dynamic.is_dynamic_of(val)) {
      if(self.values.expr_length() == 2 && self.values[0] == richmath_System_List)
        val = self.values[1];
      else
        val = Symbol(richmath_System_False);
      
      self.dynamic.assign(val, true, true, true);
    }
    else {
      val = EvaluationContexts::replace_symbol_namespace(
              std::move(val), 
              EvaluationContexts::resolve_context(&self), 
              strings::DollarContext_namespace);
    }

    self.type = calc_type(val);
  }
}

Expr CheckboxBox::Impl::next_value_when_clicked() {
  if(self.type == CheckboxChecked) {
    if(self.values.expr_length() == 2 && self.values[0] == richmath_System_List)
      return self.values[1];
    else
      return Symbol(richmath_System_False);
  }
  else {
    if(self.values.expr_length() == 2 && self.values[0] == richmath_System_List)
      return self.values[2];
    else
      return Symbol(richmath_System_True);
  }
}

ContainerType CheckboxBox::Impl::calc_type(Expr result) {
  if(self.values.is_null()) {
    if(result == richmath_System_False)
      return CheckboxUnchecked;
      
    if(result == richmath_System_True)
      return CheckboxChecked;
  }
  
  if(self.values.expr_length() == 2) {
    if(result == self.values[1])
      return CheckboxUnchecked;
      
    if(result == self.values[2])
      return CheckboxChecked;
  }
  
  return CheckboxIndeterminate;
}

Expr CheckboxBox::Impl::to_literal() {
  if(!self.dynamic.is_dynamic())
    return self.dynamic.expr();
  
  switch(self.type) {
    case CheckboxChecked:
    case OpenerTriangleOpened:
    case RadioButtonChecked:
      if(self.values.expr_length() == 2)
        return self.values[2];
      else
        return Symbol(richmath_System_True);
      
    case CheckboxUnchecked:
    case OpenerTriangleClosed:
    case RadioButtonUnchecked:
      if(self.values.expr_length() == 2)
        return self.values[1];
      else
        return Symbol(richmath_System_False);
    
    default:
      break;
  }
  
  Expr val = self.dynamic.get_value_now();
  val = EvaluationContexts::replace_symbol_namespace(
          std::move(val),
          EvaluationContexts::resolve_context(&self),
          strings::DollarContext_namespace);
  return val;
}

//} ... class CheckboxBox::Impl
