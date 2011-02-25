#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/concurrency/threadpool-private.h>

#include <pmath-builtins/all-symbols-private.h>


struct parallel_try_info_t{
  pmath_thread_t parent;
  pmath_t function;
  pmath_expr_t items;
  pmath_t *results;
  
  size_t results_count;
  intptr_t index; // 1, ..., Length(items)
  intptr_t result_index; // 0, ..., results_count-1
};

static void parallel_try(void *ptr){
  struct parallel_try_info_t *info = (struct parallel_try_info_t*)ptr;
  
  pmath_thread_t thread = _pmath_thread_new(info->parent);
  intptr_t i = pmath_atomic_fetch_add(&info->index, +1);
  size_t items_len = pmath_expr_length(info->items);
  
  if(thread){
    pmath_thread_t old = pmath_thread_get_current();
    _pmath_thread_set_current(thread);
    
    while((size_t)i <= items_len && !pmath_thread_aborting(thread)){
      pmath_t obj;
      
      obj = pmath_expr_get_item(info->items, i);
      obj = pmath_expr_new_extended(
        pmath_ref(info->function), 1, 
        obj);
      obj = pmath_evaluate(obj);
      
      if(obj != PMATH_SYMBOL_FAILED && !pmath_thread_aborting(thread)){
        size_t ri = (size_t)pmath_atomic_fetch_add(&info->result_index, +1);
        
        if(ri >= info->results_count){
          pmath_unref(obj);
          
          _pmath_thread_throw(
            info->parent,
            pmath_ref(PMATH_SYMBOL_PARALLEL_RETURN));
          
          break;
        }
        
        assert(info->results[ri] == NULL);
        info->results[ri] = obj;
      }
      else
        pmath_unref(obj);
      
      i = pmath_atomic_fetch_add(&info->index, +1);
    }
    
    _pmath_thread_clean(FALSE);
    _pmath_thread_set_current(old);
  }
  
  _pmath_thread_free(thread);
}

static void dummy(void *arg){
}

PMATH_PRIVATE pmath_t builtin_paralleltry(pmath_expr_t expr){
/* ParallelTry(list, f)
   ParallelTry(list, f, k)
 */
  struct parallel_try_info_t info;
  pmath_t result;
  pmath_t exception;
  pmath_task_t *tasks;
  size_t exprlen, i, task_count;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 2 || exprlen > 3){
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  task_count = (size_t)_pmath_processor_count();
  
  if(exprlen == 3){
    pmath_t count = pmath_expr_get_item(expr, 3);
    
    if(!pmath_is_integer(count) || !pmath_integer_fits_ui(count)){
      pmath_unref(count);
      pmath_message(NULL, "intnm", 2, 
        pmath_integer_new_si(3), 
        pmath_ref(expr));
      return expr;
    }
    
    info.results_count = pmath_integer_get_ui(count);
    pmath_unref(count);
  }
  else
    info.results_count = 1;
  
  result = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), info.results_count);
  if(!result){
    pmath_unref(expr);
    return NULL;
  }
  
  assert(result->refcount == 1);
  assert(result->type_shift == PMATH_TYPE_SHIFT_EXPRESSION_GENERAL);
  info.results = &((struct _pmath_unpacked_expr_t*)result)->items[1];
  info.result_index = 0;
  
  info.index = 1;
  info.items = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(info.items)){
    pmath_unref(info.items);
    pmath_unref(result);
    pmath_message(NULL, "nexprat", 2,
      pmath_integer_new_si(1),
      pmath_ref(expr));
    return expr;
  }
  
  info.function = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  if(task_count > pmath_expr_length(info.items))
    task_count = pmath_expr_length(info.items);
    
  tasks = pmath_mem_alloc(task_count * sizeof(pmath_task_t));
  if(!tasks){
    pmath_unref(info.function);
    pmath_unref(info.items);
    pmath_unref(result);
  }
  
  info.parent = pmath_thread_get_current();
  
  for(i = 0;i < (size_t)task_count;++i){
    tasks[i] = _pmath_task_new_with_thread(
      parallel_try,
      dummy,
      &info,
      NULL);
  }

  for(i = 0;i < (size_t)task_count;++i){
    pmath_task_wait(tasks[i]);
    pmath_task_unref(tasks[i]);
  }
  
  pmath_mem_free(tasks);
  
  exception = pmath_catch();
  if(exception != PMATH_UNDEFINED
  && exception != PMATH_SYMBOL_PARALLEL_RETURN){
    pmath_throw(exception);
  }
  else
    pmath_unref(exception);
  
  pmath_unref(info.function);
  pmath_unref(info.items);
  
  pmath_atomic_barrier();
  
  if((size_t)info.result_index < info.results_count){
    pmath_message(NULL, "toofew", 2, 
      pmath_integer_new_si(info.result_index),
      pmath_integer_new_size(info.results_count));
    
    if(exprlen == 3){
      pmath_t res = pmath_expr_get_item_range(
        result,
        1,
        (size_t)info.result_index);
      pmath_unref(result);
      return res;
    }
    
    pmath_unref(result);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  if(exprlen == 2){
    pmath_t res;
    
    assert(info.results_count == 1);
    res = pmath_expr_get_item(result, 1);
    pmath_unref(result);
    return res;
  }
  
  return result;
}
