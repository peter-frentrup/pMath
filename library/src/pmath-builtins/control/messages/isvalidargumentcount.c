#include <pmath-core/numbers-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_isvalidargumentcount(pmath_expr_t expr){
 /* IsValidArgumentCount(head, len, min, max)
  */
  pmath_t head = pmath_expr_get_item(expr, 1);
  pmath_t len  = pmath_expr_get_item(expr, 2);
  pmath_t min  = pmath_expr_get_item(expr, 3);
  pmath_t max  = pmath_expr_get_item(expr, 4);
  
  
  if(pmath_instance_of(head, PMATH_TYPE_SYMBOL)
  && pmath_instance_of(len, PMATH_TYPE_INTEGER)
  && pmath_instance_of(min, PMATH_TYPE_INTEGER)
  && pmath_integer_fits_ui(len)
  && pmath_integer_fits_ui(min)){
    size_t z_len = pmath_integer_get_ui(len);
    size_t z_min = pmath_integer_get_ui(min);
    
    if(pmath_instance_of(max, PMATH_TYPE_INTEGER)
    && pmath_integer_fits_ui(max)
    && pmath_compare(min, max) <= 0){
      size_t z_max = pmath_integer_get_ui(max);
      
      if(z_min <= z_len && z_len <= z_max){
        pmath_unref(expr);
        expr = pmath_ref(PMATH_SYMBOL_TRUE);
      }
      else{
        pmath_thread_t thread = pmath_thread_get_current();
        if(thread && thread->stack_info){
          pmath_t old_head = thread->stack_info->value;
          thread->stack_info->value = head;
          
          pmath_message_argxxx(z_len, z_min, z_max);
          
          thread->stack_info->value = old_head;
        }
        
        pmath_unref(expr);
        expr = pmath_ref(PMATH_SYMBOL_FALSE);
      }
    }
    else if(pmath_equals(max, _pmath_object_infinity)){
      if(z_min <= z_len){
        pmath_unref(expr);
        expr = pmath_ref(PMATH_SYMBOL_TRUE);
      }
      else{
        pmath_thread_t thread = pmath_thread_get_current();
        if(thread && thread->stack_info){
          pmath_t old_head = thread->stack_info->value;
          thread->stack_info->value = head;
          
          pmath_message_argxxx(z_len, z_min, SIZE_MAX);
          
          thread->stack_info->value = old_head;
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
