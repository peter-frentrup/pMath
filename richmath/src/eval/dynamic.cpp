#include <eval/dynamic.h>

#include <eval/application.h>
#include <eval/job.h>

#include <gui/document.h>

using namespace richmath;

//{ class Dynamic ...

int Dynamic::current_evaluation_box_id = 0;

Dynamic::Dynamic()
  : Base(),
  _owner(nullptr),
  _synchronous_updating(0)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

Dynamic::Dynamic(Box *owner, Expr expr)
  : Base(),
  _owner(nullptr),
  _synchronous_updating(0)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  init(owner, expr);
}

void Dynamic::init(Box *owner, Expr expr) {
  assert(_owner == nullptr && owner != nullptr);
  
  _owner = owner;
  *this = expr;
}

Expr Dynamic::operator=(Expr expr) {
  _expr = expr;
  
  if(is_dynamic()) {
    Expr options(PMATH_UNDEFINED);
    
    if(_expr.expr_length() >= 2) {
      Expr snd = _expr[2];
      if(snd.is_rule())
        options = Expr(pmath_options_extract_ex(_expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_QUIET));
      else
        options = Expr(pmath_options_extract_ex(_expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_QUIET));
    }
    
    if(!options.is_null()) {
      Expr su(pmath_option_value(
                PMATH_SYMBOL_DYNAMIC,
                PMATH_SYMBOL_SYNCHRONOUSUPDATING,
                options.get()));
                
      if(su == PMATH_SYMBOL_TRUE)
        _synchronous_updating = 1;
      else if(su == PMATH_SYMBOL_AUTOMATIC)
        _synchronous_updating = 2;
      else
        _synchronous_updating = 0;
    }
  }
  
  return _expr;
}

void Dynamic::assign(Expr value) {
  if(!is_dynamic()) {
    //if(!value.is_evaluated())
    //  value = Application::interrupt(value, Application::dynamic_timeout);
    
    _expr = value;
    _owner->dynamic_updated();
    return;
  }
  
  Application::delay_dynamic_updates(false);
  
  Expr run;
  
  if(_expr.expr_length() >= 2)
    run = Call(_expr[2], value);
  else
    run = Call(Symbol(PMATH_SYMBOL_ASSIGN), _expr[1], value);
    
  run = _owner->prepare_dynamic(run);
  Application::interrupt_wait_for(run, _owner, Application::dynamic_timeout);
}

Expr Dynamic::get_value_now() {
  if(!is_dynamic()) {
    return _expr;
  }
  
  if(_owner->style) {
    _owner->style->remove(InternalUsesCurrentValueOfMouseOver);
  }
  
  int old_eval_id = current_evaluation_box_id;
  current_evaluation_box_id = _owner->id();
  
  Expr call = _owner->prepare_dynamic(_expr);
  
  Expr value = Application::interrupt_wait(
                 Call(
                   Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATEMULTIPLE),
                   call,
                   _owner->id()),
                 Application::dynamic_timeout);
                 
  current_evaluation_box_id = old_eval_id;
  
  if(value == PMATH_UNDEFINED)
    return Symbol(PMATH_SYMBOL_ABORTED);
    
  return value;
}

void Dynamic::get_value_later() {
  if(!is_dynamic()) {
    return;
  }
  
  if(_owner->style) {
    _owner->style->remove(InternalUsesCurrentValueOfMouseOver);
  }
  
  Expr call = _owner->prepare_dynamic(_expr);
  
  Application::add_job(new DynamicEvaluationJob(
                         Expr(),
                         Call(
                           Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATEMULTIPLE),
                           call,
                           _owner->id()),
                         _owner));
}

bool Dynamic::get_value(Expr *result) {
  if(result)
    *result = Expr();
    
  int sync = _synchronous_updating;
  
  if(sync == 2) {
    sync = 1;
    if(auto doc = _owner->find_parent<Document>(true))
      sync = doc->is_mouse_down();
  }
  
  if(sync != 0 || !is_dynamic()) {
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
