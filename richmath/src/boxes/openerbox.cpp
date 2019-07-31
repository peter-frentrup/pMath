#include <boxes/openerbox.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_OpenerBox;

//{ class OpenerBox ...

OpenerBox::OpenerBox()
  : EmptyWidgetBox(OpenerTriangleClosed)
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
    must_update = true;
    dynamic = dyn_expr;
  }
  
  finish_load_from_object(std::move(expr));
  return true;
}

void OpenerBox::paint(Context *context) {
  if(must_update) {
    must_update = false;
    
    Expr val;
    if(dynamic.get_value(&val))
      type = calc_type(val);
  }
  
  base::paint(context);
}

Expr OpenerBox::to_pmath_symbol() {
  return Symbol(richmath_System_OpenerBox); 
}

Expr OpenerBox::to_pmath(BoxOutputFlags flags) {
  Gather gather;
  
  Expr val;
  if(has(flags, BoxOutputFlags::Literal))
    val = to_literal();
  else
    val = dynamic.expr();
    
  Gather::emit(val);
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style->get(BaseStyleName, &s) && s.equals("Opener"))
      with_inherited = false;
    
    style->emit_to_pmath(with_inherited);
  }
    
  Expr result = gather.end();
  result.set(0, Symbol(richmath_System_OpenerBox));
  return result;
}

void OpenerBox::reset_style() {
  Style::reset(style, "Opener");
}

void OpenerBox::dynamic_finished(Expr info, Expr result) {
  type = calc_type(result);
  
  request_repaint_all();
}

Expr OpenerBox::to_literal() {
  if(!dynamic.is_dynamic())
    return dynamic.expr();
  
  switch(type) {
    case CheckboxChecked:
    case OpenerTriangleOpened:
    case RadioButtonChecked:
      return Symbol(PMATH_SYMBOL_TRUE);
      
    case CheckboxUnchecked:
    case OpenerTriangleClosed:
    case RadioButtonUnchecked:
      return Symbol(PMATH_SYMBOL_FALSE);
    
    default:
      break;
  }
  
  return dynamic.get_value_now();
}

Box *OpenerBox::dynamic_to_literal(int *start, int *end) {
  dynamic = to_literal();
  return this;
}

ContainerType OpenerBox::calc_type(Expr result) {
  if(result == PMATH_SYMBOL_TRUE)
    return OpenerTriangleOpened;
  
  return OpenerTriangleClosed;
}

void OpenerBox::click() {
  if(type == OpenerTriangleOpened) 
    dynamic.assign(Symbol(PMATH_SYMBOL_FALSE));
  else 
    dynamic.assign(Symbol(PMATH_SYMBOL_TRUE));
}

//} ... class OpenerBox
