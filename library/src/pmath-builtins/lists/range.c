#include <pmath-core/numbers-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>

#include <limits.h>


extern pmath_symbol_t pmath_System_All;
extern pmath_symbol_t pmath_System_Automatic;
extern pmath_symbol_t pmath_System_Floor;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_Power;
extern pmath_symbol_t pmath_System_Range;
extern pmath_symbol_t pmath_System_Times;

PMATH_PRIVATE pmath_bool_t extract_number(
  pmath_t number,
  size_t  max,
  size_t *num
) {
  if(pmath_equals(number, _pmath_object_pos_infinity)) {
    *num = max;
    return TRUE;
  }

  if(pmath_is_int32(number)) {
    if(PMATH_AS_INT32(number) < 0) {
      *num = (unsigned) - PMATH_AS_INT32(number);
      if(max == SIZE_MAX || max < *num - 1)
        *num = SIZE_MAX;
      else
        *num = max + 1 - *num;
    }
    else
      *num = PMATH_AS_INT32(number);

    return TRUE;
  }

  if(!pmath_is_mpint(number))
    return FALSE;

  if(mpz_cmpabs_ui(PMATH_AS_MPZ(number), ULONG_MAX) > 0) {
    if(mpz_sgn(PMATH_AS_MPZ(number)) < 0)
      *num = SIZE_MAX;
    else
      *num = max;
  }
  else {
    *num = mpz_get_ui(PMATH_AS_MPZ(number));
    if(mpz_sgn(PMATH_AS_MPZ(number)) < 0) {
      if(max == SIZE_MAX || max < *num - 1)
        *num = SIZE_MAX;
      else
        *num = max + 1 - *num;
    }
  }

  return TRUE;
}

PMATH_PRIVATE pmath_bool_t extract_range(
  pmath_t       range,
  size_t       *min,
  size_t       *max,
  pmath_bool_t  change_min_on_number
) {
  if(pmath_is_expr_of_len(range, pmath_System_Range, 2)) {
    pmath_t obj = pmath_expr_get_item(range, 1);

    if( !pmath_same(obj, pmath_System_Automatic) &&
        !extract_number(obj, *max, min))
    {
      pmath_unref(obj);
      return FALSE;
    }
    pmath_unref(obj);

    obj = pmath_expr_get_item(range, 2);
    if( !pmath_same(obj, pmath_System_Automatic) &&
        !extract_number(obj, *max, max))
    {
      pmath_unref(obj);
      return FALSE;
    }
    pmath_unref(obj);

    return TRUE;
  }

  if(change_min_on_number) {
    pmath_bool_t result = extract_number(range, *max, min);
    *max = *min;
    return result;
  }
  else {
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
) {
  pmath_t count_obj;
  *start = *delta = PMATH_NULL;
  *count = 0;

  if(pmath_is_expr_of_len(range, pmath_System_Range, 2)) {
    *start = pmath_expr_get_item(range, 1);
    *delta = INT(1);

    count_obj = PLUS3( // end + 1 - start
                  pmath_expr_get_item(range, 2),
                  INT(1),
                  NEG(pmath_ref(*start)));
  }
  else if(pmath_is_expr_of_len(range, pmath_System_Range, 3)) {
    *start = pmath_expr_get_item(range, 1);
    *delta = pmath_expr_get_item(range, 3);
    
    if(pmath_is_number(*delta) && pmath_number_sign(*delta) == 0)
      return FALSE;

    count_obj = PLUS( // 1 + (end - start)/delta
                  INT(1),
                  DIV(
                    MINUS(
                      pmath_expr_get_item(range, 2),
                      pmath_ref(*start)),
                    pmath_ref(*delta)));
  }
  else {
    *start = INT(1);
    *delta = INT(1);

    count_obj = pmath_ref(range);
  }

  count_obj = pmath_evaluate(FUNC(pmath_ref(pmath_System_Floor), count_obj));

  if(!pmath_is_int32(count_obj)) {
    pmath_unref(count_obj);
    return FALSE;
  }

  if(PMATH_AS_INT32(count_obj) < 0) {
    *count = 0;
    return TRUE;
  }

  *count = (unsigned)PMATH_AS_INT32(count_obj);
  return TRUE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_extract_longrange(pmath_t obj, struct _pmath_range_t *range) {
  if(pmath_same(obj, pmath_System_All)) {
    range->start = 1;
    range->end   = -1;
    range->step  = 1;
    return TRUE;
  }

  if(pmath_is_int32(obj)) {
    range->end  = PMATH_AS_INT32(obj);
    range->step = 1;

    if(range->end < 0) {
      range->start = range->end;
      range->end   = -1;
    }
    else
      range->start = 1;

    return TRUE;
  }

  if(pmath_is_expr_of_len(obj, pmath_System_Range, 2)) {
    pmath_t a = pmath_expr_get_item(obj, 1);
    pmath_t b = pmath_expr_get_item(obj, 2);
    range->step = 1;

    if(pmath_is_int32(a)) {
      range->start = PMATH_AS_INT32(a);
    }
    else if(pmath_same(a, pmath_System_Automatic)) {
      range->start = 1;
    }
    else {
      pmath_unref(a);
      pmath_unref(b);
      return FALSE;
    }

    if(pmath_is_int32(b)) {
      range->end = PMATH_AS_INT32(b);
    }
    else if(pmath_same(b, pmath_System_Automatic)) {
      range->end = -1;
    }
    else {
      pmath_unref(a);
      pmath_unref(b);
      return FALSE;
    }

    pmath_unref(a);
    pmath_unref(b);
    return TRUE;
  }

  if(pmath_is_expr_of_len(obj, pmath_System_Range, 3)) {
    pmath_t a = pmath_expr_get_item(obj, 1);
    pmath_t b = pmath_expr_get_item(obj, 2);
    pmath_t c = pmath_expr_get_item(obj, 3);

    if(pmath_is_int32(c)) {
      range->step = PMATH_AS_INT32(c);
    }
    else {
      pmath_unref(a);
      pmath_unref(b);
      pmath_unref(c);
      return FALSE;
    }

    if(pmath_is_int32(a)) {
      range->start = PMATH_AS_INT32(a);
    }
    else if(pmath_same(a, pmath_System_Automatic)) {
      range->start = 1;
    }
    else {
      pmath_unref(a);
      pmath_unref(b);
      pmath_unref(c);
      return FALSE;
    }

    if(pmath_is_int32(b)) {
      range->end = PMATH_AS_INT32(b);
    }
    else if(pmath_same(b, pmath_System_Automatic)) {
      range->end = -1;
    }
    else {
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

PMATH_PRIVATE pmath_t builtin_range(pmath_expr_t expr) {
  size_t exprlen = pmath_expr_length(expr);
  if(exprlen < 2 || exprlen > 3)
    pmath_message_argxxx(exprlen, 2, 3);

  return expr;
}
