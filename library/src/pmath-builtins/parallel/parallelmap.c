#include <pmath-core/expressions-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/concurrency/threadpool-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>


struct parallel_map_info_t{
  struct _pmath_map_info_t info;
  pmath_thread_t     parent;
  pmath_t           *items;
  volatile intptr_t  index; // > 0 ...
};

static void parallel_map(struct parallel_map_info_t *info){
  pmath_thread_t thread = _pmath_thread_new(info->parent);
  intptr_t i = pmath_atomic_fetch_add(&info->index, -1);
  
  if(i > 0 && thread){
    pmath_thread_t old = pmath_thread_get_current();
    _pmath_thread_set_current(thread);
    
    info->items[i] = pmath_evaluate(
      _pmath_map(&info->info, info->items[i], 1));
    
    i = pmath_atomic_fetch_add(&info->index, -1);
    while(i > 0 && !pmath_thread_aborting(thread)){
      _pmath_thread_clean(FALSE);
      
      info->items[i] = pmath_evaluate(_pmath_map(&info->info, info->items[i], 1));
      
      i = pmath_atomic_fetch_add(&info->index, -1);
    }
    
    _pmath_thread_clean(FALSE);
    _pmath_thread_set_current(old);
  }
  
  _pmath_thread_free(thread);
}

static void dummy(void *arg){
}

PMATH_PRIVATE pmath_t builtin_parallelmap(pmath_expr_t expr){
/* ParallelMap(list, f, startlevel..endlevel)
   ParallelMap(list, f)    = ParallelMap(list, f, 1..1)
   ParallelMap(list, f, n) = ParallelMap(list, f, n..n)
 */
  struct parallel_map_info_t info;
  pmath_t obj;
  size_t len = pmath_expr_length(expr);
  int reldepth;
  
  if(len < 2 || len > 3){
    pmath_message_argxxx(len, 2, 3);
    return expr;
  }
  
  info.info.with_heads = FALSE;
  info.info.levelmin = 1;
  info.info.levelmax = 1;
  
  if(len == 3){
    pmath_t levels = pmath_expr_get_item(expr, 3);
    
    if(!_pmath_extract_levels(levels, &info.info.levelmin, &info.info.levelmax)){
      pmath_message(PMATH_NULL, "level", 1, levels);
      return expr;
    }
    
    pmath_unref(levels);
  }
  
  obj                = pmath_expr_get_item(expr, 1);
  info.info.function = pmath_expr_get_item(expr, 2);
  
  info.index = (intptr_t)pmath_expr_length(obj);
  
  reldepth = _pmath_object_in_levelspec(
    obj, info.info.levelmin, info.info.levelmax, 0);
  
  if(reldepth <= 0 && pmath_is_expr(obj)){
    pmath_task_t *tasks;
    int task_count = _pmath_processor_count();
    
    pmath_unref(expr);
    
    if(task_count > info.index)
      task_count = info.index;
    
    tasks = (pmath_task_t*)pmath_mem_alloc(task_count * sizeof(pmath_task_t));
    if(tasks){
      size_t i;
      
      info.items = NULL;
      
      if(PMATH_AS_PTR(obj)->refcount > 1 
      || PMATH_AS_PTR(obj)->type_shift != PMATH_TYPE_SHIFT_EXPRESSION_GENERAL){
        pmath_expr_t obj2 = pmath_expr_new(
          pmath_expr_get_item(obj, 0),
          info.index);
        
        if(!pmath_is_null(obj2)){
          info.items = ((struct _pmath_unpacked_expr_t*)PMATH_AS_PTR(obj2))->items;
          
          for(i = (size_t)info.index;i > 0;--i)
            info.items[i] = pmath_expr_get_item(obj, i);
          
          pmath_unref(obj);
          obj = obj2;
        }
      }
      else
        info.items = ((struct _pmath_unpacked_expr_t*)PMATH_AS_PTR(obj))->items;
      
      info.parent = pmath_thread_get_current();
      
      if(info.items){
        for(i = 0;i < (size_t)task_count;++i){
          tasks[i] = _pmath_task_new_with_thread(
            (pmath_callback_t)parallel_map,
            dummy,
            &info,
            NULL);
        }
        
        for(i = 0;i < (size_t)task_count;++i){
          pmath_task_wait(tasks[i]);
          pmath_task_unref(tasks[i]);
        }
        
        if(reldepth == 0){
          obj = pmath_expr_new_extended(
            pmath_ref(info.info.function), 1,
            obj);
        }
        
        _pmath_expr_update(obj);
      }
      
      pmath_mem_free(tasks);
    }
  }
  else if(reldepth > 0){
    pmath_message(PMATH_NULL, "nopar1", 1, expr);
    obj = _pmath_map(&info.info, obj, 0);
  }
  else
    pmath_unref(expr);
  
  pmath_unref(info.info.function);
  return obj;
}
