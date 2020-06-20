#include <eval/dynamic.h>

#include <eval/application.h>
#include <eval/job.h>

#include <boxes/templatebox.h>

#include <gui/document.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_SynchronousUpdating;


namespace richmath {
  class DynamicImpl {
    public:
      DynamicImpl(Dynamic &_self) : self(_self) {}
      
      static bool is_template_slot(Expr expr, int *index);
      bool is_template_slot(int *index);
      bool find_template_box_dynamic(Expr *source, TemplateBox **source_template, int *source_index);
    
    private:
      static bool find_template_box_dynamic(Box *box, int i, Expr *source, TemplateBox **source_template, int *source_index);
      
    public:
      static void get_assignment_functions(Expr expr, Expr *pre, Expr *middle, Expr *post);
      
      bool has_pre_or_post_assignment();
      bool has_temporary_assignment();
    
    public:
      void assign(Expr value, bool pre, bool middle, bool post);
      
      Expr get_value_unevaluated();
      Expr get_value_unevaluated(bool *is_dynamic);
      Expr get_value_now();
      void get_value_later(Expr job_info);
      bool get_value(Expr *result, Expr job_info);
    
    private:
      Dynamic &self;
  };
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

FrontEndReference Dynamic::current_observer_id = FrontEndReference::None;

Dynamic::Dynamic()
  : Base(),
  _owner(nullptr),
  _synchronous_updating(AutoBoolFalse)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

Dynamic::Dynamic(Box *owner, Expr expr)
  : Base(),
  _owner(nullptr),
  _synchronous_updating(AutoBoolFalse)
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
  _synchronous_updating = AutoBoolFalse;
  
  if(!_expr.is_expr())
    return _expr;
  
  if(_expr[0] == richmath_System_Dynamic) {
    if(_expr.expr_length() < 1)
      return _expr;
    
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
        _synchronous_updating = AutoBoolTrue;
      else if(su == PMATH_SYMBOL_AUTOMATIC)
        _synchronous_updating = AutoBoolAutomatic;
      else
        _synchronous_updating = AutoBoolFalse;
    }
  }
  // for PureArgument(n), this->_synchronous_updating does not matter, because it is looked up in get_value()
  
  return _expr;
}

bool Dynamic::has_pre_or_post_assignment() {
  return DynamicImpl(*this).has_pre_or_post_assignment();
}

bool Dynamic::has_temporary_assignment() {
  return DynamicImpl(*this).has_temporary_assignment();
}

void Dynamic::assign(Expr value, bool pre, bool middle, bool post) {
  return DynamicImpl(*this).assign(value, pre, middle, post);
}

Expr Dynamic::get_value_unevaluated() {
  return DynamicImpl(*this).get_value_unevaluated();
}

Expr Dynamic::get_value_now() {
  return DynamicImpl(*this).get_value_now();
}

void Dynamic::get_value_later(Expr job_info) {
  DynamicImpl(*this).get_value_later(std::move(job_info));
}

bool Dynamic::get_value(Expr *result, Expr job_info) {
  return DynamicImpl(*this).get_value(result, std::move(job_info));
}

//} ... class Dynamic

//{ class DynamicImpl ...

bool DynamicImpl::is_template_slot(Expr expr, int *index) {
  if(expr[0] == PMATH_SYMBOL_PUREARGUMENT && expr.expr_length() == 1) {
    Expr arg = expr[1];
    if(arg.is_int32()) {
      if(index) 
        *index = PMATH_AS_INT32(arg.get());
      return true;
    }
  }
  return false;
}

bool DynamicImpl::is_template_slot(int *index) {
  return is_template_slot(self._expr, index);
}

bool DynamicImpl::find_template_box_dynamic(Expr *source, TemplateBox **source_template, int *source_index) {
  int i;
  if(is_template_slot(&i))
    return find_template_box_dynamic(self._owner, i, source, source_template, source_index);
  
  return false;
}

