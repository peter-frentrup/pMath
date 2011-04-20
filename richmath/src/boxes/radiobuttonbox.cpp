#include <boxes/radiobuttonbox.h>

using namespace richmath;

//{ class RadioButtonBox ...

RadioButtonBox::RadioButtonBox()
: EmptyWidgetBox(RadioButtonUnchecked),
  first_paint(true)
{
  dynamic.init(this, Expr());
}

RadioButtonBox *RadioButtonBox::create(Expr expr){
  RadioButtonBox *rb = new RadioButtonBox();
  Expr options(PMATH_UNDEFINED);
  
  if(expr.expr_length() >= 1){
    rb->dynamic = expr[1];
  }
  else{
    rb->dynamic = Symbol(PMATH_SYMBOL_FALSE);
  }
  
  if(expr.expr_length() >= 2){
    rb->value = expr[2];
    options = Expr(pmath_options_extract(expr.get(), 2));
  }
  else{
    rb->value = Symbol(PMATH_SYMBOL_TRUE);
  }
  
  if(options.is_null()){
    delete rb;
    return 0;
  }
  
  if(options != PMATH_UNDEFINED){
    if(rb->style)
      rb->style->add_pmath(options);
    else
      rb->style = new Style(options);
  }
  
  return rb;
}

void RadioButtonBox::paint(Context *context){
  if(must_update){
    must_update = false;
    
    Expr val;
    if(dynamic.get_value(&val))
      type = calc_type(val);
  }
  
  if(first_paint)
    old_type = type;
  
  EmptyWidgetBox::paint(context);
  first_paint = false;
}

Expr RadioButtonBox::to_pmath(bool parseable){
  Gather gather;
  
  Gather::emit(dynamic.expr());
  Gather::emit(value);
  
  if(style)
    style->emit_to_pmath(false, false);
  
  Expr result = gather.end();
  if(value == PMATH_SYMBOL_TRUE && result.expr_length() == 2){
    return Call(Symbol(PMATH_SYMBOL_RADIOBUTTONBOX), dynamic.expr());
  }
  
  result.set(0, Symbol(PMATH_SYMBOL_RADIOBUTTONBOX));
  return result;
}

void RadioButtonBox::dynamic_finished(Expr info, Expr result){
  type = calc_type(result);
  
  request_repaint_all();
}

ContainerType RadioButtonBox::calc_type(Expr result){
  if(result == value)
    return RadioButtonChecked;
  
  return RadioButtonUnchecked;
}

void RadioButtonBox::on_mouse_up(MouseEvent &event){
  if(event.left){
    dynamic.assign(value);
  }
  
  EmptyWidgetBox::on_mouse_up(event);
}

//} ... class RadioButtonBox
