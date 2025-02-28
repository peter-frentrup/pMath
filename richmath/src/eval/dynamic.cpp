#include <eval/dynamic.h>

#include <eval/application.h>
#include <eval/eval-contexts.h>
#include <eval/job.h>
#include <eval/simple-evaluator.h>

#include <boxes/templatebox.h>
#include <util/styled-object.h>

#include <gui/document.h>

using namespace richmath;

namespace richmath { namespace strings {
  extern String DollarContext_namespace;
}}

extern pmath_symbol_t richmath_System_DollarAborted;
extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_Assign;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_CurrentValue;
extern pmath_symbol_t richmath_System_EvaluationSequence;
extern pmath_symbol_t richmath_System_Hold;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_SynchronousUpdating;
extern pmath_symbol_t richmath_System_Temporary;
extern pmath_symbol_t richmath_System_True;
extern pmath_symbol_t richmath_Internal_DynamicEvaluateMultiple;


namespace richmath {
  class Dynamic::Impl {
    public:
      Impl(Dynamic &self) : self(self) {}
      
      static bool is_template_slot(Expr expr, int *index);
      bool is_template_slot(int *index);
      bool find_template_box_dynamic(Expr *source, TemplateBox **source_template, int *source_index);
    
    private:
      static bool find_template_box_dynamic(StyledObject *obj, int i, Expr *source, TemplateBox **source_template, int *source_index);
      
    public:
      static void get_assignment_functions(Expr expr, Expr *pre, Expr *middle, Expr *post);
      static Expr make_assignment_call(Expr func, Expr name, Expr value);

      bool has_pre_or_post_assignment();
      bool has_temporary_assignment();
    
      void assign(Expr value, bool pre, bool middle, bool post);
      
      bool is_dynamic_of(Expr sym);
      Expr get_value_unevaluated();
      Expr get_value_unevaluated(bool *is_dynamic);
      Expr get_value_now();
      bool try_get_value_now(Expr *result, Evaluator evaluator);
      void get_value_later(Expr job_info);
      bool get_value(Expr *result, Expr job_info);
      
      Expr wrap_dyn_eval_call(Expr expr);
    
    private:
      Expr get_prepared_dynamic(bool *is_dynamic);
    
    private:
      Dynamic &self;
  };
}

static Expr eval_sequence(Expr e1, Expr e2, Expr e3) {
  if(e1.is_null()) {
    if(e2.is_null())
      return e3;
    return Call(Symbol(richmath_System_EvaluationSequence), PMATH_CPP_MOVE(e2), PMATH_CPP_MOVE(e3));
  }
  
  if(e2.is_null()) {
    if(e3.is_null())
      return e1;
    return Call(Symbol(richmath_System_EvaluationSequence), PMATH_CPP_MOVE(e1), PMATH_CPP_MOVE(e3));
  }
  
  if(e3.is_null()) 
    return Call(Symbol(richmath_System_EvaluationSequence), PMATH_CPP_MOVE(e1), PMATH_CPP_MOVE(e2));
  
  return Call(Symbol(richmath_System_EvaluationSequence), PMATH_CPP_MOVE(e1), PMATH_CPP_MOVE(e2), PMATH_CPP_MOVE(e3));
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

Dynamic::Dynamic(StyledObject *owner, Expr expr)
  : Base(),
  _owner(nullptr),
  _synchronous_updating(AutoBoolFalse)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  init(owner, expr);
}

void Dynamic::init(StyledObject *owner, Expr expr) {
  RICHMATH_ASSERT(_owner == nullptr && owner != nullptr);
  
  _owner = owner;
  *this = expr;
}

Expr Dynamic::operator=(Expr expr) {
  _expr = expr;
  _synchronous_updating = AutoBoolFalse;
  
  if(!_expr.is_expr())
    return _expr;
  
  if(_expr.item_equals(0, richmath_System_Dynamic)) {
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
                
      if(su == richmath_System_True)
        _synchronous_updating = AutoBoolTrue;
      else if(su == richmath_System_Automatic)
        _synchronous_updating = AutoBoolAutomatic;
      else
        _synchronous_updating = AutoBoolFalse;
    }
  }
  // for PureArgument(n), this->_synchronous_updating does not matter, because it is looked up in get_value()
  
  return _expr;
}

