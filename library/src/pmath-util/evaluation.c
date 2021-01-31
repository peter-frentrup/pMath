#include <pmath-util/evaluation.h>

#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/dynamic-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/control/flow-private.h>
#include <pmath-builtins/lists-private.h>


extern pmath_symbol_t pmath_System_Evaluate;
extern pmath_symbol_t pmath_System_General;
extern pmath_symbol_t pmath_System_Hold;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_MessageName;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_Return;
extern pmath_symbol_t pmath_System_Sequence;
extern pmath_symbol_t pmath_System_Unevaluated;

// pmath_maxrecursion is in pmath-objects.h

//#define DEBUG_LOG_EVAL

#ifdef DEBUG_LOG_EVAL
static int indented = 0;
static void debug_indent(void) {
  int i;
  
#ifdef PMATH_DEBUG_MEMORY
  pmath_debug_print("[t=%6"PRIuPTR"]", _pmath_debug_global_time);
#endif
  
  for(i = indented % 30; i > 0; --i)
    pmath_debug_print("  ");
}
#endif

static pmath_t evaluate_expression(
  pmath_expr_t   expr,    // will be freed
  pmath_thread_t current_thread,
  pmath_bool_t   apply_rules);

static pmath_t evaluate_symbol(
  pmath_symbol_t sym,         // wont be freed
  pmath_thread_t current_thread);

static pmath_t evaluate(
  pmath_t        obj,
  pmath_thread_t current_thread
) {
#ifdef DEBUG_LOG_EVAL
  int iter = 0;
  ++indented;
  debug_indent();
  pmath_debug_print_object("eval ", obj, "");
  pmath_debug_print(" [%"PRIx64"] ...\n", obj.as_bits);
#endif
  
  while(!pmath_aborting()) {
#ifdef DEBUG_LOG_EVAL
    if(iter > 0) {
      debug_indent();
      pmath_debug_print("... [# %d]: ", iter);
      pmath_debug_print_object("", obj, "");
      pmath_debug_print(" [%"PRIx64"] ...\n", obj.as_bits);
    }
#endif
    
    if(pmath_is_expr(obj)) {
      if(_pmath_expr_is_updated(obj))
        break;
        
      obj = evaluate_expression(obj, current_thread, TRUE);
      continue;
    }
    
    if(pmath_is_symbol(obj)) {
      pmath_t result = evaluate_symbol(obj, current_thread);
      
      if(pmath_same(result, PMATH_UNDEFINED))
        break;
        
      pmath_unref(obj);
      obj = result;
      continue;
    }
    
#ifdef PMATH_DEBUG_LOG
    if(pmath_is_mpint(obj)) {
      if(mpz_fits_sint_p(PMATH_AS_MPZ(obj))) {
        pmath_debug_print_object("\n[WARNING unnormalized mp int ", obj, "]\n");
      }
    }
#endif
    
    if(!pmath_is_string(obj) && !pmath_is_number(obj)) {
      obj = PMATH_NULL;
      break;
    }
    
    break;
  }
  
#ifdef DEBUG_LOG_EVAL
  debug_indent();
  pmath_debug_print_object("... eval ", obj, "");
  pmath_debug_print(" [%"PRIx64"] ...\n", obj.as_bits);
  --indented;
#endif
  
  return obj;
}

static pmath_t handle_explicit_return(pmath_t expr) {
  pmath_t obj;
  
  if(_pmath_contains_symbol(expr, pmath_System_Return))
    expr = pmath_evaluate(expr);
    
  if(pmath_is_expr_of(expr, pmath_System_Return)) {
    switch(pmath_expr_length(expr)) {
      case 0:
        pmath_unref(expr);
        return PMATH_NULL;
        
      case 1:
        obj = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        return obj;
        
      case 2:
        obj = pmath_expr_get_item(expr, 2);
        if(pmath_is_integer(obj)) {
          if(pmath_compare(obj, PMATH_FROM_INT32(1)) <= 0) {
            pmath_unref(obj);
            obj = pmath_expr_get_item(expr, 1);
            pmath_unref(expr);
            return obj;
          }
          
          return pmath_expr_set_item(expr, 2,
                                     pmath_expr_new_extended(
                                       pmath_ref(pmath_System_Plus), 2,
                                       obj,
                                       PMATH_FROM_INT32(-1)));
        }
        pmath_unref(obj);
        break;
    }
  }
  
  return expr;
}

