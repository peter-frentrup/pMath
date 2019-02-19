#include <eval/dynamic.h>

#include <eval/application.h>
#include <eval/job.h>

#include <gui/document.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_SynchronousUpdating;

static Expr make_assignment_call(Expr func, Expr name, Expr value) {
  if(func == PMATH_SYMBOL_AUTOMATIC)
    return Call(Symbol(PMATH_SYMBOL_ASSIGN), std::move(name), std::move(value));
  
  if(func == PMATH_SYMBOL_NONE)
    return Expr();
  
  return Call(std::move(func), std::move(value), std::move(name));
}

static Expr eval_sequence(Expr e1, Expr e2, Expr e3) {
  if(e1.is_null()) {
    if(e2.is_null())
      return e3;
    return Call(Symbol(PMATH_SYMBOL_EVALUATIONSEQUENCE), std::move(e2), std::move(e3));
  }
  
  if(e2.is_null()) {
    if(e3.is_null())
      return e1;
    return Call(Symbol(PMATH_SYMBOL_EVALUATIONSEQUENCE), std::move(e1), std::move(e3));
  }
  
  if(e3.is_null()) 
    return Call(Symbol(PMATH_SYMBOL_EVALUATIONSEQUENCE), std::move(e1), std::move(e2));
  
  return Call(Symbol(PMATH_SYMBOL_EVALUATIONSEQUENCE), std::move(e1), std::move(e2), std::move(e3));
}

//{ class Dynamic ...

FrontEndReference Dynamic::current_evaluation_box_id = FrontEndReference::None;

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
                richmath_System_Dynamic,
                richmath_System_SynchronousUpdating,
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

Expr Dynamic::pre_assignment_function() {
  if(!is_dynamic())
    return Symbol(PMATH_SYMBOL_NONE);
  
  if(_expr.expr_length() < 2)
    return Symbol(PMATH_SYMBOL_NONE);
  
  Expr fun = _expr[2];
  if(fun[0] != PMATH_SYMBOL_LIST)
    return Symbol(PMATH_SYMBOL_NONE);
  
  if(fun.expr_length() == 3)
    return fun[1];
  
  return Symbol(PMATH_SYMBOL_NONE);
}

Expr Dynamic::middle_assignment_function() {
  if(!is_dynamic())
    return Symbol(PMATH_SYMBOL_AUTOMATIC);
  
  if(_expr.expr_length() < 2)
    return Symbol(PMATH_SYMBOL_AUTOMATIC);
  
  Expr fun = _expr[2];
  if(fun[0] != PMATH_SYMBOL_LIST)
    return fun;
  
  auto num_fun = fun.expr_length();
  if(num_fun == 3)
    return fun[2];
  
  return fun[1];
}

Expr Dynamic::post_assignment_function() {
  if(!is_dynamic())
    return Symbol(PMATH_SYMBOL_NONE);
  
  if(_expr.expr_length() < 2)
    return Symbol(PMATH_SYMBOL_NONE);
  
  Expr fun = _expr[2];
  if(fun[0] != PMATH_SYMBOL_LIST)
    return Symbol(PMATH_SYMBOL_NONE);
  
  auto num_fun = fun.expr_length();
  if(num_fun == 2 || num_fun == 3)
    return fun[num_fun];
  
  return Symbol(PMATH_SYMBOL_NONE);
}

bool Dynamic::has_pre_or_post_assignment() {
  if(_expr[0] != richmath_System_Dynamic)
    return false;

  if(_expr.expr_length() < 2)
    return false;
  
  Expr fun = _expr[2];
  if(fun[0] != PMATH_SYMBOL_LIST)
    return false;
  
  return fun.expr_length() > 1;
}
      
void Dynamic::assign(Expr value, bool pre, bool middle, bool post) {
  if(!is_dynamic()) {
    //if(!value.is_evaluated())
    //  value = Application::interrupt(value, Application::dynamic_timeout);
    
    if(middle)
      _expr = value;
    
    _owner->dynamic_updated();
    return;
  }
  
  Application::delay_dynamic_updates(false);
  
  Expr run;
  
  Expr name = _expr[1];
  if(_expr.expr_length() >= 2) {
    Expr pre_run;
    Expr middle_run;
    Expr post_run;
    
    if(pre)
      pre_run = make_assignment_call(pre_assignment_function(), name, value);
    if(middle)
      middle_run = make_assignment_call(middle_assignment_function(), name, value);
    if(post)
      post_run = make_assignment_call(post_assignment_function(), name, value);
      
    run = eval_sequence(pre_run, middle_run, post_run);
  }
  else if(middle)
    run = Call(Symbol(PMATH_SYMBOL_ASSIGN), name, value);
  
  if(run.is_null())
    return;
  
  run = _owner->prepare_dynamic(run);
  Application::interrupt_wait_for(run, _owner, Application::dynamic_timeout);
}

Expr Dynamic::get_value_now() {
  if(!is_dynamic()) {
    if(is_unevaluated())
      return _expr[1];
    
    return _expr;
  }
  
  if(_owner->style) {
    _owner->style->remove(InternalUsesCurrentValueOfMouseOver);
  }
  
  auto old_eval_id = current_evaluation_box_id;
  current_evaluation_box_id = _owner->id();
  
  Expr call = _owner->prepare_dynamic(_expr);
  
  Expr value = Application::interrupt_wait(
                 Call(
                   Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATEMULTIPLE),
                   call,
                   _owner->id().to_pmath_raw()),
                 Application::dynamic_timeout);
                 
  current_evaluation_box_id = old_eval_id;
  
  if(value == PMATH_UNDEFINED)
    return Symbol(PMATH_SYMBOL_ABORTED);
    
  return value;
}

void Dynamic::get_value_later() {
  if(!is_dynamic()) 
    return;
  
  if(_owner->style) {
    _owner->style->remove(InternalUsesCurrentValueOfMouseOver);
  }
  
  Expr call = _owner->prepare_dynamic(_expr);
  
  Application::add_job(new DynamicEvaluationJob(
                         Expr(),
                         Call(
                           Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATEMULTIPLE),
                           call,
                           _owner->id().to_pmath_raw()),
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
