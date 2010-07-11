#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/messages.h>

#include <pmath-util/concurrency/atomic.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/strings-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

static size_t bytecount(
  pmath_t obj // wont be freed
){
  if(PMATH_UNLIKELY(PMATH_IS_MAGIC(obj)))
    return 0;
  
  switch(obj->type_shift){
    case PMATH_TYPE_SHIFT_INTEGER:
      return abs(((struct _pmath_integer_t*)obj)->value->_mp_size) * sizeof(mp_limb_t)
           + sizeof(struct _pmath_integer_t);
    
    case PMATH_TYPE_SHIFT_QUOTIENT: 
      return bytecount((pmath_t)((struct _pmath_quotient_t*)obj)->numerator)
           + bytecount((pmath_t)((struct _pmath_quotient_t*)obj)->denominator)
           + sizeof(struct _pmath_quotient_t);
    
    case PMATH_TYPE_SHIFT_MP_FLOAT:
      return (((struct _pmath_mp_float_t*)obj)->value->_mpfr_prec + 8 * sizeof(mp_limb_t) - 1) / 8
           + sizeof(struct _pmath_mp_float_t);
    
    case PMATH_TYPE_SHIFT_MACHINE_FLOAT:
      return sizeof(struct _pmath_machine_float_t);
    
    case PMATH_TYPE_SHIFT_STRING:
      return LENGTH_TO_CAPACITY(((struct _pmath_string_t*)obj)->length) * sizeof(uint16_t)
           + STRING_HEADER_SIZE;
    
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
      size_t i, len, result;
      
      len = ((struct _pmath_unpacked_expr_t*)obj)->length;
      
      result = len * sizeof(pmath_t) 
             + sizeof(struct _pmath_unpacked_expr_t);
      
      for(i = 0;i <= len;++i){
        pmath_t item = pmath_expr_get_item(obj, i);
        
        result+= bytecount(item);
        
        pmath_unref(item);
      }
      
      return result;
    }
  }
  
  return 0;
}

PMATH_PRIVATE pmath_t builtin_bytecount(pmath_expr_t expr){
  pmath_t  obj;
  size_t          result;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  result = bytecount(obj);
  pmath_unref(obj);

  return pmath_integer_new_size(result);
}