static pmath_expr_t evaluate_arguments(
  pmath_expr_t   expr,
  pmath_thread_t current_thread,
  pmath_bool_t   hold_first,
  pmath_bool_t   hold_rest
) {
  pmath_t item;
  pmath_t debug_info;
  size_t i;
  size_t exprlen = pmath_expr_length(expr);
  
  debug_info = pmath_get_debug_info(expr);
  item = pmath_expr_get_item(expr, 1);
  
  if(!hold_first || pmath_is_expr_of_len(item, pmath_System_Evaluate, 1)) {
    pmath_unref(item);
    item = pmath_expr_extract_item(expr, 1);
    
    item = evaluate(item, current_thread);
    expr = pmath_expr_set_item(expr, 1, item);
  }
  else
    pmath_unref(item);
    
  if(hold_rest) {
    for(i = 2; i <= exprlen; ++i) {
      item = pmath_expr_get_item(expr, i);
      
      if(pmath_is_expr_of_len(item, pmath_System_Evaluate, 1)) {
        item = evaluate(item, current_thread);
        expr = pmath_expr_set_item(expr, i, item);
      }
      else
        pmath_unref(item);
    }
  }
  else {
    for(i = 2; i <= exprlen; ++i) {
      item = pmath_expr_extract_item(expr, i);
      item = evaluate(item, current_thread);
      expr = pmath_expr_set_item(expr, i, item);
    }
  }
  
  expr = pmath_try_set_debug_info(expr, debug_info);
  
  return expr;
}

static pmath_bool_t evaluator_thread_list_arguments(pmath_expr_t *expr) {
  size_t i;
  size_t exprlen = pmath_expr_length(*expr);
  pmath_t item;
  
  for(i = exprlen; i > 0; --i) {
    item = pmath_expr_get_item(*expr, i);
    
    if(pmath_is_expr_of(item, pmath_System_List)) {
      pmath_bool_t error_message = TRUE;
      pmath_unref(item);
      
      *expr = _pmath_expr_thread(
                *expr, pmath_System_List, 1, i, &error_message);
                
      if(error_message)
        _pmath_expr_update(*expr);
        
      return TRUE;
    }
    
    pmath_unref(item);
  }
  
  return FALSE;
}

static pmath_expr_t evaluator_flatten_sequences(
  pmath_expr_t expr,
  pmath_t      head_if_associative,
  pmath_bool_t associative
) {
  size_t i;
  size_t exprlen = pmath_expr_length(expr);
  
  pmath_bool_t more = TRUE;
  
  while(more) {
    more = FALSE;
    
    for(i = 1; i <= exprlen; ++i) {
      pmath_t item = pmath_expr_get_item(expr, i);
      
      if(pmath_is_expr_of(item, pmath_System_Sequence)) {
        pmath_unref(item);
        expr = pmath_expr_flatten(
                 expr,
                 pmath_ref(pmath_System_Sequence),
                 PMATH_EXPRESSION_FLATTEN_MAX_DEPTH);
                 
        more = associative;
        break;
      }
      
      if(associative && pmath_is_expr_of(item, head_if_associative)) {
        pmath_unref(item);
        expr = pmath_expr_flatten(
                 expr,
                 pmath_ref(head_if_associative),
                 PMATH_EXPRESSION_FLATTEN_MAX_DEPTH);
                 
        more = TRUE;
        break;
      }
      
      pmath_unref(item);
    }
  }
  
  return expr;
}

static pmath_expr_t evaluator_strip_unevaluated(
  pmath_expr_t expr,
  pmath_expr_t *out_orig_expr_if_changed
) {
  size_t i;
  size_t exprlen = pmath_expr_length(expr);
  pmath_t item;
  
  if(out_orig_expr_if_changed)
    *out_orig_expr_if_changed = PMATH_NULL;
    
  for(i = 1; i <= exprlen; ++i) {
    item = pmath_expr_get_item(expr, i);
    
    if(pmath_is_expr_of_len(item, pmath_System_Unevaluated, 1)) {
      if(out_orig_expr_if_changed)
        *out_orig_expr_if_changed = pmath_ref(expr);
        
      expr = pmath_expr_set_item(
               expr, i,
               pmath_expr_get_item(
                 item, 1));
                 
      pmath_unref(item);
      
      for(++i; i <= exprlen; ++i) {
        item = pmath_expr_get_item(expr, i);
        
        if(pmath_is_expr_of_len(item, pmath_System_Unevaluated, 1)) {
          expr = pmath_expr_set_item(
                   expr, i,
                   pmath_expr_get_item(
                     item, 1));
        }
        
        pmath_unref(item);
      }
      
      return expr;
    }
    
    pmath_unref(item);
  }
  
  return expr;
}