bool DynamicImpl::find_template_box_dynamic(Box *box, int i, Expr *source, TemplateBox **source_template, int *source_index) {
  if(i == 0 || !box)
    return false;
  
  box = box->parent();
  while(box) {
    if(TemplateBox *template_box = dynamic_cast<TemplateBox*>(box)) {
      int num_arg = (int)template_box->arguments.expr_length();
      
      if(i < 0)
        i+= num_arg + 1;
      
      Dynamic dyn { template_box, template_box->arguments[i] };
      if(!DynamicImpl(dyn).find_template_box_dynamic(source, source_template, source_index)) {
        if(source)          *source          = std::move(dyn._expr);
        if(source_template) *source_template = template_box;
        if(source_index)    *source_index    = i;
      }
      return true;
    }
    
    if(TemplateBoxSlot *slot = dynamic_cast<TemplateBoxSlot*>(box)) {
      box = slot->find_owner();
      if(!box)
        break;
    }
    
    box = box->parent();
  }
  
  return false;
}

void DynamicImpl::get_assignment_functions(Expr expr, Expr *pre, Expr *middle, Expr *post) {
  *pre    = Symbol(PMATH_SYMBOL_NONE);
  *middle = Symbol(PMATH_SYMBOL_NONE);
  *post   = Symbol(PMATH_SYMBOL_NONE);
  
  if(expr[0] != richmath_System_Dynamic)
    return;
  
  if(expr.expr_length() < 2) {
    *middle = Symbol(PMATH_SYMBOL_AUTOMATIC);
    return;
  }
  
  Expr fun = expr[2];
  if(fun[0] != PMATH_SYMBOL_LIST) {
    *middle = std::move(fun);
    if(*middle == PMATH_SYMBOL_TEMPORARY)
      *post = Symbol(PMATH_SYMBOL_AUTOMATIC);
    return;
  }
  
  switch(fun.expr_length()) {
    case 1:
      *middle = fun[1];
      return;
    
    case 2:
      *middle = fun[1];
      *post   = fun[2];
      return;
    
    case 3:
      *pre    = fun[1];
      *middle = fun[2];
      *post   = fun[3];
      return;
  }
}

bool DynamicImpl::has_pre_or_post_assignment() {
  int i;
  if(is_template_slot(&i)) {
    Expr         source;
    TemplateBox *source_template;
    if(find_template_box_dynamic(&source, &source_template, nullptr)) {
      Dynamic dyn{ source_template, source };
      return dyn.has_pre_or_post_assignment();
    }
  }
  
  if(self._expr[0] != richmath_System_Dynamic)
    return false;

  if(self._expr.expr_length() < 2)
    return false;
  
  Expr fun = self._expr[2];
  if(fun == PMATH_SYMBOL_TEMPORARY)
    return true;
  
  if(fun[0] != PMATH_SYMBOL_LIST)
    return false;
  
  return fun.expr_length() > 1;
}

bool DynamicImpl::has_temporary_assignment() {
  int i;
  if(is_template_slot(&i)) {
    Expr         source;
    TemplateBox *source_template;
    if(find_template_box_dynamic(&source, &source_template, nullptr)) {
      Dynamic dyn{ source_template, source };
      return dyn.has_temporary_assignment();
    }
  }
  
  if(self._expr[0] != richmath_System_Dynamic)
    return false;

  if(self._expr.expr_length() < 2)
    return false;
  
  Expr pre;
  Expr middle;
  Expr post;
  get_assignment_functions(self._expr, &pre, &middle, &post);
  return middle == PMATH_SYMBOL_TEMPORARY;
}

static Expr make_assignment_call(Expr func, Expr name, Expr value) {
  if(func == PMATH_SYMBOL_AUTOMATIC)
    return Call(Symbol(PMATH_SYMBOL_ASSIGN), std::move(name), std::move(value));
  
  if(func == PMATH_SYMBOL_NONE)
    return Expr();
  
  return Call(std::move(func), std::move(value), std::move(name));
}

