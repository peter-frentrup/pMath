#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

static void synchronize_callback(pmath_t *block){
  *block = pmath_evaluate(*block);
}

typedef struct{
  pmath_thread_t       me;
  pmath_expr_t sync_list;
  size_t             sync_index; // running backwards
  pmath_t     block;
}multi_syncronize_data_t;

static void multi_synchronize_callback(multi_syncronize_data_t *data){
  pmath_symbol_t sync;
  
  if(data->sync_index == 0){
    data->block = pmath_evaluate(data->block);
    return;
  }
  
  assert(data->me != PMATH_NULL);
  if(data->me->evaldepth >= pmath_maxrecursion){
    if(!data->me->critical_messages){
      int tmp = data->me->evaldepth;
      data->me->evaldepth = 0;
      data->me->critical_messages = TRUE;

      pmath_message(
        PMATH_SYMBOL_GENERAL, "reclim", 1,
        pmath_integer_new_si(pmath_maxrecursion));

      data->me->critical_messages = FALSE;
      data->me->evaldepth = tmp;
    }
    else{
      pmath_throw(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_MESSAGENAME), 2,
          pmath_ref(PMATH_SYMBOL_GENERAL),
          PMATH_C_STRING("reclim")));
    }
    
    data->block = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_HOLD), 1, 
      data->block);
    _pmath_expr_update((pmath_expr_t)data->block);
  }
  
  sync = (pmath_symbol_t)pmath_expr_get_item(
    data->sync_list, 
    data->sync_index);
  
  data->sync_index--;
  
  assert(!sync || pmath_is_symbol(sync));
  
  pmath_symbol_synchronized((pmath_symbol_t)sync, 
    (pmath_callback_t) multi_synchronize_callback,
                            data);
  
  pmath_unref(sync);
}

PMATH_PRIVATE pmath_t builtin_synchronize(pmath_expr_t expr){
/* Synchronize(sym, block)
   Synchronize({syms}, block)
   
   messages:
     General::nosym
 */
  pmath_t sync, block;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  sync = pmath_expr_get_item(expr, 1);
  block = pmath_expr_get_item(expr, 2);
  
  if(pmath_is_expr(sync)){
    multi_syncronize_data_t data;
    size_t i;
    
    for(i = pmath_expr_length((pmath_expr_t)sync);i >= 1;--i){
      pmath_t synci = pmath_expr_get_item(
        (pmath_expr_t)sync, i);
      
      if(!pmath_is_symbol(synci)){
        pmath_message(PMATH_NULL, "sym", 2, synci, 
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_LIST), 2,
            pmath_integer_new_si(1),
            pmath_integer_new_size(i)));
        pmath_unref(sync);
        pmath_unref(block);
        return expr;
      }
      
      pmath_unref(synci);
    }
    
    pmath_unref(expr);
    
    data.me = pmath_thread_get_current();
    if(!data.me){
      pmath_unref(sync);
      pmath_unref(block);
      return PMATH_NULL;
    }
    
    data.sync_list = pmath_expr_sort((pmath_expr_t)sync);
    data.sync_index = pmath_expr_length(data.sync_list);
    data.block = block;
    
    multi_synchronize_callback(&data);
                              
    return data.block;
  }
  
  if(!pmath_is_symbol(sync)){
    pmath_message(PMATH_NULL, "sym", 1, sync, pmath_integer_new_si(1));
    pmath_unref(block);
    return expr;
  }
  
  pmath_unref(expr);
  
  pmath_symbol_synchronized((pmath_symbol_t)sync, 
      (pmath_callback_t) synchronize_callback,
                              &block);
  
  pmath_unref(sync);
  return block;
}
