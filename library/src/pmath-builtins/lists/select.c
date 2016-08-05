#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/packed-arrays.h>

#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>

#include <string.h>


#ifdef MIN
#  undef MIN
#endif

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))


static pmath_bool_t test_at_0(
  pmath_t  expr,
  size_t   index_0,
  void    *test_context
) {
  pmath_t *crit = test_context;
  pmath_t item = pmath_expr_get_item(expr, index_0 + 1);
  
  item = pmath_expr_new_extended(pmath_ref(*crit), 1, item);
  item = pmath_evaluate(item);
  pmath_unref(item);
  
  return pmath_same(item, PMATH_SYMBOL_TRUE);
}

// all indices are 0-based
static size_t test_all_slow(
  pmath_expr_t   expr,                  // wont be freed
  size_t        *first_success, 
  size_t        *success_list_length, 
  uint8_t      **success_list,          // boolean[success_list_length] or NULL on return, to be freed with pmath_mem_free()
  void          *test_context,
  size_t         max_successes
) {
  size_t first, next, gap, last, count;
  size_t length = pmath_expr_length(expr);
  uint8_t *bool_array;
  
  assert(first_success != NULL);
  assert(success_list_length != NULL);
  assert(success_list != NULL);
  assert(*success_list == NULL);
  
  first = 0;
  while(first < length && !test_at_0(expr, first, test_context))
    ++first;
  
  if(first == length) {
    *first_success = first;
    *success_list_length = 0;
    *success_list = NULL;
    return 0;
  }
  
  if(max_successes > length - first)
    max_successes = length - first;
  
  next = first + 1;
  while(next < length && max_successes > 0 && test_at_0(expr, next, test_context)) {
    ++next;
    --max_successes;
  }
  
  if(next == length || max_successes == 0) { // all in [first, next), none before, no more needed
    *first_success = first;
    *success_list_length = next - first;
    *success_list = NULL;
    return next - first;
  }
  
  gap = next + 1;
  while(gap < length && !test_at_0(expr, gap, test_context)) 
    ++gap;
  
  if(gap == length) { // all in [first, next), none outside
    *first_success = first;
    *success_list_length = next - first;
    *success_list = NULL;
    return next - first;
  }
  
  bool_array = pmath_mem_alloc(max_successes);
  *success_list = bool_array;
  if(!bool_array) {
    *first_success = first;
    *success_list_length = next - first;
    return 0;
  }
  
  count = next - first;
  memset(bool_array, 1, count);
  memset(bool_array + next - first, 0, max_successes - count);
  
  bool_array[gap - first] = 1;
  ++count;
  
  last = gap;
  for(++gap; gap < length; ++gap) {
    if(test_at_0(expr, gap, test_context)) {
      last = gap;
      ++count;
      bool_array[gap - first] = 1;
    }
  }
  
  *first_success = first;
  *success_list_length = last + 1 - first;
  return count;
}

PMATH_PRIVATE
pmath_expr_t _pmath_expr_create_similar(pmath_t expr, size_t length) {
  if(length == 0)
    return pmath_expr_new(pmath_expr_get_item(expr, 0), 0);
    
  if(pmath_is_packed_array(expr)) {
    pmath_packed_array_t result;
    
    const size_t *old_sizes = pmath_packed_array_get_sizes(expr);
    const size_t *old_steps = pmath_packed_array_get_steps(expr);
    
    size_t buffer[6];
    size_t *sizes;
    size_t *steps;
    
    size_t dims = pmath_packed_array_get_dimensions(expr);
    if(dims == 0)
        return PMATH_NULL;
      
    if(2 * dims < sizeof(buffer) / sizeof(buffer[0])) {
      sizes = buffer;
      steps = sizes + dims;
    }
    else {
      sizes = pmath_mem_alloc(2 * dims * sizeof(size_t));
      if(!sizes)
        return PMATH_NULL;
        
      steps = sizes + dims;
    }
    
    memcpy(sizes, old_sizes, dims * sizeof(size_t));
    memcpy(steps, old_steps, dims * sizeof(size_t));
    
    sizes[0] = length;
    
    result = pmath_packed_array_new(
      PMATH_NULL, 
      pmath_packed_array_get_element_type(expr),
      dims,
      sizes,
      steps,
      0);
    
    if(sizes != buffer)
      pmath_mem_free(sizes);
    
    return result;
  }
  
  return pmath_expr_new(pmath_expr_get_item(expr, 0), length);
}

// all indices are 0-based
static pmath_expr_t pick_slow(
  pmath_expr_t  expr, // wont be freed
  size_t        first_success,
  size_t        num_successes,
  size_t        success_list_length,
  uint8_t      *successes // wont be freed, can be NULL
) {
  pmath_t result;
  size_t i,j;
  
  if(!successes)
    return pmath_expr_get_item_range(expr, first_success + 1, success_list_length);
  
  result = _pmath_expr_create_similar(expr, num_successes);
  j = 1;
  for(i = 0;i < success_list_length; ++i) {
    if(successes[i]) {
      result = pmath_expr_set_item(result, j, pmath_expr_get_item(expr, first_success + i + 1));
      ++j;
    }
  }
  
  assert(num_successes == j - 1);
  
  return result;
}

static pmath_t select_slow(
  pmath_expr_t   expr, // will be freed
  void          *test_context,
  size_t         max
) {
  size_t first_success;
  size_t success_list_length;
  uint8_t *success_list = NULL;  
  size_t count;
  pmath_t result;
  
  count = test_all_slow(expr, &first_success, &success_list_length, &success_list, test_context, max);
  
  result = pick_slow(expr, first_success, count, success_list_length, success_list);
  pmath_unref(expr);
  
  pmath_mem_free(success_list);
  return result;
}

static pmath_bool_t eval_test(pmath_t item, void *context) {
  pmath_t *crit = context;
  
  item = pmath_expr_new_extended(pmath_ref(*crit), 1, pmath_ref(item));
  item = pmath_evaluate(item);
  pmath_unref(item);
  
  return pmath_same(item, PMATH_SYMBOL_TRUE);
}

PMATH_PRIVATE pmath_t builtin_select(pmath_expr_t expr) {
  /* Select(list, crit, n)
     Select(list, crit)    = Select(list, crit, Infinity)
  
     messages:
       General::innf
       General::nexprat
   */
  pmath_t list, crit;
  size_t exprlen, count;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2 || exprlen > 3) {
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)) {
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  count = SIZE_MAX;
  if(exprlen == 3) {
    pmath_t n = pmath_expr_get_item(expr, 3);
    
    if(pmath_is_int32(n) && PMATH_AS_INT32(n) >= 0) {
      count = (unsigned)PMATH_AS_INT32(n);
    }
    else if( !pmath_equals(n, _pmath_object_pos_infinity) &&
             !pmath_is_set_of_options(n))
    {
      pmath_unref(n);
      pmath_unref(list);
      pmath_message(PMATH_NULL, "innf", 2, PMATH_FROM_INT32(4), pmath_ref(expr));
      return expr;
    }
    
    pmath_unref(n);
  }
  
  crit = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  list = select_slow(list, &crit, count);
  
  pmath_unref(crit);
  return list;
}
