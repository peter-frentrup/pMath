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
  
  if(expr.expr_length() >= 1) {
    Expr dyn_expr = expr[1];
    if(dynamic.expr() != dyn_expr || has(opts, BoxInputFlags::ForceResetDynamic)) {
      must_update = true;
      dynamic = dyn_expr;
    }
  }
  else
    dynamic = Symbol(PMATH_SYMBOL_FALSE);
    
  reset_style();
  style->add_pmath(options);
    
  finish_load_from_object(std::move(expr));
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
  
  Expr val;
  if(has(flags, BoxOutputFlags::Literal))
    val = to_literal();
  else
    val = dynamic.expr();
    
  Gather::emit(val);
  Gather::emit(value);
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style->get(BaseStyleName, &s) && s.equals("RadioButton"))
      with_inherited = false;
    
    style->emit_to_pmath(with_inherited);
  }
  
  Expr result = gather.end();
  if(value == PMATH_SYMBOL_TRUE && result.expr_length() == 2) {
    return Call(Symbol(richmath_System_RadioButtonBox), dynamic.expr());
  }
  
  result.set(0, Symbol(richmath_System_RadioButtonBox));
  return result;
}

void RadioButtonBox::reset_style() {
  Style::reset(style, "RadioButton");
}

void RadioButtonBox::dynamic_finished(Expr info, Expr result) {
  type = calc_type(result);
  //pmath_debug_print("[%d RadioButtonBox::dynamic_finished: calc_type -> %d]\n", id(), (int)type);
  
  request_repaint_all();
}

Expr RadioButtonBox::to_literal() {
  if(!dynamic.is_dynamic())
    return dynamic.expr();
  
  switch(type) {
    case CheckboxChecked:
    case OpenerTriangleOpened:
    case RadioButtonChecked:
      return value;
    
    default:
      break;
  }
  
  if(value == PMATH_SYMBOL_FALSE)
    return Symbol(PMATH_SYMBOL_NONE);
  
  return Symbol(PMATH_SYMBOL_FALSE);
}

Box *RadioButtonBox::dynamic_to_literal(int *start, int *end) {
  dynamic = to_literal();
  return this;
}

ContainerType RadioButtonBox::calc_type(Expr result) {
  if(result == value)
    return RadioButtonChecked;
    
  return RadioButtonUnchecked;
}

void RadioButtonBox::click() {
  if(!enabled())
    return;
  
  dynamic.assign(value);
}

//} ... class RadioButtonBox
