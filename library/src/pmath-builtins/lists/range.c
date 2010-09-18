#include <pmath-core/numbers-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

#include <limits.h>


PMATH_PRIVATE pmath_bool_t extract_number(
  pmath_t number,
  size_t max,
  size_t *num
){
  if(pmath_equals(number, _pmath_object_infinity)){
    *num = max;
    return TRUE;
  }
  if(!pmath_instance_of(number, PMATH_TYPE_INTEGER))
    return FALSE;

  if(mpz_cmpabs_ui(((struct _pmath_integer_t*)number)->value, ULONG_MAX) > 0){
    if(mpz_sgn(((struct _pmath_integer_t*)number)->value) < 0)
      *num = SIZE_MAX;
    else
      *num = max;
  }
  else{
    *num = mpz_get_ui(((struct _pmath_integer_t*)number)->value);
    if(mpz_sgn(((struct _pmath_integer_t*)number)->value) < 0){
      if(max == SIZE_MAX || max < *num - 1)
        *num = SIZE_MAX;
      else
        *num = max + 1 - *num;
    }
  }

  return TRUE;
}

PMATH_PRIVATE pmath_bool_t extract_range(
  pmath_t range,
  size_t *min,
  size_t *max,
  pmath_bool_t change_min_on_number
){
  if(pmath_instance_of(range, PMATH_TYPE_EXPRESSION)
  && pmath_is_expr_of_len(range, PMATH_SYMBOL_RANGE, 2)){
    pmath_t obj = pmath_expr_get_item(range, 1);
    if(obj && !extract_number(obj, *max, min)){
      pmath_unref(obj);
      return FALSE;
    }
    pmath_unref(obj);

    obj = pmath_expr_get_item(range, 2);
    if(obj && !extract_number(obj, *max, max)){
      pmath_unref(obj);
      return FALSE;
    }
    pmath_unref(obj);

    return TRUE;
  }

  if(change_min_on_number){
    pmath_bool_t result = extract_number(range, *max, min);
    *max = *min;
    return result;
  }
  else{
    size_t tmp = *min;
    pmath_bool_t result = extract_number(range, SIZE_MAX, &tmp);
    if(tmp != SIZE_MAX)
      *max = tmp;

    return result;
  }
}

PMATH_PRIVATE pmath_bool_t extract_delta_range(
  pmath_t  range,
  pmath_t *start,
  pmath_t *delta,
  size_t         *count
){
  pmath_t obj, count_obj;
  *start = *delta = NULL;
  *count = 0;

  if(pmath_is_expr_of_len(range, PMATH_SYMBOL_RANGE, 2)){
    obj = pmath_expr_get_item(range, 1);

    if(pmath_is_expr_of_len(obj, PMATH_SYMBOL_RANGE, 2)){
      *start = pmath_expr_get_item(obj, 1);
      *delta = pmath_expr_get_item(range, 2);

      count_obj = pmath_expr_new_extended( // 1 + (end - start)/delta
        pmath_ref(PMATH_SYMBOL_PLUS), 2,
        pmath_integer_new_ui(1),
        pmath_expr_new_extended( 
          pmath_ref(PMATH_SYMBOL_TIMES), 2,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PLUS), 2,
            pmath_expr_get_item(obj, 2), // end
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_TIMES), 2,
              pmath_integer_new_si(-1),
              pmath_ref(*start))),
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_POWER), 2,
            pmath_ref(*delta),
            pmath_ref(PMATH_NUMBER_MINUSONE))));

      pmath_unref(obj);
    }
    else{
      *start = obj;
      *delta = pmath_integer_new_ui(1);

      count_obj = pmath_expr_new_extended( // end + 1 - start
        pmath_ref(PMATH_SYMBOL_PLUS), 3,
        pmath_expr_get_item(range, 2),
        pmath_ref(*delta),
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_TIMES), 2,
          pmath_ref(PMATH_NUMBER_MINUSONE),
          pmath_ref(*start)));
    }
  }
  else{
    *start = pmath_integer_new_ui(1);
    *delta = pmath_integer_new_ui(1);

    count_obj = pmath_ref(range);
  }
  
  count_obj = pmath_evaluate(pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_FLOOR), 1, count_obj));

  if(!pmath_instance_of(count_obj, PMATH_TYPE_INTEGER)
  || !pmath_integer_fits_ui((pmath_integer_t)count_obj)){
    pmath_unref(count_obj);
    return FALSE;
  }

  *count = pmath_integer_get_ui((pmath_integer_t)count_obj);
  pmath_unref(count_obj);
  return TRUE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_extract_longrange(
  pmath_t  range,
  long           *start,
  long           *end,
  long           *step
){
  if(range == PMATH_SYMBOL_ALL){
    *start = 1;
    *end   = -1;
    *step  = 1;
    return TRUE;
  }
  
  if(pmath_instance_of(range, PMATH_TYPE_INTEGER)
  && pmath_integer_fits_si(range)){
    *end  = pmath_integer_get_si(range);
    *step = 1;
    
    if(*end <= 0){
      *start = *end;
      *end   = -1;
    }
    else
      *start = 1;
      
    return TRUE;
  }
  
  if(pmath_is_expr_of_len(range, PMATH_SYMBOL_RANGE, 2)){
    pmath_t a = pmath_expr_get_item(range, 1);
    pmath_t b = pmath_expr_get_item(range, 2);
    
    if(pmath_is_expr_of_len(a, PMATH_SYMBOL_RANGE, 2)){
      pmath_t c;
      
      if(pmath_instance_of(b, PMATH_TYPE_INTEGER)
      && pmath_integer_fits_si(b)){
        *step = pmath_integer_get_si(b);
      }
      else{
        pmath_unref(a);
        pmath_unref(b);
        return FALSE;
      }
      
      pmath_unref(b);
      c = a;
      a = pmath_expr_get_item(c, 1);
      b = pmath_expr_get_item(c, 2);
      pmath_unref(c);
    }
    else
      *step = 1;
    
    if(pmath_instance_of(a, PMATH_TYPE_INTEGER)
    && pmath_integer_fits_si(a)){
      *start = pmath_integer_get_si(a);
    }
    else if(!a){
      *start = 1;
    }
    else{
      pmath_unref(a);
      pmath_unref(b);
      return FALSE;
    }
    
    if(pmath_instance_of(b, PMATH_TYPE_INTEGER)
    && pmath_integer_fits_si(b)){
      *end = pmath_integer_get_si(b);
    }
    else if(!b){
      *end = -1;
    }
    else{
      pmath_unref(a);
      pmath_unref(b);
      return FALSE;
    }
    
    pmath_unref(a);
    pmath_unref(b);
    return TRUE;
  }
  
  return FALSE;
}
  
PMATH_PRIVATE pmath_t builtin_range(pmath_expr_t expr){
  if(pmath_expr_length(expr) != 2)
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);

  return expr;
}
