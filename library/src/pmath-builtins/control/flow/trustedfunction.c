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


extern pmath_symbol_t pmath_System_HoldComplete;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_MessageName;
extern pmath_symbol_t pmath_System_SecurityException;
extern pmath_symbol_t pmath_System_TrustedFunction;

struct _pmath_trusted_function_t {
  pmath_security_level_t  min_level; ///< trusted function can be used if ambient security level is at least min_level
  pmath_security_level_t  temporary_higher_level;
};

static void trusted_function_destructor(void *_data);
static pmath_custom_t create_trust_certificate(pmath_t expr, const struct _pmath_trusted_function_t *data);
static const struct _pmath_trusted_function_t *check_trust_certificate(pmath_custom_t cert, pmath_t expr, pmath_t msg_arg); // arguments wont be freed


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
  
  pmath_expr_t cert = pmath_expr_new_extended(pmath_ref(pmath_System_HoldComplete), 1,
    pmath_integer_new_ui32(pmath_hash(func_data))); // the hash contents is actually not relevant. Only the cert_custom will be checked.
  _pmath_expr_attach_custom_metadata(cert, trusted_function_destructor, cert_custom);
    
  
  pmath_unref(expr);
  expr = pmath_expr_new_extended(pmath_ref(pmath_System_TrustedFunction), 2, func_data, cert);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_call_trustedfunction(pmath_expr_t expr) {
  /* TrustedFunction({func, forlevel, tmphigherlevel}, cert)(args)
   */
  
  pmath_t head = pmath_expr_get_item(expr, 0);
  if(pmath_is_expr_of(head, pmath_System_TrustedFunction)) {
    pmath_t func_data = pmath_expr_get_item(head, 1);
    if(!pmath_is_expr_of(func_data, pmath_System_List)) {
      pmath_unref(func_data);
      pmath_unref(head);
      return expr;
    }
    
    pmath_expr_t cert = pmath_expr_get_item(head, 2);
    pmath_custom_t cert_custom = PMATH_NULL;
    if(pmath_is_expr_of(cert, pmath_System_HoldComplete)) {
      cert_custom = _pmath_expr_get_custom_metadata(cert, trusted_function_destructor);
    }
    
    pmath_unref(cert);
    const struct _pmath_trusted_function_t *cert_data = check_trust_certificate(cert_custom, func_data, expr);
    if(!cert_data) {
      pmath_unref(cert_custom);
      pmath_unref(func_data);
      pmath_unref(head);
      return expr;
    }
    
    pmath_t func = pmath_expr_get_item(func_data, 1);
    expr = pmath_expr_set_item(expr, 0, func);
    
    expr = pmath_evaluate_secured(expr, cert_data->temporary_higher_level);
    
    pmath_unref(cert_custom);
    pmath_unref(func_data);
  }
  pmath_unref(head);
  
  return expr;
}

volatile unsigned trust_dummy;
static void trusted_function_destructor(void *_data) {
  struct _pmath_trusted_function_t *data = _data;
  
  trust_dummy+= 1; // Access a unique memory location to ensure that COMDAT folding does not kick in and this remains unique.
  
  pmath_mem_free(data);
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

static const struct _pmath_trusted_function_t *check_trust_certificate(pmath_custom_t cert, pmath_t expr, pmath_t msg_arg) {
  if(!pmath_custom_has_destructor(cert, trusted_function_destructor)) { // no certificate
    pmath_throw(
      pmath_expr_new_extended(pmath_ref(pmath_System_SecurityException), 2,
        pmath_expr_new_extended(pmath_ref(pmath_System_MessageName), 2, 
          pmath_ref(pmath_System_TrustedFunction),
          PMATH_C_STRING("nocert")),
        pmath_ref(msg_arg)));
    return NULL;
  }
  
  pmath_t orig = pmath_custom_get_attached_object(cert);
  if(!pmath_equals(orig, expr)) {
    pmath_throw(
      pmath_expr_new_extended(pmath_ref(pmath_System_SecurityException), 2,
        pmath_expr_new_extended(pmath_ref(pmath_System_MessageName), 2, 
          pmath_ref(pmath_System_TrustedFunction),
          PMATH_C_STRING("badcert")),
        pmath_ref(msg_arg)));
    pmath_unref(orig);
    return NULL;
  }
  pmath_unref(orig);
  
  const struct _pmath_trusted_function_t *cert_data = pmath_custom_get_data(cert);
  if(!pmath_security_check(cert_data->min_level, msg_arg)) {
    return NULL;
  }
  
  return cert_data;
}
