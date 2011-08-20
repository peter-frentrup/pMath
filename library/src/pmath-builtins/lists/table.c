#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/flow-private.h>


struct iterate_table_data_t {
  pmath_t      body;
  pmath_bool_t started;
  pmath_expr_t result;
  size_t       i;
};

static void init_table(size_t count, pmath_symbol_t sym, void *p) {
  struct iterate_table_data_t *data = p;
  data->started = TRUE;
  data->result = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), count);
}

static pmath_bool_t table_next(void *p) {
  struct iterate_table_data_t *data = p;
  data->result = pmath_expr_set_item(
                   data->result,
                   ++(data->i),
                   pmath_evaluate(pmath_ref(data->body)));
  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_table(pmath_expr_t expr) {
  /* Table(body, n)
     Table(body, i->n)
     Table(body, i->a..b..d)
     Table(body, i->{a1, a2, ...})
     Table(body, iter1, iter2, ...) = Table(Table(Table(body, ...), iter2), iter1)
  
     messages:
       General::iter
       General::iterb
   */
  struct iterate_table_data_t data;
  pmath_t                     iter;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2) {
    pmath_message_argxxx(exprlen, 2, SIZE_MAX);
    return expr;
  }
  
  iter = pmath_expr_get_item(expr, 2);
  
  if(exprlen > 2) {
    size_t i;
    data.body = pmath_ref(expr);
    for(i = 2; i < exprlen; ++i) {
      data.body = pmath_expr_set_item(
                    data.body, i,
                    pmath_expr_get_item(data.body, i + 1));
    }
    data.body = pmath_expr_resize(data.body, exprlen - 1);
  }
  else
    data.body = pmath_expr_get_item(expr, 1);
    
  data.started = FALSE;
  data.result = PMATH_NULL;
  data.i = 0;
  
  _pmath_iterate(iter, init_table, table_next, &data);
  
  pmath_unref(data.body);
  if(!data.started)
    return expr;
    
  pmath_unref(expr);
  return data.result;
}
