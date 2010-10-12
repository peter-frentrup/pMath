#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/incremental-hash-private.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/formating-private.h>
#include <pmath-builtins/lists-private.h>

#include <string.h>

struct _pmath_unpacked_expr_part_t{
  struct _pmath_unpacked_expr_t   inherited; // item: length = 1
  size_t                          start;
  struct _pmath_unpacked_expr_t  *buffer;
};

// initialization in pmath_init():
PMATH_PRIVATE pmath_expr_t _pmath_object_emptylist; // read-only
PMATH_PRIVATE pmath_expr_t _pmath_object_stop_message; // read-only
//PMATH_PRIVATE pmath_expr_t _pmath_object_newsym_message; // read-only

#define CACHES_MAX  8
#define CACHE_SIZE  32
#define CACHE_MASK  (CACHE_SIZE-1)

static intptr_t expr_caches[CACHES_MAX][CACHE_SIZE];
static PMATH_DECLARE_ATOMIC(expr_cache_pos[CACHES_MAX]);

#ifdef PMATH_DEBUG_LOG
static PMATH_DECLARE_ATOMIC(expr_cache_hits);
static PMATH_DECLARE_ATOMIC(expr_cache_misses);
#endif

  static uintptr_t expr_cache_inc(size_t length, intptr_t delta){
    assert(length < CACHES_MAX);
    
    return (uintptr_t)pmath_atomic_fetch_add(&expr_cache_pos[length], delta);
  }

  static struct _pmath_unpacked_expr_t *expr_cache_swap(
    size_t                               length,
    uintptr_t                            i, 
    struct _pmath_unpacked_expr_t *value
  ){
    i = i & CACHE_MASK;
    
    assert(length < CACHES_MAX);
    assert(!value || value->length == length);
    
    return (struct _pmath_unpacked_expr_t*)
      pmath_atomic_fetch_set(&expr_caches[length][i], (intptr_t)value);
  }

  static void expr_cache_clear(void){
    size_t len;
    uintptr_t i;
    
    for(len = 0;len < CACHES_MAX;++len){
      for(i = 0;i < CACHE_SIZE;++i){
        struct _pmath_unpacked_expr_t *expr = expr_cache_swap(len, i, NULL);
        
        if(expr){
          assert(expr->inherited.inherited.inherited.refcount == 0);
          
          pmath_mem_free(expr);
        }
      }
    }
  }

/*============================================================================*/

PMATH_API 
pmath_expr_t pmath_expr_new(
  pmath_t head,
  size_t  length
){
  struct _pmath_unpacked_expr_t *expr;
  size_t size = length * sizeof(pmath_t);
  
  if(PMATH_UNLIKELY(size / sizeof(pmath_t) != length
  || size + sizeof(struct _pmath_unpacked_expr_t) < size)){
    // overflow
    pmath_unref(head);
    pmath_abort_please();
    return NULL;
  }
  
  if(length < CACHES_MAX){
    uintptr_t i = expr_cache_inc(length, -1);
    
    expr = expr_cache_swap(length, i-1, NULL);
    if(expr){
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&expr_cache_hits, 1);
      #endif
      
      if(expr->inherited.inherited.inherited.refcount != 0){
        pmath_debug_print("expr refcount = %d\n", (int)expr->inherited.inherited.inherited.refcount);
        assert(expr->inherited.inherited.inherited.refcount == 0);
      }
      expr->inherited.inherited.inherited.refcount = 1;
    }
    else{
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&expr_cache_misses, 1);
      #endif
    }
  }
  else
    expr = NULL;
  
  if(!expr){
    expr = (struct _pmath_unpacked_expr_t*)_pmath_create_stub(
      PMATH_TYPE_SHIFT_EXPRESSION_GENERAL,
      sizeof(struct _pmath_unpacked_expr_t) + size);
  
    if(PMATH_UNLIKELY(!expr)){
      pmath_unref(head);
      return NULL;
    }
  }
  
  expr->inherited.inherited.last_change = -_pmath_timer_get_next();
  expr->inherited.gc_refcount = 0;
  expr->length   = length;
  expr->items[0] = head;
  memset(&(expr->items[1]), 0, length * sizeof(pmath_t));

  return (pmath_expr_t)expr;
}

PMATH_API 
pmath_expr_t pmath_expr_new_extended(
  pmath_t head,
  size_t  length,
  ...
){
  struct _pmath_unpacked_expr_t *expr;
  size_t i, size;
  va_list items;
  va_start(items, length);
  
  size = length * sizeof(pmath_t);
  
  if(PMATH_UNLIKELY(size / sizeof(pmath_t) != length
  || size + sizeof(struct _pmath_unpacked_expr_t) < size)){
    // overflow
    pmath_unref(head);
    
    for(i = 1;i <= length;i++)
      pmath_unref(va_arg(items, pmath_t));

    va_end(items);
    pmath_abort_please();
    return NULL;
  }
  
  if(length < CACHES_MAX){
    uintptr_t i = expr_cache_inc(length, -1);
    
    expr = expr_cache_swap(length, i-1, NULL);
    if(expr){
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&expr_cache_hits, 1);
      #endif
      
      if(expr->inherited.inherited.inherited.refcount != 0){
        pmath_debug_print("expr refcount = %d\n", (int)expr->inherited.inherited.inherited.refcount);
        assert(expr->inherited.inherited.inherited.refcount == 0);
      }
      expr->inherited.inherited.inherited.refcount = 1;
    }
    else{
      #ifdef PMATH_DEBUG_LOG
        (void)pmath_atomic_fetch_add(&expr_cache_misses, 1);
      #endif
    }
  }
  else
    expr = NULL;
  
  if(!expr){
    expr = (struct _pmath_unpacked_expr_t*)_pmath_create_stub(
      PMATH_TYPE_SHIFT_EXPRESSION_GENERAL,
      sizeof(struct _pmath_unpacked_expr_t) + size);
  
    if(PMATH_UNLIKELY(!expr)){
      pmath_unref(head);
      
      for(i = 1;i <= length;i++)
        pmath_unref(va_arg(items, pmath_t));

      va_end(items);
      return NULL;
    }
  }
  
  expr->inherited.inherited.last_change = -_pmath_timer_get_next();
  expr->inherited.gc_refcount = 0;
  expr->length   = length;
  expr->items[0] = head;
  
  for(i = 1;i <= length;i++)
    expr->items[i] = va_arg(items, pmath_t);

  va_end(items);
  return (pmath_expr_t)expr;
}

PMATH_API 
pmath_expr_t pmath_expr_resize(
  pmath_expr_t expr,
  size_t       new_length
){
  struct _pmath_unpacked_expr_t *new_expr;
  size_t old_length;
  
  if(PMATH_UNLIKELY(!expr))
    return pmath_expr_new(NULL, new_length);
    
  assert(pmath_instance_of(expr, PMATH_TYPE_EXPRESSION));

  if(new_length == ((struct _pmath_unpacked_expr_t*)expr)->length)
    return expr;

  if(expr->refcount > 1
  || expr->type_shift != PMATH_TYPE_SHIFT_EXPRESSION_GENERAL){
    struct _pmath_unpacked_expr_t *new_expr =
      (struct _pmath_unpacked_expr_t*)pmath_expr_new(
        pmath_ref(((struct _pmath_unpacked_expr_t*)expr)->items[0]),
        new_length);

    if(new_expr){
      size_t max = ((struct _pmath_unpacked_expr_t*)expr)->length;
      if(max > new_length)
        max = new_length;

      switch(((struct _pmath_t*)expr)->type_shift){
        case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL: {
          size_t i;
          for(i = 1;i <= max;i++)
            new_expr->items[i] = pmath_ref(
              ((struct _pmath_unpacked_expr_t*)expr)->items[i]);
        } break;
        case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
          size_t i;
          for(i = 1;i <= max;i++)
            new_expr->items[i] = pmath_ref(
              ((struct _pmath_unpacked_expr_part_t*)expr)->buffer->items[
                ((struct _pmath_unpacked_expr_part_t*)expr)->start + i-1]);
        } break;
        default:
          assert("invalid expression type" && 0);
      }
    }

    pmath_unref(expr);
    return (pmath_expr_t)new_expr;
  }

  if(new_length < ((struct _pmath_unpacked_expr_t*)expr)->length){
    size_t i;
    for(i = new_length+1;i <= ((struct _pmath_unpacked_expr_t*)expr)->length;i++)
      pmath_unref(((struct _pmath_unpacked_expr_t*)expr)->items[i]);
  }

  old_length = ((struct _pmath_unpacked_expr_t*)expr)->length;

  new_expr =
    (struct _pmath_unpacked_expr_t*)pmath_mem_realloc_no_failfree(
      expr,
      sizeof(struct _pmath_unpacked_expr_t) + new_length * sizeof(pmath_t)
    );

  if(new_expr == NULL){
    if(new_length < old_length){
      memset(
        &(((struct _pmath_unpacked_expr_t*)expr)->items[new_length+1]),
        0,
        (old_length - new_length) * sizeof(pmath_t));
    }
    pmath_unref(expr);
    return NULL;
  }

  if(new_length > old_length){
    memset(
      &(new_expr->items[old_length+1]),
      0,
      (new_length - old_length) * sizeof(pmath_t));
  }

  new_expr->length = new_length;
  new_expr->inherited.inherited.last_change = -_pmath_timer_get_next();

  return (pmath_expr_t)new_expr;
}

PMATH_API pmath_expr_t pmath_expr_append(
  pmath_expr_t expr,
  size_t       count,
  ...
){
  struct _pmath_unpacked_expr_t *new_expr;
  size_t i, old_length, new_length;
  va_list items;
  va_start(items, count);

  old_length = pmath_expr_length(expr);
  new_length = old_length + count;

  if(new_length < old_length){ // overflow
    pmath_unref(expr);
    
    for(i = 1;i <= count;i++)
      pmath_unref(va_arg(items, pmath_t));
    va_end(items);

    pmath_abort_please();
    return NULL;
  }

  new_expr =
    (struct _pmath_unpacked_expr_t*)pmath_expr_resize(
      expr, new_length);

  if(!new_expr){
    for(i = 1;i <= count;i++)
      pmath_unref(va_arg(items, pmath_t));

    va_end(items);
    return NULL;
  }
  
  for(i = old_length+1;i <= old_length+count;i++)
    new_expr->items[i] = va_arg(items, pmath_t);

  va_end(items);
  return (pmath_expr_t)new_expr;
}

PMATH_API size_t pmath_expr_length(
  pmath_expr_t expr
){
  if(!expr)
    return 0;
    
  assert(pmath_instance_of(expr, PMATH_TYPE_EXPRESSION));

  return ((struct _pmath_unpacked_expr_t*)expr)->length;
}

