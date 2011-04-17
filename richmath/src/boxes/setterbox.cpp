#include <boxes/setterbox.h>

#include <boxes/mathsequence.h>

using namespace richmath;

//{ class SetterBox ...

SetterBox::SetterBox(MathSequence *content)
: ContainerWidgetBox(PaletteButton, content),
  must_update(true),
  is_down(false)
{
  dynamic.init(this, Expr());
}

SetterBox *SetterBox::create(Expr expr, int opts){
  if(expr.expr_length() < 3)
    return 0;
  
  Expr options(pmath_options_extract(expr.get(), 3));
  
  if(options.is_null())
    return 0;
    
  SetterBox *box = new SetterBox(new MathSequence);
  box->dynamic = expr[1];
  box->value   = expr[2];
  box->is_down = (!box->dynamic.is_dynamic() && box->dynamic.expr() == box->value);
  box->content()->load_from_object(expr[3], opts);
  
  if(options != PMATH_UNDEFINED){
    if(box->style)
      box->style->add_pmath(options);
    else
      box->style = new Style(options);
  }
  
  return box;
}

ControlState SetterBox::calc_state(Context *context){
  if(is_down)
    return Pressed;
  
  ControlState state = ContainerWidgetBox::calc_state(context);
  if(state == Normal)
    return Hovered;
  return state;
}

bool SetterBox::expand(const BoxSize &size){
  _extents = size;
  cx = (_extents.width - _content->extents().width) / 2;
  return true;
}

void SetterBox::paint(Context *context){
  if(must_update){
    must_update = false;
    
    Expr dyn_val;
    if(dynamic.get_value(&dyn_val)){
      is_down = (dyn_val == value);
    }
  }
  
  ContainerWidgetBox::paint(context);
}

Expr SetterBox::to_pmath(bool parseable){
  Gather g;
  
  g.emit(dynamic.expr());
  g.emit(value);
  g.emit(_content->to_pmath(false));
  
  if(style)
    style->emit_to_pmath();
  
  Expr e = g.end();
  e.set(0, Symbol(PMATH_SYMBOL_SETTERBOX));
  return e;
}

void SetterBox::on_mouse_down(MouseEvent &event){
  animation = 0;
  
  ContainerWidgetBox::on_mouse_down(event);
}

void SetterBox::on_mouse_up(MouseEvent &event){
  if(event.left){
    request_repaint_all();
    
    if(mouse_inside && mouse_left_down)
      click();
  }
  
  ContainerWidgetBox::on_mouse_up(event);
}

void SetterBox::click(){
  dynamic.assign(value);
  is_down = true;
  request_repaint_all();
}

void SetterBox::dynamic_updated(){
  if(must_update)
    return;
  
  must_update = true;
  request_repaint_all();
}

void SetterBox::dynamic_finished(Expr info, Expr result){
  bool new_is_down = result == value;
  
  if(is_down != new_is_down){
    is_down = new_is_down;
    request_repaint_all();
  }
}

//} ... class SetterBox
