#include <boxes/radiobuttonbox.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_RadioButtonBox;

//{ class RadioButtonBox ...

RadioButtonBox::RadioButtonBox()
  : EmptyWidgetBox(RadioButtonUnchecked),
    first_paint(true)
{
  dynamic.init(this, Expr());
}

bool RadioButtonBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_RadioButtonBox)
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
    new_value = Symbol(PMATH_SYMBOL_TRUE);
  }
  
  /* now success is guaranteed */
  
  value = new_value;
  
  if(expr.expr_length() >= 1)
    dynamic = expr[1];
  else
    dynamic = Symbol(PMATH_SYMBOL_FALSE);
    
  if(style) {
    reset_style();
    style->add_pmath(options);
  }
  else if(options != PMATH_UNDEFINED)
    style = new Style(options);
    
  return true;
}

void RadioButtonBox::paint(Context *context) {
  if(must_update) {
    must_update = false;
    
    Expr val;
    if(dynamic.get_value(&val)) {
      type = calc_type(val);
      //pmath_debug_print("[%d RadioButtonBox::paint: calc_type -> %d]\n", id(), (int)type);
    }
  }
  
  if(type == RadioButtonChecked || first_paint) {
    old_type = type;
    animation = 0;
  }
  
  EmptyWidgetBox::paint(context);
  first_paint = false;
}

Expr RadioButtonBox::to_pmath_symbol() {
  return Symbol(richmath_System_RadioButtonBox);
}

Expr RadioButtonBox::to_pmath(BoxOutputFlags flags) {
  Gather gather;
  
  Expr val = dynamic.expr();
  if(has(flags, BoxOutputFlags::Literal) && dynamic.is_dynamic())
    val = val[1];
    
  Gather::emit(val);
  Gather::emit(value);
  
  if(style)
    style->emit_to_pmath(false);
    
  Expr result = gather.end();
  if(value == PMATH_SYMBOL_TRUE && result.expr_length() == 2) {
    return Call(Symbol(richmath_System_RadioButtonBox), dynamic.expr());
  }
  
  result.set(0, Symbol(richmath_System_RadioButtonBox));
  return result;
}

void RadioButtonBox::dynamic_finished(Expr info, Expr result) {
  type = calc_type(result);
  //pmath_debug_print("[%d RadioButtonBox::dynamic_finished: calc_type -> %d]\n", id(), (int)type);
  
  request_repaint_all();
}

Box *RadioButtonBox::dynamic_to_literal(int *start, int *end) {
  if(dynamic.is_dynamic())
    dynamic = dynamic.expr()[1];
  return this;
}

ContainerType RadioButtonBox::calc_type(Expr result) {
  if(result == value)
    return RadioButtonChecked;
    
  return RadioButtonUnchecked;
}

void RadioButtonBox::click() {
  dynamic.assign(value);
  //EmptyWidgetBox::click();
}

//} ... class RadioButtonBox
