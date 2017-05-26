#include <pmath-builtins/language-private.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>

#include <pmath-core/expressions.h>

#include <pmath-language/number-parsing-private.h>
#include <pmath-language/number-writing-private.h>

#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>


#define RULE(NAME, VALUE)        pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_RULE), 2, PMATH_C_STRING(NAME), VALUE)

PMATH_PRIVATE
pmath_t builtin_internal_writerealball(pmath_expr_t expr) {
  pmath_string_t str;
  pmath_t options;
  pmath_mpfloat_t value;
  pmath_t obj;
  int base = 10;
  int max_digits = INT_MAX;
  pmath_bool_t allow_inexact_digits = FALSE;
  struct _pmath_number_string_parts_t parts;
  
  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  options = pmath_options_extract(expr, 1);
  if(pmath_is_null(options))
    return expr;
  
  value = pmath_expr_get_item(expr, 1);
  if(!pmath_is_mpfloat(value)) {
    pmath_unref(value);
    pmath_message(PMATH_NULL, "mpf", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
    
  str = PMATH_C_STRING("Base");
  obj = pmath_option_value(PMATH_NULL, str, options);
  pmath_unref(str);
  if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 2 && PMATH_AS_INT32(obj) <= 36) {
    base = PMATH_AS_INT32(obj);
  }
  else {
    pmath_message(PMATH_NULL, "basf", 2, obj, PMATH_FROM_INT32(36));
    pmath_unref(value);
    return expr;
  }
  
  str = PMATH_C_STRING("MaxDigits");
  obj = pmath_option_value(PMATH_NULL, str, options);
  pmath_unref(str);
  if(pmath_same(obj, PMATH_SYMBOL_AUTOMATIC)) {
    max_digits = (int)(PMATH_AS_ARB_WORKING_PREC(value) / _pmath_log2_of(base) + 2);
    pmath_unref(obj);
  }
  else if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) > 0) {
    max_digits = PMATH_AS_INT32(obj);
  }
  else {
    pmath_message(PMATH_NULL, "maxdig", 1, obj);
    pmath_unref(value);
    return expr;
  }
  
  str = PMATH_C_STRING("AllowInexactDigits");
  obj = pmath_option_value(PMATH_NULL, str, options);
  pmath_unref(str);
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)) {
    allow_inexact_digits = TRUE;
    pmath_unref(obj);
  }
  else if(pmath_same(obj, PMATH_SYMBOL_FALSE)) {
    allow_inexact_digits = FALSE;
    pmath_unref(obj);
  }
  else {
    pmath_message(PMATH_NULL, "opttf", 2, PMATH_C_STRING("AllowInexactDigits"), obj);
    pmath_unref(value);
    return expr;
  }
  
  pmath_unref(options);
  
  _pmath_mpfloat_get_string_parts(&parts, value, base, max_digits, allow_inexact_digits);
  pmath_unref(value);
  pmath_unref(expr);
  
  str = PMATH_NULL;
  if(parts.is_negative)
    str = pmath_string_insert_latin1(str, INT_MAX, "-", 1);
    
  if(base != 10) {
    char buf[3];
    itoa(base, buf, 10);
    str = pmath_string_insert_latin1(str, INT_MAX, buf, -1);
    str = pmath_string_insert_latin1(str, INT_MAX, "^^", 2);
  }
  
  str = pmath_string_concat(str, pmath_ref(parts.midpoint_fractional_mantissa_digits));
  str = pmath_string_insert_latin1(str, INT_MAX, "[+-", 3);
  str = pmath_string_concat(str, pmath_ref(parts.radius_fractional_mantissa_digits));
  if(pmath_string_length(parts.radius_exponent_part_decimal_digits) > 0) {
    str = pmath_string_insert_latin1(str, INT_MAX, "*^", 2);
    str = pmath_string_concat(str, pmath_ref(parts.radius_exponent_part_decimal_digits));
  }
  str = pmath_string_insert_latin1(str, INT_MAX, "]`", 2);
  str = pmath_string_concat(str, pmath_ref(parts.precision_decimal_digits));
  if(pmath_string_length(parts.exponent_decimal_digits) > 0) {
    str = pmath_string_insert_latin1(str, INT_MAX, "*^", 2);
    str = pmath_string_concat(str, pmath_ref(parts.exponent_decimal_digits));
  }
  
  expr = pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_LIST), 8,
           RULE("String", str),
           RULE("Sign", parts.is_negative ? PMATH_C_STRING("-") : PMATH_C_STRING("")),
           RULE("Base", PMATH_FROM_INT32(parts.base)),
           RULE("MidpointMantissa", parts.midpoint_fractional_mantissa_digits),
           RULE("RadiusMantissa", parts.radius_fractional_mantissa_digits),
           RULE("RadiusExponentExtra", parts.radius_exponent_part_decimal_digits),
           RULE("Precision", parts.precision_decimal_digits),
           RULE("Exponent", parts.exponent_decimal_digits));
           
  return expr;
}
