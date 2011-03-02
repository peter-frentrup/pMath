#include <pmath-util/concurrency/threadpool.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


struct custom_task_data{
  volatile pmath_t value;
};

static void task_destroy(struct custom_task_data *data){
  pmath_unref(data->value);
  pmath_mem_free(data);
}

static void task_run(struct custom_task_data *data){
  pmath_t value = data->value;
  data->value = 0;
  
  value = pmath_evaluate(value);
  
  if(pmath_aborting()){
    pmath_unref(value);
    value = pmath_ref(PMATH_SYMBOL_ABORTED);
  }
  data->value = value;
}

PMATH_PRIVATE void _pmath_custom_task_destroy(void *data){
  pmath_task_unref((pmath_task_t)data);
}

static pmath_custom_t create_custom_task(pmath_t body){
  struct custom_task_data *data;
  pmath_task_t task;
  
  data = (struct custom_task_data*)pmath_mem_alloc(sizeof(struct custom_task_data));
  
  if(!data){
    pmath_unref(body);
    return PMATH_NULL;
  }
  
  data->value = body;
  
  task = pmath_task_new(
    (pmath_callback_t)task_run,
    (pmath_callback_t)task_destroy,
    data);
  
  return pmath_custom_new(task, _pmath_custom_task_destroy);
}

PMATH_PRIVATE pmath_t builtin_newtask(pmath_expr_t expr){
/* NewTask(body)
 */
  pmath_t  body;
  pmath_custom_t  custom_task;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  body = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  custom_task = create_custom_task(body);
  
  if(custom_task){
    pmath_symbol_t sym = pmath_symbol_create_temporary(
      PMATH_C_STRING("System`Tasks`task"), TRUE);
    
    pmath_symbol_set_value(sym, custom_task);
    
    return sym;
  }
  
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_wait(pmath_expr_t expr){
/* Wait(task)
   
   messages:
     General::nothread
 */
  pmath_symbol_t  sym;
  pmath_custom_t  custom_task;
  pmath_task_t    task;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  sym = (pmath_symbol_t)pmath_expr_get_item(expr, 1);
  if(!pmath_is_symbol(sym)){
    pmath_message(PMATH_NULL, "nothread", 1, sym);
    return expr;
  }
  
  custom_task = (pmath_custom_t)pmath_symbol_get_value(sym);
  if(!pmath_is_custom(custom_task)
  || !pmath_custom_has_destructor(custom_task, _pmath_custom_task_destroy)){
    pmath_unref(custom_task);
    pmath_message(PMATH_NULL, "nothread", 1, sym);
    return expr;
  }
  
  task = (pmath_task_t)pmath_custom_get_data(custom_task);
  if(task){
    struct custom_task_data *data;
    pmath_t value;
    
    pmath_task_wait(task);
    
    data = pmath_task_get_data(task);
    assert(data != PMATH_NULL);
    assert(pmath_task_has_destructor(task, (pmath_callback_t)task_destroy));
    
    value = pmath_ref(data->value);
    
    pmath_unref(custom_task);
    pmath_unref(sym);
    pmath_unref(expr);
    return value;
  }
  
  pmath_unref(custom_task);
  pmath_unref(sym);
  pmath_unref(expr);
  return PMATH_NULL;
}

