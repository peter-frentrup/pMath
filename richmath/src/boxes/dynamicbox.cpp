#include <boxes/dynamicbox.h>

#include <boxes/mathsequence.h>
#include <eval/client.h>
#include <graphics/context.h>

#include <stdio.h>

using namespace richmath;

//{ class DynamicBox ...abandon

DynamicBox::DynamicBox(Expr _dynamic_content)
: OwnerBox(),
  dynamic_content(_dynamic_content),
  must_update(true)
{
}

DynamicBox::~DynamicBox(){
  Client::execute_for(Call(Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICREMOVE), id()), 0);
}

DynamicBox *DynamicBox::create(Expr expr, int opts){
  if(expr[0] == PMATH_SYMBOL_DYNAMICBOX
  && expr.expr_length() == 1){
    return new DynamicBox(expr[1]);
  }
  
  return 0;
}

void DynamicBox::paint_content(Context *context){
  OwnerBox::paint_content(context);
  
  if(must_update){
    must_update = false;
    
    Expr run = Call(
      Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATE),
      Call(
        Symbol(PMATH_SYMBOL_TOBOXES),
        dynamic_content),
      this->id());
    
    // TODO: Add option to not wait for result here, but to inform this box when 
    //       a result is calculated.
    run = Client::interrupt(run, Client::dynamic_timeout);
    if(run == PMATH_UNDEFINED)
      run = String("$Aborted");
    
    int opt = BoxOptionDefault;
    if(get_style(AutoNumberFormating))
      opt |= BoxOptionFormatNumbers;
    
    content()->load_from_object(run, opt);
    invalidate();
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

//} ... class DynamicBox
