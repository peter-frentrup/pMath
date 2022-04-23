#include <eval/partial-dynamic.h>

#include <eval/application.h>
#include <eval/dynamic.h>
#include <eval/eval-contexts.h>
#include <eval/job.h>

#include <gui/document.h>


using namespace richmath;

namespace {
  template<typename T>
  struct LinkedList {
    LinkedList<T> *next;
    T value;
  };
}

extern pmath_symbol_t richmath_System_Directive;
extern pmath_symbol_t richmath_System_Dynamic;
extern pmath_symbol_t richmath_System_HoldComplete;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_PureArgument;
extern pmath_symbol_t richmath_System_ReplacePart;
extern pmath_symbol_t richmath_System_Rule;
extern pmath_symbol_t richmath_System_RuleDelayed;
extern pmath_symbol_t richmath_Internal_DynamicEvaluateMultiple;

namespace richmath {
  class PartialDynamic::Impl {
    public:
      Impl(PartialDynamic &self) : self{self} {}
      
      static Expr prepare_dyn_eval_template(Expr expr);
      static Expr replace_parts(Expr expr, Expr part_rules);
      Expr get_value_now();
      Expr get_dyncall_unevaluated(bool &synchronous_updating);
      
      Expr eval_finish_now(Expr eval);
    
    private:
      static size_t collect_dynamic_positions(Array<int32_t> &all_flat, Expr expr);
      static size_t collect_dynamic_positions(Array<int32_t> &all_flat, LinkedList<int32_t> *cur, int32_t cur_depth, Expr expr);
      static void append_pos(Array<int32_t> &all_flat, LinkedList<int32_t> *cur, int32_t cur_depth);
      static Expr get(Expr expr, Expr indices);
      static Expr get(Expr expr, const int32_t *indices, size_t depth);
    
    private:
      PartialDynamic &self;
  };
}

//{ class PartialDynamic ...

PartialDynamic::PartialDynamic() : _owner(nullptr) {
}

PartialDynamic::PartialDynamic(StyledObject *owner, Expr expr)
 : _owner(owner)
{
  _held_expr = Call(Symbol(richmath_System_HoldComplete), std::move(expr));
  _dyn_eval_template = Impl::prepare_dyn_eval_template(_held_expr);
}

void PartialDynamic::operator=(Expr expr) {
  _held_expr = Call(Symbol(richmath_System_HoldComplete), std::move(expr));
  _dyn_eval_template = Impl::prepare_dyn_eval_template(_held_expr);
}

Expr PartialDynamic::get_value_now() {
  if(!_owner || !_dyn_eval_template) 
    return expr();
  
  bool synchronous_updating;
  Expr eval = Impl(*this).get_dyncall_unevaluated(synchronous_updating);
  
  if(auto style = _owner->own_style()) 
    style->remove(InternalUsesCurrentValueOfMouseOver);

  return Impl(*this).eval_finish_now(std::move(eval));
}

void PartialDynamic::get_value_later(Expr job_info) {
  if(!_owner)
    return;
  
  if(!_dyn_eval_template) {
    _owner->dynamic_finished(std::move(job_info), expr());
    return;
  }
  
  bool synchronous_updating;
  Expr eval = Impl(*this).get_dyncall_unevaluated(synchronous_updating);
  
  if(auto style = _owner->own_style()) 
    style->remove(InternalUsesCurrentValueOfMouseOver);
  
  Application::add_job(new DynamicEvaluationJob(std::move(job_info), std::move(eval), _owner));
}

bool PartialDynamic::get_value(Expr *result, Expr job_info) {
  if(!_owner || !_dyn_eval_template) {
    *result = expr();
    return true;
  }
  
  bool synchronous_updating;
  Expr eval = Impl(*this).get_dyncall_unevaluated(synchronous_updating);
  
  if(auto style = _owner->own_style()) 
    style->remove(InternalUsesCurrentValueOfMouseOver);
  
  if(synchronous_updating) {
    *result = Impl(*this).eval_finish_now(std::move(eval));
    return true;
  }
  else {
    Application::add_job(new DynamicEvaluationJob(std::move(job_info), std::move(eval), _owner));
    return false;
  }
}

Expr PartialDynamic::finish_dynamic(Expr dyn_eval_result) {
  Expr held = Impl::replace_parts(_held_expr, std::move(dyn_eval_result));
  if(held.expr_length() == 1 && held[0] == richmath_System_HoldComplete)
    return held[1];
  
  return Expr();
}
      
//} ... class PartialDynamic

//{ class PartialDynamic::Impl ...

Expr PartialDynamic::Impl::prepare_dyn_eval_template(Expr expr) {
// Returns part rules {{i1,i2,...} -> Dynamic(...), {j1, j2, ...} -> Dynamic(...), ...} or /\/

  Array<int32_t> all_flat;
  size_t num_dyn = collect_dynamic_positions(all_flat, expr);
  
  if(num_dyn == 0)
    return Expr();
  
  pmath_t idx_blob = pmath_blob_new(((size_t)all_flat.length() - num_dyn) * sizeof(int32_t), FALSE);
  if(auto idx = (int32_t*)pmath_blob_try_write(idx_blob)) {
    int i = 0;
    int j = 0;
    while(i < all_flat.length()) {
      for(int d = all_flat[i++]; d > 0; --d) {
        idx[j++] = all_flat[i++];
      }
    }
    RICHMATH_ASSERT(i == all_flat.length());
    RICHMATH_ASSERT(j == all_flat.length() - (int)num_dyn);
  }
  
  Expr templ = MakeCall(Symbol(richmath_System_List), num_dyn);
  size_t k = 0;
  size_t off = 0;
  int i = 0;
  while(i < all_flat.length()) {
    size_t depth = (size_t)all_flat[i++];
    Expr idx(pmath_packed_array_new(pmath_ref(idx_blob), PMATH_PACKED_INT32, 1, &depth, nullptr, off));
    
    off+= depth * sizeof(int32_t);
    i+= (int)depth;
    
    templ.set(++k, Rule(idx, get(expr, idx)));
  }
  
  return templ;
}

