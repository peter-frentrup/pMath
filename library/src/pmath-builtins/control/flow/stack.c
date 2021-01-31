#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

struct stack_to_list_t {
  size_t current;
  pmath_expr_t list;
  pmath_bool_t first;
  
  pmath_t head_key;
  pmath_t location_key;
} _stack_to_list_t;

extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Rule;

static pmath_bool_t walk_stack_count(pmath_t head, void *p) {
  struct stack_to_list_t *data = (struct stack_to_list_t*)p;
  
  data->current++;
  return TRUE;
}

static pmath_bool_t walk_stack_store(pmath_t head, pmath_t debug_info, void *p) {
  struct stack_to_list_t *data = (struct stack_to_list_t*)p;
  
  if(!data->first) {
    pmath_t entry = pmath_expr_new_extended(
                      pmath_ref(pmath_System_List), 1,
                      pmath_expr_new_extended(
                        pmath_ref(pmath_System_Rule), 2,
                        pmath_ref(data->head_key),
                        pmath_ref(head)));
                        
    if(!pmath_is_null(debug_info)) {
      entry = pmath_expr_append(
                entry, 1,
                pmath_expr_new_extended(
                  pmath_ref(pmath_System_Rule), 2,
                  pmath_ref(data->location_key),
                  pmath_ref(debug_info)));
    }
    
    data->list = pmath_expr_set_item(
                   data->list,
                   data->current--,
                   entry);
  }
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
  data.head_key     = PMATH_C_STRING("Head");
  data.location_key = PMATH_C_STRING("Location");
  
  pmath_walk_stack(walk_stack_count, &data);
  
  data.current--;
  data.list = pmath_expr_new(pmath_ref(pmath_System_List), data.current);
  pmath_walk_stack_2(walk_stack_store, &data);
  
  pmath_unref(data.head_key);
  pmath_unref(data.location_key);
  
  return data.list;
}