bool Dynamic::has_pre_or_post_assignment() {
  return Impl(*this).has_pre_or_post_assignment();
}

bool Dynamic::has_temporary_assignment() {
  return Impl(*this).has_temporary_assignment();
}

void Dynamic::assign(Expr value, bool pre, bool middle, bool post) {
  return Impl(*this).assign(value, pre, middle, post);
}

Expr Dynamic::get_value_unevaluated() {
  return Impl(*this).get_value_unevaluated();
}

Expr Dynamic::get_value_now() {
  return Impl(*this).get_value_now();
}

bool Dynamic::try_get_simple_value(Expr *result) {
  return Impl(*this).try_get_value_now(result, Evaluator::Simple);
}

void Dynamic::get_value_later(Expr job_info) {
  Impl(*this).get_value_later(PMATH_CPP_MOVE(job_info));
}

bool Dynamic::get_value(Expr *result, Expr job_info) {
  return Impl(*this).get_value(result, PMATH_CPP_MOVE(job_info));
}

bool Dynamic::is_dynamic_of(Expr sym) {
  return Impl(*this).is_dynamic_of(PMATH_CPP_MOVE(sym));
}

//} ... class Dynamic

//{ class Dynamic::Impl ...

bool Dynamic::Impl::is_template_slot(Expr expr, int *index) {
  if(expr.item_equals(0, richmath_System_PureArgument) && expr.expr_length() == 1) {
    Expr arg = expr[1];
    if(arg.is_int32()) {
      if(index) 
        *index = PMATH_AS_INT32(arg.get());
      return true;
    }
  }
  return false;
}

bool Dynamic::Impl::is_template_slot(int *index) {
  return is_template_slot(self._expr, index);
}

bool Dynamic::Impl::find_template_box_dynamic(Expr *source, TemplateBox **source_template, int *source_index) {
  int i;
  if(is_template_slot(&i))
    return find_template_box_dynamic(self._owner, i, source, source_template, source_index);
  
  return false;
}

bool Dynamic::Impl::find_template_box_dynamic(StyledObject *obj, int i, Expr *source, TemplateBox **source_template, int *source_index) {
  if(i == 0 || !obj)
    return false;
  
  obj = obj->style_parent();
  while(obj) {
    if(TemplateBox *template_box = dynamic_cast<TemplateBox*>(obj)) {
      int num_arg = (int)template_box->arguments.expr_length();
      
      if(i < 0)
        i+= num_arg + 1;
      
      Dynamic dyn { template_box, template_box->arguments[i] };
      if(!Impl(dyn).find_template_box_dynamic(source, source_template, source_index)) {
        if(source)          *source          = PMATH_CPP_MOVE(dyn._expr);
        if(source_template) *source_template = template_box;
        if(source_index)    *source_index    = i;
      }
      return true;
    }
    
    if(TemplateBoxSlot *slot = dynamic_cast<TemplateBoxSlot*>(obj)) {
      obj = slot->find_owner();
      if(!obj)
        break;
    }
    
    obj = obj->style_parent();
  }
  
  return false;
}

