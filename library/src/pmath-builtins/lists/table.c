#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/flow-private.h>


typedef struct{
  pmath_t      body;
  pmath_bool_t     started;
  pmath_expr_t  result;
  size_t              i;
}iterate_table_data_t;

static void init_table(size_t count, pmath_symbol_t sym, iterate_table_data_t *data){
  data->started = TRUE;
  data->result = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), count);
}

static pmath_bool_t table_next(iterate_table_data_t *data){
  data->result = pmath_expr_set_item(
    data->result,
    ++(data->i),
    pmath_evaluate(pmath_ref(data->body)));
  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_table(pmath_expr_t expr){
/* Table(body, n)
   Table(body, i->n)
   Table(body, i->a..b..d)
   Table(body, i->{a1, a2, ...})

   messages:
     General::iter
     General::iterb
 */
  iterate_table_data_t  data;
  pmath_t        iter;

  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  iter = pmath_expr_get_item(expr, 2);

  data.body = pmath_expr_get_item(expr, 1);
  data.started = FALSE;
  data.result = NULL;
  data.i = 0;

  _pmath_iterate(
    iter,
    (void(*)(size_t,pmath_symbol_t,void*)) init_table,
    (pmath_bool_t(*)(void*))               table_next,
    &data);

  pmath_unref(data.body);
  if(!data.started)
    return expr;

  pmath_unref(expr);
  return data.result;
}
