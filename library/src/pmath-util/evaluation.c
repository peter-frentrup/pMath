#include <pmath-util/evaluation.h>

#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/dynamic-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>


// pmath_maxrecursion is in pmath-objects.h

//#define DEBUG_LOG_EVAL

#ifdef DEBUG_LOG_EVAL
  static int indented = 0;
  static void debug_indent(void){
    int i;
    
    #ifdef PMATH_DEBUG_MEMORY
      pmath_debug_print("[t=%6"PRIuPTR"]", _pmath_debug_global_time);
    #endif
    
    for(i = indented % 30;i > 0;--i)
      pmath_debug_print("  ");
  }
#endif

static pmath_t evaluate_expression(
  pmath_expr_t  expr,    // will be freed
  pmath_thread_t     *thread_ptr,
  pmath_bool_t     apply_rules);

static pmath_t evaluate_symbol(
  pmath_symbol_t  sym,         // wont be freed
  pmath_thread_t *thread_ptr);

static pmath_t evaluate(
  pmath_t  obj,
  pmath_thread_t *thread_ptr
){
  #ifdef DEBUG_LOG_EVAL
    int iter = 0;
    ++indented;
    debug_indent();
    pmath_debug_print_object("eval ", obj, "");
    pmath_debug_print(" [%p] ...\n", obj);
  #endif

  while(!pmath_aborting()){
    #ifdef DEBUG_LOG_EVAL
      if(iter > 0){
        debug_indent();
        pmath_debug_print("... [# %d]: ", iter);
        pmath_debug_print_object("", obj, "");
        pmath_debug_print(" [%p] ...\n", obj);
      }
    #endif

    if(PMATH_IS_MAGIC(obj)){
      obj = NULL;
      goto FINISH;
    }
    
    if(pmath_is_string(obj) || pmath_is_number(obj))
      goto FINISH;
      
    if(pmath_is_expr(obj)){
      if(_pmath_expr_is_updated(obj))
        goto FINISH;
      
      obj = evaluate_expression(obj, thread_ptr, TRUE);
    }
    else if(pmath_is_symbol(obj)){
      pmath_t result = evaluate_symbol(obj, thread_ptr);
      
      if(result == PMATH_UNDEFINED)
        goto FINISH;
        
      pmath_unref(obj);
      obj = result;
    }
  }
  
 FINISH:
  #ifdef DEBUG_LOG_EVAL
    debug_indent();
    pmath_debug_print_object("... eval ", obj, "");
    pmath_debug_print(" [%p]\n", obj);
    --indented;
  #endif

  return obj;
}

static pmath_t handle_explicit_return(pmath_t expr){
  pmath_t obj;
  
  if(_pmath_contains_symbol(expr, PMATH_SYMBOL_RETURN))
    expr = pmath_evaluate(expr);
              
  if(pmath_is_expr_of(expr, PMATH_SYMBOL_RETURN)){
    switch(pmath_expr_length(expr)){
      case 0: 
        pmath_unref(expr); 
        return NULL; 
      
      case 1:
        obj = pmath_expr_get_item(expr, 1);
        pmath_unref(expr); 
        return obj;
      
      case 2: 
        obj = pmath_expr_get_item(expr, 2);
        if(pmath_is_integer(obj)){
          if(pmath_compare(obj, PMATH_NUMBER_ONE) <= 0){
            pmath_unref(obj);
            obj = pmath_expr_get_item(expr, 1);
            pmath_unref(expr); 
            return obj;
          }
          
          return pmath_expr_set_item(expr, 2,
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PLUS), 2,
              obj,
              pmath_integer_new_si(-1)));
        }
        pmath_unref(obj);
        break;
    }
  }
  
  return expr;
}

