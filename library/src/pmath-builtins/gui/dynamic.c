#include <pmath-util/evaluation.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/dynamic-private.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-util/concurrency/threadlocks.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/concurrency/threads-private.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

PMATH_PRIVATE pmath_t builtin_internal_dynamicevaluate(pmath_expr_t expr){
  pmath_t id_obj;
  pmath_thread_t thread = pmath_thread_get_current();
  
  if(!thread){
    return expr;
  }
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  id_obj = pmath_evaluate(pmath_expr_get_item(expr, 2));
  if(pmath_instance_of(id_obj, PMATH_TYPE_INTEGER)
  && pmath_integer_fits_si(id_obj)){
    intptr_t old_id;
    
    long id = pmath_integer_get_si(id_obj);
    pmath_unref(id_obj);
    
    id_obj = pmath_expr_get_item(expr, 1);
    
    pmath_unref(expr);
    
    pmath_atomic_fetch_add(&_pmath_dynamic_trackers, 1);
    old_id = thread->current_dynamic_id;
    thread->current_dynamic_id = id;
    
    id_obj = pmath_evaluate(id_obj);
    
    thread->current_dynamic_id = old_id;
    pmath_atomic_fetch_add(&_pmath_dynamic_trackers, -1);
    
    return id_obj;
  }
  
  pmath_unref(id_obj);
  pmath_message(NULL, "intm", 2, pmath_ref(expr), pmath_integer_new_si(2));
  return expr;
}

PMATH_PRIVATE pmath_t builtin_internal_dynamicremove(pmath_expr_t expr){
  size_t len = pmath_expr_length(expr);
  size_t i;
  
  for(i = len;i > 0;--i){
    pmath_t id_obj = pmath_evaluate(pmath_expr_get_item(expr, i));
    
    if(pmath_instance_of(id_obj, PMATH_TYPE_INTEGER)
    && pmath_integer_fits_si(id_obj)){
      long id = pmath_integer_get_si(id_obj);
      
      _pmath_dynamic_remove(id);
      
      pmath_unref(id_obj);
      continue;
    }
    
    if(pmath_is_expr_of(id_obj, PMATH_SYMBOL_LIST)){
      pmath_unref(builtin_internal_dynamicremove(id_obj));
      continue;
    }
    
    pmath_unref(id_obj);
  }
  
  pmath_unref(expr);
  return NULL;
}
