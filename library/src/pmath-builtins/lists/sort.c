#include <pmath-core/symbols.h>
#include <pmath-util/evaluation.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/expressions-private.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_FORCE_INLINE void insertion_sort(
  pmath_t *list, 
  size_t n,
  pmath_bool_t (*lessfn)(pmath_t,pmath_t,void*), // lesseqfn ?
  void *fndata
){
  size_t i, j;
  pmath_t key;
  
  --list; // index begins with 1
  
  for(j = 2;j <= n;++j){
    key = list[j];
    i = j - 1;
    while(i > 0 && lessfn(key, list[i], fndata))
      --i;
    list[i + 1] = key;
  }
}

PMATH_FORCE_INLINE pmath_t median_of_3(
  pmath_t a,
  pmath_t b,
  pmath_t c,
  pmath_bool_t (*lessfn)(pmath_t,pmath_t,void*), // lesseqfn ?
  void *fndata
){
  if(lessfn(a, b, fndata)){
    if(lessfn(b, c, fndata))
      return b;
    
    if(lessfn(a, c, fndata))
      return c;
    
    return a;
  }
  
  if(lessfn(a, c, fndata))
    return a;
  
  if(lessfn(b, c, fndata))
    return c;
  
  return b;
}

PMATH_FORCE_INLINE size_t partition(
  pmath_t *list, 
  size_t a, 
  size_t b,
  pmath_bool_t (*lessfn)(pmath_t,pmath_t,void*), // lesseqfn ?
  void *fndata
){
  size_t i, j;
  pmath_t pivot;
  
  assert(a < b);
  
  i = a;
  j = b - 1;
  pivot = median_of_3(list[a], list[b], list[a + b / 2], lessfn, fndata);
  do{
    while(i < b && lessfn(list[i], pivot, fndata))
      ++i;
    
    while(j > a && lessfn(pivot, list[j], fndata))
      --j;
    
    if(i < j){
      pmath_t tmp = list[i];
      list[i] = list[j];
      list[j] = tmp;
    }
  }while(i < j);
  
  if(lessfn(pivot, list[i], fndata)){
    pmath_t tmp = list[i];
    list[i] = list[b];
    list[b] = tmp;
  }
  
  return i;
}

#define INSERTION_SORT_MAX  16

/*static void quicksort(
  pmath_t *list, 
  size_t a, 
  size_t b,
  pmath_bool_t (*lessfn)(pmath_t,pmath_t,void*), // lesseqfn ?
  void *fndata
){
  while(b - a < INSERTION_SORT_MAX){
    size_t p = partition(list, a, b, lessfn, fndata);
    if(b - p > p - a){
      quicksort(list, a, p - 1, lessfn, fndata);
      a = p + 1; 
    }
    else{
      quicksort(list, p + 1, b, lessfn, fndata);
      b = p - 1; 
    }
  }
  
  if(a < b)
    insertion_sort(list + a, b - a + 1, lessfn, fndata);
}*/






// using nested functions where available (gcc)

/* profile results (vista, pentium dual core):

   gcc (-O3 -march=pentium-mmx):
    Timing(Sort(Array(10^6),Greater);)
    {57.487, ()}
   
   msvc (/Ox):
    Timing(Sort(Array(10^6),Greater);)
    {51.683, ()}
  
  Mircosoft seams to be better at optimizing than gcc and I.
 */
 
#ifndef __GNUC__
/* fallback: use thread local storage (slow) and a global function.
   
   Another solution to avoid thread local storage would be to reimplement
   qsort with an additional parameter. But I guess the standard qsort was
   optimized heavily.
 */
  static int user_cmp_objs(const void *a, const void *b){
    if(!pmath_equals(*(pmath_t*)a, *(pmath_t*)b)){
      int cmp;
      pmath_t less = pmath_evaluate(
        pmath_expr_new_extended(
          pmath_thread_local_load(PMATH_THREAD_KEY_SORTFN), 2,
          pmath_ref(*(pmath_t*)a),
          pmath_ref(*(pmath_t*)b)));

      pmath_unref(less);
      if(less == PMATH_SYMBOL_TRUE)
        return -1;
      if(less == PMATH_SYMBOL_FALSE)
        return 1;
      
      cmp = pmath_compare(*(pmath_t*)a, *(pmath_t*)b);
      if(cmp != 0)
        return cmp;
    }
    
    if((uintptr_t)a < (uintptr_t)b) return -1;
    if((uintptr_t)a > (uintptr_t)b) return +1;
    return 0;
  }
#endif

