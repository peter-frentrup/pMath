#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>

#include <string.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-private.h>
#include <pmath-core/expressions-private.h>

#include <pmath-builtins/all-symbols-private.h>


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

static int ordering_default_cmp(pmath_t ctx, pmath_t a, pmath_t b) {
  return pmath_compare(a, b);
}

static int ordering_user_cmp(pmath_t ctx, pmath_t a, pmath_t b) {
  if(!pmath_equals(a, b)) {
    pmath_t less = pmath_evaluate(
                     pmath_expr_new_extended(
                       pmath_ref(ctx), 2,
                       pmath_ref(a),
                       pmath_ref(b)));
                       
    pmath_unref(less);
    if(pmath_same(less, PMATH_SYMBOL_TRUE))
      return -1;
    if(pmath_same(less, PMATH_SYMBOL_FALSE))
      return 1;
      
    return pmath_compare(a, b);
  }
  
  return 0;
}

struct ordering_context_t {
  pmath_expr_t list;
  int (*cmp)(pmath_t ctx, pmath_t a, pmath_t b);
  pmath_t cmp_ctx;
};

static int ordering_cmp(void *p, const pmath_t *a, const pmath_t *b) {
  struct ordering_context_t *context = (struct ordering_context_t*)p;
  int result;
  pmath_t a_item, b_item;
  
  uintptr_t ia = pmath_integer_get_uiptr(*a);
  uintptr_t ib = pmath_integer_get_uiptr(*b);
  
  a_item = pmath_expr_get_item(context->list, ia);
  b_item = pmath_expr_get_item(context->list, ib);
  
  result = context->cmp(context->cmp_ctx, a_item, b_item);
  
  pmath_unref(a_item);
  pmath_unref(b_item);
  if(result != 0)
    return result;
    
  if((uintptr_t)a < (uintptr_t)b) return -1;
  if((uintptr_t)a > (uintptr_t)b) return +1;
  return 0;
}

PMATH_PRIVATE pmath_t builtin_ordering(pmath_expr_t expr) {
  /* Ordering(expr)
     Ordering(expr, n)
     Ordering(expr, n, lessfn)
   */
  
  pmath_expr_t indices;
  size_t i, len;
  struct ordering_context_t context;
  
  size_t exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 3) {
    pmath_message_argxxx(exprlen, 1, 3);
    return expr;
  }
  
  context.list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(context.list)) {
    pmath_unref(context.list);
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  len = pmath_expr_length(context.list);
  
  indices = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), len);
  for(i = len; i > 0; --i) {
    indices = pmath_expr_set_item(indices, i, pmath_integer_new_uiptr(i));
  }
  
  if(exprlen == 3) {
    context.cmp_ctx = pmath_expr_get_item(expr, 3);
    context.cmp     = ordering_user_cmp;
  }
  else {
    context.cmp_ctx = PMATH_NULL;
    context.cmp     = ordering_default_cmp;
  }
  
  indices = _pmath_expr_sort_ex_context(indices, ordering_cmp, &context);
  
  pmath_unref(context.cmp_ctx);
  pmath_unref(context.list);
  
  if(exprlen >= 2) {
    pmath_t take = pmath_expr_new_extended(
                     pmath_ref(PMATH_SYMBOL_TAKE), 2,
                     indices,
                     pmath_expr_get_item(expr, 2));
                     
    take = pmath_evaluate(take);
    if(!pmath_is_expr_of(take, PMATH_SYMBOL_LIST)) {
      pmath_unref(take);
      return expr;
    }
    
    pmath_unref(expr);
    return take;
  }
  
  pmath_unref(expr);
  return indices;
}

struct sort_context_t {
  pmath_t cmp;
};

static int user_cmp_objs(void *p, const pmath_t *a, const pmath_t *b) {
  struct sort_context_t *context = (struct sort_context_t*)p;
  
  if(!pmath_equals(*a, *b)) {
    int cmp;
    pmath_t less = pmath_evaluate(
                     pmath_expr_new_extended(
                       pmath_ref(context->cmp), 2,
                       pmath_ref(*a),
                       pmath_ref(*b)));
                       
    pmath_unref(less);
    if(pmath_same(less, PMATH_SYMBOL_TRUE))
      return -1;
    if(pmath_same(less, PMATH_SYMBOL_FALSE))
      return 1;
      
    cmp = pmath_compare(*a, *b);
    if(cmp != 0)
      return cmp;
  }
  
  if((uintptr_t)a < (uintptr_t)b) return -1;
  if((uintptr_t)a > (uintptr_t)b) return +1;
  return 0;
}

PMATH_PRIVATE pmath_t builtin_sort(pmath_expr_t expr) {
  /* Sort(expr)
     Sort(expr, lessfn)
   */
  pmath_expr_t list;
  size_t exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)) {
    pmath_unref(list);
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  if(exprlen == 2) {
    struct sort_context_t context;
    context.cmp = pmath_expr_get_item(expr, 2);
    
    pmath_unref(expr);
    expr = _pmath_expr_sort_ex_context(list, user_cmp_objs, &context);
    
    pmath_unref(context.cmp);
    return expr;
  }
  
  pmath_unref(expr);
  return pmath_expr_sort(list);
}

static int sortby_cmp(const pmath_t *a, const pmath_t *b) {
  /* (*a) and (*b) are expressions of the form fn(x)(x) with evaluated fn(x)
     compare fn(x)... first, if it equals, compare x
   */
  pmath_t objA, objB;
  int cmp;
  
  assert(pmath_is_null(*a) || pmath_is_expr(*a));
  assert(pmath_is_null(*b) || pmath_is_expr(*b));
  
  objA = pmath_expr_get_item(*a, 0);
  objB = pmath_expr_get_item(*b, 0);
  cmp = pmath_compare(objA, objB);
  pmath_unref(objA);
  pmath_unref(objB);
  
  if(cmp != 0)
    return cmp;
    
  objA = pmath_expr_get_item(*a, 1);
  objB = pmath_expr_get_item(*b, 1);
  cmp = pmath_compare(objA, objB);
  pmath_unref(objA);
  pmath_unref(objB);
  
  return cmp;
}

PMATH_PRIVATE pmath_t builtin_sortby(pmath_expr_t expr) {
  /* SortBy(expr, fn)
   */
  pmath_expr_t list;
  pmath_t fn;
  size_t i, len;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)) {
    pmath_unref(list);
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  len = pmath_expr_length(list);
  
  fn = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  // change list from {a,b,...} to {fn(a)(a), fn(b)(b), ...} with fn(...) evaluated
  for(i = 1; i <= len; ++i) {
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
  
  for(i = 1; i <= len; ++i) {
    pmath_expr_t item = pmath_expr_get_item(list, i);
    assert(pmath_is_null(item) || pmath_is_expr(item));
    
    list = pmath_expr_set_item(list, i, pmath_expr_get_item(item, 1));
    pmath_unref(item);
  }
  
  return list;
}
