#include <boxes/setterbox.h>

#include <boxes/mathsequence.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_SetterBox;

namespace richmath { namespace strings {
  extern String Setter;
}}

//{ class SetterBox ...

SetterBox::SetterBox(MathSequence *content)
  : base(content, PaletteButton),
    must_update(true),
    is_down(false)
{
  dynamic.init(this, Expr());
}

bool SetterBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_SetterBox)
    return false;
    
  if(expr.expr_length() < 3)
    return false;
    
  Expr options(pmath_options_extract_ex(expr.get(), 3, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  
  if(options.is_null())
    return false;
    
  /* now success is guaranteed */
  
  if(dynamic.expr() != expr[1] || has(opts, BoxInputFlags::ForceResetDynamic)) {
    must_update = true;
    dynamic     = expr[1];
  }
  
  value   = expr[2];
  is_down = (!dynamic.is_dynamic() && dynamic.expr() == value);
  
  _content->load_from_object(expr[3], opts);
  
  reset_style();
  style->add_pmath(options);
    
  finish_load_from_object(std::move(expr));
  return true;
}

ControlState SetterBox::calc_state(Context &context) {
  if(is_down) {
    //if(mouse_inside)
    return PressedHovered;
    
    //return Pressed;
  }
  
  ControlState state = base::calc_state(context);
  //if(state == Normal)
  //  return Hovered;
  return state;
}

void SetterBox::paint(Context &context) {
  if(must_update) {
    must_update = false;
    
    Expr dyn_val;
    if(dynamic.get_value(&dyn_val)) {
      is_down = (dyn_val == value);
    }
  }
  
  base::paint(context);
}

Expr SetterBox::to_pmath_symbol() {
  return Symbol(richmath_System_SetterBox);
}

Expr SetterBox::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  if(has(flags, BoxOutputFlags::Literal))
    g.emit(to_literal());
  else
    g.emit(dynamic.expr());
  
  g.emit(value);
  g.emit(_content->to_pmath(flags - BoxOutputFlags::Parseable));
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style->get(BaseStyleName, &s) && s == strings::Setter)
      with_inherited = false;
    
    style->emit_to_pmath(with_inherited);
  }
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_SetterBox));
  return e;
}

void SetterBox::reset_style() {
  Style::reset(style, strings::Setter);
}

void SetterBox::on_mouse_down(MouseEvent &event) {
  if(event.left && enabled())
    dynamic.assign(value, true, false, false);
  
  base::on_mouse_down(event);
}

void SetterBox::click() {
  if(!enabled())
    return;
    
  dynamic.assign(value, false, true, true);
  is_down = true;
  request_repaint_all();
}

void SetterBox::dynamic_updated() {
  if(must_update)
    return;
    
  must_update = true;
  request_repaint_all();
}

void SetterBox::dynamic_finished(Expr info, Expr result) {
  bool new_is_down = result == value;
  
  if(is_down != new_is_down) {
    is_down = new_is_down;
    request_repaint_all();
  }
}

Expr SetterBox::to_literal() {
  if(!dynamic.is_dynamic())
    return dynamic.expr();
  
  if(is_down)
    return value;
  
  if(value == PMATH_SYMBOL_FALSE)
    return Symbol(PMATH_SYMBOL_NONE);
  
  return Symbol(PMATH_SYMBOL_FALSE);
}

VolatileSelection SetterBox::dynamic_to_literal(int start, int end) {
  dynamic = to_literal();
  return {this, start, end};
}

//} ... class SetterBox

