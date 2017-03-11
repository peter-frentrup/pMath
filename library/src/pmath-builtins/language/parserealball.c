#include <pmath-builtins/language-private.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>

#include <pmath-core/expressions.h>

#include <pmath-language/number-parsing-private.h>

#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>


#define RULE(NAME, VALUE)     pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_RULE), 2, PMATH_C_STRING(NAME), VALUE)
#define LIST2(A, B)           pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 2, A, B)
#define LIST3(A, B, C)        pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 3, A, B, C)
#define LIST4(A, B, C, D)     pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 4, A, B, C, D)
#define LIST5(A, B, C, D, E)  pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 5, A, B, C, D, E)


static pmath_t precision_from_digits(double digits) {
  if(isfinite(digits))
    return PMATH_FROM_DOUBLE(digits);
  
  if(digits == -HUGE_VAL)
    return pmath_ref(PMATH_SYMBOL_MACHINEPRECISION);
  
  return pmath_ref(_pmath_object_pos_infinity);
}

PMATH_PRIVATE
pmath_t builtin_internal_parserealball(pmath_expr_t expr) {
  pmath_string_t str;
  pmath_t min_prec_obj;
  pmath_t options;
  pmath_t mid, rad, val;
  fmpz_t mid_mant;
  fmpz_t mid_exp;
  fmpz_t rad_mant;
  fmpz_t rad_exp;
  int base;
  double prec;
  double min_prec = -HUGE_VAL;
  const uint16_t *buf;
  const uint16_t *buf_end;
  const uint16_t *end;
  
  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  options = pmath_options_extract(expr, 1);
  if(pmath_is_null(options))
    return expr;
  
  str = PMATH_C_STRING("MinPrecision");
  min_prec_obj = pmath_option_value(PMATH_NULL, str, options);
  pmath_unref(str);
  if(!_pmath_to_precision(min_prec_obj, &min_prec))
    pmath_message(PMATH_NULL, "invprec", 1, pmath_ref(min_prec_obj));
  
  pmath_unref(min_prec_obj);
  pmath_unref(options);
  
  str = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(str)) {
    pmath_unref(str);
    pmath_message(PMATH_NULL, "str", 2, INT(1), pmath_ref(expr));
    return expr;
  }
  
  buf = pmath_string_buffer(&str);
  buf_end = buf + pmath_string_length(str);
  
  fmpz_init(mid_mant);
  fmpz_init(mid_exp);
  fmpz_init(rad_mant);
  fmpz_init(rad_exp);
  
  end = _pmath_parse_float_ball(mid_mant, mid_exp, rad_mant, rad_exp, &base, &prec, buf, buf_end, min_prec);
  if(end != buf_end) {
    if(buf < end) {
      int index = (int)(end - buf);
      pmath_message(
        PMATH_NULL, "nxt", 2,
        pmath_string_part(pmath_ref(str), 0, index),
        pmath_string_part(pmath_ref(str), index, -1));
    }
    else
      pmath_message(PMATH_NULL, "bgn", 1, pmath_ref(str));
  }
  
  mid = _pmath_compose_number(mid_mant, mid_exp, base, prec);
  rad = _pmath_compose_number(rad_mant, rad_exp, base, prec);
  
  if(pmath_is_mpfloat(mid) && pmath_is_mpfloat(rad)) {
    val = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(mid));
    if(!pmath_is_null(val)) {
      arb_set(PMATH_AS_ARB(val), PMATH_AS_ARB(mid));
      arb_add_error(PMATH_AS_ARB(val), PMATH_AS_ARB(rad));
      arf_get_mpfr(PMATH_AS_MP_VALUE(val), arb_midref(PMATH_AS_ARB(val)), MPFR_RNDN);
    }
  }
  else if(fmpz_is_zero(rad_mant))
    val = pmath_ref(mid);
  else
    val = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_INTERVAL), 1,
      LIST2(
        MINUS(pmath_ref(mid), pmath_ref(rad)),
        PLUS(pmath_ref(mid), pmath_ref(rad))));
  
  pmath_unref(str);
  pmath_unref(expr);
  expr = LIST5(
           RULE("Value", val),
           RULE("Base", INT(base)),
           RULE("Midpoint",
                LIST3(
                  RULE("Mantissa", _pmath_integer_from_fmpz(mid_mant)),
                  RULE("Exponent", _pmath_integer_from_fmpz(mid_exp)),
                  RULE("Value",    mid))),
           RULE("Radius",
                LIST3(
                  RULE("Mantissa", _pmath_integer_from_fmpz(rad_mant)),
                  RULE("Exponent", _pmath_integer_from_fmpz(rad_exp)),
                  RULE("Value",    rad))),
           RULE("SignificantDigits", precision_from_digits(prec))
         );
         
  fmpz_clear(mid_mant);
  fmpz_clear(mid_exp);
  fmpz_clear(rad_mant);
  fmpz_clear(rad_exp);
  
  return expr;
}
