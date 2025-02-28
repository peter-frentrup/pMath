#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_General;
extern pmath_symbol_t pmath_System_Hold;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_MessageName;

static void synchronize_callback(void *block) {
  *(pmath_t*)block = pmath_evaluate(*(pmath_t*)block);
}

typedef struct {
  pmath_thread_t  me;
  pmath_expr_t    sync_list;
  size_t          sync_index; // running backwards
  pmath_t         block;
} multi_syncronize_data_t;

static void multi_synchronize_callback(void *p) {
  multi_syncronize_data_t *data = (multi_syncronize_data_t*)p;
  pmath_symbol_t sync;
  
  if(data->sync_index == 0) {
    data->block = pmath_evaluate(data->block);
    return;
  }
  
  assert(data->me != NULL);
  if(data->me->evaldepth >= pmath_maxrecursion) {
    if(!data->me->critical_messages) {
      int tmp = data->me->evaldepth;
      data->me->evaldepth = 0;
      data->me->critical_messages = TRUE;
      
      pmath_message(
        pmath_System_General, "reclim", 1,
        PMATH_FROM_INT32(pmath_maxrecursion));
        
      data->me->critical_messages = FALSE;
      data->me->evaldepth = tmp;
    }
    else {
      pmath_throw(
        pmath_expr_new_extended(
          pmath_ref(pmath_System_MessageName), 2,
          pmath_ref(pmath_System_General),
          PMATH_C_STRING("reclim")));
    }
    
    data->block = pmath_expr_new_extended(
                    pmath_ref(pmath_System_Hold), 1,
                    data->block);
    _pmath_expr_update(data->block);
  }
  
  sync = pmath_expr_get_item(
           data->sync_list,
           data->sync_index);
           
  data->sync_index--;
  
  assert(pmath_is_null(sync) || pmath_is_symbol(sync));
  
  pmath_symbol_synchronized(sync, multi_synchronize_callback, data);
  
  pmath_unref(sync);
}

PMATH_PRIVATE pmath_t builtin_synchronize(pmath_expr_t expr) {
  /* Synchronize(sym, block)
     Synchronize({syms}, block)
  
     messages:
       General::nosym
   */
  pmath_t sync, block;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  sync = pmath_expr_get_item(expr, 1);
  block = pmath_expr_get_item(expr, 2);
  
  if(pmath_is_expr_of(sync, pmath_System_List)) {
    multi_syncronize_data_t data;
    size_t i;
    
    for(i = pmath_expr_length(sync); i >= 1; --i) {
      pmath_t synci = pmath_expr_get_item(sync, i);
      
      if(!pmath_is_symbol(synci)) {
        pmath_message(PMATH_NULL, "sym", 2, synci,
                      pmath_expr_new_extended(
                        pmath_ref(pmath_System_List), 2,
                        PMATH_FROM_INT32(1),
                        pmath_integer_new_uiptr(i)));
        pmath_unref(sync);
        pmath_unref(block);
        return expr;
      }
      
      pmath_unref(synci);
    }
    
    pmath_unref(expr);
    
    data.me = pmath_thread_get_current();
    if(!data.me) {
      pmath_unref(sync);
      pmath_unref(block);
      return PMATH_NULL;
    }
    
    data.sync_list = pmath_expr_sort(sync);
    data.sync_index = pmath_expr_length(data.sync_list);
    data.block = block;
    
    multi_synchronize_callback(&data);
    
    pmath_unref(data.sync_list);
    return data.block;
  }
  
  if(!pmath_is_symbol(sync)) {
    pmath_message(PMATH_NULL, "sym", 2, sync, PMATH_FROM_INT32(1));
    pmath_unref(block);
    return expr;
  }
  
  pmath_unref(expr);
  
  pmath_symbol_synchronized(sync, synchronize_callback, &block);
  
  pmath_unref(sync);
  return block;
}
