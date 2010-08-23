#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>
#include <pmath-util/evaluation.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-util/concurrency/threads.h>

#include <pmath-core/numbers-private.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols-private.h>

//PMATH_PRIVATE pmath_t builtin_rangearray(pmath_expr_t expr);

PMATH_PRIVATE 
pmath_symbol_t _pmath_topmost_symbol(pmath_t obj){ // obj wont be freed
  if(pmath_instance_of(obj, PMATH_TYPE_SYMBOL))
    return pmath_ref(obj);
  
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    pmath_t head = pmath_expr_get_item(obj, 0);
    pmath_t result = _pmath_topmost_symbol(head);
    pmath_unref(head);
    return result;
  }
  
  return NULL;
}

struct _array_data_t{
  pmath_t start;
  pmath_t dims;
  pmath_t index;
  pmath_t function;
  pmath_t head;
  
  size_t dim;
  size_t depth;
  pmath_bool_t start_is_list;
};

static pmath_t array(struct _array_data_t *data){
  pmath_expr_t list;
  pmath_t obj;
  size_t i, len;
  
  obj = pmath_expr_get_item(data->dims, data->dim);
  len = pmath_integer_get_ui(obj);
  pmath_unref(obj);
  
  list = pmath_expr_new(pmath_ref(data->head), len);
  
  if(data->dim >= data->depth){
    if(data->start_is_list){
      for(i = 1;i <= len && !pmath_aborting();++i){
        data->index = pmath_expr_set_item(
          data->index, data->dim, 
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PLUS), 2,
            pmath_expr_get_item(data->start, data->dim),
            pmath_integer_new_ui(i - 1)));
        
        list = pmath_expr_set_item(list, i, 
          pmath_expr_set_item(
            pmath_ref(data->index), 0, 
            pmath_ref(data->function)));
      }
    }
    else{
      for(i = 1;i <= len && !pmath_aborting();++i){
        data->index = pmath_expr_set_item(
          data->index, data->dim, 
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PLUS), 2,
            pmath_ref(data->start),
            pmath_integer_new_ui(i - 1)));
        
        list = pmath_expr_set_item(list, i, 
          pmath_expr_set_item(
            pmath_ref(data->index), 0, 
            pmath_ref(data->function)));
      }
    }
    
    return list;
  }
  
  data->dim++;
  if(data->start_is_list){
    for(i = 1;i <= len && !pmath_aborting();++i){
      data->index = pmath_expr_set_item(
        data->index, data->dim-1, 
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PLUS), 2,
          pmath_expr_get_item(data->start, data->dim-1),
          pmath_integer_new_ui(i - 1)));
          
      list = pmath_expr_set_item(list, i, array(data));
    }
  }
  else{
    for(i = 1;i <= len && !pmath_aborting();++i){
      data->index = pmath_expr_set_item(
        data->index, data->dim-1, 
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PLUS), 2,
          pmath_ref(data->start),
          pmath_integer_new_ui(i - 1)));
          
      list = pmath_expr_set_item(list, i, array(data));
    }
  }
  
  return list;
}

PMATH_PRIVATE pmath_t builtin_array(pmath_expr_t expr){
/* Array(f, n)
   Array(f, {n1, n2, ...})
   Array(f, {n1, n2, ...}, {s1, s2, ...})
   Array(f, dims, origs, h)
   
   Array(s..e..d)
   Array(s..e)
   Array(e)
   
   messages:
     General::range
 */
  struct _array_data_t data;
  size_t exprlen;

  exprlen = pmath_expr_length(expr);
  if(exprlen == 1){
    pmath_t range = pmath_expr_get_item(expr, 1);
    pmath_t delta;
    
    if(!extract_delta_range(range, &data.start, &delta, &data.depth)){
      pmath_unref(data.start);
      pmath_unref(delta);
      pmath_message(NULL, "range", 1, range);
      return expr;
    }
    
    pmath_unref(range);
    pmath_unref(expr);
    expr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), data.depth);
    if(expr && data.depth > 0){
      expr = pmath_expr_set_item(expr, 1, pmath_ref(data.start));
      
      for(data.dim = 2;data.dim <= data.depth && !pmath_aborting();data.dim++){
        data.start = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PLUS), 2,
            data.start,
            pmath_ref(delta)));
        
        expr = pmath_expr_set_item(expr, data.dim, pmath_ref(data.start));
      }
    }
    
    pmath_unref(data.start);
    pmath_unref(delta);
    return expr;
  }
  
  if(exprlen < 1 || exprlen > 4){
    pmath_message_argxxx(exprlen, 1, 4);
    return expr;
  }
  
  data.dims = pmath_expr_get_item(expr, 2);
  if(!pmath_is_expr_of(data.dims, PMATH_SYMBOL_LIST))
    data.dims = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_LIST), 1, 
      data.dims);
  
  data.depth = pmath_expr_length(data.dims);
  for(data.dim = data.depth;data.dim > 0;data.dim--){
    pmath_t d = pmath_expr_get_item(data.dims, data.dim);
    
    if(!pmath_instance_of(d, PMATH_TYPE_INTEGER)
    || !pmath_integer_fits_ui(d)){
      pmath_unref(d);
      pmath_unref(data.dims);
      pmath_message(NULL, "intnm", 2, pmath_integer_new_si(2), pmath_ref(expr));
      return expr;
    }
    
    pmath_unref(d);
  }
  
  data.start_is_list = FALSE;
  if(exprlen >= 3){
    data.start = pmath_expr_get_item(expr, 3);
    if(pmath_is_expr_of(data.start, PMATH_SYMBOL_LIST)){
      if(pmath_expr_length(data.start) != data.depth){
        pmath_message(NULL, "plen", 2, data.dims, data.start);
        return expr;
      }
      
      data.start_is_list = TRUE;
    }
  }
  else
    data.start = pmath_integer_new_si(1);
  
  data.function = pmath_expr_get_item(expr, 1);
  if(exprlen == 4)
    data.head = pmath_expr_get_item(expr, 4);
  else
    data.head = pmath_ref(PMATH_SYMBOL_LIST);
  
  data.dim = 1;
  data.index = pmath_expr_new(NULL, data.depth);
  pmath_unref(expr);
  expr = array(&data);
  
  pmath_unref(data.function);
  pmath_unref(data.head);
  pmath_unref(data.dims);
  pmath_unref(data.start);
  pmath_unref(data.index);
  return expr;
}

