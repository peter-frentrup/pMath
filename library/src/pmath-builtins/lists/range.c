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
  
  if(!pmath_is_integer(number))
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
  if(pmath_is_expr_of_len(range, PMATH_SYMBOL_RANGE, 2)){
    pmath_t obj = pmath_expr_get_item(range, 1);
    if(obj != PMATH_SYMBOL_AUTOMATIC 
    && !extract_number(obj, *max, min)){
      pmath_unref(obj);
      return FALSE;
    }
    pmath_unref(obj);

    obj = pmath_expr_get_item(range, 2);
    if(obj != PMATH_SYMBOL_AUTOMATIC 
    && !extract_number(obj, *max, max)){
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
  size_t  *count
){
  pmath_t count_obj = NULL;
  *start = *delta = NULL;
  *count = 0;

  if(pmath_is_expr_of_len(range, PMATH_SYMBOL_RANGE, 2)){
    *start = pmath_expr_get_item(range, 1);
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
  else if(pmath_is_expr_of_len(range, PMATH_SYMBOL_RANGE, 3)){
    *start = pmath_expr_get_item(range, 1);
    *delta = pmath_expr_get_item(range, 3);

    count_obj = pmath_expr_new_extended( // 1 + (end - start)/delta
      pmath_ref(PMATH_SYMBOL_PLUS), 2,
      pmath_integer_new_ui(1),
      pmath_expr_new_extended( 
        pmath_ref(PMATH_SYMBOL_TIMES), 2,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PLUS), 2,
          pmath_expr_get_item(range, 2), // end
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_TIMES), 2,
            pmath_integer_new_si(-1),
            pmath_ref(*start))),
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_POWER), 2,
          pmath_ref(*delta),
          pmath_ref(PMATH_NUMBER_MINUSONE))));
  }
  else{
    *start = pmath_integer_new_ui(1);
    *delta = pmath_integer_new_ui(1);

    count_obj = pmath_ref(range);
  }
  
  count_obj = pmath_evaluate(pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_FLOOR), 1, count_obj));

  if(!pmath_is_integer(count_obj) || !pmath_integer_fits_ui(count_obj)){
    pmath_unref(count_obj);
    return FALSE;
  }

  *count = pmath_integer_get_ui(count_obj);
  pmath_unref(count_obj);
  return TRUE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_extract_longrange(
  pmath_t  range,
  long    *start,
  long    *end,
  long    *step
){
  if(range == PMATH_SYMBOL_ALL){
    *start = 1;
    *end   = -1;
    *step  = 1;
    return TRUE;
  }
  
  if(pmath_is_integer(range) && pmath_integer_fits_si(range)){
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
    *step = 1;
    
    if(pmath_is_integer(a) && pmath_integer_fits_si(a)){
      *start = pmath_integer_get_si(a);
    }
    else if(a == PMATH_SYMBOL_AUTOMATIC){
      *start = 1;
    }
    else{
      pmath_unref(a);
      pmath_unref(b);
      return FALSE;
    }
    
    if(pmath_is_integer(b) && pmath_integer_fits_si(b)){
      *end = pmath_integer_get_si(b);
    }
    else if(b == PMATH_SYMBOL_AUTOMATIC){
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
  
  if(pmath_is_expr_of_len(range, PMATH_SYMBOL_RANGE, 3)){
    pmath_t a = pmath_expr_get_item(range, 1);
    pmath_t b = pmath_expr_get_item(range, 2);
    pmath_t c = pmath_expr_get_item(range, 3);
    
    if(pmath_is_integer(c) && pmath_integer_fits_si(c)){
      *step = pmath_integer_get_si(c);
    }
    else{
      pmath_unref(a);
      pmath_unref(b);
      pmath_unref(c);
      return FALSE;
    }
    
    if(pmath_is_integer(a) && pmath_integer_fits_si(a)){
      *start = pmath_integer_get_si(a);
    }
    else if(a == PMATH_SYMBOL_AUTOMATIC){
      *start = 1;
    }
    else{
      pmath_unref(a);
      pmath_unref(b);
      pmath_unref(c);
      return FALSE;
    }
    
    if(pmath_is_integer(b) && pmath_integer_fits_si(b)){
      *end = pmath_integer_get_si(b);
    }
    else if(b == PMATH_SYMBOL_AUTOMATIC){
      *end = -1;
    }
    else{
      pmath_unref(a);
      pmath_unref(b);
      pmath_unref(c);
      return FALSE;
    }
    
    pmath_unref(a);
    pmath_unref(b);
    pmath_unref(c);
    return TRUE;
  }
  
  return FALSE;
}
  
PMATH_PRIVATE pmath_t builtin_range(pmath_expr_t expr){
  size_t exprlen = pmath_expr_length(expr);
  if(exprlen < 2 || exprlen > 3)
    pmath_message_argxxx(exprlen, 2, 3);

  return expr;
}
