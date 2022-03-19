#include <pmath-core/expressions.h>

#include <pmath-util/dispatch-tables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-language/patterns-private.h> // for PMATH_MAGIC_PATTERN_SEQUENCE

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>


PMATH_PRIVATE pmath_t _pmath_string_keyabsent;  /* readonly */

extern pmath_symbol_t pmath_System_Key;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Missing;

static pmath_t peel_off_key_head(pmath_t key) {
  if(pmath_is_expr_of(key, pmath_System_Key)) {
    if(pmath_expr_length(key) == 1) {
      pmath_t arg = pmath_expr_get_item(key, 1);
      pmath_unref(key);
      return arg;
    }
    
    return pmath_expr_set_item(key, 0, PMATH_MAGIC_PATTERN_SEQUENCE);
  }
  
  return key;
}

static pmath_t missing_key_result(
  pmath_t key,          // will be freed
  pmath_t default_value // won't be freed
) {
  if(!pmath_same(default_value, PMATH_UNDEFINED)) {
    pmath_unref(key);
    return pmath_ref(default_value);
  }
  
  if(pmath_is_expr_of(key, pmath_System_List)) {
    size_t len = pmath_expr_length(key);
    size_t i;
    for(i = len; i > 0; --i) {
      pmath_t obj = pmath_expr_extract_item(key, i);
      obj = peel_off_key_head(obj);
      obj = missing_key_result(obj, PMATH_UNDEFINED);
      key = pmath_expr_set_item(key, i, obj);
    }
    return key;
  }
  
  // The possible Key(...) head was already peeled of by lookup(), so don't 
  // call peel_off_key_head() again. However, for multi-argument Key, we need to add the
  // Key head again instead of the internal `PMATH_MAGIC_PATTERN_SEQUENCE`.
  
  if(pmath_is_expr_of(key, PMATH_MAGIC_PATTERN_SEQUENCE)) {
    key = pmath_expr_set_item(key, 0, pmath_ref(pmath_System_Key));
  }
  
  return pmath_expr_new_extended(
           pmath_ref(pmath_System_Missing), 2, pmath_ref(_pmath_string_keyabsent), key);
}

// frees rules and key, but not default_value
static pmath_t lookup(pmath_t rules, pmath_t key, pmath_t default_value, pmath_bool_t *failure_flag) {
  size_t i, len;
  pmath_t obj;
  
  if(!pmath_is_expr_of(rules, pmath_System_List)) {
    *failure_flag = TRUE;
    pmath_unref(rules);
    pmath_unref(key);
    return PMATH_NULL;
  }
  
  len = pmath_expr_length(rules);
  if(len == 0) {
    pmath_unref(rules);
    return missing_key_result(key, default_value);
  }
  
  obj = pmath_expr_get_item(rules, 1);
  if(pmath_is_rule(obj)) {
    pmath_unref(obj);
    
    if(!pmath_is_list_of_rules(rules)) {
      *failure_flag = TRUE;
      pmath_unref(rules);
      pmath_unref(key);
      return PMATH_NULL;
    }
    
    if(pmath_is_expr_of(key, pmath_System_List)) {
      len = pmath_expr_length(key);
      for(i = 1; i <= len && !*failure_flag; ++i) {
        obj = pmath_expr_extract_item(key, i);
        obj = lookup(pmath_ref(rules), obj, default_value, failure_flag);
        key = pmath_expr_set_item(key, i, obj);
      }
      pmath_unref(rules);
      return key;
    }
    
    key = peel_off_key_head(key);
    
    obj = PMATH_UNDEFINED;
    if(!pmath_rules_lookup(rules, pmath_ref(key), &obj)) {
      pmath_unref(obj);
      obj = missing_key_result(key, default_value);
    }
    else
      pmath_unref(key);
    
    pmath_unref(rules);
    return obj;
  }
  else {
    pmath_unref(obj);
    
    for(i = 1; i <= len && !*failure_flag; ++i) {
      obj = pmath_expr_extract_item(rules, i);
      obj = lookup(obj, pmath_ref(key), default_value, failure_flag);
      rules = pmath_expr_set_item(rules, i, obj);
    }
    
    pmath_unref(key);
    return rules;
  }
}

PMATH_PRIVATE pmath_t builtin_lookup(pmath_expr_t expr) {
  /*  Lookup(rules, key)                 = Lookup(rules, key, Missing("KeyAbsent")
      Lookup(rules, key, default_value)
  
      Lookup(rules, {key1, key2, ...})
      Lookup(rules, Key(...))
   */
  size_t exprlen = pmath_expr_length(expr);
  pmath_t rules;
  pmath_t key;
  pmath_t default_value;
  pmath_t value;
  pmath_bool_t failure_flag;
  
  if(exprlen < 2 || exprlen > 3) {
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  rules = pmath_expr_get_item(expr, 1);
  key = pmath_expr_get_item(expr, 2);
  if(exprlen == 3)
    default_value = pmath_expr_get_item(expr, 3);
  else
    default_value = PMATH_UNDEFINED;
    
  failure_flag = FALSE;
  value = lookup(rules, key, default_value, &failure_flag);
  pmath_unref(default_value);
  
  if(failure_flag) {
    pmath_unref(value);
    pmath_message(PMATH_NULL, "reps", 1, pmath_expr_get_item(expr, 1));
    return expr;
  }
  
  pmath_unref(expr);
  return value;
}

PMATH_PRIVATE pmath_t builtin_call_list(pmath_expr_t expr) {
  /* {lhs->rhs, ...}(...)
   */
  
  pmath_bool_t failure_flag = FALSE;
  pmath_t head = pmath_expr_get_item(expr, 0);
  pmath_t args = pmath_expr_set_item(pmath_ref(expr), 0, pmath_ref(pmath_System_Key));
  pmath_t result = lookup(head, args, expr, &failure_flag);
  
  if(failure_flag) {
    pmath_unref(result);
    pmath_message(PMATH_NULL, "reps", 1, pmath_expr_get_item(expr, 0));
    return expr;
  }
  
  pmath_unref(expr);
  return result;
}
