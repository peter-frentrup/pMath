#include <pmath-core/numbers.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>


extern pmath_symbol_t pmath_System_List;

struct reverse_info_t {
  long levelmin;
  long levelmax;
};

static pmath_t reverse(
  struct reverse_info_t *info,
  pmath_t                obj,  // will be freed
  long                   level
) {
  int reldepth = _pmath_object_in_levelspec(
                   obj, info->levelmin, info->levelmax, level);
                   
  if(reldepth <= 0 && pmath_is_expr(obj)) {
    size_t len = pmath_expr_length(obj);
    size_t i;
    
    for(i = len; i > 0; --i) {
      pmath_t item = pmath_expr_get_item(obj, i);
      obj = pmath_expr_set_item(obj, i, PMATH_NULL);
      
      item = reverse(info, item, level + 1);
      obj = pmath_expr_set_item(obj, i, item);
    }
  }
  
  if(reldepth == 0 && pmath_is_expr(obj)) {
    size_t i, len;
    
    len = pmath_expr_length(obj);
    for(i = len / 2; i > 0; --i) {
      pmath_t a = pmath_expr_get_item(obj, i);
      pmath_t b = pmath_expr_get_item(obj, len + 1 - i);
      
      obj = pmath_expr_set_item(obj, i,           b);
      obj = pmath_expr_set_item(obj, len + 1 - i, a);
    }
    
    return obj;
  }
  
  return obj;
}

PMATH_PRIVATE pmath_t builtin_reverse(pmath_expr_t expr) {
  /* Reverse(expr, levelspeclist)
     Reverse(expr, levelspec)  = Reverse(expr, {levelspec})
     Reverse(expr)             = Reverse(expr, {1})
  
     messages:
       General::level
       General::nexprat
   */
  struct reverse_info_t info;
  pmath_t obj;
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(obj)) {
    pmath_unref(obj);
    pmath_message(PMATH_NULL, "nexprat", 1, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  info.levelmin = 1;
  info.levelmax = 1;
  if(exprlen == 2) {
    pmath_t levels = pmath_expr_get_item(expr, 2);
    
    if(pmath_is_expr_of(levels, pmath_System_List)) {
      size_t i;
      
      for(i = 1; i <= pmath_expr_length(levels); ++i) {
        pmath_t leveli = pmath_expr_get_item(levels, i);
        info.levelmin = 1;
        info.levelmax = 1;
        if(!_pmath_extract_levels(leveli, &info.levelmin, &info.levelmax)) {
          pmath_unref(leveli);
          pmath_unref(obj);
          pmath_message(PMATH_NULL, "level", 1, levels);
          return expr;
        }
        
        pmath_unref(leveli);
        obj = reverse(&info, obj, 1);
      }
      
      pmath_unref(levels);
      pmath_unref(expr);
      return obj;
    }
    
    if(!_pmath_extract_levels(levels, &info.levelmin, &info.levelmax)) {
      pmath_unref(obj);
      pmath_message(PMATH_NULL, "level", 1, levels);
      return expr;
    }
    
    pmath_unref(levels);
  }
  
  pmath_unref(expr);
  return reverse(&info, obj, 1);
}