PMATH_API pmath_t pmath_expr_get_item(
  pmath_expr_t expr,
  size_t       index
){
  if(!expr)
    return NULL;
    
  assert(pmath_instance_of(expr, PMATH_TYPE_EXPRESSION));

  if(index > ((struct _pmath_unpacked_expr_t*)expr)->length)
    return NULL;

  if(expr->type_shift == PMATH_TYPE_SHIFT_EXPRESSION_GENERAL || index == 0)
    return pmath_ref(((struct _pmath_unpacked_expr_t*)expr)->items[index]);

  assert(expr->type_shift == PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART);
  return pmath_ref(
    ((struct _pmath_unpacked_expr_part_t*)expr)->buffer->items[
      ((struct _pmath_unpacked_expr_part_t*)expr)->start + index - 1]);
}

PMATH_API pmath_expr_t pmath_expr_get_item_range(
  pmath_expr_t expr,
  size_t       start,
  size_t       length
){
  const size_t exprlen = pmath_expr_length(expr);
  
  if(PMATH_UNLIKELY(!expr))
    return NULL;
  
  if(start == 1 && length >= exprlen)
    return pmath_ref(expr);

  if(start > exprlen || length == 0)
    return pmath_expr_new(pmath_expr_get_item(expr, 0), 0);

  if(length > exprlen || start + length > exprlen + 1)
    length = exprlen + 1 - start;

  if(start == 0
  && expr->type_shift == PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART
  && (((struct _pmath_unpacked_expr_part_t*)expr)->start == 0
    || !pmath_equals(
      ((struct _pmath_unpacked_expr_part_t*)expr)->buffer->items[
        ((struct _pmath_unpacked_expr_part_t*)expr)->start-1],
      ((struct _pmath_unpacked_expr_t*)expr)->items[0])))
  {
    size_t i;
    struct _pmath_unpacked_expr_t *new_expr =
      (struct _pmath_unpacked_expr_t*)pmath_expr_new(
        pmath_expr_get_item(expr, 0), length);

    if(!new_expr)
      return NULL;

    ((struct _pmath_unpacked_expr_t*)new_expr)->items[1] =
      pmath_ref(((struct _pmath_unpacked_expr_t*)expr)->items[0]);
    
    for(i = 2;i <= length;i++){
      ((struct _pmath_unpacked_expr_t*)new_expr)->items[i] =
        pmath_ref(((struct _pmath_unpacked_expr_part_t*)expr)->buffer->items[
          ((struct _pmath_unpacked_expr_part_t*)expr)->start + i - 2]);
    }

    return (pmath_expr_t)new_expr;
  }
  else{
    struct _pmath_unpacked_expr_part_t *new_expr =
      (struct _pmath_unpacked_expr_part_t*)_pmath_create_stub(
        PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART,
        sizeof(struct _pmath_unpacked_expr_part_t));

    if(!new_expr)
      return NULL;
    
    new_expr->inherited.inherited.inherited.last_change = -_pmath_timer_get_next();
    new_expr->inherited.inherited.gc_refcount = 0;
    new_expr->inherited.length   = length;
    new_expr->inherited.items[0] = pmath_expr_get_item(expr, 0);

    switch(expr->type_shift){
      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL: {
        new_expr->start = start;
        new_expr->buffer = (struct _pmath_unpacked_expr_t*)pmath_ref(expr);
      } break;
      
      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
        new_expr->start = start + ((struct _pmath_unpacked_expr_part_t*)expr)->start - 1;
        new_expr->buffer = (struct _pmath_unpacked_expr_t*)pmath_ref(
          (pmath_expr_t)((struct _pmath_unpacked_expr_part_t*)expr)->buffer);
      } break;
      
      default:
        assert("invalid expression type" && 0);
    }

    return (pmath_expr_t)new_expr;
  }
}

PMATH_API pmath_expr_t pmath_expr_set_item(
  pmath_expr_t expr,
  size_t       index,
  pmath_t      item
){
  if(PMATH_UNLIKELY(!expr)){
    pmath_unref(item);
    return NULL;
  }
  
  assert(pmath_instance_of(expr, PMATH_TYPE_EXPRESSION));

  if(index > ((struct _pmath_unpacked_expr_t*)expr)->length
  || ((index == 0 || expr->type_shift == PMATH_TYPE_SHIFT_EXPRESSION_GENERAL)
    && ((struct _pmath_unpacked_expr_t*)expr)->items[index] == item)
  || (expr->type_shift == PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART && index > 0
    && ((struct _pmath_unpacked_expr_part_t*)expr)->buffer->items[
      ((struct _pmath_unpacked_expr_part_t*)expr)->start + index - 1] == item))
  {
    pmath_unref(item);
    return expr;
  }

  if(expr->refcount > 1
  || (index > 0 && expr->type_shift != PMATH_TYPE_SHIFT_EXPRESSION_GENERAL)){
    const size_t len = ((struct _pmath_unpacked_expr_t*)expr)->length;
    struct _pmath_unpacked_expr_t *new_expr =
      (struct _pmath_unpacked_expr_t*)pmath_expr_new(
        pmath_ref(((struct _pmath_unpacked_expr_t*)expr)->items[0]),
        len
      );

    if(!new_expr){
      pmath_unref(item);
      pmath_unref(expr);
      return NULL;
    }

    if(index == 0)
      pmath_unref(new_expr->items[0]);
    new_expr->items[index] = item;

    switch(expr->type_shift){
      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:{
        size_t i;
        for(i = 1;i < index;i++)
          new_expr->items[i] =
            pmath_ref(((struct _pmath_unpacked_expr_t*)expr)->items[i]);

        for(i = index+1;i <= len;i++)
          new_expr->items[i] =
            pmath_ref(((struct _pmath_unpacked_expr_t*)expr)->items[i]);
      } break;
      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:{
        size_t i;
        for(i = 1;i < index;i++)
          new_expr->items[i] =
            pmath_ref(((struct _pmath_unpacked_expr_part_t*)expr)->buffer->items[
              ((struct _pmath_unpacked_expr_part_t*)expr)->start + i - 1]);

        for(i = index+1;i <= len;i++)
          new_expr->items[i] =
            pmath_ref(((struct _pmath_unpacked_expr_part_t*)expr)->buffer->items[
              ((struct _pmath_unpacked_expr_part_t*)expr)->start + i - 1]);
      } break;
      default:
        assert("invalid expression type" && 0);
    }

    pmath_unref(expr);
    return (pmath_expr_t)new_expr;
  }

  ((struct _pmath_timed_t*)expr)->last_change = -_pmath_timer_get_next();
  
  pmath_unref(((struct _pmath_unpacked_expr_t*)expr)->items[index]);
  ((struct _pmath_unpacked_expr_t*)expr)->items[index] = item;
  return expr;
}

PMATH_PRIVATE pmath_t _pmath_expr_shrink_associative(
  pmath_expr_t  expr,
  pmath_t       magic_rem
){
  size_t len = pmath_expr_length(expr);
  size_t srci = 1;
  size_t dsti = 1;

  while(srci <= len){
    pmath_t item = NULL;
    do{
      pmath_unref(item);
      item = pmath_expr_get_item(expr, srci++);
    }while(item == magic_rem && srci <= len);
    
    if(item == magic_rem){
      pmath_unref(item);
      break;
    }
    
    expr = pmath_expr_set_item(expr, dsti++, item);
  }
    
  if(dsti == 2){
    pmath_t item = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return item;
  }

  return pmath_expr_resize(expr, dsti-1);
}

PMATH_API pmath_expr_t pmath_expr_remove_all(
  pmath_expr_t expr,
  pmath_t      rem
){
  pmath_bool_t equaled = FALSE;
  size_t len = pmath_expr_length(expr);
  size_t srci = 1;
  size_t dsti = 1;

  while(srci <= len){
    pmath_t item = NULL;
    do{
      pmath_unref(item);
      item = pmath_expr_get_item(expr, srci++);
      equaled = pmath_equals(item, rem);
    }while(equaled && srci <= len);
    
    if(equaled){
      pmath_unref(item);
      break;
    }
    
    expr = pmath_expr_set_item(expr, dsti++, item);
  }
    
  return pmath_expr_resize(expr, dsti-1);
}

/*----------------------------------------------------------------------------*/

  static int cmp_objs(const void *a, const void *b){
    return pmath_compare(*(pmath_t*)a, *(pmath_t*)b);
  }

/* returns 0 on failure and index of found item otherwise */
PMATH_PRIVATE
size_t _pmath_expr_find_sorted(
  pmath_expr_t sorted_expr, // wont be freed
  pmath_t      item         // wont be freed
){
  size_t length = pmath_expr_length(sorted_expr);
  if(length == 0)
    return FALSE;
  
  switch(sorted_expr->type_shift){
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL: {
      struct _pmath_unpacked_expr_t *ge;
      void *found_elem;
      
      ge = (struct _pmath_unpacked_expr_t*)sorted_expr;
      
      found_elem = bsearch(
        &item, 
        &ge->items[1],
        length,
        sizeof(pmath_t),
        cmp_objs);
      
      if(found_elem)
        return ((size_t)found_elem - (size_t)ge->items)/sizeof(pmath_t);
      return 0;
    }
    
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
      struct _pmath_unpacked_expr_part_t *ue;
      void *found_elem;
      
      ue = (struct _pmath_unpacked_expr_part_t*)sorted_expr;
      
      found_elem = bsearch(
        &item, 
        &ue->buffer->items[ue->start],
        length,
        sizeof(pmath_t),
        cmp_objs);
      
      if(found_elem)
        return 1 + ((size_t)found_elem - (size_t)&ue->buffer->items[ue->start])/sizeof(pmath_t);
      return 0;
    }
  }
  
  assert("unknown expression type" && 0);
  
  return 0;
}

  static int stable_sort_cmp_objs(const void *a, const void *b){
    int cmp = pmath_compare(*(pmath_t*)a, *(pmath_t*)b);
    if(cmp != 0)
      return cmp;
    
    if((uintptr_t)a < (uintptr_t)b) return -1;
    if((uintptr_t)a > (uintptr_t)b) return +1;
    return 0;
  }

