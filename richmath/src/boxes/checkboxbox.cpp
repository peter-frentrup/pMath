#include <boxes/checkboxbox.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_CheckboxBox;
extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_True;

namespace richmath {
  namespace strings {
    extern String Checkbox;
  }

  class CheckboxBox::Impl {
    public:
      Impl(CheckboxBox &_self) : self(_self) {}
      
      Expr next_value_when_clicked();
      
    private:
      CheckboxBox &self;
  };
}

//{ class CheckboxBox ...

CheckboxBox::CheckboxBox()
  : base(CheckboxIndeterminate),
    mouse_down_value(PMATH_UNDEFINED)
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
      must_update = true;
      dynamic = dyn_expr;
    }
  }
  else {
    dynamic = Symbol(richmath_System_False);
  }
  
  finish_load_from_object(std::move(expr));
  return true;
}

void CheckboxBox::paint(Context &context) {
  if(must_update) {
    must_update = false;
    
    Expr val;
    if(dynamic.get_value(&val))
      type = calc_type(val);
  }
  
  base::paint(context);
}

Expr CheckboxBox::to_pmath_symbol() {
  return Symbol(richmath_System_CheckboxBox); 
}

Expr CheckboxBox::to_pmath(BoxOutputFlags flags) {
  Gather gather;
  
  Expr val;
  if(has(flags, BoxOutputFlags::Literal))
    val = to_literal();
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
  type = calc_type(result);
  
  request_repaint_all();
}

Expr CheckboxBox::to_literal() {
  if(!dynamic.is_dynamic())
    return dynamic.expr();
  
  switch(type) {
    case CheckboxChecked:
    case OpenerTriangleOpened:
    case RadioButtonChecked:
      if(values.expr_length() == 2)
        return values[2];
      else
        return Symbol(richmath_System_True);
      
    case CheckboxUnchecked:
    case OpenerTriangleClosed:
    case RadioButtonUnchecked:
      if(values.expr_length() == 2)
        return values[1];
      else
        return Symbol(richmath_System_False);
    
    default:
      break;
  }
  
  return dynamic.get_value_now();
}

VolatileSelection CheckboxBox::dynamic_to_literal(int start, int end) {
  dynamic = to_literal();
  return {this, start, end};
}

ContainerType CheckboxBox::calc_type(Expr result) {
  if(values.is_null()) {
    if(result == richmath_System_False)
      return CheckboxUnchecked;
      
    if(result == richmath_System_True)
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

//} ... class CheckboxBox::Impl