void DynamicImpl::assign(Expr value, bool pre, bool middle, bool post) {
  Box *dyn_source = self._owner;
  Expr dyn_expr = self._expr;
  
  int i;
  if(is_template_slot(dyn_expr, &i)) {
    Expr         source;
    TemplateBox *source_template;
    int          source_index;
    
    if(find_template_box_dynamic(self._owner, i, &source, &source_template, &source_index)) {
      Dynamic dyn{ source_template, source };
      dyn_source = source_template;
      
      while(DynamicImpl(dyn).find_template_box_dynamic(&source, &source_template, &source_index)) {
        //dyn.init(source_template, source);
        dyn_source = source_template;
        dyn._owner = std::move(source_template);
        dyn._expr  = std::move(source);
      }
      
      dyn_expr = dyn._expr;
      if(dyn._expr[0] != richmath_System_Dynamic) {
        dyn.assign(value, pre, middle, post);
        if(dyn._expr != dyn_expr) {
          source_template->reset_argument(source_index, dyn._expr);
          // TODO: notify other observers of this change ...
          self._owner->dynamic_updated();
        }
        return;
      }
    }
  }
  else if(!self.is_dynamic()) {
    //if(!value.is_evaluated())
    //  value = Application::interrupt(value, Application::dynamic_timeout);
    
    if(middle)
      self._expr = value;
    
    self._owner->dynamic_updated();
    return;
  }
  
  Application::delay_dynamic_updates(false);
  
  Expr run;
  
  Expr name = dyn_expr[1];
  if(dyn_expr.expr_length() >= 2) {
    Expr pre_run;
    Expr middle_run;
    Expr post_run;
    
    get_assignment_functions(dyn_expr, &pre_run, &middle_run, &post_run);
    
    pre_run    = pre    ? make_assignment_call(pre_run,    name, value) : Expr();
    middle_run = middle ? make_assignment_call(middle_run, name, value) : Expr();
    post_run   = post   ? make_assignment_call(post_run,   name, value) : Expr();
      
    run = eval_sequence(pre_run, middle_run, post_run);
  }
  else if(middle)
    run = Call(Symbol(PMATH_SYMBOL_ASSIGN), name, value);
  
  if(run.is_null())
    return;
  
  run = dyn_source->prepare_dynamic(run);
  Application::interrupt_wait_for(run, self._owner, Application::dynamic_timeout);
}

Expr DynamicImpl::get_value_unevaluated() {
  bool is_dynamic;
  return get_value_unevaluated(&is_dynamic);
}

Expr DynamicImpl::get_value_unevaluated(bool *is_dynamic) {
  Box *dyn_source = self._owner;
  Expr dyn_expr = self._expr;
  
  int i;
  if(is_template_slot(&i)) {
    Expr         source;
    TemplateBox *source_template;
    
    if(find_template_box_dynamic(self._owner, i, &source, &source_template, nullptr)) {
      Dynamic dyn{ source_template, source };
      dyn_source = source_template;
      
      while(DynamicImpl(dyn).find_template_box_dynamic(&source, &source_template, nullptr)) {
        //dyn.init(source_template, source);
        dyn_source = source_template;
        dyn._owner = std::move(source_template);
        dyn._expr  = std::move(source);
      }
      
      dyn_expr = dyn._expr;
      if(dyn._expr[0] != richmath_System_Dynamic) {
        /* Note that dynamic changes in the find_template_box_dynamic-chain above would not be visible 
           for self._owner and would thus break its dynamic updating facility.
           This scenario can happen if a TemplateBox in the chain (a parent of self._owner)
           is modified e.g. programmatically.
           More importantly, it happens
         */
        source_template->register_observer(self._owner->id());
        
        *is_dynamic = false;
        return dyn_expr;
      }
    }
  }
  else if(!self.is_dynamic()) {
    *is_dynamic = false;
    return self._expr;
  }
  
  *is_dynamic = true;
  dyn_expr = dyn_source->prepare_dynamic(std::move(dyn_expr));
  return Call(
           Symbol(PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATEMULTIPLE),
           std::move(dyn_expr),
           self._owner->id().to_pmath_raw());
}