PMATH_PRIVATE pmath_expr_t _pmath_expr_sort_ex(
  pmath_expr_t expr, // will be freed
  int(*cmp)(const void*, const void*)
){
  size_t i, length;
  
  if(PMATH_UNLIKELY(!expr))
    return NULL;

  length = pmath_expr_length(expr);
  if(length < 2)
    return expr;

  if(expr->refcount > 1
  || expr->type_shift != PMATH_TYPE_SHIFT_EXPRESSION_GENERAL){
    struct _pmath_unpacked_expr_t *new_expr =
      (struct _pmath_unpacked_expr_t*)pmath_expr_new(
        NULL, length);

    if(!new_expr){
      pmath_unref(expr);
      return NULL;
    }

    switch(expr->type_shift){
      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL: {
        size_t i;
        for(i = 0;i <= length;i++)
          new_expr->items[i] = pmath_ref(
            ((struct _pmath_unpacked_expr_t*)expr)->items[i]);
      } break;
      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
        new_expr->items[0] = pmath_ref(
          ((struct _pmath_unpacked_expr_t*)expr)->items[0]);
        
        for(i = 1;i <= length;i++)
          new_expr->items[i] = pmath_ref(
            ((struct _pmath_unpacked_expr_part_t*)expr)->buffer->items[
              ((struct _pmath_unpacked_expr_part_t*)expr)->start + i - 1]);
      } break;
      default:
        assert("invalid expression type" && 0);
    }

    pmath_unref(expr);
    expr = (pmath_expr_t)new_expr;
  }

  qsort(
    ((struct _pmath_unpacked_expr_t*)expr)->items + 1,
    length,
    sizeof(pmath_t),
    cmp);

  return expr;
}

PMATH_API pmath_expr_t pmath_expr_sort(
  pmath_expr_t expr
){
  return _pmath_expr_sort_ex(expr, stable_sort_cmp_objs);
}

/*----------------------------------------------------------------------------*/

  #define PUSH(e,i) do{stack[stack_pos].expr = e; stack[stack_pos++].index = i;}while(0)
  #define POP(e,i)  do{e = stack[--stack_pos].expr; i = stack[stack_pos].index;}while(0)

  static pmath_bool_t flatten_calc_newlen(
    size_t             *newlen, // [in/out] final length of flattened expr
    pmath_expr_t  expr,   // wont be freed
    pmath_t      head,   // wont be freed
    size_t              depth
  ){
    struct{
      pmath_expr_t expr;
      size_t             index;
    }stack[PMATH_EXPRESSION_FLATTEN_MAX_DEPTH];
    size_t stack_pos = 0;

    pmath_bool_t any_change = FALSE;
    size_t srci = 1;
    
    if(depth == 0){
      (*newlen)+= pmath_expr_length(expr);
      return FALSE;
    }
    
    expr = pmath_ref(expr);
    //PUSH(expr, srci);
    for(;;){
      size_t exprlen;
      
     COUNT_NEXT_EXPR: ;

      exprlen = pmath_expr_length(expr);
      if(stack_pos == depth){
        (*newlen)+= exprlen - srci + 1;
        pmath_unref(expr);
        POP(expr, srci);
        continue;
      }
      for(;srci <= exprlen;srci++){
        pmath_t item = pmath_expr_get_item(expr, srci);
        
        if(pmath_instance_of(item, PMATH_TYPE_EXPRESSION)
        && stack_pos < depth){
          pmath_t this_head = pmath_expr_get_item(
            (pmath_expr_t)item, 0);
            
          if(pmath_equals(head, this_head)){
            pmath_unref(this_head);
            
            if(stack_pos < PMATH_EXPRESSION_FLATTEN_MAX_DEPTH){
              PUSH(expr, srci+1);
              srci = 1;
              expr = (pmath_expr_t)item;
              any_change = TRUE;
              goto COUNT_NEXT_EXPR;
            }
            
            if(depth > PMATH_EXPRESSION_FLATTEN_MAX_DEPTH){
              any_change|= flatten_calc_newlen(
                newlen,
                (pmath_expr_t)item,
                head,
                depth - 1 - PMATH_EXPRESSION_FLATTEN_MAX_DEPTH);
              pmath_unref(item);
              continue;
            }
          }
          pmath_unref(this_head);
        }
        (*newlen)++;
        pmath_unref(item);
      }
      
      pmath_unref(expr);
      if(stack_pos == 0)
        return any_change;
      POP(expr, srci);
    }

    //return any_change;
  }

  static void flatten_rearrange(
    pmath_t      **result,   // [in/out] pointer to an array position
    pmath_expr_t   expr,  // wont be freed
    pmath_t        head,  // wont be freed
    size_t         depth
  ){
    struct{
      pmath_expr_t expr;
      size_t             index;
    }stack[PMATH_EXPRESSION_FLATTEN_MAX_DEPTH];
    size_t stack_pos = 0;

    size_t srci = 1;
    
    if(depth == 0){
      size_t i;
      size_t exprlen = pmath_expr_length(expr);
      for(i = 1;i <= exprlen;i++,(*result)++)
        **result = pmath_expr_get_item(expr, i);
      return;
    }

    expr = pmath_ref(expr);
    //PUSH(expr, srci);
    for(;;){
      size_t exprlen;
      
     FLATTEN_NEXT_EXPR: ;

      exprlen = pmath_expr_length(expr);
      for(;srci <= exprlen;srci++){
        pmath_t item = pmath_expr_get_item(expr, srci);
        
        if(pmath_instance_of(item, PMATH_TYPE_EXPRESSION)
        && stack_pos < depth){
          pmath_t this_head = pmath_expr_get_item(
            (pmath_expr_t)item, 0);
            
          if(pmath_equals(head, this_head)){
            pmath_unref(this_head);
            
            if(stack_pos < PMATH_EXPRESSION_FLATTEN_MAX_DEPTH){
              PUSH(expr, srci+1);
              srci = 1;
              expr = (pmath_expr_t)item;
              goto FLATTEN_NEXT_EXPR;
            }
            
            if(depth > PMATH_EXPRESSION_FLATTEN_MAX_DEPTH){
              flatten_rearrange(
                result,
                (pmath_expr_t)item,
                head,
                depth - 1 - PMATH_EXPRESSION_FLATTEN_MAX_DEPTH);
              pmath_unref(item);
              continue;
            }
          }
          pmath_unref(this_head);
        }
        **result = item;
        ++*result;
      }
      
      pmath_unref(expr);
      if(stack_pos == 0)
        return;
      POP(expr, srci);
    }
  }

  #undef PUSH
  #undef POP

