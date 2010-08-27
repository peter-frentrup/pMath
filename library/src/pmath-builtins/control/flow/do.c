#include <pmath-core/numbers.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/control/flow-private.h>
#include <pmath-builtins/control-private.h>

PMATH_PRIVATE void _pmath_iterate(
  pmath_t             iter, // will be freed
  void              (*init)(size_t,pmath_symbol_t,void*),
  pmath_bool_t      (*next)(void*),
  void               *data
){
  pmath_thread_t thread = pmath_thread_get_current();
  size_t count;

  if(_pmath_is_rule(iter)){
    pmath_t            start = NULL;
    pmath_t            delta = NULL;
    pmath_symbol_t            sym;
    pmath_expr_t        range;
    pmath_symbol_attributes_t old_attr;
    pmath_t            old_value;

    sym = pmath_expr_get_item(iter, 1);

    if(!pmath_instance_of(sym, PMATH_TYPE_SYMBOL)){
      pmath_unref(sym);
      pmath_message(NULL, "iter", 1, iter);
      return;
    }

    range = pmath_evaluate(pmath_expr_get_item(iter, 2));

    if(pmath_instance_of(range, PMATH_TYPE_EXPRESSION)){
      pmath_t head = pmath_expr_get_item(range, 0);
      pmath_unref(head);
      
      count = pmath_expr_length(range);
      
      if(head == PMATH_SYMBOL_LIST){
        size_t i;

        pmath_unref(iter);

        init(count, sym, data);
        old_attr = pmath_symbol_get_attributes(sym);
        pmath_symbol_set_attributes(sym, 0);
        old_value = pmath_symbol_get_value(sym);

        for(i = 1;i <= count && !pmath_thread_aborting(thread);++i){
          pmath_symbol_set_value(sym, pmath_expr_get_item(range, i));

          if(!next(data)){
            pmath_unref(range);
            pmath_symbol_set_value(sym, old_value);
            pmath_symbol_set_attributes(sym, old_attr);
            pmath_unref(sym);
            return;
          }
        }

        pmath_unref(range);
        pmath_symbol_set_value(sym, old_value);
        pmath_symbol_set_attributes(sym, old_attr);
        pmath_unref(sym);
        return;
      }
    }

    if(!extract_delta_range(range, &start, &delta, &count)){
      pmath_unref(range);
      pmath_unref(start);
      pmath_unref(delta);
      pmath_unref(sym);
      pmath_message(NULL, "iterb", 1, iter);
      return;
    }

    pmath_unref(range);
    
    init(count, sym, data);
    ++count;

    old_attr = pmath_symbol_get_attributes(sym);
    pmath_symbol_set_attributes(sym, 0);
    old_value = pmath_symbol_get_value(sym);
    while(--count > 0 && !pmath_thread_aborting(thread)){
      pmath_symbol_set_value(sym, pmath_ref(start));

      if(!next(data)){
        pmath_unref(iter);
        pmath_unref(start);
        pmath_unref(delta);
        pmath_symbol_set_value(sym, old_value);
        pmath_symbol_set_attributes(sym, old_attr);
        pmath_unref(sym);
        return;
      }

      start = pmath_evaluate(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PLUS), 2,
          start,
          pmath_ref(delta)));
    }

    pmath_unref(iter);
    pmath_unref(start);
    pmath_unref(delta);
    pmath_symbol_set_value(sym, old_value);
    pmath_symbol_set_attributes(sym, old_attr);
    pmath_unref(sym);
    return;
  }

  iter = pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_FLOOR), 1,
      iter));

  if(!pmath_instance_of(iter, PMATH_TYPE_INTEGER)
  || !pmath_integer_fits_ui((pmath_integer_t)iter)){
    pmath_message(NULL, "iterb", 1, iter);
    return;
  }

  count = pmath_integer_get_ui((pmath_integer_t)iter);

  pmath_unref(iter);
  init(count, NULL, data);
  ++count;

  while(--count > 0 && !pmath_thread_aborting(thread)){
    if(!next(data))
      return;
  }

  return;
}

struct iterate_do_data_t{
  pmath_t  body;
  pmath_bool_t started;
  pmath_t  result;
};

static void init_do(size_t count, pmath_symbol_t sym, struct iterate_do_data_t *data){
  data->started = TRUE;
}

static pmath_bool_t do_next(struct iterate_do_data_t *data){
  pmath_unref(data->result);
  data->result = pmath_ref(data->body);
  return !_pmath_run(&data->result);
}

PMATH_PRIVATE pmath_t builtin_do(pmath_expr_t expr){
/* Do(body, n)
   Do(body, i->n)              = Do(body, i->1..n..1)
   Do(body, i->a..b..d)
   Do(body, i->{a1, a2, ...})

   messages:
     General::iter
     General::iterb
 */
  pmath_t            iter;
  struct iterate_do_data_t  data;

  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  iter = pmath_expr_get_item(expr, 2);

  data.body = pmath_expr_get_item(expr, 1);
  data.started = FALSE;
  data.result = NULL;

  _pmath_iterate(
    iter,
    (void(*)(size_t,pmath_symbol_t,void*))    init_do,
    (pmath_bool_t(*)(void*))               do_next,
    &data);

  pmath_unref(data.body);
  if(!data.started)
    return expr;

  pmath_unref(expr);
  
  if(pmath_is_expr_of(data.result, PMATH_SYMBOL_RETURN)){
    size_t len = pmath_expr_length(data.result);
    
    if(len == 0){
      pmath_unref(data.result);
      return NULL;
    }
    
    if(len == 1){
      expr = pmath_expr_get_item(data.result, 1);
      pmath_unref(data.result);
      return expr;
    }
  }
  
  return data.result;
}
