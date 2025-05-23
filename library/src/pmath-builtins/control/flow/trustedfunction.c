#include <pmath-util/evaluation.h>

#include <pmath-core/custom.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/security-private.h>

#include <string.h>


extern pmath_symbol_t pmath_System_HoldComplete;
extern pmath_symbol_t pmath_System_HoldForm;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Message;
extern pmath_symbol_t pmath_System_MessageName;
extern pmath_symbol_t pmath_System_SecurityException;
extern pmath_symbol_t pmath_System_TrustedFunction;

struct _pmath_trusted_function_t {
  pmath_security_level_t  min_level; ///< trusted function can be used if ambient security level is at least min_level
  pmath_security_level_t  temporary_higher_level;
};

static void trusted_function_destructor(void *_data);
static pmath_custom_t decode_trust_certificate(pmath_t cert); // cert will be freed
static pmath_custom_t create_trust_certificate(pmath_t expr, const struct _pmath_trusted_function_t *data);
static const struct _pmath_trusted_function_t *check_trust_certificate(pmath_custom_t cert, pmath_t expr, pmath_t msg_arg, pmath_bool_t do_throw); // arguments wont be freed


PMATH_PRIVATE pmath_t builtin_internal_maketrustedfunction(pmath_expr_t expr) {
  /* Internal`MakeTrustedFunction(func, forlevel)
   */
  pmath_thread_t me = pmath_thread_get_current();
  if(!me)
    return expr;
  
  struct _pmath_trusted_function_t data;
  data.temporary_higher_level = me->security_level;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  pmath_t forlevel_obj = pmath_expr_get_item(expr, 2);
  if(!_pmath_security_level_from_expr(&data.min_level, forlevel_obj)) {
    pmath_message(PMATH_NULL, "seclvl", 1, forlevel_obj);
    return expr;
  }
  
  pmath_t higher_level_obj = _pmath_security_level_to_expr(data.temporary_higher_level);
  if(PMATH_SECURITY_REQUIREMENT_MATCHES_LEVEL(data.temporary_higher_level, data.min_level)) {
    pmath_message(PMATH_NULL, "neednotrust", 2, pmath_ref(forlevel_obj), pmath_ref(higher_level_obj));
  }
  
  // TODO: warn about every non-Protected symbol within `func`
  pmath_t func = pmath_expr_extract_item(expr, 1);
  pmath_t func_data = pmath_expr_new_extended(
                        pmath_ref(pmath_System_List), 3, 
                        func, 
                        forlevel_obj, 
                        higher_level_obj);
  
  pmath_custom_t cert_custom = create_trust_certificate(func_data, &data);
  // That would create a reference cycle of a kind the garbage collector cannot detect:
  
  pmath_symbol_t cert = pmath_symbol_create_temporary(PMATH_C_STRING("System`TrustedFunction`cert"), TRUE);
  pmath_symbol_set_value(cert, cert_custom);
  
  pmath_unref(expr);
  expr = pmath_expr_new_extended(pmath_ref(pmath_System_TrustedFunction), 2, func_data, cert);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_trustedfunction(pmath_expr_t expr) {
  /* TrustedFunction({func, forlevel, tmphigherlevel}, cert)
   */
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  pmath_t func_data = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of_len(func_data, pmath_System_List, 3)) {
    pmath_message(PMATH_NULL, "list", 2, pmath_ref(expr), PMATH_FROM_INT32(1));
    pmath_unref(func_data);
    return expr;
  }
  
  pmath_custom_t cert_custom = decode_trust_certificate(pmath_expr_get_item(expr, 2));
  if(!check_trust_certificate(cert_custom, func_data, expr, FALSE)) {
    pmath_unref(cert_custom);
    pmath_unref(func_data);
    return expr;
  }
  
  pmath_atomic_or_uint8(&PMATH_AS_PTR(expr)->flags8, PMATH_OBJECT_FLAGS8_VALID); // SetValid() for easy check by formatting functions.
  pmath_unref(cert_custom);
  pmath_unref(func_data);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_call_trustedfunction(pmath_expr_t expr) {
  /* TrustedFunction({func, forlevel, tmphigherlevel}, cert)(args)
   */
  pmath_thread_t me = pmath_thread_get_current();
  if(!me)
    return expr;
  
  pmath_t head = pmath_expr_get_item(expr, 0);
  if(pmath_is_expr_of(head, pmath_System_TrustedFunction)) {
    pmath_t func_data = pmath_expr_get_item(head, 1);
    if(!pmath_is_expr_of(func_data, pmath_System_List)) {
      pmath_unref(func_data);
      pmath_unref(head);
      return expr;
    }
    
    pmath_custom_t cert_custom = decode_trust_certificate(pmath_expr_get_item(head, 2));
    const struct _pmath_trusted_function_t *cert_data = check_trust_certificate(cert_custom, func_data, expr, TRUE);
    if(!cert_data) {
      pmath_unref(cert_custom);
      pmath_unref(func_data);
      pmath_unref(head);
      return expr;
    }
    
    pmath_t func = pmath_expr_get_item(func_data, 1);
    expr = pmath_expr_set_item(expr, 0, func);
    
    { // Note that pmath_evaluate_secured() cannot increase security level.
      pmath_security_level_t old_level = me->security_level;
      me->security_level = cert_data->temporary_higher_level;
      
      expr = pmath_evaluate(expr);
      
      me->security_level = old_level;
    }
    
    pmath_unref(cert_custom);
    pmath_unref(func_data);
  }
  pmath_unref(head);
  
  return expr;
}

volatile unsigned trust_dummy;
static void trusted_function_destructor(void *_data) {
  struct _pmath_trusted_function_t *data = _data;
  
  trust_dummy+= 1; // Access a unique memory location to ensure that COMDAT folding does not kick in and this function remains unique.
  
  pmath_mem_free(data);
}

static pmath_custom_t decode_trust_certificate(pmath_t cert) { // cert will be freed
  if(!pmath_is_symbol(cert)) {
    pmath_unref(cert);
    return PMATH_NULL;
  }
  
  pmath_custom_t cert_custom = pmath_symbol_get_value(cert);
  if(!pmath_is_custom(cert_custom) || !pmath_custom_has_destructor(cert_custom, trusted_function_destructor)) {
    pmath_unref(cert_custom);
    pmath_unref(cert);
    return PMATH_NULL;
  }
  
  pmath_unref(cert);
  return cert_custom;
}

static pmath_custom_t create_trust_certificate(pmath_t func_data, const struct _pmath_trusted_function_t *data) { // expr wont be freed
  struct _pmath_trusted_function_t *data_copy;
  
  data_copy = pmath_mem_alloc(sizeof(struct _pmath_trusted_function_t));
  if(!data_copy) {
    return PMATH_NULL;
  }
  
  memcpy(data_copy, data, sizeof(struct _pmath_trusted_function_t));
  return pmath_custom_new_with_object(data_copy, trusted_function_destructor, pmath_ref(func_data));
}

static const struct _pmath_trusted_function_t *check_trust_certificate(pmath_custom_t cert, pmath_t expr, pmath_t msg_arg, pmath_bool_t do_throw) {
  if(!pmath_custom_has_destructor(cert, trusted_function_destructor)) { // no certificate
    pmath_t msg = pmath_expr_new_extended(
                    pmath_ref(do_throw ? pmath_System_SecurityException : pmath_System_Message), 2,
                    pmath_expr_new_extended(pmath_ref(pmath_System_MessageName), 2, 
                      pmath_ref(pmath_System_TrustedFunction),
                      PMATH_C_STRING("nocert")),
                    pmath_expr_new_extended(pmath_ref(pmath_System_HoldForm), 1, pmath_ref(msg_arg)));
    if(do_throw)
      pmath_throw(msg);
    else
      pmath_unref(pmath_evaluate(msg));
    return NULL;
  }
  
  pmath_t orig = pmath_custom_get_attached_object(cert);
  if(!pmath_equals(orig, expr)) {
    pmath_t msg = pmath_expr_new_extended(
                    pmath_ref(do_throw ? pmath_System_SecurityException : pmath_System_Message), 2,
                    pmath_expr_new_extended(pmath_ref(pmath_System_MessageName), 2, 
                      pmath_ref(pmath_System_TrustedFunction),
                      PMATH_C_STRING("badcert")),
                    pmath_expr_new_extended(pmath_ref(pmath_System_HoldForm), 1, pmath_ref(msg_arg)));
    if(do_throw)
      pmath_throw(msg);
    else
      pmath_unref(pmath_evaluate(msg));
    pmath_unref(orig);
    return NULL;
  }
  pmath_unref(orig);
  
  const struct _pmath_trusted_function_t *cert_data = pmath_custom_get_data(cert);
  if(do_throw) {
    if(!pmath_security_check(cert_data->min_level, msg_arg)) {
      return NULL;
    }
  }
  
  return cert_data;
}