static pmath_t evaluate_expression(
  pmath_expr_t   expr,
  pmath_thread_t current_thread,
  pmath_bool_t   apply_rules
) {
  struct _pmath_stack_info_t     stack_frame;
  struct _pmath_symbol_rules_t  *rules;
  pmath_symbol_t                 head;
  pmath_symbol_t                 head_sym;
  pmath_symbol_attributes_t      attr;
  pmath_bool_t                   hold_complete;
  pmath_t                        item;
  pmath_expr_t                   expr_with_unevaluated;
  size_t                         i;
  size_t                         exprlen;
  _pmath_timer_t                 expr_changes;
  
  assert(pmath_is_expr(expr));
  
  if(!current_thread) {
    pmath_unref(expr);
    return PMATH_NULL;
  }
  
  if(pmath_maxrecursion < current_thread->evaldepth) {
    if(!current_thread->critical_messages) {
      int tmp = current_thread->evaldepth;
      current_thread->evaldepth = 0;
      current_thread->critical_messages = TRUE;
      
      pmath_debug_print("reclim with expr = ");
      pmath_debug_print_object("", expr, "\n");
      
      pmath_message(
        pmath_System_General, "reclim", 1,
        PMATH_FROM_INT32(pmath_maxrecursion));
        
      current_thread->critical_messages = FALSE;
      current_thread->evaldepth = tmp;
    }
    else {
      pmath_debug_print("[abort] reclim with expr = ");
      pmath_debug_print_object("", expr, "\n");
      pmath_throw(
        pmath_expr_new_extended(
          pmath_ref(pmath_System_MessageName), 2,
          pmath_ref(pmath_System_General),
          PMATH_C_STRING("reclim")));
    }
    
    expr = pmath_expr_new_extended(pmath_ref(pmath_System_Hold), 1, expr);
    _pmath_expr_update(expr);
    return expr;
  }
  
  current_thread->evaldepth++;
  stack_frame.head          = PMATH_NULL;
  stack_frame.debug_info    = pmath_get_debug_info(expr);
  stack_frame.next          = current_thread->stack_info;
  current_thread->stack_info = &stack_frame;
  
  head             = evaluate(pmath_expr_get_item(expr, 0), current_thread);
  expr             = pmath_expr_set_item(expr, 0, pmath_ref(head));
  stack_frame.head = pmath_ref(head);
  head_sym         = _pmath_topmost_symbol(head);
  
  exprlen               = pmath_expr_length(expr);
  expr_with_unevaluated = PMATH_NULL;
  
  attr = _pmath_get_function_attributes(head);
  hold_complete = (attr & PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE) != 0;
  
  if(!hold_complete) {
    pmath_bool_t hold_first    = (attr & PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST)    != 0;
    pmath_bool_t hold_rest     = (attr & PMATH_SYMBOL_ATTRIBUTE_HOLDREST)     != 0;
    pmath_bool_t listable      = (attr & PMATH_SYMBOL_ATTRIBUTE_LISTABLE)     != 0;
    pmath_bool_t associative   = (attr & PMATH_SYMBOL_ATTRIBUTE_ASSOCIATIVE)  != 0;
    pmath_bool_t sequence_hold = (attr & PMATH_SYMBOL_ATTRIBUTE_SEQUENCEHOLD) != 0;
    pmath_bool_t symmetric     = (attr & PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC)    != 0;
    
    expr = evaluate_arguments(
             expr,
             current_thread,
             hold_first,
             hold_rest);
             
    if(apply_rules) {
      if(_pmath_have_code(head, PMATH_CODE_USAGE_EARLYCALL)) {
        expr_changes = _pmath_expr_last_change(expr);
        
        if(_pmath_run_code(current_thread, head, PMATH_CODE_USAGE_EARLYCALL, &expr)) {
          if( !pmath_is_expr(expr) ||
              expr_changes != _pmath_expr_last_change(expr))
          {
            goto FINISH;
          }
        }
      }
    }
    
    if(listable) {
      if(evaluator_thread_list_arguments(&expr))
        goto FINISH;
    }
    
    if(associative) {
      expr = pmath_expr_flatten(
               expr,
               pmath_ref(head),
               PMATH_EXPRESSION_FLATTEN_MAX_DEPTH);
               
      exprlen = pmath_expr_length(expr);
    }
    
    if(!sequence_hold) {
      expr = evaluator_flatten_sequences(expr, head, associative);
      
      exprlen = pmath_expr_length(expr);
    }
    
    if(symmetric)
      expr = pmath_expr_sort(expr);
      
    if(apply_rules) {
      expr = evaluator_strip_unevaluated(
               expr,
               &expr_with_unevaluated);
               
      for(i = 1; i <= exprlen; ++i) { // up rules
        pmath_symbol_t sym;
        
        item = pmath_expr_get_item(expr, i);
        sym = _pmath_topmost_symbol(item);
        pmath_unref(item);
        
        if(!pmath_is_null(sym)) {
          rules = _pmath_symbol_get_rules(sym, RULES_READ);
          
          if(rules) {
            pmath_unref(stack_frame.head);
            stack_frame.head = pmath_ref(sym);
            if(_pmath_rulecache_find(&rules->up_rules, &expr)) {
              expr = handle_explicit_return(expr);
              pmath_unref(sym);
              goto FINISH;
            }
          }
          
          pmath_unref(sym);
        }
      }
    }
  }
  
  if(apply_rules) {
    if(!pmath_is_null(head_sym)) { // down/sub rules ...
      rules = _pmath_symbol_get_rules(head_sym, RULES_READ);
      if(rules) {
        pmath_unref(stack_frame.head);
        stack_frame.head = pmath_ref(head_sym);
        if(pmath_same(head_sym, head)) {
          if(_pmath_rulecache_find(&rules->down_rules, &expr)) {
            expr = handle_explicit_return(expr);
            goto FINISH;
          }
        }
        else if(_pmath_rulecache_find(&rules->sub_rules, &expr)) {
          expr = handle_explicit_return(expr);
          goto FINISH;
        }
      }
    }
    
    expr_changes = _pmath_expr_last_change(expr);
    
    if(!hold_complete) { // up code
      for(i = 1; i <= exprlen; ++i) {
        pmath_symbol_t sym;
        
        item = pmath_expr_get_item(expr, i);
        sym = _pmath_topmost_symbol(item);
        pmath_unref(item);
        
        if(!pmath_is_null(sym)) {
          pmath_unref(stack_frame.head);
          stack_frame.head = pmath_ref(sym);
          
          if(_pmath_run_code(current_thread, sym, PMATH_CODE_USAGE_UPCALL, &expr)) {
            if(!pmath_is_expr(expr) ||
                expr_changes != _pmath_expr_last_change(expr))
            {
              pmath_unref(sym);
              goto FINISH;
            }
          }
          
          pmath_unref(sym);
        }
      }
    }
    
    if(pmath_same(head_sym, head)) { // down code
      pmath_unref(stack_frame.head);
      stack_frame.head = pmath_ref(head_sym);
      
      if(_pmath_run_code(current_thread, head_sym, PMATH_CODE_USAGE_DOWNCALL, &expr)) {
        if( !pmath_is_expr(expr) ||
            expr_changes != _pmath_expr_last_change(expr))
        {
          goto FINISH;
        }
      }
    }
    else { // sub code
      pmath_unref(stack_frame.head);
      stack_frame.head = pmath_ref(head_sym);
      
      if(_pmath_run_code(current_thread, head_sym, PMATH_CODE_USAGE_SUBCALL, &expr)) {
        if( !pmath_is_expr(expr) ||
            expr_changes != _pmath_expr_last_change(expr))
        {
          goto FINISH;
        }
      }
    }
    
    
    assert(pmath_is_expr(expr));
    assert(expr_changes == _pmath_expr_last_change(expr));
    
    _pmath_expr_update(expr);
  }
  
  if(!pmath_is_null(expr_with_unevaluated)) {
    pmath_unref(expr);
    expr = expr_with_unevaluated;
    expr_with_unevaluated = PMATH_NULL;
    
    _pmath_expr_update(expr);
  }
  
FINISH:
  current_thread->stack_info = stack_frame.next;
  pmath_unref(stack_frame.head);
  pmath_unref(stack_frame.debug_info);
  pmath_unref(head_sym);
  pmath_unref(head);
  current_thread->evaldepth--;
  
  pmath_unref(expr_with_unevaluated);
  
  return expr;
}

