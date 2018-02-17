#include <boxes/checkboxbox.h>

using namespace richmath;

//{ class CheckboxBox ...

CheckboxBox::CheckboxBox()
  : EmptyWidgetBox(CheckboxIndeterminate)
{
  dynamic.init(this, Expr());
}
      
bool CheckboxBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != PMATH_SYMBOL_CHECKBOXBOX)
    return false;
  
  Expr options(PMATH_UNDEFINED);
  
  if(expr.expr_length() >= 2) {
    Expr _values = expr[2];
    
    if(_values.expr_length() != 2 || _values[0] != PMATH_SYMBOL_LIST) {
      _values = Expr();
      options = Expr(pmath_options_extract(expr.get(), 1));
    }
    else 
      options = Expr(pmath_options_extract(expr.get(), 2));
    
    if(options.is_null())
      return false;
      
    values = _values;
  }
  else {
    values = Expr();
  }

  /* now success is guaranteed */

  if(style){
    reset_style();
    style->add_pmath(options);
  }
  else if(options != PMATH_UNDEFINED)
    style = new Style(options);
  
  if(expr.expr_length() >= 1) {
    dynamic = expr[1];
  }
  else {
    dynamic = Symbol(PMATH_SYMBOL_FALSE);
  }
  
  return true;
}

void CheckboxBox::paint(Context *context) {
  if(must_update) {
    must_update = false;
    
    Expr val;
    if(dynamic.get_value(&val))
      type = calc_type(val);
  }
  
  EmptyWidgetBox::paint(context);
}

Expr CheckboxBox::to_pmath(BoxOutputFlags flags) {
  Gather gather;
  
  Expr val = dynamic.expr();
  if(has(flags, BoxOutputFlags::Literal) && dynamic.is_dynamic())
    val = val[1];
    
  Gather::emit(val);
  
  if(values.is_null())
    Gather::emit(List(Symbol(PMATH_SYMBOL_FALSE), Symbol(PMATH_SYMBOL_TRUE)));
  else
    Gather::emit(values);
    
  if(style)
    style->emit_to_pmath(false);
    
  Expr result = gather.end();
  if(values.is_null() && result.expr_length() == 2) {
    return Call(Symbol(PMATH_SYMBOL_CHECKBOXBOX), val);
  }
  
  result.set(0, Symbol(PMATH_SYMBOL_CHECKBOXBOX));
  return result;
}

void CheckboxBox::dynamic_finished(Expr info, Expr result) {
  type = calc_type(result);
  
  request_repaint_all();
}

Box *CheckboxBox::dynamic_to_literal(int *start, int *end) {
  if(dynamic.is_dynamic())
    dynamic = dynamic.expr()[1];
  return this;
}

ContainerType CheckboxBox::calc_type(Expr result) {
  if(values.is_null()) {
    if(result == PMATH_SYMBOL_FALSE)
      return CheckboxUnchecked;
      
    if(result == PMATH_SYMBOL_TRUE)
      return CheckboxChecked;
  }
  
  if(values.expr_length() == 2) {
    if(result == values[1])
      return CheckboxUnchecked;
      
    if(result == values[2])
      return CheckboxChecked;
  }
  
  return CheckboxIndeterminate;
}

void CheckboxBox::click() {
  if(type == CheckboxChecked) {
    if(values.is_null()) {
      dynamic.assign(Symbol(PMATH_SYMBOL_FALSE));
    }
    else if(values.expr_length() == 2) {
      dynamic.assign(values[1]);
    }
  }
  else {
    if(values.is_null()) {
      dynamic.assign(Symbol(PMATH_SYMBOL_TRUE));
    }
    else if(values.expr_length() == 2) {
      dynamic.assign(values[2]);
    }
  }
  
  // EmptyWidgetBox::click();
}

//} ... class CheckboxBox
