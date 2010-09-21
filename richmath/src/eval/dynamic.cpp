#include <eval/dynamic.h>

#include <eval/client.h>
#include <eval/job.h>

using namespace richmath;

//{ class Dynamic ...

Dynamic::Dynamic(Box *owner, Expr expr)
: _owner(owner),
  _expr(expr),
  synchronous_updating(0)
{
  assert(_owner != 0);
  *this = _expr;
}

Expr Dynamic::operator=(Expr expr){
  _expr = expr;
  
  if(is_dynamic()){
    Expr options(PMATH_UNDEFINED);
    
    if(_expr.expr_length() >= 2){
      Expr snd = _expr[2];
      if(snd.expr_length() == 2
      && (snd[0] == PMATH_SYMBOL_RULE || snd[0] == PMATH_SYMBOL_RULEDELAYED))
        options = Expr(pmath_options_extract(_expr.get(), 1));
      else
        options = Expr(pmath_options_extract(_expr.get(), 2));
    }
    
    if(options.is_valid()){
      Expr su(pmath_option_value(
        PMATH_SYMBOL_DYNAMIC, 
        PMATH_SYMBOL_SYNCHRONOUSUPDATING,
        options.get()));
      
      if(su == PMATH_SYMBOL_TRUE)
        synchronous_updating = 1;
      else if(su == PMATH_SYMBOL_AUTOMATIC)
        synchronous_updating = 2;
    }
  }
  
  return _expr;
}

void Dynamic::assign(Expr value){
  if(!is_dynamic()){
    _expr = value;
    return;
  }
  
  Expr run;
  
  if(_expr.expr_length() >= 2) 
    run = Call(_expr[2], value);
  else
    run = Call(Symbol(PMATH_SYMBOL_ASSIGN), _expr[1], value);
  
  Client::execute_for(run, _owner, Client::dynamic_timeout);
}

Expr Dynamic::get_value_now(){
  if(!is_dynamic())
    return _expr;
  
  Expr run = Call(
    Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATE),
    _expr[1],
    _owner->id());
  
  run = Client::interrupt(run, Client::dynamic_timeout);
  if(run == PMATH_UNDEFINED)
    return Symbol(PMATH_SYMBOL_ABORTED);
  
  return run;
}

void Dynamic::get_value_later(){
  if(!is_dynamic())
    return;
  
  Expr run = Call(
    Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATE),
    _expr[1],
    _owner->id());
  
  Client::add_job(new DynamicEvaluationJob(Expr(), run, _owner));
}

bool Dynamic::get_value(Expr *result){
  if(result)
    *result = Expr();
  
  // TODO: handle synchronous_updating == 2 (Automatic)
  if(synchronous_updating != 0){
    if(result)
      *result = get_value_now();
    else
      get_value_now();
    
    return true;
  }
  
  get_value_later();
  return false;
}


//} ... class Dynamic
