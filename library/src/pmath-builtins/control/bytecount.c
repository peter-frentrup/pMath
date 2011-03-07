#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

static size_t bytecount(
  pmath_t obj // wont be freed
){
  if(PMATH_UNLIKELY(pmath_is_magic(obj)))
    return 0;
  
  switch(PMATH_AS_PTR(obj)->type_shift){
    case PMATH_TYPE_SHIFT_INTEGER:
      return abs(PMATH_AS_MPZ(obj)->_mp_size) * sizeof(mp_limb_t)
           + sizeof(struct _pmath_integer_t_);
    
    case PMATH_TYPE_SHIFT_QUOTIENT: 
      return bytecount(PMATH_QUOT_NUM(obj))
           + bytecount(PMATH_QUOT_DEN(obj))
           + sizeof(struct _pmath_quotient_t_);
    
    case PMATH_TYPE_SHIFT_MP_FLOAT:
      return (PMATH_AS_MP_VALUE(obj)->_mpfr_prec + 8 * sizeof(mp_limb_t) - 1) / 8
           + sizeof(struct _pmath_mp_float_t);
    
    case PMATH_TYPE_SHIFT_MACHINE_FLOAT:
      return sizeof(struct _pmath_machine_float_t);
    
    case PMATH_TYPE_SHIFT_STRING:
      return LENGTH_TO_CAPACITY(pmath_string_length(obj)) * sizeof(uint16_t)
           + STRING_HEADER_SIZE;
    
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
      size_t i, len, result;
      
      len = pmath_expr_length(obj);
      result = len * sizeof(pmath_t) + sizeof(struct _pmath_unpacked_expr_t);
      
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
  size_t   result;

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