#define MAX_DIM 10

typedef struct{
  size_t          lengths[MAX_DIM];
  pmath_t  c;
  size_t          dim;
  size_t          dims;
}special_array_data_t;

static pmath_t special_array(special_array_data_t *data){
  pmath_expr_t list;
  size_t i;

  if(data->dim == data->dims)
    return pmath_ref(data->c);

  data->dim++;

  list = pmath_expr_new(
    pmath_ref(PMATH_SYMBOL_LIST),
    data->lengths[data->dim-1]);

  for(i = 1;i <= data->lengths[data->dim-1];++i){
    list = pmath_expr_set_item(list, i, special_array(data));
  }

  data->dim--;
  return list;
}

PMATH_PRIVATE pmath_t builtin_constantarray(pmath_expr_t expr){
/* ConstantArray(c, n)
   ConstantArray(c, {n1, n2, ...})

   messages:
     General::ilsmn
 */
  special_array_data_t data;
  pmath_t n;

  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  n = pmath_expr_get_item(expr, 2);

  if(pmath_instance_of(n, PMATH_TYPE_EXPRESSION)){
    pmath_t h = pmath_expr_get_item((pmath_expr_t)n, 0);
    pmath_unref(h);

    if(h == PMATH_SYMBOL_LIST){
      data.dims = pmath_expr_length((pmath_expr_t)n);
      if(data.dims > MAX_DIM){
        pmath_unref(n);

        return expr;
      }

      for(data.dim = data.dims;data.dim > 0;data.dim--){
        pmath_t l = pmath_expr_get_item((pmath_expr_t)n, data.dim);

        if(!pmath_instance_of(l, PMATH_TYPE_INTEGER)
        || !pmath_integer_fits_ui((pmath_integer_t)l)){
          pmath_unref(l);
          pmath_unref(n);
          pmath_message(
            NULL, "ilsmn", 2,
            pmath_integer_new_ui(2),
            pmath_ref(expr));
          return expr;
        }

        data.lengths[data.dim-1] = pmath_integer_get_ui((pmath_integer_t)l);

        pmath_unref(l);
      }

      data.c = pmath_expr_get_item(expr, 1);
      data.dim = 0;

      pmath_unref(n);
      pmath_unref(expr);

      n = special_array(&data);

      pmath_unref(data.c);
      return n;
    }
  }

  if(!pmath_instance_of(n, PMATH_TYPE_INTEGER)
  || !pmath_integer_fits_ui((pmath_integer_t)n)){
    pmath_unref(n);
    pmath_message(
      NULL, "ilsmn", 2,
      pmath_integer_new_ui(2),
      pmath_ref(expr));
    return expr;
  }

  data.lengths[0] = pmath_integer_get_ui((pmath_integer_t)n);
  data.c = pmath_expr_get_item(expr, 1);
  data.dim = 0;
  data.dims = 1;

  pmath_unref(n);
  pmath_unref(expr);

  n = special_array(&data);

  pmath_unref(data.c);
  return n;
}

/*PMATH_PRIVATE pmath_t builtin_rangearray(pmath_expr_t expr){
/ * RangeArray(n)
   RangeArray(a..b)
   RangeArray(a..b..d)

   messages:
     General::range
 * /
  pmath_t range, start, delta;
  pmath_thread_t thread = pmath_thread_get_current();
  size_t i, count;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  range = pmath_expr_get_item(expr, 1);

  if(!extract_delta_range(range, &start, &delta, &count)){
    pmath_unref(start);
    pmath_unref(delta);
    pmath_message(NULL, "range", 1, range);
    return expr;
  }

  pmath_unref(range);
  pmath_unref(expr);

  expr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), count);

  if(pmath_equals(start, PMATH_NUMBER_ONE)
  && pmath_equals(delta, PMATH_NUMBER_ONE)){
    pmath_unref(start);
    pmath_unref(delta);

    for(i = 1;i <= count && !pmath_thread_aborting(thread);++i){
      expr = pmath_expr_set_item(
        expr, i,
        pmath_integer_new_size(i));
    }

    return expr;
  }

  for(i = 1;i <= count && !pmath_thread_aborting(thread);++i){
    expr = pmath_expr_set_item(expr, i, pmath_ref(start));
    
    start = pmath_evaluate(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_PLUS), 2,
        start,
        pmath_ref(delta)));
  }

  pmath_unref(start);
  pmath_unref(delta);

  return expr;
}*/
