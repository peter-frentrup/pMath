#include <boxes/dynamicbox.h>

#include <boxes/dynamiclocalbox.h>
#include <boxes/mathsequence.h>
#include <eval/application.h>
#include <eval/job.h>
#include <eval/observable.h>
#include <graphics/context.h>
#include <gui/document.h>

#include <cstdio>

using namespace richmath;

extern pmath_symbol_t richmath_System_DynamicBox;

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
  
  auto seq = dynamic_cast<MathSequence*>(parent());
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
  Application::interrupt_wait_for(
    Call(Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICREMOVE), id().to_pmath_raw()), 
    0,
    Application::interrupt_timeout);
  Observable::unregister_oberserver(id());
}

bool DynamicBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_DynamicBox)
    return false;
    
  if(expr.expr_length() < 1) 
    return false;
    
  Expr options_expr = Expr(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options_expr.is_null())
    return false;
    
  /* now success is guaranteed */
  reset_style();
  if(options_expr[0] == PMATH_SYMBOL_LIST) {
    if(!style)
      style = new Style();
    style->add_pmath(options_expr);
  }
  
  expr.set(0, Symbol(richmath_System_Dynamic)); // TODO: update the Dynamic expr when a style changes
  
  if(dynamic.expr() != expr || has(opts, BoxInputFlags::ForceResetDynamic)){
    must_update = true;
    dynamic     = expr;
  }
  
  finish_load_from_object(std::move(expr));
  return true;
}

void DynamicBox::resize_default_baseline(Context &context) {
  AbstractDynamicBox::resize_default_baseline(context);
  must_resize = false;
  
  if(_extents.width <= 0)
    _extents.width = 0.75;
    
  if(_extents.height() <= 0) {
    _extents.ascent  = 0.75;
    _extents.descent = 0.0;
  }
}

void DynamicBox::paint_content(Context &context) {
  if(must_resize) {
    context.canvas().save();
    AbstractDynamicBox::resize(context);
    must_resize = false;
    context.canvas().restore();
  }
  
  AbstractDynamicBox::paint_content(context);
  
  if(must_update) {
    must_update = false;
    
    if(style) {
      dynamic.synchronous_updating((AutoBoolValues)get_own_style(SynchronousUpdating, dynamic.synchronous_updating()));
      // TODO: update TrackedSymbols setting inside dynamic from our style
    }
    
    Expr result;
    if(dynamic.get_value(&result)) 
      dynamic_finished(Expr(), result);
  }
}

Expr DynamicBox::to_pmath_symbol() { 
  return Symbol(richmath_System_DynamicBox); 
}

Expr DynamicBox::to_pmath(BoxOutputFlags flags) {
  if(has(flags, BoxOutputFlags::Literal))
    return content()->to_pmath(flags);
  
  Expr expr = dynamic.expr();
  if(style) {
    Gather g;
    
    Gather::emit(expr[1]);
    style->emit_to_pmath(false);
    
    expr = g.end();
  }
  
  expr.set(0, Symbol(richmath_System_DynamicBox));
  return expr;
}

void DynamicBox::dynamic_updated() {
  if(must_update)
    return;
    
  must_update = true;
  request_repaint_all();
}

void DynamicBox::dynamic_finished(Expr info, Expr result) {
  BoxInputFlags opt = BoxInputFlags::Default;
  if(get_style(AutoNumberFormating))
    opt |= BoxInputFlags::FormatNumbers;
    
  content()->load_from_object(result, opt);
  if(find_parent<Document>(false))
    content()->after_insertion();
  
  must_resize = true;
  invalidate();
}

bool DynamicBox::edit_selection(Context &context) {
  if(get_own_style(Editable, false)) {
    if(_parent)
      return _parent->edit_selection(context);
  }
  
  return false;
}

//} ... class DynamicBox