PMATH_PRIVATE pmath_t builtin_sort(pmath_expr_t expr){
/* Sort(expr)
   Sort(expr, lessfn)
 */
  pmath_expr_t list;
  size_t exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 2){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }

  list = (pmath_expr_t)pmath_expr_get_item(expr, 1);
  if(!pmath_instance_of(list, PMATH_TYPE_EXPRESSION)){
    pmath_message(NULL, "noexpr", 1, list);
    return expr;
  }

  if(exprlen == 2){
    #ifdef __GNUC__
      pmath_t fn = pmath_expr_get_item(expr, 2);
      
      int user_cmp_objs(const void *a, const void *b){ // gcc extension: inner function
        if(!pmath_equals(*(pmath_t*)a, *(pmath_t*)b)){
          int cmp;
          pmath_t less = pmath_evaluate(
            pmath_expr_new_extended(
              pmath_ref(fn), 2,
              pmath_ref(*(pmath_t*)a),
              pmath_ref(*(pmath_t*)b)));

          pmath_unref(less);
          if(less == PMATH_SYMBOL_TRUE)
            return -1;
          if(less == PMATH_SYMBOL_FALSE)
            return 1;
          
          cmp = pmath_compare(*(pmath_t*)a, *(pmath_t*)b);
          if(cmp != 0)
            return cmp;
        }
        
        if((uintptr_t)a < (uintptr_t)b) return -1;
        if((uintptr_t)a > (uintptr_t)b) return +1;
        return 0;
      }
      
      pmath_t result = _pmath_expr_sort_ex(list, user_cmp_objs);

      pmath_unref(expr);
      pmath_unref(fn);
      return result;
    #else
      pmath_t old_sortfn = pmath_thread_local_save(
        PMATH_THREAD_KEY_SORTFN,
        pmath_expr_get_item(expr, 2));

      pmath_t result = _pmath_expr_sort_ex(list, user_cmp_objs);

      pmath_unref(expr);
      pmath_unref(pmath_thread_local_save(
        PMATH_THREAD_KEY_SORTFN,
        old_sortfn));
      return result;
    #endif
  }

  pmath_unref(expr);
  return pmath_expr_sort(list);
}

static int sortby_cmp(const void *a, const void *b){
/* (*a) and (*b) are expressions of the form fn(x)(x) with evaluated fn(x)
   compare fn(x)... first, if it equals, compare x
 */
  pmath_t objA, objB;
  int cmp;
  
  assert(*(pmath_t*)a == NULL || pmath_instance_of(*(pmath_t*)a, PMATH_TYPE_EXPRESSION));
  assert(*(pmath_t*)b == NULL || pmath_instance_of(*(pmath_t*)b, PMATH_TYPE_EXPRESSION));

  objA = pmath_expr_get_item(*(pmath_expr_t*)a, 0);
  objB = pmath_expr_get_item(*(pmath_expr_t*)b, 0);
  cmp = pmath_compare(objA, objB);
  pmath_unref(objA);
  pmath_unref(objB);

  if(cmp != 0)
    return cmp;

  objA = pmath_expr_get_item(*(pmath_expr_t*)a, 1);
  objB = pmath_expr_get_item(*(pmath_expr_t*)b, 1);
  cmp = pmath_compare(objA, objB);
  pmath_unref(objA);
  pmath_unref(objB);

  return cmp;
}

PMATH_PRIVATE pmath_t builtin_sortby(pmath_expr_t expr){
/* SortBy(expr, fn)
 */
  pmath_expr_t list;
  pmath_t fn;
  size_t i, len;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  list = (pmath_expr_t)pmath_expr_get_item(expr, 1);
  if(!pmath_instance_of(list, PMATH_TYPE_EXPRESSION)){
    pmath_message(NULL, "noexpr", 1, list);
    return expr;
  }
  
  len = pmath_expr_length((pmath_expr_t)list);

  fn = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  // change list from {a,b,...} to {fn(a)(a), fn(b)(b), ...} with fn(...) evaluated
  for(i = 1;i <= len;++i){
    pmath_t item = pmath_expr_get_item(list, i);
    list = pmath_expr_set_item(
      list, i,
      pmath_expr_new_extended(
        pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(fn), 1,
            pmath_ref(item))), 1,
        pmath_ref(item)));
    pmath_unref(item);
  }

  pmath_unref(fn);
  list = _pmath_expr_sort_ex(list, sortby_cmp);

  for(i = 1;i <= len;++i){
    pmath_expr_t item = (pmath_expr_t)pmath_expr_get_item(list, i);
    assert(!item || pmath_instance_of(item, PMATH_TYPE_EXPRESSION));

    list = pmath_expr_set_item(list, i, pmath_expr_get_item(item, 1));
    pmath_unref(item);
  }

  return list;
}