PMATH_API pmath_expr_t pmath_expr_flatten(
  pmath_expr_t  expr,
  pmath_t       head,
  size_t        depth
){
  struct _pmath_unpacked_expr_t *new_expr;
  size_t newlen = 0;
  pmath_t *items;
  
  if(!expr){
    pmath_unref(head);
    return expr;
  }
  
  if(!flatten_calc_newlen(&newlen, expr, head, depth)){
    pmath_unref(head);
    return expr;
  }

  new_expr =
    (struct _pmath_unpacked_expr_t*)pmath_expr_new(
      pmath_expr_get_item(expr, 0),
      newlen);

  if(!new_expr){
    pmath_unref(head);
    return expr;
  }

  items = new_expr->items + 1;
  flatten_rearrange(&items, expr, head, depth);
  assert(items == new_expr->items + newlen + 1);
  pmath_unref(head);
  pmath_unref(expr);
  return (pmath_expr_t)new_expr;
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE pmath_expr_t _pmath_expr_thread(
  pmath_expr_t expr, // will be freed
  pmath_t     head, // wont be freed
  size_t             start,
  size_t             end,
  pmath_bool_t   *error_message
){
  pmath_bool_t have_sth_to_thread = FALSE;
  pmath_bool_t show_message = error_message && *error_message;
  size_t i, len, exprlen;
  pmath_expr_t result;
  
  exprlen = pmath_expr_length(expr);
  
  if(end > exprlen)
    end = exprlen;
  
  len = 0;
  
  if(show_message)
    *error_message = FALSE;
  
  for(i = start;i <= end;++i){
    pmath_t arg = pmath_expr_get_item(expr, i);
    if(pmath_instance_of(arg, PMATH_TYPE_EXPRESSION)){
      pmath_t arg_head = pmath_expr_get_item((pmath_expr_t)arg, 0);
      
      if(pmath_equals(head, arg_head)){
        if(have_sth_to_thread){
          if(len != pmath_expr_length((pmath_expr_t)arg)){
            pmath_unref(arg_head);
            pmath_unref(arg);
            if(show_message){
              *error_message = TRUE;
              pmath_message(PMATH_SYMBOL_THREAD, "len", 1, pmath_ref(expr));
            }
            return expr;
          }
        }
        else{
          len = pmath_expr_length((pmath_expr_t)arg);
          have_sth_to_thread = TRUE;
        }
      }
      pmath_unref(arg_head);
    }
    pmath_unref(arg);
  }

  if(!have_sth_to_thread)
    return expr;

  result = pmath_expr_new(pmath_ref(head), len);
  
  for(i = 1;i <= len;++i){
    pmath_expr_t f = pmath_ref(expr);

    size_t j;
    for(j = start;j <= end;++j){
      pmath_t arg = pmath_expr_get_item(expr, j);
      if(pmath_instance_of(arg, PMATH_TYPE_EXPRESSION)){
        pmath_t arg_head = pmath_expr_get_item((pmath_expr_t)arg, 0);
        if(pmath_equals(head, arg_head)){
          f = pmath_expr_set_item(
            f, j,
            pmath_expr_get_item((pmath_expr_t)arg, i));
          pmath_unref(arg);
        }
        else
          f = pmath_expr_set_item(f, j, arg);
        pmath_unref(arg_head);
      }
      else
        f = pmath_expr_set_item(f, j, arg);
    }

    result = pmath_expr_set_item(result, i, f);
  }

  pmath_unref(expr);
  return result;
}

/*----------------------------------------------------------------------------*/

PMATH_PRIVATE pmath_bool_t _pmath_expr_is_updated(
  pmath_expr_t expr
){
  size_t i, len;
  
  if(PMATH_UNLIKELY(!expr))
    return TRUE;
    
  assert(pmath_instance_of(expr, PMATH_TYPE_EXPRESSION));
  
  if(((struct _pmath_timed_t*)expr)->last_change < 0)
    return FALSE;
  
//  if(((struct _pmath_unpacked_expr_t*)expr)->last_change == 0)
//    return FALSE;
//  
//  if(last_change == (uintptr_t)global_update_counter)
//    return TRUE;
  
  len = pmath_expr_length(expr);
  
  for(i = 0;i <= len;i++){
    pmath_t item = pmath_expr_get_item(expr, i);
    if((pmath_instance_of(item, PMATH_TYPE_SYMBOL | PMATH_TYPE_EXPRESSION)
    && ((struct _pmath_timed_t*)item)->last_change > ((struct _pmath_timed_t*)expr)->last_change)){
      pmath_unref(item);
      return FALSE;
    }
    pmath_unref(item);
  }
  
  return TRUE;
}

PMATH_PRIVATE 
void _pmath_expr_update(pmath_expr_t expr){
  if(PMATH_LIKELY(expr)){
    ((struct _pmath_timed_t*)expr)->last_change = _pmath_timer_get();
  }
}

PMATH_PRIVATE 
_pmath_timer_t _pmath_expr_last_change(pmath_expr_t expr){
  if(PMATH_LIKELY(expr))
    return ((struct _pmath_timed_t*)expr)->last_change;
  
  return 0;
}

/*----------------------------------------------------------------------------*/

// result: *expr might be a list now (Listable attribute)
PMATH_PRIVATE pmath_bool_t _pmath_expr_eval_items(
  pmath_expr_t *expr // will be freed; head already evaluated.
){
  pmath_t head = pmath_expr_get_item(*expr, 0);

  pmath_bool_t eval_first   = TRUE;
  pmath_bool_t eval_rest    = TRUE;
  pmath_bool_t sort_args    = FALSE;
  pmath_bool_t flatten_args = FALSE;
  pmath_bool_t thread_args  = FALSE;
  
  size_t len;
  
  if(pmath_instance_of(head, PMATH_TYPE_SYMBOL)){
    pmath_symbol_attributes_t attrib = pmath_symbol_get_attributes(
      (pmath_symbol_t)head);

    if((attrib & PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE) != 0){
      pmath_unref(head);
      return FALSE;
    }

    eval_first   = (attrib & PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST)   == 0;
    eval_rest    = (attrib & PMATH_SYMBOL_ATTRIBUTE_HOLDREST)    == 0;
    sort_args    = (attrib & PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC)   != 0;
    flatten_args = (attrib & PMATH_SYMBOL_ATTRIBUTE_ASSOCIATIVE) != 0;
    thread_args  = (attrib & PMATH_SYMBOL_ATTRIBUTE_LISTABLE)    != 0;
  }
  else{
    pmath_symbol_t sym = _pmath_topmost_symbol(*expr);
    if(sym){
      pmath_symbol_attributes_t attrib = pmath_symbol_get_attributes(sym);
      
      if((attrib & PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE) != 0
      && (attrib & PMATH_SYMBOL_ATTRIBUTE_DEEPHOLDALL) != 0){
        pmath_unref(head);
        pmath_unref(sym);
        return FALSE;
      }
      
      eval_first = (attrib & PMATH_SYMBOL_ATTRIBUTE_DEEPHOLDALL) == 0;
      eval_rest = eval_first;
      pmath_unref(sym);
    }
  }
  
  len = pmath_expr_length(*expr);
  if(len >= 1){
    pmath_t item = pmath_expr_get_item(*expr, 1);
    if(eval_first){
      item = pmath_evaluate(item);
      
      if(pmath_is_expr_of_len(item, PMATH_SYMBOL_UNEVALUATED, 1)){
        pmath_expr_t item_expr = (pmath_expr_t)item;
        item = pmath_expr_get_item(item_expr, 1);
        pmath_unref(item_expr);
      }
      
      *expr = pmath_expr_set_item(*expr, 1, item);
    }
    else if(pmath_instance_of(item, PMATH_TYPE_EXPRESSION)
    && pmath_expr_length((pmath_expr_t)item) == 1){
      pmath_t item_head = pmath_expr_get_item((pmath_expr_t)item, 0);
      pmath_unref(item_head);
      if(item_head == PMATH_SYMBOL_EVALUATE)
        *expr = pmath_expr_set_item(*expr, 1, pmath_evaluate(item));
      else
        pmath_unref(item);
    }
    else
      pmath_unref(item);
  }

  if(eval_rest){
    size_t i;
    for(i = 2;i <= len;i++){ // *expr[i] = Evaluate(*expr[i])
      pmath_t item = pmath_evaluate(
        pmath_expr_get_item(*expr, i));
      
      if(pmath_is_expr_of_len(item, PMATH_SYMBOL_UNEVALUATED, 1)){
        pmath_expr_t item_expr = (pmath_expr_t)item;
        item = pmath_expr_get_item(item_expr, 1);
        pmath_unref(item_expr);
      }
      
      *expr = pmath_expr_set_item(*expr, i, item);
    }
  }
  else{ // evaluate items that match Evaluate(~)
    size_t i;
    for(i = 2;i <= len;i++){
      pmath_t item = pmath_expr_get_item(*expr, i);
      if(pmath_instance_of(item, PMATH_TYPE_EXPRESSION)
      && pmath_expr_length((pmath_expr_t)item) == 1){
        pmath_t item_head = pmath_expr_get_item((pmath_expr_t)item, 0);
        pmath_unref(item_head);
        if(item_head == PMATH_SYMBOL_EVALUATE)
          *expr = pmath_expr_set_item(*expr, i, pmath_evaluate(item));
        else
          pmath_unref(item);
      }
      else
        pmath_unref(item);
    }
  }

  if(flatten_args)
    *expr = pmath_expr_flatten(
      *expr,
      pmath_ref(head),
      PMATH_EXPRESSION_FLATTEN_MAX_DEPTH);

  if(sort_args)
    *expr = pmath_expr_sort(*expr);

  if(thread_args){
    pmath_bool_t error_message = TRUE;
    *expr = _pmath_expr_thread(
      *expr, PMATH_SYMBOL_LIST, 1, SIZE_MAX, &error_message);
  }
  
  pmath_unref(head);
  return thread_args;
}

/*============================================================================*/
/* pMath object functions ... */

PMATH_PRIVATE
int _pmath_compare_exprsym(pmath_t a, pmath_t b){
  switch(a->type_shift){
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
      struct _pmath_unpacked_expr_t *ua = (struct _pmath_unpacked_expr_t*)a;
      
      switch(b->type_shift){
        case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
        case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
          struct _pmath_unpacked_expr_t *ub = (struct _pmath_unpacked_expr_t*)b;
          size_t i;
          
          if(ua->items[0] == PMATH_SYMBOL_COMPLEX){
            if(ub->items[0] != PMATH_SYMBOL_COMPLEX)
              return -1;
          }
          else if(ub->items[0] == PMATH_SYMBOL_COMPLEX)
            return +1;
          
          if(ua->length < ub->length)  return -1;
          if(ua->length > ub->length)  return +1;
          
          for(i = 0;i <= ua->length;++i){
            pmath_t itema = pmath_expr_get_item(a, i);
            pmath_t itemb = pmath_expr_get_item(b, i);
            
            int cmp = pmath_compare(itema, itemb);
            
            pmath_unref(itema);
            pmath_unref(itemb);
            if(cmp != 0)
              return cmp;
          }
          
          return 0;
        } break;
        
        case PMATH_TYPE_SHIFT_SYMBOL: {
          if(ua->items[0] == PMATH_SYMBOL_COMPLEX)
            return -1;
          
          return +1;
        } break;
      }
      
    } break;
    
    case PMATH_TYPE_SHIFT_SYMBOL: {
      
      switch(b->type_shift){
        case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
        case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: 
          return -_pmath_compare_exprsym(b, a);
        
        case PMATH_TYPE_SHIFT_SYMBOL: {
          pmath_string_t namea = pmath_symbol_name(a);
          pmath_string_t nameb = pmath_symbol_name(b);
          int cmp = pmath_compare(namea, nameb);
          pmath_unref(namea);
          pmath_unref(nameb);
          return cmp;
        } break;
      }
      
    } break;
  }
  
  assert("neither an expression nor a symbol" && 0);
  
  return 0;
}

static pmath_bool_t equal_expressions(
  pmath_expr_t exprA,
  pmath_expr_t exprB
){
  size_t i, lenA;
  
  lenA = pmath_expr_length(exprA);

  if(lenA != pmath_expr_length(exprB))
    return FALSE;

  for(i = 0;i <= lenA;i++){
    pmath_t itemA = pmath_expr_get_item(exprA, i);
    pmath_t itemB = pmath_expr_get_item(exprB, i);
    pmath_bool_t eq = pmath_equals(itemA, itemB);
    pmath_unref(itemA);
    pmath_unref(itemB);
    if(!eq)
      return FALSE;
  }
  return TRUE;
}

static unsigned int hash_expression(
  pmath_expr_t expr
){
  unsigned int next = 0;
  size_t i;
  
  for(i = 0;i <= pmath_expr_length(expr);i++){
    pmath_t item = pmath_expr_get_item(expr, i);
    unsigned int h = pmath_hash(item);
    pmath_unref(item);
    next = incremental_hash(&h, sizeof(h), next);
  }
  return next;
}

static void destroy_general_expression(struct _pmath_unpacked_expr_t *expr){
  size_t i;
  for(i = 0;i <= expr->length;i++)
    pmath_unref(expr->items[i]);
  
  if(expr->length < CACHES_MAX){
    uintptr_t ui = expr_cache_inc(expr->length, +1);
    
    expr = expr_cache_swap(expr->length, ui, expr);
  }
  pmath_mem_free(expr);
}

static void destroy_part_expression(struct _pmath_unpacked_expr_part_t *expr){
  pmath_unref(expr->inherited.items[0]);
  pmath_unref((pmath_expr_t)expr->buffer);

  pmath_mem_free(expr);
}

//{ writing expressions

#define PRIO_ANY                 0
#define PRIO_ASSIGN             10 // a:= b, a::= b
#define PRIO_FUNCTION           20 // body _
#define PRIO_COLON              25 // a : b : ...
#define PRIO_RULE               30 // a -> b, a:> b
#define PRIO_STREXPR            35 // a ++ b ++ ...
#define PRIO_ARROWS             40 // a => b ...
#define PRIO_CONDITION          50 // pattern // condition
#define PRIO_ALTERNATIVES       60 // pat1 | pat2 | ...
#define PRIO_LOGIC              70 // a || b || ..., a && b && ...
#define PRIO_NOT                80 // !a
#define PRIO_IDENTITY           90 // a == b, a =!= b
#define PRIO_EQUATION          100 // a = b, a < b, ...
#define PRIO_RANGE             110 // a .. b
#define PRIO_PLUS              120 // a + b + ...
#define PRIO_TIMES             130 // a * b * ...
#define PRIO_FACTOR            (PRIO_TIMES + 1)
#define PRIO_POWER             140 // a^b
#define PRIO_INCDEC            150 // a+= b, a++, ++a, a-= b, a--, --a
#define PRIO_PATTERN           160 // pattern ? function, pattern**, pattern***
#define PRIO_CALL              170 // f(...)
#define PRIO_SYMBOL            180

#define WRITE_CSTR(str) write_cstr((str), write, user)

static void write_expr_ex(
  pmath_expr_t      expr,
  pmath_write_options_t   options,
  int                     priority,
  pmath_write_func_t      write,
  void                   *user);

