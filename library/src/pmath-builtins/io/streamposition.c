#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>
#include <pmath-util/files.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/io-private.h>


PMATH_PRIVATE pmath_t builtin_streamposition(pmath_expr_t expr) {
  pmath_t file;
  int64_t pos;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  file = pmath_expr_get_item(expr, 1);
  if(!_pmath_file_check(file, 0)) {
    pmath_unref(file);
    return expr;
  }
  
  pmath_unref(expr);
  
  pos = pmath_file_get_position(file);
  pmath_unref(file);
  
  if(pos < 0)
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    
  return pmath_integer_new_si64(pos);
}


PMATH_PRIVATE pmath_t builtin_setstreamposition(pmath_expr_t expr) {
  pmath_t file, posobj;
  int64_t newpos;
  pmath_bool_t success = FALSE;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  file = pmath_expr_get_item(expr, 1);
  if(!_pmath_file_check(file, 0)) {
    pmath_unref(file);
    return expr;
  }
  
  posobj = pmath_expr_get_item(expr, 2);
  
  if(pmath_equals(posobj, _pmath_object_infinity)) {
    success = pmath_file_set_position(
                file,
                0,
                SEEK_END);
  }
  else if(pmath_is_integer(posobj)) {
    if(pmath_integer_fits_si64(posobj) && pmath_number_sign(posobj) >= 0) {
      int64_t offset = pmath_integer_get_si64(posobj);
      
      if(offset < 0)
        success = pmath_file_set_position(file, offset, SEEK_END);
      else
        success = pmath_file_set_position(file, offset, SEEK_SET);
    }
    else {
      success = FALSE;
    }
  }
  else {
    pmath_message(PMATH_NULL, "int", 2,
                  pmath_ref(expr),
                  PMATH_FROM_INT32(2));
                  
    pmath_unref(file);
    pmath_unref(posobj);
    return expr;
  }
  
  newpos = pmath_file_get_position(file);
  pmath_unref(file);
  pmath_unref(posobj);
  pmath_unref(expr);
    
  if(!success) {
    pmath_message(PMATH_NULL, "stmrng", 2,
      pmath_ref(posobj), 
      pmath_ref(file));
  }
  
  if(newpos < 0)
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    
  return pmath_integer_new_si64(newpos);
}
