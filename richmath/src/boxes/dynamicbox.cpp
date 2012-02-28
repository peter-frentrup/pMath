#include <boxes/dynamicbox.h>

#include <boxes/dynamiclocalbox.h>
#include <boxes/mathsequence.h>
#include <eval/application.h>
#include <eval/job.h>
#include <graphics/context.h>

#include <cstdio>

using namespace richmath;

//{ class AbstractDynamicBox ...

AbstractDynamicBox::AbstractDynamicBox()
  : OwnerBox()
{
}

AbstractDynamicBox::~AbstractDynamicBox() {
}

Box *AbstractDynamicBox::dynamic_to_literal(int *start, int *end) {
  if(*start > 0 || *end < 1)
    return this;
    
  int s = 0;
  int e = content()->length();
  content()->dynamic_to_literal(&s, &e);
  
  MathSequence *seq = dynamic_cast<MathSequence*>(parent());
  if(!seq)
    return this;
    
  *start = index();
  *end = seq->insert(index(), content(), 0, content()->length());
  seq->remove(*end, *end + 1); // remove this
  return seq;
}

//} ... class AbstractDynamicBox

//{ class DynamicBox ...

DynamicBox::DynamicBox()
  : AbstractDynamicBox(),
  must_update(true),
  must_resize(false)
{
  dynamic.init(this, Expr());
}

DynamicBox::~DynamicBox() {
  Application::execute_for(Call(Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICREMOVE), id()), 0);
}

bool DynamicBox::try_load_from_object(Expr expr, int opts) {
  if(expr[0] != PMATH_SYMBOL_DYNAMICBOX)
    return false;
    
  if(expr.expr_length() < 1) 
    return false;
    
  Expr options_expr = Expr(pmath_options_extract(expr.get(), 1));
  if(options_expr.is_null())
    return false;
    
  /* now success is guaranteed */
  
  expr.set(0, Symbol(PMATH_SYMBOL_DYNAMIC));
  
  if(dynamic.expr() != expr){
    must_update = true;
    dynamic     = expr;
  }
  
  return true;
}

void DynamicBox::resize(Context *context) {
  AbstractDynamicBox::resize(context);
  must_resize = false;
  
  if(_extents.width <= 0)
    _extents.width = 0.75;
    
  if(_extents.height() <= 0) {
    _extents.ascent  = 0.75;
    _extents.descent = 0.0;
  }
}

void DynamicBox::paint_content(Context *context) {
  if(must_resize) {
    context->canvas->save();
    AbstractDynamicBox::resize(context);
    must_resize = false;
    context->canvas->restore();
  }
  
  AbstractDynamicBox::paint_content(context);
  
  if(must_update) {
    must_update = false;
    
    Expr result;
    if(dynamic.get_value(&result)) {
      int opt = BoxOptionDefault;
      if(get_style(AutoNumberFormating))
        opt |= BoxOptionFormatNumbers;
        
      content()->load_from_object(result, opt);
      must_resize = true;
      invalidate();
    }
  }
}

Expr DynamicBox::to_pmath(int flags) {
  if(flags & BoxFlagLiteral)
    return content()->to_pmath(flags);
    
  Expr e = dynamic.expr();
  e.set(0, Symbol(PMATH_SYMBOL_DYNAMICBOX));
  return e;
}

void DynamicBox::dynamic_updated() {
  if(must_update)
    return;
    
  must_update = true;
  request_repaint_all();
}

void DynamicBox::dynamic_finished(Expr info, Expr result) {
  int opt = BoxOptionDefault;
  if(get_style(AutoNumberFormating))
    opt |= BoxOptionFormatNumbers;
    
  content()->load_from_object(result, opt);
  must_resize = true;
  invalidate();
}

bool DynamicBox::edit_selection(Context *context) {
  if(get_own_style(Editable, false)) {
    if(_parent)
      return _parent->edit_selection(context);
  }
  
  return false;
}

void DynamicBox::on_mouse_enter() {
  AbstractDynamicBox::on_mouse_enter();
  if(get_own_style(InternalUsesCurrentValueOfMouseOver, false))
    dynamic_updated();
}

void DynamicBox::on_mouse_exit() {
  AbstractDynamicBox::on_mouse_exit();
  if(get_own_style(InternalUsesCurrentValueOfMouseOver, false))
    dynamic_updated();
}

//} ... class DynamicBox
