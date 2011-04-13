#include <pmath-util/emit-and-gather.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

PMATH_API void pmath_gather_begin(pmath_t pattern){
  struct _pmath_gather_info_t *info;
  pmath_thread_t thread = pmath_thread_get_current();
  if(!thread){
    pmath_unref(pattern);
    return;
  }
  
  if(thread->gather_failed){
    pmath_unref(pattern);
    thread->gather_failed++;
    return;
  }
    
  info = (struct _pmath_gather_info_t*)
    pmath_mem_alloc(sizeof(struct _pmath_gather_info_t));
  if(!info){
    pmath_unref(pattern);
    pmath_mem_free(info);
    thread->gather_failed++;
    return;
  }

  info->next               = thread->gather_info;
  info->pattern            = pattern;
  pmath_atomic_write_release(&info->emitted_values, 0);
  pmath_atomic_write_release(&info->value_count,    0);
  
  thread->gather_info      = info;
}

PMATH_API pmath_expr_t pmath_gather_end(void){
  struct _pmath_gather_info_t *info;
  struct _pmath_stack_info_t  *item;
  pmath_thread_t thread = pmath_thread_get_current();
  pmath_expr_t result;
  size_t i;
   
  if(!thread)
    return PMATH_NULL;
  
  if(thread->gather_failed){
    --thread->gather_failed;
    return PMATH_NULL;
  }
  
  if(!thread->gather_info)
    return PMATH_NULL;

  info = thread->gather_info;
  thread->gather_info = info->next;
  pmath_unref(info->pattern);
  
  i = pmath_atomic_read_aquire(&info->value_count);
  result = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), i);
  ++i;

  while(NULL != (item = (void*)pmath_atomic_read_aquire(&info->emitted_values))){
    pmath_atomic_write_release(&info->emitted_values, (intptr_t)item->next);

    result = pmath_expr_set_item(result, --i, item->value);
    pmath_mem_free(item);
  }

  pmath_mem_free(info);
  return result;
}

PMATH_API void pmath_emit(pmath_t object, pmath_t tag){
  pmath_bool_t at_least_one_gather = FALSE;
  pmath_thread_t thread = pmath_thread_get_current();
  struct _pmath_gather_info_t *info;
  struct _pmath_stack_info_t  *item = pmath_mem_alloc(sizeof(struct _pmath_stack_info_t));
  
  if(!thread || !item){
    pmath_unref(object);
    pmath_unref(tag);
    pmath_mem_free(item);
    return;
  }

  item->value = object;
  while(thread){
    info = thread->gather_info;
    at_least_one_gather = at_least_one_gather || info != NULL;
    while(info && !_pmath_pattern_match(tag, pmath_ref(info->pattern), NULL))
      info = info->next;

    if(info){
      item->next = (void*)pmath_atomic_fetch_set(
        &info->emitted_values,
        (intptr_t)item);
      
      (void)pmath_atomic_fetch_add(&info->value_count, +1);
      pmath_unref(tag);
      return;
    }
    thread = thread->parent;
  }
  if(at_least_one_gather)
    pmath_message(PMATH_SYMBOL_EMIT, "nogather2", 1, tag);
  else{
    pmath_unref(tag);
    pmath_message(PMATH_SYMBOL_EMIT, "nogather", 0);
  }
  pmath_unref(object);
  pmath_mem_free(item);
}