static pmath_t evaluate_expression(
  pmath_expr_t     expr,
  pmath_thread_t  *thread_ptr,
  pmath_bool_t     apply_rules
){
  struct _pmath_stack_info_t     stack_frame;
  struct _pmath_symbol_rules_t  *rules;
  pmath_symbol_t                 head;
  pmath_symbol_t                 head_sym;
  pmath_symbol_attributes_t      attr;
  pmath_bool_t                   hold_first;
  pmath_bool_t                   hold_rest;
  pmath_bool_t                   hold_complete;
  pmath_bool_t                   listable;
  pmath_bool_t                   associative;
  pmath_bool_t                   sequence_hold;
  pmath_bool_t                   symmetric;
  pmath_t                        item;
  pmath_expr_t                   expr_with_unevaluated;
  pmath_expr_t                   expr_without_unevaluated;
  size_t                         i;
  size_t                         exprlen;
  _pmath_timer_t                 expr_changes;
  
  assert(thread_ptr != NULL);
  assert(pmath_is_expr(expr));
    
  if(!*thread_ptr){
    *thread_ptr = pmath_thread_get_current();
    if(!*thread_ptr){
      pmath_unref(expr);
      return NULL;
    }
  }
  
  if(pmath_maxrecursion < (*thread_ptr)->evaldepth){
    if(!(*thread_ptr)->critical_messages){
      int tmp = (*thread_ptr)->evaldepth;
      (*thread_ptr)->evaldepth = 0;
      (*thread_ptr)->critical_messages = TRUE;
  
      pmath_debug_print("reclim with expr = %p ", (void*)expr);
      pmath_debug_print_object(" = ", expr, "\n");
      
      pmath_message(
        PMATH_SYMBOL_GENERAL, "reclim", 1,
        pmath_integer_new_si(pmath_maxrecursion));

      (*thread_ptr)->critical_messages = FALSE;
      (*thread_ptr)->evaldepth = tmp;
    }
    else{
      pmath_debug_print("[abort] reclim with expr=%p", (void*)expr);
      pmath_debug_print_object("= ", expr, "\n");
      pmath_throw(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
          pmath_ref(PMATH_SYMBOL_GENERAL),
          PMATH_C_STRING("reclim")));
    }
    
    expr = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_HOLD), 1, expr);
    _pmath_expr_update(expr);
    return expr;
  }
  
  (*thread_ptr)->evaldepth++;
  stack_frame.value = NULL;
  stack_frame.next = (*thread_ptr)->stack_info;
  (*thread_ptr)->stack_info = &stack_frame;
  
  head = evaluate(pmath_expr_get_item(expr, 0), thread_ptr);
  expr = pmath_expr_set_item(expr, 0, pmath_ref(head));
  stack_frame.value = pmath_ref(head);
  head_sym = _pmath_topmost_symbol(head);
  attr = pmath_symbol_get_attributes(head_sym);
  
  exprlen = pmath_expr_length(expr);
  expr_with_unevaluated = NULL;
  expr_without_unevaluated = NULL;
  
  hold_first    = FALSE;
  hold_rest     = FALSE;
  hold_complete = FALSE;
  listable      = FALSE;
  associative   = FALSE;
  sequence_hold = FALSE;
  symmetric     = FALSE;
  if(head_sym == head){
    hold_complete = (attr & PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE) != 0;
    if(!hold_complete){
      hold_first    = (attr & PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST) != 0;
      hold_rest     = (attr & PMATH_SYMBOL_ATTRIBUTE_HOLDREST)  != 0;
      listable      = (attr & PMATH_SYMBOL_ATTRIBUTE_LISTABLE) != 0;
      associative   = (attr & PMATH_SYMBOL_ATTRIBUTE_ASSOCIATIVE) != 0;
      sequence_hold = (attr & PMATH_SYMBOL_ATTRIBUTE_SEQUENCEHOLD) != 0;
      symmetric     = (attr & PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC) != 0;
    }
  }
  else{
    hold_first = hold_rest =      (attr & PMATH_SYMBOL_ATTRIBUTE_DEEPHOLDALL) != 0;
    hold_complete = hold_first && (attr & PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE) != 0;
  }
  
  if(!hold_complete){
    item = pmath_expr_get_item(expr, 1);
    
    if(!hold_first || pmath_is_expr_of_len(item, PMATH_SYMBOL_EVALUATE, 1)){
      item = evaluate(item, thread_ptr);
      expr = pmath_expr_set_item(expr, 1, item);
    }
    else
      pmath_unref(item);
    
    if(hold_rest){
      for(i = 2;i <= exprlen;++i){
        item = pmath_expr_get_item(expr, i);
        
        if(pmath_is_expr_of_len(item, PMATH_SYMBOL_EVALUATE, 1)){
          item = evaluate(item, thread_ptr);
          expr = pmath_expr_set_item(expr, i, item);
        }
        else
          pmath_unref(item);
      }
    }
    else{
      for(i = 2;i <= exprlen;++i){
        item = pmath_expr_get_item(expr, i);
        item = evaluate(item, thread_ptr);
        expr = pmath_expr_set_item(expr, i, item);
      }
    }

    if(apply_rules){
      if(_pmath_have_code(head, PMATH_CODE_USAGE_EARLYCALL)){
        expr_changes = _pmath_expr_last_change(expr);
        
        if(_pmath_run_code(head, PMATH_CODE_USAGE_EARLYCALL, &expr)){
          if(!pmath_is_expr(expr)
          || expr_changes != _pmath_expr_last_change(expr)){
            goto FINISH;
          }
        }
      }
    }
    
    if(listable){
      for(i = exprlen;i > 0;--i){
        item = pmath_expr_get_item(expr, i);
        
        if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST)){
          pmath_bool_t error_message = TRUE;
          pmath_unref(item);
            
          expr = _pmath_expr_thread(
            expr, PMATH_SYMBOL_LIST, 1, exprlen, &error_message);
          
          if(error_message)
            _pmath_expr_update(expr);
            
          goto FINISH;
        }
        
        pmath_unref(item);
      }
    }
    
    for(i = 1;i <= exprlen;++i){ // Unevaluated(...) items
      item = pmath_expr_get_item(expr, i);
      
      if(pmath_is_expr_of_len(item, PMATH_SYMBOL_UNEVALUATED, 1)){
        expr_with_unevaluated = pmath_ref(expr);
        
        expr = pmath_expr_set_item(
          expr, i, 
          pmath_expr_get_item(
            item, 1));
        
        pmath_unref(item);
        
        for(++i;i <= exprlen;++i){
          item = pmath_expr_get_item(expr, i);
          
          if(pmath_is_expr_of_len(item, PMATH_SYMBOL_UNEVALUATED, 1)){
            expr = pmath_expr_set_item(
              expr, i, 
              pmath_expr_get_item(
                item, 1));
          }
          
          pmath_unref(item);
        }
        
        break;
      }
      
      pmath_unref(item);
    }
    
    if(associative){
      expr = pmath_expr_flatten(
        expr,
        pmath_ref(head),
        PMATH_EXPRESSION_FLATTEN_MAX_DEPTH);
    }
    
    if(!sequence_hold){
      pmath_bool_t more = TRUE;
      
      while(more){
        more = FALSE;
        
        for(i = 1;i <= exprlen;++i){
          item = pmath_expr_get_item(expr, i);
          
          if(pmath_is_expr_of(item, PMATH_SYMBOL_SEQUENCE)){
            pmath_unref(item);
            expr = pmath_expr_flatten(
              expr,
              pmath_ref(PMATH_SYMBOL_SEQUENCE),
              PMATH_EXPRESSION_FLATTEN_MAX_DEPTH);
            
            more = associative;
            break;
          }
          
          if(associative && pmath_is_expr_of(item, head)){
            pmath_unref(item);
            expr = pmath_expr_flatten(
              expr,
              pmath_ref(head),
              PMATH_EXPRESSION_FLATTEN_MAX_DEPTH);
            
            more = TRUE;
            break;
          }
          
          pmath_unref(item);
        }
      }
    }
    
    if(symmetric)
      expr = pmath_expr_sort(expr);
    
    if(expr_with_unevaluated) // expr contained Unevaluated(...) items
      expr_without_unevaluated = pmath_ref(expr);
    
    if(apply_rules){
      for(i = 1;i <= exprlen;++i){ // up rules
        pmath_symbol_t sym;
        
        item = pmath_expr_get_item(expr, i);
        sym = _pmath_topmost_symbol(item);
        pmath_unref(item);
        
        if(sym){
          rules = _pmath_symbol_get_rules(sym, RULES_READ);
          if(rules){
            pmath_unref(stack_frame.value);
            stack_frame.value = pmath_ref(sym);
            if(_pmath_rulecache_find(&rules->up_rules, &expr)){
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
  
  if(apply_rules){
    if(head_sym){ // down/sub rules ...
      rules = _pmath_symbol_get_rules(head_sym, RULES_READ);
      if(rules){
        pmath_unref(stack_frame.value);
        stack_frame.value = pmath_ref(head_sym);
        if(head_sym == head){
          if(_pmath_rulecache_find(&rules->down_rules, &expr)){
            expr = handle_explicit_return(expr);
            goto FINISH;
          }
        }
        else if(_pmath_rulecache_find(&rules->sub_rules, &expr)){
          expr = handle_explicit_return(expr);
          goto FINISH;
        }
      }
    }
    
    expr_changes = _pmath_expr_last_change(expr);
    
    if(!hold_complete){ // up code
      for(i = 1;i <= exprlen;++i){
        pmath_symbol_t sym;
        
        item = pmath_expr_get_item(expr, i);
        sym = _pmath_topmost_symbol(item);
        pmath_unref(item);
        
        if(sym){
          pmath_unref(stack_frame.value);
          stack_frame.value = pmath_ref(sym);
          
          if(_pmath_run_code(sym, PMATH_CODE_USAGE_UPCALL, &expr)){
            if(!pmath_is_expr(expr)
            || expr_changes != _pmath_expr_last_change(expr)){
              pmath_unref(sym);
              goto FINISH;
            }
          }
          
          pmath_unref(sym);
        }
      }
    }
    
    if(head_sym == head){ // down code
      pmath_unref(stack_frame.value);
      stack_frame.value = pmath_ref(head_sym);
      
      if(_pmath_run_code(head_sym, PMATH_CODE_USAGE_DOWNCALL, &expr)){
        if(!pmath_is_expr(expr)
        || expr_changes != _pmath_expr_last_change(expr)){
          goto FINISH;
        }
      }
    }
    else{ // sub code
      pmath_unref(stack_frame.value);
      stack_frame.value = pmath_ref(head_sym);
      
      if(_pmath_run_code(head_sym, PMATH_CODE_USAGE_SUBCALL, &expr)){
        if(!pmath_is_expr(expr)
        || expr_changes != _pmath_expr_last_change(expr)){
          goto FINISH;
        }
      }
    }

    
    assert(pmath_is_expr(expr));
    assert(expr_changes == _pmath_expr_last_change(expr));
    
    _pmath_expr_update(expr);
  }
  
 FINISH:
  if(expr_with_unevaluated || expr_without_unevaluated){
    if(apply_rules && pmath_equals(expr, expr_without_unevaluated)){
      pmath_unref(expr);
      expr = expr_with_unevaluated;
      _pmath_expr_update(expr);
    }
    else
      pmath_unref(expr_with_unevaluated);
    
    pmath_unref(expr_without_unevaluated);
  }
  
  (*thread_ptr)->stack_info = stack_frame.next;
  pmath_unref(stack_frame.value);
  pmath_unref(head_sym);
  pmath_unref(head);
  (*thread_ptr)->evaldepth--;
  
  return expr;
}

static pmath_t evaluate_symbol(
  pmath_symbol_t  sym,         // wont be freed
  pmath_thread_t *thread_ptr
){
  pmath_symbol_attributes_t   attr;
  pmath_t                     value;
  
  assert(thread_ptr != NULL);
  assert(pmath_is_symbol(sym));
  
  attr = pmath_symbol_get_attributes(sym);
  
  if((attr & PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL) != 0){
    if(!*thread_ptr){
      *thread_ptr = pmath_thread_get_current();
      
      if(!*thread_ptr)
        return NULL;
    }
    
    value = _pmath_thread_local_load_with(sym, *thread_ptr);
  }
  else{
    value = _pmath_symbol_get_global_value(sym);
  }
  
  if(_pmath_dynamic_trackers){
    if(!*thread_ptr){
      *thread_ptr = pmath_thread_get_current();
      
      if(!*thread_ptr){
        pmath_unref(value);
        return NULL;
      }
    }
    
    if((*thread_ptr)->current_dynamic_id){
      _pmath_symbol_track_dynamic(sym, (*thread_ptr)->current_dynamic_id);
    }
  }
  
  value = _pmath_symbol_value_prepare(sym, value);
  
  if(pmath_instance_of(value, PMATH_TYPE_EVALUATABLE)
  || !value)
    return value;
  
  pmath_unref(value);
  return PMATH_UNDEFINED;
}
  
PMATH_API
pmath_t pmath_evaluate(pmath_t obj){
  pmath_thread_t current_thread = NULL;
  
  return evaluate(obj, &current_thread);
}

PMATH_API
pmath_t pmath_evaluate_expression(
  pmath_expr_t  expr,    // will be freed
  pmath_bool_t     apply_rules
){
  pmath_thread_t current_thread = NULL;
  
  return evaluate_expression(expr, &current_thread, apply_rules);
}