void Dynamic::Impl::get_assignment_functions(Expr expr, Expr *pre, Expr *middle, Expr *post) {
  *pre    = Symbol(richmath_System_None);
  *middle = Symbol(richmath_System_None);
  *post   = Symbol(richmath_System_None);
  
  if(!expr.item_equals(0, richmath_System_Dynamic))
    return;
  
  if(expr.expr_length() < 2) {
    *middle = Symbol(richmath_System_Automatic);
    return;
  }
  
  Expr fun = expr[2];
  if(!fun.item_equals(0, richmath_System_List)) {
    *middle = PMATH_CPP_MOVE(fun);
    if(*middle == richmath_System_Temporary)
      *post = Symbol(richmath_System_Automatic);
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

bool Dynamic::Impl::has_pre_or_post_assignment() {
  int i;
  if(is_template_slot(&i)) {
    Expr         source;
    TemplateBox *source_template;
    if(find_template_box_dynamic(&source, &source_template, nullptr)) {
      Dynamic dyn{ source_template, source };
      return dyn.has_pre_or_post_assignment();
    }
  }
  
  if(!self._expr.item_equals(0, richmath_System_Dynamic))
    return false;

  if(self._expr.expr_length() < 2)
    return false;
  
  Expr fun = self._expr[2];
  if(fun == richmath_System_Temporary)
    return true;
  
  if(!fun.item_equals(0, richmath_System_List))
    return false;
  
  return fun.expr_length() > 1;
}

bool Dynamic::Impl::has_temporary_assignment() {
  int i;
  if(is_template_slot(&i)) {
    Expr         source;
    TemplateBox *source_template;
    if(find_template_box_dynamic(&source, &source_template, nullptr)) {
      Dynamic dyn{ source_template, source };
      return dyn.has_temporary_assignment();
    }
  }
  
  if(!self._expr.item_equals(0, richmath_System_Dynamic))
    return false;

  if(self._expr.expr_length() < 2)
    return false;
  
  Expr pre;
  Expr middle;
  Expr post;
  get_assignment_functions(self._expr, &pre, &middle, &post);
  return middle == richmath_System_Temporary;
}

Expr Dynamic::Impl::make_assignment_call(Expr func, Expr name, Expr value) {
  if(func == richmath_System_Automatic)
    return Call(Symbol(richmath_System_Assign), PMATH_CPP_MOVE(name), PMATH_CPP_MOVE(value));
  
  if(func == richmath_System_None)
    return Expr();
  
  return Call(PMATH_CPP_MOVE(func), PMATH_CPP_MOVE(value), PMATH_CPP_MOVE(name));
}

void Dynamic::Impl::assign(Expr value, bool pre, bool middle, bool post) {
  StyledObject *dyn_source = self._owner;
  Expr dyn_expr = self._expr;
  
  int i;
  if(is_template_slot(dyn_expr, &i)) {
    Expr         source;
    TemplateBox *source_template;
    int          source_index;
    
    if(find_template_box_dynamic(self._owner, i, &source, &source_template, &source_index)) {
      Dynamic dyn{ source_template, source };
      dyn_source = source_template;
      
      while(Impl(dyn).find_template_box_dynamic(&source, &source_template, &source_index)) {
        //dyn.init(source_template, source);
        dyn_source = source_template;
        dyn._owner = source_template;
        dyn._expr  = PMATH_CPP_MOVE(source);
      }
      
      dyn_expr = dyn._expr;
      if(!dyn._expr.item_equals(0, richmath_System_Dynamic)) {
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
    
    if(middle) {
      self._expr = EvaluationContexts::replace_symbol_namespace(
                     PMATH_CPP_MOVE(value), 
                     EvaluationContexts::resolve_context(self._owner),
                     strings::DollarContext_namespace);
    }
    
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
    run = Call(Symbol(richmath_System_Assign), name, value);
  
  if(run.is_null())
    return;
  
  run = dyn_source->prepare_dynamic(PMATH_CPP_MOVE(run));
  run = EvaluationContexts::prepare_namespace_for(PMATH_CPP_MOVE(run), dyn_source);
  run = EvaluationContexts::make_context_block(PMATH_CPP_MOVE(run), EvaluationContexts::resolve_context(self.owner()));
  
  Application::interrupt_wait_for_interactive(PMATH_CPP_MOVE(run), self._owner, Application::dynamic_timeout);
}

bool Dynamic::Impl::is_dynamic_of(Expr sym) {
  bool is_dyn;
  Expr dyn = get_prepared_dynamic(&is_dyn);
  return is_dyn && dyn.item_equals(0, richmath_System_Dynamic) && dyn.item_equals(1, sym);
}

Expr Dynamic::Impl::get_value_unevaluated() {
  bool is_dynamic;
  return get_value_unevaluated(&is_dynamic);
}

Expr Dynamic::Impl::get_value_unevaluated(bool *is_dynamic) {
  Expr expr = get_prepared_dynamic(is_dynamic);
  if(!*is_dynamic)
    return expr;
  
  return wrap_dyn_eval_call(PMATH_CPP_MOVE(expr));
}

Expr Dynamic::Impl::wrap_dyn_eval_call(Expr expr) {
  return Call(
           Symbol(richmath_Internal_DynamicEvaluateMultiple),
           PMATH_CPP_MOVE(expr),
           self._owner->id().to_pmath_raw());
}

Expr Dynamic::Impl::get_prepared_dynamic(bool *is_dynamic) {
  StyledObject *dyn_source = self._owner;
  Expr dyn_expr = self._expr;
  
  int i;
  if(is_template_slot(&i)) {
    Expr         source;
    TemplateBox *source_template;
    
    if(find_template_box_dynamic(self._owner, i, &source, &source_template, nullptr)) {
      Dynamic dyn{ source_template, source };
      dyn_source = source_template;
      
      while(Impl(dyn).find_template_box_dynamic(&source, &source_template, nullptr)) {
        //dyn.init(source_template, source);
        dyn_source = source_template;
        dyn._owner = source_template;
        dyn._expr  = PMATH_CPP_MOVE(source);
      }
      
      dyn_expr = PMATH_CPP_MOVE(dyn._expr);
      if(!dyn_expr.item_equals(0, richmath_System_Dynamic)) {
        /* Note that dynamic changes in the find_template_box_dynamic-chain above would not be visible 
           for self._owner and would thus break its dynamic updating facility.
           This scenario can happen if a TemplateBox in the chain (a parent of self._owner)
           is modified e.g. programmatically.
           More importantly, it happens
         */
        source_template->register_observer(self._owner->id());
        
        *is_dynamic = false;
        return EvaluationContexts::prepare_namespace_for(PMATH_CPP_MOVE(dyn_expr), dyn._owner);
      }
      
    }
  }
  else if(!self.is_dynamic()) {
    *is_dynamic = false;
    return EvaluationContexts::prepare_namespace_for(self._expr, self._owner);
  }
  
  *is_dynamic = true;
  dyn_expr = dyn_source->prepare_dynamic(PMATH_CPP_MOVE(dyn_expr));
  return EvaluationContexts::prepare_namespace_for(PMATH_CPP_MOVE(dyn_expr), dyn_source);
}

Expr Dynamic::Impl::get_value_now() {
  Expr result;
  if(try_get_value_now(&result, Evaluator::Full)) 
    return result;
  return Symbol(richmath_System_DollarAborted);
}

bool Dynamic::Impl::try_get_value_now(Expr *result, Evaluator evaluator) {
  bool is_dynamic = false;
  Expr call = get_prepared_dynamic(&is_dynamic);
  
  if(!is_dynamic) {
    if(call.expr_length() == 1 && call.item_equals(0, richmath_System_Unevaluated)) {
      *result = call[1];
      return true;
    }
    *result = call;
    return true;
  }
  
  int mouseover_observer;
  if(!self._owner->own_style().get(InternalUsesCurrentValueOfMouseOver, &mouseover_observer))
    mouseover_observer = ObserverKindNone;
  
  self._owner->own_style().remove(InternalUsesCurrentValueOfMouseOver);
  
  bool has_result = SimpleEvaluator::try_eval(self._owner, result, call);
  if(!has_result && evaluator == Evaluator::Full) {
    call = wrap_dyn_eval_call(PMATH_CPP_MOVE(call));
    call = EvaluationContexts::make_context_block(PMATH_CPP_MOVE(call), EvaluationContexts::resolve_context(self.owner()));
    
    auto old_observer_id         = Dynamic::current_observer_id;
    Dynamic::current_observer_id = self._owner->id();
  
    *result = Application::interrupt_wait_for(PMATH_CPP_MOVE(call), self._owner, Application::dynamic_timeout);
  
    Dynamic::current_observer_id = old_observer_id;
    
    has_result = true;
  }
  
  if(has_result) {
    if(*result == PMATH_UNDEFINED)
      *result = Symbol(richmath_System_DollarAborted);
  }
  else {
    if(mouseover_observer != ObserverKindNone) {
      self._owner->own_style().set(InternalUsesCurrentValueOfMouseOver, mouseover_observer);
    }
  }
  
  return has_result;
}

void Dynamic::Impl::get_value_later(Expr job_info) {
  bool is_dynamic = false;
  Expr call = get_value_unevaluated(&is_dynamic);
  
  if(!is_dynamic) {
    self._owner->dynamic_finished(PMATH_CPP_MOVE(job_info), PMATH_CPP_MOVE(call));
    return;
  }
  
  self._owner->own_style().remove(InternalUsesCurrentValueOfMouseOver);
  
  Application::add_job(new DynamicEvaluationJob(PMATH_CPP_MOVE(job_info), PMATH_CPP_MOVE(call), self._owner));
}

bool Dynamic::Impl::get_value(Expr *result, Expr job_info) {
  if(result)
    *result = Expr();
    
  AutoBoolValues sync = self._synchronous_updating;
  
  int i;
  if(is_template_slot(&i)) {
    Expr         source;
    TemplateBox *source_template;
    
    if(find_template_box_dynamic(self._owner, i, &source, &source_template, nullptr)) {
      Dynamic dyn { source_template, source };
      
      while(Dynamic::Impl(dyn).find_template_box_dynamic(&source, &source_template, nullptr)) 
        dyn.init(PMATH_CPP_MOVE(source_template), PMATH_CPP_MOVE(source));
      
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
    if(Document *doc = Box::find_nearest_parent<Document>(self._owner))
      sync = doc->is_mouse_down() ? AutoBoolTrue : AutoBoolFalse;
  }
  
  if(sync != AutoBoolFalse || !self.is_dynamic()) {
    if(result)
      *result = get_value_now();
    else 
      get_value_now();
    
    return true;
  }
  
  get_value_later(PMATH_CPP_MOVE(job_info));
  return false;
}

//} ... class Dynamic::Impl

Expr richmath_eval_FrontEnd_PrepareDynamicEvaluation(Expr expr) {
  /*  FrontEnd`PrepareDynamicEvaluation(FrontEndObject(...), Dynamic(...))
      FrontEnd`PrepareDynamicEvaluation(Automatic,           Dynamic(...))
   */
  if(expr.expr_length() != 2)
    return Symbol(richmath_System_DollarFailed);
  
  StyledObject *obj = nullptr;
  if(expr.item_equals(1, richmath_System_Automatic))
    obj = dynamic_cast<StyledObject*>(Application::get_evaluation_object());
  else
    obj = FrontEndObject::find_cast<StyledObject>(FrontEndReference::from_pmath(expr[1]));
  
  if(obj)
    return Call(Symbol(richmath_System_Hold), Dynamic(obj, expr[2]).get_value_unevaluated());
  
  return Symbol(richmath_System_DollarFailed);
}

Expr richmath_eval_FrontEnd_AssignDynamicValue(Expr expr) {
  /*  FrontEnd`AssignDynamicValue(FrontEndObject(...), Dynamic(...), value)
      FrontEnd`AssignDynamicValue(Automatic,           Dynamic(...), value)
   */
  if(expr.expr_length() != 3)
    return Symbol(richmath_System_DollarFailed);
  
  StyledObject *obj = nullptr;
  if(expr.item_equals(1, richmath_System_Automatic))
    obj = dynamic_cast<StyledObject*>(Application::get_evaluation_object());
  else
    obj = FrontEndObject::find_cast<StyledObject>(FrontEndReference::from_pmath(expr[1]));
  
  if(obj) {
    Dynamic(obj, expr[2]).assign(expr[3]);
    return Expr();
  }
  
  return Symbol(richmath_System_DollarFailed);
}
