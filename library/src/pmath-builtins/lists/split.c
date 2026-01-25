#include <pmath-core/numbers.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_True;

// list will be freed:
static pmath_t split_by(pmath_expr_t list, pmath_bool_t (*same_test)(pmath_t, pmath_t, void*), void *ctx);

static pmath_bool_t is_identical(pmath_t a, pmath_t b, void *_ignored) {
  return pmath_equals(a, b);
}
static pmath_bool_t call_same_test(pmath_t a, pmath_t b, void *_test_ptr) {
  pmath_t *test_ptr = _test_ptr;
  
  pmath_t test_result = pmath_evaluate(pmath_expr_new_extended(
                          pmath_ref(*test_ptr), 2,
                          pmath_ref(a),
                          pmath_ref(b)));
  pmath_unref(test_result);
  
  return pmath_same(test_result, pmath_System_True);
}

PMATH_PRIVATE pmath_t builtin_split(pmath_expr_t expr) {
  /* Split(list, test)
     Split(list)        == Split(expr, Identical)
   */
  size_t exprlen = pmath_expr_length(expr);
  pmath_t list, head, test;
  
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
  
  head = pmath_expr_get_item(list, 0);
  
  if(exprlen == 1) {
    pmath_unref(expr);
    list = split_by(list, is_identical, NULL);
  }
  else {
    test = pmath_expr_get_item(expr, 2);
    pmath_unref(expr);
    
    list = split_by(list, call_same_test, &test);
    pmath_unref(test);
  }
  
  list = pmath_expr_set_item(list, 0, head);
  return list;
}

static pmath_t split_by(pmath_expr_t list, pmath_bool_t (*same_test)(pmath_t, pmath_t, void*), void *ctx) {
  size_t len = pmath_expr_length(list);
  
  pmath_gather_begin(PMATH_NULL);
  if(len >= 1) {
    size_t start = 1;
    pmath_t a = pmath_expr_get_item(list, start);
    while(start <= len) {
      size_t next;
      for(next = start + 1; next <= len; ++next) {
        pmath_t b = pmath_expr_get_item(list, next);
        
        if(same_test(a, b, ctx)) {
          pmath_unref(a);
          a = b;
        }
        else {
          pmath_unref(a);
          a = b;
          break;
        }
      }
      
      pmath_emit(pmath_expr_get_item_range(list, start, next - start), PMATH_NULL);
      start = next;
    }
    pmath_unref(a);
  }
  
  pmath_unref(list);
  return pmath_gather_end();
}