Expr DynamicImpl::get_value_now() {
  bool is_dynamic = false;
  Expr call = get_value_unevaluated(&is_dynamic);
  
  if(!is_dynamic) {
    if(call.expr_length() == 1 && call[0] == PMATH_SYMBOL_UNEVALUATED)
      return call[1];
    return call;
  }
  
  if(self._owner->style) {
    self._owner->style->remove(InternalUsesCurrentValueOfMouseOver);
  }
  
  auto old_eval_id = Dynamic::current_observer_id;
  Dynamic::current_observer_id = self._owner->id();
  
  Expr value = Application::interrupt_wait_for(call, self._owner, Application::dynamic_timeout);
                 
  Dynamic::current_observer_id = old_eval_id;
  
  if(value == PMATH_UNDEFINED)
    return Symbol(PMATH_SYMBOL_ABORTED);
    
  return value;
}

void DynamicImpl::get_value_later(Expr job_info) {
  bool is_dynamic = false;
  Expr call = get_value_unevaluated(&is_dynamic);
  
  if(!is_dynamic) 
    return;
  
  if(self._owner->style) {
    self._owner->style->remove(InternalUsesCurrentValueOfMouseOver);
  }
  
  Application::add_job(new DynamicEvaluationJob(job_info, call, self._owner));
}

bool DynamicImpl::get_value(Expr *result, Expr job_info) {
  if(result)
    *result = Expr();
    
  AutoBoolValues sync = self._synchronous_updating;
  
  int i;
  if(is_template_slot(&i)) {
    Expr         source;
    TemplateBox *source_template;
    
    if(find_template_box_dynamic(self._owner, i, &source, &source_template, nullptr)) {
      Dynamic dyn { source_template, source };
      
      while(DynamicImpl(dyn).find_template_box_dynamic(&source, &source_template, nullptr)) 
        dyn.init(std::move(source_template), std::move(source));
      
      if(dyn.is_dynamic()) {
        sync = dyn._synchronous_updating;
      }
      else {
        source_template->register_observer(self._owner->id());
        sync = AutoBoolTrue;
      }
    }
  }
  
  if(sync == AutoBoolAutomatic) {
    sync = AutoBoolTrue;
    if(auto doc = self._owner->find_parent<Document>(true))
      sync = doc->is_mouse_down() ? AutoBoolTrue : AutoBoolFalse;
  }
  
  if(sync != AutoBoolFalse || !self.is_dynamic()) {
    if(result)
      *result = get_value_now();
    else
      get_value_now();
      
    return true;
  }
  
  get_value_later(std::move(job_info));
  return false;
}

//} ... class DynamicImpl

Expr richmath_eval_FrontEnd_PrepareDynamicEvaluation(Expr expr) {
  /*  FrontEnd`PrepareDynamicEvaluation(FrontEndObject(...), Dynamic(...))
      FrontEnd`PrepareDynamicEvaluation(Automatic,           Dynamic(...))
   */
  if(expr.expr_length() != 2)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  Box *box = nullptr;
  if(expr[1] == PMATH_SYMBOL_AUTOMATIC)
    box = Application::get_evaluation_box();
  else
    box = FrontEndObject::find_cast<Box>(FrontEndReference::from_pmath(expr[1]));
  
  if(box)
    return Call(Symbol(PMATH_SYMBOL_HOLD), Dynamic(box, expr[2]).get_value_unevaluated());
  
  return Symbol(PMATH_SYMBOL_FAILED);
}

Expr richmath_eval_FrontEnd_AssignDynamicValue(Expr expr) {
  /*  FrontEnd`AssignDynamicValue(FrontEndObject(...), Dynamic(...), value)
      FrontEnd`AssignDynamicValue(Automatic,           Dynamic(...), value)
   */
  if(expr.expr_length() != 3)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  Box *box = nullptr;
  if(expr[1] == PMATH_SYMBOL_AUTOMATIC)
    box = Application::get_evaluation_box();
  else
    box = FrontEndObject::find_cast<Box>(FrontEndReference::from_pmath(expr[1]));
  
  if(box) {
    Dynamic(box, expr[2]).assign(expr[3]);
    return Expr();
  }
  
  return Symbol(PMATH_SYMBOL_FAILED);
}