static void write_ex(
  pmath_t          obj,
  pmath_write_options_t   options,
  int                     priority,
  pmath_write_func_t      write,
  void                   *user
){
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    write_expr_ex((pmath_expr_t)obj, options, priority, write, user);
  }
  else if(pmath_instance_of(obj, PMATH_TYPE_NUMBER)
  && ((priority > PRIO_TIMES && pmath_number_sign(obj) < 0)
   || (priority > PRIO_FACTOR && pmath_instance_of(obj, PMATH_TYPE_QUOTIENT)))){
    WRITE_CSTR("(");

    pmath_write(obj, options, write, user);

    WRITE_CSTR(")");
  }
  else
    pmath_write(obj, options, write, user);
}

typedef struct{
  void               *next_user;
  pmath_write_func_t  next;
  int                 prefix_status;
  pmath_bool_t     special_end; // for product_writer
}_writer_hook_data_t;

/* Hook in the given writer function and insert a space before the first 
   character if it is '?'.
   
   needed to distinguish 
     a/?b    (= Condition(a, b)) and 
     a/ ?b   (= Times(a, Power(Optional(b), -1)))
 */
static void division_writer(
  _writer_hook_data_t *user,
  const uint16_t *data,
  int len
){
  if(len == 0)
    return;
  
  if(user->prefix_status == 0){
    user->prefix_status = 1;
    
    if(data[0] == '?')
      write_cstr(" ", user->next, user->next_user);
  }
  
  user->next(user->next_user, data, len);
}

/* Hook in the given writer function and place stars as multilication signs when
   needed (before (,[,{ ) or a space otherwise.
   => times(a,b) becomes "a b",
      times(a,plus(b,c)) becomes "a*(b+c)".
 */
static void product_writer(
  _writer_hook_data_t *user,
  const uint16_t *data,
  int len
){
  int i = 0;
  
  if(len == 0)
    return;

  while(i < len && data[i] <= ' ')
    i++;

  if(user->prefix_status == 1){
    if(i < len && data[i] == '/')
      write_cstr("1", user->next, user->next_user);
    user->prefix_status = 2;
  }
  else if(user->prefix_status == 0){
    user->prefix_status = 2;//i < len;
    if(i < len && (data[i] == '('
                || data[i] == '['
                || data[i] == '.'))
    {
      write_cstr("*", user->next, user->next_user);
    }
    else if(i < len && data[i] != '/')
      write_cstr(" ", user->next, user->next_user);
  }
  //user->prefix_status = user->prefix_status && i >= len;
  i = len;
  while(i > 0 && data[i-1] <= ' ')
    i--;
  user->special_end = i > 0 && data[i-1] == '~';

  user->next(user->next_user, data, len);
}

/* Hook in the given writer function and place plus or minus signs
   appropriately.
   => plus(a,times(-2,b)) becomes "a - 2 b" instead of "a + -2 b".
 */
static void sum_writer(
  _writer_hook_data_t *user,
  const uint16_t *data,
  int len
){
  if(len == 0)
    return;

  if(!user->prefix_status){
    int i = 0;
    while(i < len && data[i] <= ' ')
      i++;
    user->prefix_status = i < len;
    if(i < len && data[i] == '-'){
      write_cstr(" - ", user->next, user->next_user);
      data+= i+1;
      len-= i+1;
      user->prefix_status = TRUE;
    }
    else if(i < len){
      write_cstr(" + ", user->next, user->next_user);
      user->prefix_status = TRUE;
    }
  }
  user->next(user->next_user, data, len);
}

