#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/concurrency/threadpool-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/flow-private.h>
#include <pmath-builtins/lists-private.h>


struct parallel_scan_info_t {
  pmath_t function;
  long levelmin;
  long levelmax;
  
  pmath_thread_t parent;
  pmath_expr_t items;
  pmath_atomic_t index; // > 0 ...
};

static void parallel_scan(struct parallel_scan_info_t *info) {
  pmath_thread_t thread = _pmath_thread_new(info->parent);
  intptr_t i = pmath_atomic_fetch_add(&info->index, -1);
  struct _pmath_scan_info_t info2;
  
  info2.with_heads = FALSE;
  info2.function = info->function;
  info2.result = PMATH_NULL;
  info2.levelmin = info->levelmin;
  info2.levelmax = info->levelmax;
  
  if(i > 0 && thread) {
    pmath_thread_t old = pmath_thread_get_current();
    _pmath_thread_set_current(thread);
    
    if(_pmath_scan(&info2, pmath_expr_get_item(info->items, i), 1)) {
      _pmath_thread_throw(
        info->parent,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PARALLEL_RETURN), 1,
          info2.result));
    }
    else {
      i = pmath_atomic_fetch_add(&info->index, -1);
      
      while(i > 0 && !pmath_thread_aborting(thread)) {
        _pmath_thread_clean(FALSE);
        
        if(_pmath_scan(&info2, pmath_expr_get_item(info->items, i), 1)) {
          _pmath_thread_throw(
            info->parent,
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PARALLEL_RETURN), 1,
              info2.result));
          break;
        }
        
        i = pmath_atomic_fetch_add(&info->index, -1);
      }
    }
    
    _pmath_thread_clean(FALSE);
    
    _pmath_thread_set_current(old);
  }
  
  _pmath_thread_free(thread);
}

static void dummy(void *arg) {
}

PMATH_PRIVATE pmath_t builtin_parallelscan(pmath_expr_t expr) {
  /* ParallelScan(list, f, startlevel..endlevel)
     ParallelScan(list, f)    = ParallelScan(list, f, 1..1)
     ParallelScan(list, f, n) = ParallelScan(list, f, n..n)
   */
  struct parallel_scan_info_t info;
  pmath_t obj;
  size_t len = pmath_expr_length(expr);
  int reldepth;
  pmath_bool_t scan_returns_value = FALSE;
  
  if(len < 2 || len > 3) {
    pmath_message_argxxx(len, 2, 3);
    return expr;
  }
  
  info.levelmin = 1;
  info.levelmax = 1;
  
  if(len == 3) {
    pmath_t levels = pmath_expr_get_item(expr, 3);
    
    if(!_pmath_extract_levels(levels, &info.levelmin, &info.levelmax)) {
      pmath_message(PMATH_NULL, "level", 1, levels);
      return expr;
    }
    
    pmath_unref(levels);
  }
  
  obj           = pmath_expr_get_item(expr, 1);
  info.function = pmath_expr_get_item(expr, 2);
  
  info.index._data = (intptr_t)pmath_expr_length(obj);
  
  reldepth = _pmath_object_in_levelspec(obj, info.levelmin, info.levelmax, 0);
  
  if(reldepth <= 0 && pmath_is_expr(obj)) {
    pmath_t exception;
    pmath_task_t *tasks;
    int task_count = _pmath_processor_count();
    
    pmath_unref(expr);
    
    if(task_count > info.index._data)
      task_count = (int)info.index._data;
      
    tasks = (pmath_task_t*)pmath_mem_alloc(task_count * sizeof(pmath_task_t));
    if(tasks) {
      size_t i;
      
      info.items = obj;
      info.parent = pmath_thread_get_current();
      
      for(i = 0; i < (size_t)task_count; ++i) {
        tasks[i] = _pmath_task_new_with_thread(
                     (pmath_callback_t)parallel_scan,
                     dummy,
                     &info,
                     NULL);
      }
      
      for(i = 0; i < (size_t)task_count; ++i) {
        pmath_task_wait(tasks[i]);
        pmath_task_unref(tasks[i]);
      }
      
      exception = pmath_catch();
      if(!pmath_same(exception, PMATH_UNDEFINED)) {
        pmath_unref(obj);
        obj = PMATH_NULL;
        
        if(pmath_is_expr_of_len(exception, PMATH_SYMBOL_PARALLEL_RETURN, 1)) {
          obj = pmath_expr_get_item(exception, 1);
          pmath_unref(exception);
          scan_returns_value = TRUE;
        }
        else
          pmath_throw(exception);
      }
      else if(reldepth == 0) {
        obj = pmath_expr_new_extended(
                pmath_ref(info.function), 1,
                obj);
                
        _pmath_run(&obj);
      }
      
      pmath_mem_free(tasks);
    }
  }
  else if(reldepth > 0) {
    struct _pmath_scan_info_t info2;
    info2.with_heads = FALSE;
    info2.function = info.function;
    info2.result = PMATH_NULL;
    info2.levelmin = info.levelmin;
    info2.levelmax = info.levelmax;
    
    pmath_unref(expr);
    
    scan_returns_value = _pmath_scan(&info2, obj, 0);
    obj = info2.result;
  }
  else
    pmath_unref(expr);
    
  pmath_unref(info.function);
  if(scan_returns_value) 
    return obj;
  
  pmath_unref(obj);
  return PMATH_NULL;
}
