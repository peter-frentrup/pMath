#include <boxes/dynamicbox.h>

#include <boxes/mathsequence.h>
#include <eval/client.h>
#include <eval/job.h>
#include <graphics/context.h>

#include <stdio.h>

using namespace richmath;

//{ class DynamicBox ...abandon

DynamicBox::DynamicBox()
: OwnerBox(),
  must_update(true),
  must_resize(false)
{
  dynamic.init(this, Expr());
}

DynamicBox::~DynamicBox(){
  Client::execute_for(Call(Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICREMOVE), id()), 0);
}

DynamicBox *DynamicBox::create(Expr expr, int opts){
  if(expr[0] == PMATH_SYMBOL_DYNAMICBOX
  && expr.expr_length() >= 1){
    Expr options = Expr(pmath_options_extract(expr.get(), 1));
    
    if(options.is_valid()){
      DynamicBox *box = new DynamicBox();
      
      expr.set(0, Symbol(PMATH_SYMBOL_DYNAMIC));
      box->dynamic = expr;
    
      return box;
    }
  }
  
  return 0;
}

void DynamicBox::resize(Context *context){
  OwnerBox::resize(context);
  must_resize = false;
  
  if(_extents.width <= 0)
     _extents.width = 0.75;
  
  if(_extents.height() <= 0){
    _extents.ascent  = 0.75;
    _extents.descent = 0.0;
  }
}

void DynamicBox::paint_content(Context *context){
  if(must_resize){
    context->canvas->save();
    OwnerBox::resize(context);
    must_resize = false;
    context->canvas->restore();
  }
  
  OwnerBox::paint_content(context);
  
  if(must_update){
    must_update = false;
    
    Expr result;
    if(dynamic.get_value(&result)){
      int opt = BoxOptionDefault;
      if(get_style(AutoNumberFormating))
        opt |= BoxOptionFormatNumbers;
      
      content()->load_from_object(result, opt);
      invalidate();
      must_resize = true;
    }
  }
}

Expr DynamicBox::to_pmath(bool parseable){
  Expr e = dynamic.expr();
  e.set(0, Symbol(PMATH_SYMBOL_DYNAMICBOX));
  return e;
}

void DynamicBox::dynamic_updated(){
  if(must_update)
    return;
  
  must_update = true;
  request_repaint_all();
}

void DynamicBox::dynamic_finished(Expr info, Expr result){
  int opt = BoxOptionDefault;
  if(get_style(AutoNumberFormating))
    opt |= BoxOptionFormatNumbers;
  
  content()->load_from_object(result, opt);
  invalidate();
  must_resize = true;
}

//} ... class DynamicBox