static void write_expr_ex(
  pmath_expr_t      expr,
  pmath_write_options_t   options,
  int                     priority,
  pmath_write_func_t      write,
  void                   *user
){
  size_t exprlen = pmath_expr_length(expr);
  pmath_t head = pmath_expr_get_item(expr, 0);

  if(options & PMATH_WRITE_OPTIONS_FULLEXPR)
    goto FULLFORM;

  if(options & PMATH_WRITE_OPTIONS_INPUTEXPR)
    goto INPUTFORM;

  if(     head == PMATH_SYMBOL_DIRECTEDINFINITY){
    if(exprlen == 0){
      pmath_write(PMATH_SYMBOL_COMPLEXINFINITY, options, write, user);
    }
    else if(exprlen == 1){
      pmath_t direction;
      
      direction = pmath_expr_get_item(expr, 1);
      if(pmath_equals(direction, PMATH_NUMBER_ONE)){
        pmath_write(PMATH_SYMBOL_INFINITY, options, write, user);
      }
      else if(pmath_equals(direction, PMATH_NUMBER_MINUSONE)){
        if(priority > PRIO_FACTOR)
          WRITE_CSTR("(-");
        else
          WRITE_CSTR("-");

        pmath_write(PMATH_SYMBOL_INFINITY, options, write, user);

        if(priority > PRIO_FACTOR)
          WRITE_CSTR(")");
      }
      else{
        pmath_unref(direction);
        goto FULLFORM;
      }
      pmath_unref(direction);
    }
    else
      goto FULLFORM;
  }
  else if(head == PMATH_SYMBOL_FULLFORM){
    pmath_t item;
    
    if(exprlen != 1)
      goto FULLFORM;

    item = pmath_expr_get_item(expr, 1);
    pmath_write(
      item,
      options
        | PMATH_WRITE_OPTIONS_FULLSTR
        | PMATH_WRITE_OPTIONS_FULLEXPR,
      write,
      user);
    pmath_unref(item);
  }
  else if(head == PMATH_SYMBOL_INPUTFORM){
    pmath_t item;
    
    if(exprlen != 1)
      goto FULLFORM;

    item = pmath_expr_get_item(expr, 1);
    pmath_write(
      item,
      options
        | PMATH_WRITE_OPTIONS_FULLSTR
        | PMATH_WRITE_OPTIONS_INPUTEXPR,
      write,
      user);
    pmath_unref(item);
  }
  else if(head == PMATH_SYMBOL_HOLDFORM
  ||      head == PMATH_SYMBOL_OUTPUTFORM
  ||      head == PMATH_SYMBOL_STANDARDFORM){
    pmath_t item;
    if(exprlen != 1)
      goto FULLFORM;

    item = pmath_expr_get_item(expr, 1);
    pmath_write(item, options, write, user);
    pmath_unref(item);
  }
  else if(head == PMATH_SYMBOL_LONGFORM){
    pmath_t item;
    
    if(exprlen != 1)
      goto FULLFORM;

    item = pmath_expr_get_item(expr, 1);
    pmath_write(item, options | PMATH_WRITE_OPTIONS_FULLNAME, write, user);
    pmath_unref(item);
  }
  else if(head == PMATH_SYMBOL_STRINGFORM){
    if(!_pmath_stringform_write(
      expr,
      options,
      write,
      user)
    ){
      goto FULLFORM;
    }
  }
  else if(head == PMATH_SYMBOL_COLON){
    pmath_t item;
    size_t i;
    
    if(exprlen < 2)
      goto FULLFORM;

    if(priority > PRIO_COLON)
      WRITE_CSTR("(");

    for(i = 1;i <= exprlen;i++){
      if(i > 1)
        WRITE_CSTR(" : ");
      
      item = pmath_expr_get_item(expr, i);
      write_ex(item, options, PRIO_COLON+1, write, user);
      pmath_unref(item);
    }

    if(priority > PRIO_COLON)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_ROW){
    pmath_expr_t list;
    pmath_t obj;
    size_t i, listlen;
    
    if(exprlen < 1 || exprlen > 2)
      goto FULLFORM;

    list = (pmath_expr_t)pmath_expr_get_item(expr, 1);
    if(!pmath_instance_of(list, PMATH_TYPE_EXPRESSION)){
      pmath_unref(list);
      goto FULLFORM;
    }

    obj = pmath_expr_get_item(list, 0);
    pmath_unref(obj);
    if(obj != PMATH_SYMBOL_LIST){
      pmath_unref(list);
      goto FULLFORM;
    }

    listlen = pmath_expr_length(list);
    if(exprlen == 2){
      pmath_t seperator = pmath_expr_get_item(expr, 2);
      
      for(i = 1;i < listlen;++i){
        obj = pmath_expr_get_item(list, i);
        pmath_write(obj, options, write, user);
        pmath_unref(obj);
        pmath_write(seperator, options, write, user);
      }

      pmath_unref(seperator);

      if(listlen > 0){
        obj = pmath_expr_get_item(list, listlen);
        pmath_write(obj, options, write, user);
        pmath_unref(obj);
      }
    }
    else{
      for(i = 1;i <= listlen;++i){
        obj = pmath_expr_get_item(list, i);
        pmath_write(obj, options, write, user);
        pmath_unref(obj);
      }
    }
    pmath_unref(list);
  }
  else if(head == PMATH_SYMBOL_SHALLOW){
    pmath_t item;
    size_t maxdepth = 4;
    size_t maxlength = 10;
    
    if(exprlen < 1 || exprlen > 2)
      goto FULLFORM;
    
    if(exprlen == 2){
      item = pmath_expr_get_item(expr, 2);
      
      if(pmath_instance_of(item, PMATH_TYPE_INTEGER)
      && pmath_integer_fits_ui(item)){
        maxdepth = pmath_integer_get_ui(item);
      }
      else if(pmath_equals(item, _pmath_object_infinity)){
        maxdepth = SIZE_MAX;
      }
      else if(pmath_is_expr_of_len(item, PMATH_SYMBOL_LIST, 2)){
        pmath_t obj = pmath_expr_get_item(item, 1);
        
        if(pmath_instance_of(obj, PMATH_TYPE_INTEGER)
        && pmath_integer_fits_ui(obj)){
          maxdepth = pmath_integer_get_ui(obj);
        }
        else if(pmath_equals(obj, _pmath_object_infinity)){
          maxdepth = SIZE_MAX;
        }
        else{
          pmath_unref(obj);
          pmath_unref(item);
          goto FULLFORM;
        }
        
        pmath_unref(obj);
        obj = pmath_expr_get_item(item, 2);
        
        if(pmath_instance_of(obj, PMATH_TYPE_INTEGER)
        && pmath_integer_fits_ui(obj)){
          maxlength = pmath_integer_get_ui(obj);
        }
        else if(pmath_equals(obj, _pmath_object_infinity)){
          maxlength = SIZE_MAX;
        }
        else{
          pmath_unref(obj);
          pmath_unref(item);
          goto FULLFORM;
        }
        
        pmath_unref(obj);
      }
      else{
        pmath_unref(item);
        goto FULLFORM;
      }
      
      pmath_unref(item);
    }
    
    item = pmath_expr_get_item(expr, 1);
    item = _pmath_prepare_shallow(item, maxdepth, maxlength);
    
    pmath_write(item, options, write, user);

    pmath_unref(item);
  }
  else if(head == PMATH_SYMBOL_SHORT){
    pmath_t item;
    
    if(exprlen < 1 || exprlen > 2)
      goto FULLFORM;
    
    if(exprlen == 2){
      item = pmath_expr_get_item(expr, 2);
      
      if((!pmath_instance_of(item, PMATH_TYPE_NUMBER) 
       || pmath_number_sign(item) < 0)
      && !pmath_equals(item, _pmath_object_infinity)){
        pmath_unref(item);
        goto FULLFORM;
      }
      
      pmath_unref(item);
    }
    
    item = pmath_expr_get_item(expr, 1);
    pmath_write(item, options, write, user);
    pmath_unref(item);
  }
  else if(head == PMATH_SYMBOL_SKELETON){
    pmath_t item;
    
    if(exprlen != 1)
      goto FULLFORM;

    item = pmath_expr_get_item(expr, 1);
    
    WRITE_CSTR("<<");
    pmath_write(item, options, write, user);
    WRITE_CSTR(">>");

    pmath_unref(item);
  }
  else INPUTFORM: if( exprlen == 2 /*=========================================*/
  && (    head == PMATH_SYMBOL_ASSIGN
  ||      head == PMATH_SYMBOL_ASSIGNDELAYED
  ||      head == PMATH_SYMBOL_DECREMENT
  ||      head == PMATH_SYMBOL_DIVIDEBY
  ||      head == PMATH_SYMBOL_INCREMENT
  ||      head == PMATH_SYMBOL_TIMESBY)){
    pmath_t lhs, rhs;
    
    if(priority > PRIO_ASSIGN)
      WRITE_CSTR("(");

    lhs = pmath_expr_get_item(expr, 1);
    rhs = pmath_expr_get_item(expr, 2);

    write_ex(lhs, options, PRIO_ASSIGN+1, write, user);
    
    if(head == PMATH_SYMBOL_ASSIGN)
      WRITE_CSTR(":= ");
    else if(head == PMATH_SYMBOL_ASSIGNDELAYED)
      WRITE_CSTR("::= ");
    else if(head == PMATH_SYMBOL_DECREMENT)
      WRITE_CSTR("-= ");
    else if(head == PMATH_SYMBOL_DIVIDEBY)
      WRITE_CSTR("/= ");
    else if(head == PMATH_SYMBOL_INCREMENT)
      WRITE_CSTR("+= ");
    else
      WRITE_CSTR("*= ");

    write_ex(rhs, options, PRIO_ASSIGN, write, user);

    pmath_unref(lhs);
    pmath_unref(rhs);

    if(priority > PRIO_ASSIGN)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_DECREMENT
  ||      head == PMATH_SYMBOL_INCREMENT
  ||      head == PMATH_SYMBOL_POSTDECREMENT
  ||      head == PMATH_SYMBOL_POSTINCREMENT){
    pmath_t arg;
    
    if(exprlen != 1)
      goto FULLFORM;

    if(priority > PRIO_INCDEC)
      WRITE_CSTR("(");

    if(head == PMATH_SYMBOL_DECREMENT)
      WRITE_CSTR("--");
    else if(head == PMATH_SYMBOL_INCREMENT)
      WRITE_CSTR("++");

    arg = pmath_expr_get_item(expr, 1);
    write_ex(arg, options, PRIO_INCDEC+1, write, user);
    pmath_unref(arg);

    if(head == PMATH_SYMBOL_POSTDECREMENT)
      WRITE_CSTR("--");
    else if(head == PMATH_SYMBOL_POSTINCREMENT)
      WRITE_CSTR("++");

    if(priority > PRIO_INCDEC)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_TAGASSIGN
  ||      head == PMATH_SYMBOL_TAGASSIGNDELAYED){
    pmath_t obj;
    
    if(exprlen != 3)
      goto FULLFORM;

    if(priority > PRIO_ASSIGN)
      WRITE_CSTR("(");

    obj = pmath_expr_get_item(expr, 1);
    write_ex(obj, options, PRIO_SYMBOL, write, user);
    pmath_unref(obj);

    WRITE_CSTR("/: ");

    obj = pmath_expr_get_item(expr, 2);
    write_ex(obj, options, PRIO_ASSIGN+1, write, user);
    pmath_unref(obj);

    if(head == PMATH_SYMBOL_TAGASSIGN)
      WRITE_CSTR(":= ");
    else
      WRITE_CSTR("::= ");

    obj = pmath_expr_get_item(expr, 3);
    write_ex(obj, options, PRIO_ASSIGN, write, user);
    pmath_unref(obj);

    if(priority > PRIO_ASSIGN)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_RULE
  ||      head == PMATH_SYMBOL_RULEDELAYED){
    pmath_t lhs, rhs;
    
    if(exprlen != 2)
      goto FULLFORM;

    if(priority > PRIO_RULE)
      WRITE_CSTR("(");

    lhs = pmath_expr_get_item(expr, 1);
    rhs = pmath_expr_get_item(expr, 2);

    write_ex(lhs, options, PRIO_RULE+1, write, user);
    if(head == PMATH_SYMBOL_RULE)
      WRITE_CSTR(" -> ");
    else
      WRITE_CSTR(" :> ");
    write_ex(rhs, options, PRIO_RULE, write, user);

    pmath_unref(lhs);
    pmath_unref(rhs);

    if(priority > PRIO_RULE)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_LIST){
    size_t i;
    
    WRITE_CSTR("{");
    for(i = 1;i <= exprlen;i++){
      pmath_t item;
      
      if(i > 1)
        WRITE_CSTR(", ");
      
      item = pmath_expr_get_item(expr, i);
      pmath_write(item, options, write, user);
      pmath_unref(item);
    }

    WRITE_CSTR("}");
  }
  else if(head == PMATH_SYMBOL_STRINGEXPRESSION){
    pmath_t item;
    size_t i;
    
    if(exprlen < 2)
      goto FULLFORM;

    if(priority > PRIO_STREXPR)
      WRITE_CSTR("(");

    for(i = 1;i <= exprlen;i++){
      if(i > 1)
        WRITE_CSTR(" ++ ");
      
      item = pmath_expr_get_item(expr, i);
      write_ex(item, options, PRIO_STREXPR+1, write, user);
      pmath_unref(item);
    }

    if(priority > PRIO_STREXPR)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_EVALUATIONSEQUENCE){
    pmath_t item;
    size_t i;
    
    if(exprlen < 2)
      goto FULLFORM;

    if(priority > PRIO_ANY)
      WRITE_CSTR("(");

    for(i = 1;i < exprlen;i++){
      if(i > 1)
        WRITE_CSTR("; ");
        
      item = pmath_expr_get_item(expr, i);
      write_ex(item, options, PRIO_ANY+1, write, user);
      pmath_unref(item);
    }
    
    item = pmath_expr_get_item(expr, exprlen);
    if(item){
      WRITE_CSTR("; ");
      write_ex(item, options, PRIO_ANY+1, write, user);
      pmath_unref(item);
    }
    else
      WRITE_CSTR(";");

    if(priority > PRIO_ANY)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_ALTERNATIVES){
    pmath_t item;
    size_t i;
    
    if(exprlen < 2)
      goto FULLFORM;
    
    if(priority > PRIO_ALTERNATIVES)
      WRITE_CSTR("(");
    
    for(i = 1;i <= exprlen;i++){
      if(i > 1)
        WRITE_CSTR(" | ");
      
      item = pmath_expr_get_item(expr, i);
      write_ex(item, options, PRIO_ALTERNATIVES+1, write, user);
      pmath_unref(item);
    }

    if(priority > PRIO_ALTERNATIVES)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_OR){
    pmath_t item;
    size_t i;
    
    if(exprlen < 2)
      goto FULLFORM;

    if(priority > PRIO_LOGIC)
      WRITE_CSTR("(");

    for(i = 1;i <= exprlen;i++){
      if(i > 1)
        WRITE_CSTR(" || ");
      
      item = pmath_expr_get_item(expr, i);
      write_ex(item, options, PRIO_LOGIC+1, write, user);
      pmath_unref(item);
    }

    if(priority > PRIO_LOGIC)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_AND){
    pmath_t item;
    size_t i;
    
    if(exprlen < 2)
      goto FULLFORM;

    if(priority > PRIO_LOGIC)
      WRITE_CSTR("(");

    for(i = 1;i <= exprlen;i++){
      if(i > 1)
        WRITE_CSTR(" && ");
      
      item = pmath_expr_get_item(expr, i);
      write_ex(item, options, PRIO_LOGIC+1, write, user);
      pmath_unref(item);
    }

    if(priority > PRIO_LOGIC)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_NOT){
    pmath_t item;
    
    if(exprlen != 1)
      goto FULLFORM;

    if(priority > PRIO_NOT)
      WRITE_CSTR("(!");
    else
      WRITE_CSTR("!");
    
    item = pmath_expr_get_item(expr, 1);
    write_ex(item, options, PRIO_NOT+1, write, user);
    pmath_unref(item);

    if(priority > PRIO_NOT)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_IDENTICAL){
    pmath_t item;
    size_t i;
    
    if(exprlen < 2)
      goto FULLFORM;

    if(priority > PRIO_IDENTITY)
      WRITE_CSTR("(");

    for(i = 1;i <= exprlen;i++){
      if(i > 1)
        WRITE_CSTR(" === ");
      
      item = pmath_expr_get_item(expr, i);
      write_ex(item, options, PRIO_IDENTITY+1, write, user);
      pmath_unref(item);
    }

    if(priority > PRIO_IDENTITY)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_UNIDENTICAL){
    pmath_t item;
    size_t i;
    
    if(exprlen < 2)
      goto FULLFORM;

    if(priority > PRIO_IDENTITY)
      WRITE_CSTR("(");

    for(i = 1;i <= exprlen;i++){
      if(i > 1)
        WRITE_CSTR(" =!= ");
        
      item = pmath_expr_get_item(expr, i);
      write_ex(item, options, PRIO_IDENTITY+1, write, user);
      pmath_unref(item);
    }

    if(priority > PRIO_IDENTITY)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_LESS
  ||      head == PMATH_SYMBOL_LESSEQUAL
  ||      head == PMATH_SYMBOL_GREATER
  ||      head == PMATH_SYMBOL_GREATEREQUAL
  ||      head == PMATH_SYMBOL_EQUAL
  ||      head == PMATH_SYMBOL_UNEQUAL){
    pmath_t item;
    char   *op;
    size_t  i;
    
    if(exprlen < 2)
      goto FULLFORM;

    if(     head == PMATH_SYMBOL_LESS)          op = " < ";
    else if(head == PMATH_SYMBOL_LESSEQUAL)     op = " <= ";
    else if(head == PMATH_SYMBOL_GREATER)       op = " > ";
    else if(head == PMATH_SYMBOL_GREATEREQUAL)  op = " >= ";
    else if(head == PMATH_SYMBOL_EQUAL)         op = " = ";
    else if(head == PMATH_SYMBOL_UNEQUAL)       op = " != ";
    else                                        op = "";

    if(priority > PRIO_EQUATION)
      WRITE_CSTR("(");

    for(i = 1;i <= exprlen;i++){
      if(i > 1)
        WRITE_CSTR(op);
      
      item = pmath_expr_get_item(expr, i);
      write_ex(item, options, PRIO_EQUATION+1, write, user);
      pmath_unref(item);
    }

    if(priority > PRIO_EQUATION)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_INEQUATION){
    pmath_t item;
    size_t i;
    
    if(exprlen < 3 || (exprlen & 1) == 0)
      goto FULLFORM;

    if(priority > PRIO_EQUATION)
      WRITE_CSTR("(");

    item = pmath_expr_get_item(expr, 1);
    write_ex(item, options, PRIO_EQUATION+1, write, user);
    pmath_unref(item);

    exprlen/= 2;
    for(i = 1;i <= exprlen;i++){
      item = pmath_expr_get_item(expr, 2*i);
      if(     item == PMATH_SYMBOL_LESS)          WRITE_CSTR(" < ");
      else if(item == PMATH_SYMBOL_LESSEQUAL)     WRITE_CSTR(" <= ");
      else if(item == PMATH_SYMBOL_GREATER)       WRITE_CSTR(" > ");
      else if(item == PMATH_SYMBOL_GREATEREQUAL)  WRITE_CSTR(" >= ");
      else if(item == PMATH_SYMBOL_EQUAL)         WRITE_CSTR(" = ");
      else if(item == PMATH_SYMBOL_UNEQUAL)       WRITE_CSTR(" != ");
      else                                        WRITE_CSTR(" <<?>> ");
      pmath_unref(item);

      item = pmath_expr_get_item(expr, 2*i+1);
      write_ex(item, options, PRIO_EQUATION+1, write, user);
      pmath_unref(item);
    }

    if(priority > PRIO_EQUATION)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_PLUS){
    _writer_hook_data_t sum_writer_data;
    size_t i;
    
    if(exprlen < 2)
      goto FULLFORM;

    if(priority > PRIO_PLUS)
      WRITE_CSTR("(");

    sum_writer_data.next_user = user;
    sum_writer_data.next = write;
    sum_writer_data.prefix_status = TRUE;

    for(i = 1;i <= exprlen;i++){
      pmath_t item = pmath_expr_get_item(expr, i);
      write_ex(
        item,
        options,
        PRIO_PLUS+1,
        (pmath_write_func_t)sum_writer,
        &sum_writer_data);
      sum_writer_data.prefix_status = FALSE;
      pmath_unref(item);
    }

    if(priority > PRIO_PLUS)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_TIMES){
    _writer_hook_data_t  product_writer_data;
    pmath_t       item;
    size_t i;
    
    if(exprlen < 2)
      goto FULLFORM;

    if(priority > PRIO_TIMES)
      WRITE_CSTR("(");

    product_writer_data.next_user = user;
    product_writer_data.next = write;
    product_writer_data.prefix_status = 1;
    product_writer_data.special_end = FALSE;

    item = pmath_expr_get_item(expr, 1);
    if(pmath_instance_of(item, PMATH_TYPE_INTEGER)
    && pmath_integer_fits_si((pmath_integer_t)item)
    && pmath_integer_get_si((pmath_integer_t)item) == -1)
      WRITE_CSTR("-");
    else{
      write_ex(
        item,
        options,
        PRIO_FACTOR,
        (pmath_write_func_t)product_writer,
        &product_writer_data);
      product_writer_data.prefix_status = 0;
    }
    pmath_unref(item);

    for(i = 2;i <= exprlen;i++){
      item = pmath_expr_get_item(expr, i);
      
      if(product_writer_data.special_end){
        WRITE_CSTR("*");
        product_writer_data.prefix_status = 1;
      }
      
      write_ex(
        item,
        options,
        PRIO_FACTOR,
        (pmath_write_func_t)product_writer,
        &product_writer_data);
      product_writer_data.prefix_status = 0;
      pmath_unref(item);
    }

    if(priority > PRIO_TIMES)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_POWER){
    _writer_hook_data_t  division_writer_data;
    pmath_t base, exponent;
    
    if(exprlen != 2)
      goto FULLFORM;

    exponent = pmath_expr_get_item(expr, 2);
    base = pmath_expr_get_item(expr, 1);
    
    division_writer_data.next_user = user;
    division_writer_data.next = write;
    division_writer_data.prefix_status = 0;

    if(pmath_equals(exponent, _pmath_one_half)){
      WRITE_CSTR("Sqrt(");
      pmath_write(base, options, write, user);
      WRITE_CSTR(")");
    }
    else if(pmath_equals(exponent, PMATH_NUMBER_MINUSONE)){
      
      if(write == (pmath_write_func_t)product_writer)
        WRITE_CSTR("/");
      else
        WRITE_CSTR("1/");
      
      write_ex(
        base, 
        options, 
        PRIO_POWER+1, 
        (pmath_write_func_t)division_writer,
        &division_writer_data);
    }
    else if(pmath_instance_of(exponent, PMATH_TYPE_INTEGER)
    && pmath_number_sign(exponent) < 0){
      if(write == (pmath_write_func_t)product_writer)
        WRITE_CSTR("/");
      else
        WRITE_CSTR("1/");
      
      exponent = pmath_number_neg(exponent);
      
      write_ex(
        base, 
        options, 
        PRIO_POWER+1, 
        (pmath_write_func_t)division_writer,
        &division_writer_data);
        
      WRITE_CSTR("^");
      write_ex(exponent, options, PRIO_POWER, write, user);
    }
    else{
      pmath_t minus_one_half = pmath_number_neg(pmath_ref(_pmath_one_half));
      
      if(pmath_equals(exponent, minus_one_half)){
        if(write == (pmath_write_func_t)product_writer)
          WRITE_CSTR("/Sqrt(");
        else
          WRITE_CSTR("1/Sqrt(");
        pmath_write(base, options, write, user);
        WRITE_CSTR(")");
      }
      else{
        if(priority > PRIO_POWER)
          WRITE_CSTR("(");

        write_ex(base, options, PRIO_POWER+1, write, user);
        WRITE_CSTR("^");
        write_ex(exponent, options, PRIO_POWER, write, user);

        if(priority > PRIO_POWER)
          WRITE_CSTR(")");
      }
      
      pmath_unref(minus_one_half);
    }
    
    pmath_unref(base);
    pmath_unref(exponent);
  }
  else if(head == PMATH_SYMBOL_RANGE){
    pmath_t item;
    if(exprlen < 2 || exprlen > 3)
      goto FULLFORM;

    if(priority > PRIO_RANGE)
      WRITE_CSTR("(");

    item = pmath_expr_get_item(expr, 1);
    if(item != PMATH_SYMBOL_AUTOMATIC || exprlen > 2)
      write_ex(item, options, PRIO_RANGE+1, write, user);
    pmath_unref(item);

    WRITE_CSTR(" .. ");

    item = pmath_expr_get_item(expr, 2);
    if(item != PMATH_SYMBOL_AUTOMATIC || exprlen > 2)
      write_ex(item, options, PRIO_RANGE+1, write, user);
    pmath_unref(item);
    
    if(exprlen == 3){
      WRITE_CSTR(" .. ");

      item = pmath_expr_get_item(expr, 3);
      write_ex(item, options, PRIO_RANGE+1, write, user);
      pmath_unref(item);
    }
    
    if(priority > PRIO_RANGE)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_PART){
    pmath_t item;
    size_t i;
    
    if(exprlen < 1)
      goto FULLFORM;

    item = pmath_expr_get_item(expr, 1);
    write_ex(item, options, PRIO_CALL, write, user);
    pmath_unref(item);

    WRITE_CSTR("[");
    for(i = 2;i <= exprlen;i++){
      if(i > 2)
        WRITE_CSTR(", ");
      
      item = pmath_expr_get_item(expr, i);
      pmath_write(item, options, write, user);
      pmath_unref(item);
    }
    WRITE_CSTR("]");
  }
  else if(head == PMATH_SYMBOL_MESSAGENAME){
    pmath_t symbol;
    pmath_t tag;
    
    if(exprlen != 2)
      goto FULLFORM;

    tag = pmath_expr_get_item(expr, 2);

    if(!pmath_instance_of(tag, PMATH_TYPE_STRING)){
      pmath_unref(tag);
      goto FULLFORM;
    }

    if(priority > PRIO_CALL)
      WRITE_CSTR("(");
    symbol = pmath_expr_get_item(expr, 1);

    write_ex(symbol, options, PRIO_CALL, write, user);
    WRITE_CSTR("::");
    pmath_write(tag, options, write, user);

    pmath_unref(symbol);
    pmath_unref(tag);

    if(priority > PRIO_CALL)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_COMPLEX){
    pmath_number_t re, im;
    pmath_bool_t lparen = FALSE;
    
    if(!_pmath_is_nonreal_complex(expr))
      goto FULLFORM;

    re = (pmath_number_t)pmath_expr_get_item(expr, 1);
    im = (pmath_number_t)pmath_expr_get_item(expr, 2);

    if(!pmath_equals(re, PMATH_NUMBER_ZERO)){
      if(priority > PRIO_TIMES){
        WRITE_CSTR("(");
        lparen = TRUE;
      }

      pmath_write(re, options, write, user);

      if(pmath_number_sign(im) >= 0)
        WRITE_CSTR(" + ");
      else
        WRITE_CSTR(" ");
    }
    if(pmath_equals(im, PMATH_NUMBER_MINUSONE)){
      if(!lparen && priority > PRIO_TIMES){
        WRITE_CSTR("(");
        lparen = TRUE;
      }
      WRITE_CSTR("-I");
    }
    else if(pmath_equals(im, PMATH_NUMBER_ONE)){
      WRITE_CSTR("I");
    }
    else{
      if(!lparen && priority > PRIO_TIMES){
        WRITE_CSTR("(");
        lparen = TRUE;
      }
      pmath_write(im, options, write, user);
      WRITE_CSTR(" I");
    }

    if(lparen)
      WRITE_CSTR(")");

    pmath_unref(re);
    pmath_unref(im);
  }
  else if(head == PMATH_SYMBOL_PATTERN){
    pmath_t sym, pat;
    
    if(exprlen != 2)
      goto FULLFORM;

    sym = pmath_expr_get_item(expr, 1);
    if(!pmath_instance_of(sym, PMATH_TYPE_SYMBOL)){
      pmath_unref(sym);
      goto FULLFORM;
    }

    pat = pmath_expr_get_item(expr, 2);
    if(pmath_equals(pat, _pmath_object_singlematch)){
      WRITE_CSTR("~");
      pmath_write(sym, options, write, user);
    }
    else if(pmath_equals(pat, _pmath_object_multimatch)){
      WRITE_CSTR("~~");
      pmath_write(sym, options, write, user);
    }
    else if(pmath_equals(pat, _pmath_object_zeromultimatch)){
      WRITE_CSTR("~~~");
      pmath_write(sym, options, write, user);
    }
    else{
      pmath_bool_t default_pattern = TRUE;
      
      if(pmath_is_expr_of_len(pat, PMATH_SYMBOL_SINGLEMATCH, 1)){
        pmath_t type = pmath_expr_get_item(pat, 1);
        
        WRITE_CSTR("~");
        pmath_write(sym, options, write, user);
        WRITE_CSTR(":");
        
        write_ex(type, options, PRIO_SYMBOL, write, user);
        
        pmath_unref(type);
        default_pattern = FALSE;
      }
      else if(pmath_is_expr_of_len(pat, PMATH_SYMBOL_REPEATED, 2)){
        pmath_t rep   = pmath_expr_get_item(pat, 1);
        pmath_t range = pmath_expr_get_item(pat, 2);
        
        if(pmath_is_expr_of_len(rep, PMATH_SYMBOL_SINGLEMATCH, 1)){
          pmath_t type = pmath_expr_get_item(rep, 1);
          
          if(pmath_equals(range, _pmath_object_range_from_one)){
            WRITE_CSTR("~~");
            pmath_write(sym, options, write, user);
            WRITE_CSTR(":");
            
            write_ex(type, options, PRIO_SYMBOL, write, user);
            default_pattern = FALSE;
          }
          else if(pmath_equals(range, _pmath_object_range_from_zero)){
            WRITE_CSTR("~~~");
            pmath_write(sym, options, write, user);
            WRITE_CSTR(":");
            
            write_ex(type, options, PRIO_SYMBOL, write, user);
            default_pattern = FALSE;
          }
           
          pmath_unref(type);
        }
        
        pmath_unref(rep);
        pmath_unref(range);
      }
      
      if(default_pattern){
        if(priority > PRIO_ALTERNATIVES)
          WRITE_CSTR("(");

        pmath_write(sym, options, write, user);
        WRITE_CSTR(": ");
        write_ex(pat, options, PRIO_ALTERNATIVES, write, user);

        if(priority > PRIO_ALTERNATIVES)
          WRITE_CSTR(")");
      }
    }
    
    pmath_unref(sym);
    pmath_unref(pat);
  }
  else if(head == PMATH_SYMBOL_TESTPATTERN){
    pmath_t item;
    
    if(exprlen != 2)
      goto FULLFORM;

    if(priority > PRIO_PATTERN)
      WRITE_CSTR("(");

    item = pmath_expr_get_item(expr, 1);
    write_ex(item, options, PRIO_PATTERN+1, write, user);
    pmath_unref(item);
    
    WRITE_CSTR(" ? ");
    
    item = pmath_expr_get_item(expr, 2);
    write_ex(item, options, PRIO_CALL, write, user);
    pmath_unref(item);

    if(priority > PRIO_PATTERN)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_CONDITION){
    pmath_t item;
    
    if(exprlen != 2)
      goto FULLFORM;

    if(priority > PRIO_CONDITION)
      WRITE_CSTR("(");

    item = pmath_expr_get_item(expr, 1);
    write_ex(item, options, PRIO_CONDITION+1, write, user);
    pmath_unref(item);
    
    WRITE_CSTR(" /? ");
    
    item= pmath_expr_get_item(expr, 2);
    write_ex(item, options, PRIO_ALTERNATIVES+1, write, user);
    pmath_unref(item);

    if(priority > PRIO_CONDITION)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_SINGLEMATCH){
    if(exprlen == 0){
      WRITE_CSTR("~");
    }
    else if(exprlen == 1){
      pmath_t type = pmath_expr_get_item(expr, 1);
      
      WRITE_CSTR("~:");
      write_ex(type, options, PRIO_SYMBOL, write, user);
      
      pmath_unref(type);
    }
    else
      goto FULLFORM;
  }
  else if(head == PMATH_SYMBOL_REPEATED){
    pmath_t pattern, range;
    
    if(exprlen != 2)
      goto FULLFORM;

    pattern = pmath_expr_get_item(expr, 1);
    range = pmath_expr_get_item(expr, 2);
    
    if(pmath_equals(range, _pmath_object_range_from_one)){
      if(pmath_equals(pattern, _pmath_object_singlematch)){
        WRITE_CSTR("~~");
      }
      else if(pmath_is_expr_of_len(pattern, PMATH_SYMBOL_SINGLEMATCH, 1)){
        pmath_t type = pmath_expr_get_item(pattern, 1);
        
        WRITE_CSTR("~~:");
        write_ex(type, options, PRIO_SYMBOL, write, user);
        
        pmath_unref(type);
      }
      else{
        if(priority > PRIO_PATTERN)
          WRITE_CSTR("(");

        write_ex(pattern, options, PRIO_PATTERN+1, write, user);
        WRITE_CSTR("**");

        if(priority > PRIO_PATTERN)
          WRITE_CSTR(")");
      }
    }
    else if(pmath_equals(range, _pmath_object_range_from_zero)){
      if(pmath_equals(pattern, _pmath_object_singlematch)){
        WRITE_CSTR("~~~");
      }
      else if(pmath_is_expr_of_len(pattern, PMATH_SYMBOL_SINGLEMATCH, 1)){
        pmath_t type = pmath_expr_get_item(pattern, 1);
        
        WRITE_CSTR("~~~:");
        write_ex(type, options, PRIO_SYMBOL, write, user);
        
        pmath_unref(type);
      }
      else{
        if(priority > PRIO_PATTERN)
          WRITE_CSTR("(");

        write_ex(pattern, options, PRIO_PATTERN+1, write, user);
        WRITE_CSTR("***");

        if(priority > PRIO_PATTERN)
          WRITE_CSTR(")");
      }
    }
    else{
      pmath_unref(pattern);
      pmath_unref(range);
      goto FULLFORM;
    }

    pmath_unref(pattern);
    pmath_unref(range);
  }
  else if(head == PMATH_SYMBOL_FUNCTION){
    pmath_t body;
    
    if(exprlen != 1)
      goto FULLFORM;

    if(priority > PRIO_FUNCTION)
      WRITE_CSTR("(");

    body = pmath_expr_get_item(expr, 1);
    write_ex(body, options, PRIO_FUNCTION, write, user);
    pmath_unref(body);

    if(priority > PRIO_FUNCTION)
      WRITE_CSTR(" &)");
    else
      WRITE_CSTR(" &");
  }
//  else if(head == PMATH_SYMBOL_SEQUENCE){
//    pmath_t item;
//    size_t i;
//    
//    WRITE_CSTR("(");
//    
//    for(i = 1;i <= exprlen;i++){
//      if(i > 1)
//        WRITE_CSTR(", ");
//      
//      item = pmath_expr_get_item(expr, i);
//      if(item || exprlen < 2)
//        pmath_write(item, options, write, user);
//      pmath_unref(item);
//    }
//
//    WRITE_CSTR(")");
//  }
  else if(head == PMATH_SYMBOL_PUREARGUMENT){
    pmath_t item;
    
    if(exprlen != 1)
      goto FULLFORM;

    if(priority > PRIO_CALL)
      WRITE_CSTR("(#");
    else
      WRITE_CSTR("#");

    item = pmath_expr_get_item(expr, 1);
    write_ex(item, options, PRIO_CALL + 1, write, user);
    pmath_unref(item);

    if(priority > PRIO_CALL)
      WRITE_CSTR(")");
  }
  else if(head == PMATH_SYMBOL_OPTIONAL){
    pmath_t item;
    
    if(exprlen < 1 || exprlen > 2)
      goto FULLFORM;

    item = pmath_expr_get_item(expr, 1);
    if(!pmath_instance_of(item, PMATH_TYPE_SYMBOL)){
      pmath_unref(item);
      goto FULLFORM;
    }

    if(exprlen == 2 && priority > PRIO_TIMES)
      WRITE_CSTR("(?");
    else
      WRITE_CSTR("?");

    pmath_write(item, options, write, user);
    pmath_unref(item);

    if(exprlen == 2){
      WRITE_CSTR(":");

      item = pmath_expr_get_item(expr, 2);
      write_ex(item, options, PRIO_PLUS+1, write, user);
      pmath_unref(item);

      if(priority > PRIO_TIMES)
        WRITE_CSTR(")");
    }
  }
  else{ /*====================================================================*/
    pmath_t item;
    size_t i;
    
   FULLFORM:
   
    write_ex(head, options, PRIO_CALL, write, user);
    WRITE_CSTR("(");
    for(i = 1;i <= exprlen;i++){
      if(i > 1)
        WRITE_CSTR(", ");
      
      item = pmath_expr_get_item(expr, i);
      if(item || exprlen < 2)
        pmath_write(item, options, write, user);
      pmath_unref(item);
    }
    WRITE_CSTR(")");
  }

  pmath_unref(head);
}

