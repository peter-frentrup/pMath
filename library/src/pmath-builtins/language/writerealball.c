#include <pmath-builtins/language-private.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>

#include <pmath-core/expressions.h>

#include <pmath-language/number-parsing-private.h>
#include <pmath-language/number-writing-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>


#define RULE(NAME, VALUE)        pmath_expr_new_extended(pmath_ref(pmath_System_Rule), 2, PMATH_C_STRING(NAME), VALUE)

extern pmath_symbol_t pmath_System_Automatic;
extern pmath_symbol_t pmath_System_BaseForm;
extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_InputForm;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Rule;
extern pmath_symbol_t pmath_System_True;

static void write_latin1_to_string(void *pointer_to_pmath_string, const char *str, int len) {
  pmath_string_t *string_ptr = pointer_to_pmath_string;
  
  *string_ptr = pmath_string_insert_latin1(*string_ptr, INT_MAX, str, len);
}

PMATH_PRIVATE
pmath_t builtin_internal_writerealball(pmath_expr_t expr) {
  pmath_string_t str;
  pmath_t options;
  pmath_mpfloat_t value;
  pmath_t obj;
  slong precision_bits;
  int base = 10;
  int max_rad_digits;
  pmath_bool_t auto_exponent = TRUE;
  int initeger_digits_goal = 0;
  struct _pmath_raw_number_parts_t parts;
  
  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  options = pmath_options_extract(expr, 1);
  if(pmath_is_null(options))
    return expr;
  
  max_rad_digits = 3;
  
  value = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_expr_of_len(value, pmath_System_BaseForm, 2)) {
    pmath_t tmp;
    tmp = pmath_expr_get_item(value, 2);
    if(pmath_is_int32(tmp)) {
      base = PMATH_AS_INT32(tmp);
    }
    else
      base = 99;
    
    pmath_unref(tmp);
    tmp = pmath_expr_get_item(value, 1);
    pmath_unref(value);
    value = tmp; 
    max_rad_digits = 5;
  }
  
  if(pmath_is_expr_of_len(value, pmath_System_InputForm, 1)) {
    pmath_t tmp;
    tmp = pmath_expr_get_item(value, 1);
    pmath_unref(value);
    value = tmp; 
    max_rad_digits = 5;
  }
  if(!pmath_is_mpfloat(value)) {
    pmath_message(PMATH_NULL, "mpf", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    pmath_unref(value);
    pmath_unref(options);
    return expr;
  }
  
  precision_bits = PMATH_AS_ARB_WORKING_PREC(value);
    
  str = PMATH_C_STRING("Base");
  obj = pmath_evaluate(pmath_option_value(PMATH_NULL, str, options));
  pmath_unref(str);
  if(pmath_is_int32(obj)) {
    base = PMATH_AS_INT32(obj);
  }
  else if(!pmath_same(obj, pmath_System_Automatic)) {
    base = 99;
  }
  
  if(base < 2 || base > 36) {
    pmath_message(PMATH_NULL, "basf", 2, obj, PMATH_FROM_INT32(36));
    pmath_unref(value);
    pmath_unref(options);
    return expr;
  }
  
  str = PMATH_C_STRING("IntegerDigits");
  obj = pmath_evaluate(pmath_option_value(PMATH_NULL, str, options));
  pmath_unref(str);
  if(pmath_is_int32(obj)) {
    initeger_digits_goal = PMATH_AS_INT32(obj);
    auto_exponent = FALSE;
  }
  else if(pmath_same(obj, pmath_System_Automatic)) {
    initeger_digits_goal = 0;
    auto_exponent = TRUE;
  }
  else {
    pmath_message(PMATH_NULL, "ioppfa", 2, PMATH_C_STRING("IntegerDigits"), obj);
    pmath_unref(value);
    pmath_unref(options);
    return expr;
  }
  
  str = PMATH_C_STRING("RadiusDigits");
  obj = pmath_evaluate(pmath_option_value(PMATH_NULL, str, options));
  pmath_unref(str);
  if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 0) {
    max_rad_digits = PMATH_AS_INT32(obj);
  }
  else if(pmath_same(obj, pmath_System_Automatic)) {
    // max_rad_digits = 3;
  }
  else if(pmath_equals(obj, _pmath_object_pos_infinity)) {
    max_rad_digits = INT_MAX;
  }
  else {
    pmath_message(PMATH_NULL, "ioppfa", 2, PMATH_C_STRING("RadiusDigits"), obj);
    pmath_unref(value);
    pmath_unref(options);
    return expr;
  }
  
  if(max_rad_digits == INT_MAX)  {
    if(base & (base-1)) { // not a power of 2
      pmath_message(PMATH_NULL, "loss", 1, INT(base));
      max_rad_digits = 5;
    }
  }
  
  pmath_unref(options);
  
  _pmath_mpfloat_get_raw_number_parts(&parts, value, base, max_rad_digits);
  pmath_unref(value);
  if(parts.mid_digits && parts.rad_digits) {
    pmath_string_t mid_exp;
    pmath_string_t mid_significant_truncated;
    pmath_string_t mid_insignificant;
    pmath_string_t mid_digits;
    pmath_string_t rad_digits;
    pmath_string_t rad_mantissa;
    pmath_string_t rad_exp_extra;
    pmath_string_t rad_exp;
    pmath_string_t prec;
    pmath_unref(expr);
    
    if(auto_exponent) {
      _pmath_raw_number_parts_set_decimal_point_automatic(&parts);
    }
    else {
      if(initeger_digits_goal <= parts.total_significant) {
        _pmath_raw_number_parts_set_decimal_point(&parts, initeger_digits_goal);
      }
      else {
        pmath_message(PMATH_NULL, "intdig", 2, INT(initeger_digits_goal), INT(parts.total_significant));
      }
    }
    
    str = PMATH_NULL;
    if(parts.is_negative)
      str = pmath_string_insert_latin1(str, INT_MAX, "-", 1);
      
    if(parts.base != 10) {
      _pmath_write_number_part(&parts, PMATH_NUMBER_PART_BASE, write_latin1_to_string, &str);
      str = pmath_string_insert_latin1(str, INT_MAX, "^^", 2);
    }
    
    mid_significant_truncated = PMATH_C_STRING("");
    _pmath_write_number_part(&parts, PMATH_NUMBER_PART_SIGNIFICANT, write_latin1_to_string, &mid_significant_truncated);
    str = pmath_string_concat(str, pmath_ref(mid_significant_truncated));
    
    mid_insignificant = PMATH_C_STRING("");
    _pmath_write_number_part(&parts, PMATH_NUMBER_PART_MID_INSIGNIFICANT_DIGITS, write_latin1_to_string, &mid_insignificant);
    
    rad_digits = PMATH_C_STRING("");
    _pmath_write_number_part(&parts, PMATH_NUMBER_PART_RADIUS_DIGITS, write_latin1_to_string, &rad_digits);
    
    rad_mantissa = PMATH_C_STRING("");
    _pmath_write_number_part(&parts, PMATH_NUMBER_PART_RADIUS_DIGITS_1, write_latin1_to_string, &rad_mantissa);
    
    rad_exp_extra = PMATH_C_STRING("");
    _pmath_write_number_part(&parts, PMATH_NUMBER_PART_RADIUS_EXTRA_EXPONENT_1, write_latin1_to_string, &rad_exp_extra);
    
    rad_exp = PMATH_C_STRING("");
    _pmath_write_number_part(&parts, PMATH_NUMBER_PART_RADIUS_EXPONENT_1, write_latin1_to_string, &rad_exp);
    
    if(parts.rad_digits[0] != '\0') {
      if(parts.needs_radius_exponent) {
        str = pmath_string_concat(str, pmath_ref(mid_insignificant));
        str = pmath_string_insert_latin1(str, INT_MAX, "[", 1);
      }
      else {
        str = pmath_string_insert_latin1(str, INT_MAX, "[", 1);
        str = pmath_string_concat(str, pmath_ref(mid_insignificant));
      }
      
      //str = pmath_string_insert_latin1(str, INT_MAX, "+/-", 3);
      str = pmath_string_insert_latin1(str, INT_MAX, "\xB1", 1); // \[PlusMinus]
      
      if(parts.needs_radius_exponent) {
        str = pmath_string_concat(str, pmath_ref(rad_mantissa));
        
        if(pmath_string_length(rad_exp_extra) > 0) {
          str = pmath_string_insert_latin1(str, INT_MAX, "*^", 2);
          str = pmath_string_concat(str, pmath_ref(rad_exp_extra));
        }
      }
      else {
        str = pmath_string_concat(str, pmath_ref(rad_digits));
      }
      
      str = pmath_string_insert_latin1(str, INT_MAX, "]", 1);
    }
    
    prec = PMATH_NULL;
    _pmath_write_precision(
      precision_bits / _pmath_log2_of(parts.base),
      precision_bits,
      write_latin1_to_string, 
      &prec);
    
    str = pmath_string_insert_latin1(str, INT_MAX, "`", 1);
    str = pmath_string_concat(str, pmath_ref(prec));
    
    mid_exp = PMATH_C_STRING("");
    _pmath_write_number_part(&parts, PMATH_NUMBER_PART_EXPONENT, write_latin1_to_string, &mid_exp);
    
    if(pmath_string_length(mid_exp) > 0) {
      str = pmath_string_insert_latin1(str, INT_MAX, "*^", 2);
      str = pmath_string_concat(str, pmath_ref(mid_exp));
    }
    
    mid_digits = pmath_string_concat(pmath_ref(mid_significant_truncated), pmath_ref(mid_insignificant));
    
    expr = pmath_expr_new_extended(
             pmath_ref(pmath_System_List), 12,
             RULE("String", str),
             RULE("Sign", parts.is_negative ? PMATH_C_STRING("-") : PMATH_C_STRING("")),
             RULE("Base", PMATH_FROM_INT32(parts.base)),
             RULE("MidpointMantissaTruncated",  mid_significant_truncated),
             RULE("MidpointMantissaRest", mid_insignificant),
             RULE("MidpointMantissa",  mid_digits),
             RULE("RadiusDigits", rad_digits),
             RULE("RadiusMantissa", rad_mantissa),
             RULE("RadiusExponentExtra", rad_exp_extra),
             RULE("RadiusExponent", rad_exp),
             RULE("Precision", prec),
             RULE("Exponent", mid_exp));
  }
  _pmath_raw_number_parts_clear(&parts);
  return expr;
}
