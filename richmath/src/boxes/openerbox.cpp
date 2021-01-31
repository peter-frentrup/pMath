#include <boxes/openerbox.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_OpenerBox;
extern pmath_symbol_t richmath_System_True;

namespace richmath {
  namespace strings {
    extern String Opener;
  }
  
  class OpenerBox::Impl {
    public:
      Impl(OpenerBox &_self) : self(_self) {}
      
      Expr next_value_when_clicked();
      
    private:
      OpenerBox &self;
  };
}

//{ class OpenerBox ...

OpenerBox::OpenerBox()
  : base(OpenerTriangleClosed),
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
    must_update = true;
    dynamic = dyn_expr;
  }
  
  finish_load_from_object(std::move(expr));
  return true;
}

void OpenerBox::paint(Context &context) {
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
      return Symbol(richmath_System_True);
      
    case CheckboxUnchecked:
    case OpenerTriangleClosed:
    case RadioButtonUnchecked:
      return Symbol(richmath_System_False);
    
    default:
      break;
  }
  
  return dynamic.get_value_now();
}

VolatileSelection OpenerBox::dynamic_to_literal(int start, int end) {
  dynamic = to_literal();
  return {this, start, end};
}

ContainerType OpenerBox::calc_type(Expr result) {
  if(result == richmath_System_True)
    return OpenerTriangleOpened;
  
  return OpenerTriangleClosed;
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
    
  dynamic.assign(std::move(value), false, true, true);
}

//} ... class OpenerBox

//{ class OpenerBox::Impl ...

Expr OpenerBox::Impl::next_value_when_clicked() {
  if(self.type == OpenerTriangleOpened) 
    return Symbol(richmath_System_False);
  else 
    return Symbol(richmath_System_True);
}

//} ... class OpenerBox::Impl
