#include <boxes/dynamicbox.h>

#include <boxes/mathsequence.h>
#include <eval/client.h>
#include <eval/job.h>
#include <graphics/context.h>

#include <stdio.h>

using namespace richmath;

//{ class DynamicBox ...abandon

DynamicBox::DynamicBox(Expr _dynamic_content)
: OwnerBox(),
  dynamic_content(_dynamic_content),
  must_update(true),
  must_resize(false)
{
}

DynamicBox::~DynamicBox(){
  Client::execute_for(Call(Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICREMOVE), id()), 0);
}

DynamicBox *DynamicBox::create(Expr expr, int opts){
  if(expr[0] == PMATH_SYMBOL_DYNAMICBOX
  && expr.expr_length() >= 1){
    Expr options = Expr(pmath_options_extract(expr.get(), 1));
    
    if(options.is_valid()){
      DynamicBox *box = new DynamicBox(expr[1]);
      
      if(options != PMATH_UNDEFINED){
        if(box->style)
          box->style->add_pmath(options);
        else
          box->style = new Style(options);
      }
    
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
    
    Expr run = Call(
      Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATE),
      Call(
        Symbol(PMATH_SYMBOL_TOBOXES),
        dynamic_content),
      this->id());
    
    if(get_style(SynchronousUpdating, 1) != 0){
      run = Client::interrupt(run, Client::dynamic_timeout);
      if(run == PMATH_UNDEFINED)
        run = String("$Aborted");
      
      int opt = BoxOptionDefault;
      if(get_style(AutoNumberFormating))
        opt |= BoxOptionFormatNumbers;
      
      content()->load_from_object(run, opt);
      invalidate();
      must_resize = true;
    }
    else
      Client::add_job(new DynamicEvaluationJob(Expr(), run, this));
  }
}

pmath_t DynamicBox::to_pmath(bool parseable){
  return pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_DYNAMICBOX), 1, 
    pmath_ref(dynamic_content.get()));
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