static pmath_t evaluate_symbol(
  pmath_symbol_t sym,         // wont be freed
  pmath_thread_t current_thread
) {
  pmath_symbol_attributes_t   attr;
  pmath_t                     value;
  
  assert(pmath_is_symbol(sym));
  
  attr = pmath_symbol_get_attributes(sym);
  
  if((attr & PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL) != 0) {
    if(!current_thread)
      return PMATH_NULL;
      
    value = _pmath_thread_local_load_with(sym, current_thread);
  }
  else {
    value = _pmath_symbol_get_global_value(sym);
  }
  
  if(pmath_atomic_read_aquire(&_pmath_dynamic_trackers)) {
    if(!current_thread) {
      pmath_unref(value);
      return PMATH_NULL;
    }
    
    if(current_thread->current_dynamic_id) {
      _pmath_symbol_track_dynamic(sym, current_thread->current_dynamic_id);
    }
  }
  
  value = _pmath_symbol_value_prepare(sym, value);
  
  if(pmath_is_evaluatable(value))
    return value;
    
  pmath_unref(value);
  return PMATH_UNDEFINED;
}

PMATH_API
pmath_t pmath_evaluate(pmath_t obj) {
  pmath_thread_t current_thread = pmath_thread_get_current();
  
  return evaluate(obj, current_thread);
}

PMATH_API
pmath_t pmath_evaluate_expression(
  pmath_expr_t  expr,    // will be freed
  pmath_bool_t  apply_rules
) {
  pmath_thread_t current_thread = pmath_thread_get_current();
  
  return evaluate_expression(expr, current_thread, apply_rules);
}