Expr PartialDynamic::Impl::replace_parts(Expr expr, Expr part_rules) {
  if(part_rules[0] != richmath_System_List)
    return expr;
  
  return Evaluate(Call(Symbol(richmath_System_ReplacePart), std::move(expr), std::move(part_rules)));
}

Expr PartialDynamic::Impl::get_dyncall_unevaluated(bool &synchronous_updating) {
  Expr eval = self._dyn_eval_template;
  int sync_settings = 0;
  for(size_t i = eval.expr_length(); i > 0; --i) {
    Expr rule = eval[i];
    Dynamic dyn(self._owner, rule[2]);
    sync_settings |= 1 << dyn.synchronous_updating();
    
    Expr eval_dyn_i = dyn.get_value_unevaluated();
    if(eval_dyn_i[0] == richmath_Internal_DynamicEvaluateMultiple) {
      rule.set(2, eval_dyn_i[1]);
      eval.set(i, std::move(rule));
    }
  }
  
  eval = Call(Symbol(richmath_Internal_DynamicEvaluateMultiple), std::move(eval), self._owner->id().to_pmath_raw());
  
  if(sync_settings == 1 << AutoBoolAutomatic) {
    sync_settings -= 1 << AutoBoolAutomatic;
    
    AutoBoolValues sync = AutoBoolTrue;
    if(Document *doc = Box::find_nearest_parent<Document>(self._owner))
      sync = doc->is_mouse_down() ? AutoBoolTrue : AutoBoolFalse;
    
    sync_settings |= 1 << sync;
  }
  
  if(sync_settings & (1 << AutoBoolFalse)) 
    synchronous_updating = false;
  else
    synchronous_updating = true;
  
  return eval;
}

Expr PartialDynamic::Impl::eval_finish_now(Expr eval) {
  eval = EvaluationContexts::make_context_block(std::move(eval), EvaluationContexts::resolve_context(self._owner));
  
  auto old_eval_id = Dynamic::current_observer_id;
  Dynamic::current_observer_id = self._owner->id();
  
  Expr repl = Application::interrupt_wait_for(std::move(eval), self._owner, Application::dynamic_timeout);
                 
  Dynamic::current_observer_id = old_eval_id;
  
  return self.finish_dynamic(repl);
}

size_t PartialDynamic::Impl::collect_dynamic_positions(Array<int32_t> &all_flat, Expr expr) {
  return collect_dynamic_positions(all_flat, nullptr, 0, std::move(expr));
}

size_t PartialDynamic::Impl::collect_dynamic_positions(Array<int32_t> &all_flat, LinkedList<int32_t> *cur, int32_t cur_depth, Expr expr) {
  if(!expr.is_expr())
    return 0;
  
  if(expr.is_packed_array())
    return 0;
  
  size_t exprlen = expr.expr_length();
  if(exprlen >= INT32_MAX)
    return 0;
  
  Expr head = expr[0];
  if(head == richmath_System_Dynamic || head == richmath_System_PureArgument) {
    append_pos(all_flat, cur, cur_depth);
    return 1;
  }
  
  LinkedList<int32_t> next;
  next.next = cur;
  
  if(head == richmath_System_Rule || head == richmath_System_RuleDelayed) {
    next.value = 2;
    return collect_dynamic_positions(all_flat, &next, cur_depth + 1, expr[next.value]);
  }
  
  if( head == richmath_System_List      || 
      head == richmath_System_Directive || 
      head == richmath_System_HoldComplete) 
  {
    size_t count = 0;
    for(next.value = 1; next.value <= (int32_t)exprlen; ++next.value) {
      count+= collect_dynamic_positions(all_flat, &next, cur_depth + 1, expr[next.value]);
    }
    return count;
  }
  
  return 0;
}

void PartialDynamic::Impl::append_pos(Array<int32_t> &all_flat, LinkedList<int32_t> *cur, int32_t cur_depth) {
  int oldlen = all_flat.length();
  all_flat.length(oldlen + cur_depth + 1);
  all_flat.set(oldlen, cur_depth);
  for(int i = cur_depth; i > 0; --i, cur = cur->next) {
    RICHMATH_ASSERT(cur);
    all_flat.set(oldlen + i, cur->value);
  }
  RICHMATH_ASSERT(!cur);
}

Expr PartialDynamic::Impl::get(Expr expr, Expr indices) {
  if( indices.is_packed_array() && 
      pmath_packed_array_get_element_type(indices.get()) == PMATH_PACKED_INT32 &&
      pmath_packed_array_get_dimensions(indices.get()) == 1 &&
      pmath_packed_array_get_non_continuous_dimensions(indices.get()) == 0)
  {
    const size_t *sizes = pmath_packed_array_get_sizes(indices.get());
    return get(std::move(expr), (const int32_t*)pmath_packed_array_read(indices.get(), nullptr, 0), *sizes);
  }
  
  return Expr();
}

Expr PartialDynamic::Impl::get(Expr expr, const int32_t *indices, size_t depth) {
  for(; depth; ++indices, --depth) {
    expr = expr[*indices];
  }
  return expr;
}

//} ... class PartialDynamic::Impl
