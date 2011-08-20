#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

struct stack_to_list_t {
  size_t current;
  pmath_expr_t list;
  pmath_bool_t first;
} _stack_to_list_t;

static pmath_bool_t walk_stack_count(pmath_t head, void *p) {
  struct stack_to_list_t *data = (struct stack_to_list_t*)p;
  
  data->current++;
  return TRUE;
}

static pmath_bool_t walk_stack_store(pmath_t head, void *p) {
  struct stack_to_list_t *data = (struct stack_to_list_t*)p;
  
  if(!data->first)
    data->list = pmath_expr_set_item(
                   data->list,
                   data->current--,
                   pmath_ref(head));
  else
    data->first = FALSE;
  return data->current > 0;
}

PMATH_PRIVATE pmath_t builtin_stack(pmath_expr_t expr) {
  struct stack_to_list_t data;
  
  if(pmath_expr_length(expr) != 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  pmath_unref(expr);
  
  data.current = 0;
  data.list = PMATH_NULL;
  data.first = TRUE;
  pmath_walk_stack(walk_stack_count, &data);
  
  data.current--;
  data.list = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), data.current);
  pmath_walk_stack(walk_stack_store, &data);
  
  return data.list;
}
