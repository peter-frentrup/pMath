#include <pmath-builtins/language-private.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>

#include <pmath-core/expressions.h>

#include <pmath-language/number-parsing-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>


extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_MachinePrecision;
extern pmath_symbol_t pmath_System_Rule;

#define RULE(NAME, VALUE)     pmath_expr_new_extended(pmath_ref(pmath_System_Rule), 2, PMATH_C_STRING(NAME), VALUE)
#define LIST2(A, B)           pmath_expr_new_extended(pmath_ref(pmath_System_List), 2, A, B)
#define LIST3(A, B, C)        pmath_expr_new_extended(pmath_ref(pmath_System_List), 3, A, B, C)
#define LIST4(A, B, C, D)     pmath_expr_new_extended(pmath_ref(pmath_System_List), 4, A, B, C, D)
#define LIST5(A, B, C, D, E)  pmath_expr_new_extended(pmath_ref(pmath_System_List), 5, A, B, C, D, E)


static pmath_t precision_from_digits(double digits) {
  if(isfinite(digits))
    return PMATH_FROM_DOUBLE(digits);
  
  if(digits == -HUGE_VAL)
    return pmath_ref(pmath_System_MachinePrecision);
  
  return pmath_ref(_pmath_object_pos_infinity);
}

PMATH_PRIVATE
pmath_t builtin_internal_parserealball(pmath_expr_t expr) {
  pmath_string_t str;
  pmath_t min_prec_obj;
  pmath_t options;
  pmath_t mid, rad, val;
  struct _pmath_real_ball_parts_t parts;
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
  min_prec_obj = pmath_evaluate(pmath_option_value(PMATH_NULL, str, options));
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
  
  _pmath_real_ball_parts_init(&parts);
  
  end = _pmath_parse_real_ball(&parts, buf, buf_end, min_prec);
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
  
  mid = _pmath_compose_number(parts.midpoint_mantissa, parts.midpoint_exponent, parts.base, parts.precision_in_base);
  rad = _pmath_compose_number(parts.radius_mantissa,   parts.radius_exponent,   parts.base, parts.precision_in_base);
  
  val = _pmath_real_ball_from_midpoint_radius(pmath_ref(mid), pmath_ref(rad));
  
  pmath_unref(str);
  pmath_unref(expr);
  expr = LIST5(
           RULE("Value", val),
           RULE("Base", INT(parts.base)),
           RULE("Midpoint",
                LIST3(
                  RULE("Mantissa", _pmath_integer_from_fmpz(parts.midpoint_mantissa)),
                  RULE("Exponent", _pmath_integer_from_fmpz(parts.midpoint_exponent)),
                  RULE("Value",    mid))),
           RULE("Radius",
                LIST3(
                  RULE("Mantissa", _pmath_integer_from_fmpz(parts.radius_mantissa)),
                  RULE("Exponent", _pmath_integer_from_fmpz(parts.radius_exponent)),
                  RULE("Value",    rad))),
           RULE("SignificantDigits", precision_from_digits(parts.precision_in_base))
         );
         
  _pmath_real_ball_parts_clear(&parts);
  
  return expr;
}
