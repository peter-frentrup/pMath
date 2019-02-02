#include <boxes/setterbox.h>

#include <boxes/mathsequence.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_SetterBox;

//{ class SetterBox ...

SetterBox::SetterBox(MathSequence *content)
  : ContainerWidgetBox(PaletteButton, content),
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
  
  if(style) {
    style->clear();
    style->add_pmath(options);
  }
  else if(options != PMATH_UNDEFINED)
    style = new Style(options);
    
  finish_load_from_object(std::move(expr));
  return true;
}

ControlState SetterBox::calc_state(Context *context) {
  if(is_down) {
    //if(mouse_inside)
    return PressedHovered;
    
    //return Pressed;
  }
  
  ControlState state = ContainerWidgetBox::calc_state(context);
  //if(state == Normal)
  //  return Hovered;
  return state;
}

bool SetterBox::expand(const BoxSize &size) {
  _extents = size;
  cx = (_extents.width - _content->extents().width) / 2;
  return true;
}

void SetterBox::resize_default_baseline(Context *context) {
  int bf = get_style(ButtonFrame, -1);
  if(bf >= 0)
    type = (ContainerType)bf;
  else
    type = PaletteButton;
    
  ContainerWidgetBox::resize_default_baseline(context);
}

void SetterBox::paint(Context *context) {
  if(must_update) {
    must_update = false;
    
    Expr dyn_val;
    if(dynamic.get_value(&dyn_val)) {
      is_down = (dyn_val == value);
    }
  }
  
  ContainerWidgetBox::paint(context);
}

Expr SetterBox::to_pmath_symbol() {
  return Symbol(richmath_System_SetterBox);
}

Expr SetterBox::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  g.emit(dynamic.expr());
  g.emit(value);
  g.emit(_content->to_pmath(flags - BoxOutputFlags::Parseable));
  
  if(style)
    style->emit_to_pmath();
    
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_SetterBox));
  return e;
}

void SetterBox::on_mouse_down(MouseEvent &event) {
  animation = 0;
  
  ContainerWidgetBox::on_mouse_down(event);
}

void SetterBox::on_mouse_up(MouseEvent &event) {
  if(event.left) {
    request_repaint_all();
    
    if(mouse_inside && mouse_left_down)
      click();
  }
  
  ContainerWidgetBox::on_mouse_up(event);
}

void SetterBox::click() {
  dynamic.assign(value);
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

Box *SetterBox::dynamic_to_literal(int *start, int *end) {
  if(dynamic.is_dynamic())
    dynamic = dynamic.expr()[1];
  return this;
}

//} ... class SetterBox