static void write_expression(
  pmath_expr_t      expr,
  pmath_write_options_t   options,
  pmath_write_func_t      write,
  void                   *user
){
  write_expr_ex(expr, options, PRIO_ANY, write, user);
}

//}

/*============================================================================*/

PMATH_PRIVATE pmath_bool_t _pmath_expressions_init(void){
//  global_change_time = 0;
  
  memset(       expr_caches,    0, sizeof(expr_caches));
  memset((void*)expr_cache_pos, 0, sizeof(expr_cache_pos));
  
  #ifdef PMATH_DEBUG_LOG
    expr_cache_hits = expr_cache_misses = 0;
  #endif
  
  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_EXPRESSION_GENERAL,
    (pmath_compare_func_t)        _pmath_compare_exprsym,
    (pmath_hash_func_t)           hash_expression,
    (pmath_proc_t)                destroy_general_expression,
    (pmath_equal_func_t)          equal_expressions,
    (_pmath_object_write_func_t)  write_expression);

  _pmath_init_special_type(
    PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART,
    (pmath_compare_func_t)        _pmath_compare_exprsym,
    (pmath_hash_func_t)           hash_expression,
    (pmath_proc_t)                destroy_part_expression,
    (pmath_equal_func_t)          equal_expressions,
    (_pmath_object_write_func_t)  write_expression);

  return TRUE;
  
//  expr_cache_clear();
}

PMATH_PRIVATE void _pmath_expressions_done(void){
  #ifdef PMATH_DEBUG_LOG
  {
    pmath_debug_print("expr cache hit rate:             %f (%d of %d)\n", 
      (double)expr_cache_hits / (double)(expr_cache_hits + expr_cache_misses),
      (int) expr_cache_hits,
      (int)(expr_cache_hits + expr_cache_misses));
  }
  #endif
  
  expr_cache_clear();
//  pmath_debug_print("%"PRIdPTR" changes to expressions\n", global_change_time);
}
