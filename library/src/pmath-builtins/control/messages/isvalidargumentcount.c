#include <pmath-core/numbers-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_isvalidargumentcount(pmath_expr_t expr) {
  /* IsValidArgumentCount(head, len, min, max)
   */
  pmath_t head = pmath_expr_get_item(expr, 1);
  pmath_t len  = pmath_expr_get_item(expr, 2);
  pmath_t min  = pmath_expr_get_item(expr, 3);
  pmath_t max  = pmath_expr_get_item(expr, 4);
  
  if( pmath_is_symbol(head)    &&
      pmath_is_int32(len)      &&
      pmath_is_int32(min)      &&
      PMATH_AS_INT32(len) >= 0 &&
      PMATH_AS_INT32(min) >= 0)
  {
    size_t z_len = (size_t)PMATH_AS_INT32(len);
    size_t z_min = (size_t)PMATH_AS_INT32(min);
    
    if( pmath_is_int32(max)      &&
        PMATH_AS_INT32(max) >= 0 &&
        pmath_compare(min, max) <= 0)
    {
      size_t z_max = (size_t)PMATH_AS_INT32(max);
      
      if(z_min <= z_len && z_len <= z_max) {
        pmath_unref(expr);
        expr = pmath_ref(PMATH_SYMBOL_TRUE);
      }
      else {
        pmath_thread_t thread = pmath_thread_get_current();
        
        if(thread && thread->stack_info) {
          pmath_t old_head = thread->stack_info->head;
          thread->stack_info->head = head;
          
          pmath_message_argxxx(z_len, z_min, z_max);
          
          thread->stack_info->head = old_head;
        }
        
        pmath_unref(expr);
        expr = pmath_ref(PMATH_SYMBOL_FALSE);
      }
    }
    else if(pmath_equals(max, _pmath_object_infinity)) {
      if(z_min <= z_len) {
        pmath_unref(expr);
        expr = pmath_ref(PMATH_SYMBOL_TRUE);
      }
      else {
        pmath_thread_t thread = pmath_thread_get_current();
        if(thread && thread->stack_info) {
          pmath_t old_head = thread->stack_info->head;
          thread->stack_info->head = head;
          
          pmath_message_argxxx(z_len, z_min, SIZE_MAX);
          
          thread->stack_info->head = old_head;
        }
        
        pmath_unref(expr);
        expr = pmath_ref(PMATH_SYMBOL_FALSE);
      }
    }
  }
  
  pmath_unref(head);
  pmath_unref(len);
  pmath_unref(min);
  pmath_unref(max);
  return expr;
}
