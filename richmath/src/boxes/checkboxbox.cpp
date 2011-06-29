#include <boxes/checkboxbox.h>

using namespace richmath;

//{ class CheckboxBox ...

CheckboxBox::CheckboxBox()
: EmptyWidgetBox(CheckboxIndeterminate)
{
  dynamic.init(this, Expr());
}

CheckboxBox *CheckboxBox::create(Expr expr){
  CheckboxBox *cb = new CheckboxBox();
  Expr options(PMATH_UNDEFINED);
  
  if(expr.expr_length() >= 1){
    cb->dynamic = expr[1];
  }
  else{
    cb->dynamic = Symbol(PMATH_SYMBOL_FALSE);
  }
  
  if(expr.expr_length() >= 2){
    cb->values = expr[2];
    
    if(cb->values.expr_length() != 2 || cb->values[0] != PMATH_SYMBOL_LIST){
      cb->values = Expr();
      options = Expr(pmath_options_extract(expr.get(), 1));
    }
    else{
      options = Expr(pmath_options_extract(expr.get(), 2));
    }
  }
  else{
    cb->values = Expr();
  }
  
  if(options.is_null()){
    delete cb;
    return 0;
  }
  
  if(options != PMATH_UNDEFINED){
    if(cb->style)
      cb->style->add_pmath(options);
    else
      cb->style = new Style(options);
  }
  
  return cb;
}

void CheckboxBox::paint(Context *context){
  if(must_update){
    must_update = false;
    
    Expr val;
    if(dynamic.get_value(&val))
      type = calc_type(val);
  }
  
  EmptyWidgetBox::paint(context);
}

Expr CheckboxBox::to_pmath(int flags){
  Gather gather;
  
  Expr val = dynamic.expr();
  if((flags & BoxFlagLiteral) && dynamic.is_dynamic())
    val = val[1];
  
  Gather::emit(val);
  
  if(values.is_null())
    Gather::emit(List(Symbol(PMATH_SYMBOL_FALSE), Symbol(PMATH_SYMBOL_TRUE)));
  else
    Gather::emit(values);
  
  if(style)
    style->emit_to_pmath(false, false);
  
  Expr result = gather.end();
  if(values.is_null() && result.expr_length() == 2){
    return Call(Symbol(PMATH_SYMBOL_CHECKBOXBOX), val);
  }
  
  result.set(0, Symbol(PMATH_SYMBOL_CHECKBOXBOX));
  return result;
}

void CheckboxBox::dynamic_finished(Expr info, Expr result){
  type = calc_type(result);
  
  request_repaint_all();
}

Box *CheckboxBox::dynamic_to_literal(int *start, int *end){
  if(dynamic.is_dynamic())
    dynamic = dynamic.expr()[1];
  return this;
}

ContainerType CheckboxBox::calc_type(Expr result){
  if(values.is_null()){
    if(result == PMATH_SYMBOL_FALSE)
      return CheckboxUnchecked;
    
    if(result == PMATH_SYMBOL_TRUE)
      return CheckboxChecked;
  }
  
  if(values.expr_length() == 2){
    if(result == values[1])
      return CheckboxUnchecked;
    
    if(result == values[2])
      return CheckboxChecked;
  }
  
  return CheckboxIndeterminate;
}

void CheckboxBox::on_mouse_up(MouseEvent &event){
  if(event.left){
    if(type == CheckboxChecked){
      if(values.is_null()){
        dynamic.assign(Symbol(PMATH_SYMBOL_FALSE));
      }
      else if(values.expr_length() == 2){
        dynamic.assign(values[1]);
      }
    }
    else{
      if(values.is_null()){
        dynamic.assign(Symbol(PMATH_SYMBOL_TRUE));
      }
      else if(values.expr_length() == 2){
        dynamic.assign(values[2]);
      }
    }
  }
  
  EmptyWidgetBox::on_mouse_up(event);
}

//} ... class CheckboxBox
