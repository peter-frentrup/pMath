#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/control/flow-private.h>
#include <pmath-builtins/lists-private.h>


struct sum_prod_data_t{
  pmath_bool_t is_valid;
  pmath_t result;
  
  pmath_t body;
  pmath_t func;
};

static void init(size_t count, pmath_symbol_t sym, void *p){
  struct sum_prod_data_t *data = (struct sum_prod_data_t*)p;
  
  data->is_valid = TRUE;
}

static pmath_bool_t next(void *p){
  struct sum_prod_data_t *data = (struct sum_prod_data_t*)p;
  
  data->result = pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(data->func), 2,
      data->result,
      pmath_ref(data->body)));
  
  return data->result != PMATH_SYMBOL_UNDEFINED;
}


pmath_t builtin_sum(pmath_expr_t expr){
  struct sum_prod_data_t data;
  pmath_t iter;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  iter = pmath_expr_get_item(expr, 2);
  if(_pmath_is_rule(iter)){
    pmath_t range = pmath_expr_get_item(iter, 2);
    pmath_t start = NULL;
    pmath_t delta = NULL;
    size_t  count;
    
    if(!extract_delta_range(range, &start, &delta, &count)){
      pmath_unref(start);
      pmath_unref(delta);
      pmath_unref(range);
      pmath_unref(iter);
      return expr;
    }
    
    pmath_unref(start);
    pmath_unref(delta);
    pmath_unref(range);
  }
  
  data.is_valid = FALSE;
  data.result   = pmath_integer_new_si(0);
  data.body     = pmath_expr_get_item(expr, 1);
  data.func     = PMATH_SYMBOL_PLUS;
  
  _pmath_iterate(iter, init, next, &data);
  
  pmath_unref(data.body);
  if(data.is_valid){
    pmath_unref(expr);
    return data.result;
  }
  
  pmath_unref(data.result);
  return expr;
}

pmath_t builtin_product(pmath_expr_t expr){
  struct sum_prod_data_t data;
  pmath_t iter;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  iter = pmath_expr_get_item(expr, 2);
  if(_pmath_is_rule(iter)){
    pmath_t range = pmath_expr_get_item(iter, 2);
    pmath_t start = NULL;
    pmath_t delta = NULL;
    size_t  count;
    
    if(!extract_delta_range(range, &start, &delta, &count)){
      pmath_unref(start);
      pmath_unref(delta);
      pmath_unref(range);
      pmath_unref(iter);
      return expr;
    }
    
    pmath_unref(start);
    pmath_unref(delta);
    pmath_unref(range);
  }
  
  data.is_valid = FALSE;
  data.result   = pmath_integer_new_si(1);
  data.body     = pmath_expr_get_item(expr, 1);
  data.func     = PMATH_SYMBOL_TIMES;
  
  _pmath_iterate(iter, init, next, &data);
  
  pmath_unref(data.body);
  if(data.is_valid){
    pmath_unref(expr);
    return data.result;
  }
  
  pmath_unref(data.result);
  return expr;
}
