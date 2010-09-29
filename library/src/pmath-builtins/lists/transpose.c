#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

/*struct transpose_info_t{
  pmath_t  tensor;
  size_t   depth;
  size_t  *tensor_dimensions;
  size_t  *permutation; // result[i1, i2, ...] := tensor[permutation(i1, i2, ...)]
  
  size_t  *tensor_indices;
};

static pmath_t transpose(
  struct transpose_info_t *info,
  size_t                   level
){
  pmath_t list;
  size_t i, len, orig_level;
  
  if(level > info->depth)
    return _pmath_tensor_get(info->tensor, info->tensor_indices);
  
  orig_level = info->permutation[level];
  len = info->tensor_dimensions[orig_level];
  list = pmath_expr_new(
    pmath_expr_get_item(tensor, 0), 
    i);
  
  for(i = 1;i <= len;++i){
    info->tensor_indices[orig_level] = i;
    pmath_expr_set_item(list, i, transpose(info, level + 1));
  }
  
  return list;
}

static pmath_bool_t expr_to_inverse_permutation(
  pmath_expr_t   perm_in,  // wont be freed
  size_t        *perm_out  // array of same size
){
  size_t i, depth;
  
  depth = pmath_expr_length(perm_in);
  
  memset(perm_out, 0, depth * sizeof(size_t));
  
  for(i = depth;i > 0;--i){
    pmath_t item = pmath_expr_get_item(perm_in, i);
    
    if(!pmath_instance_of(item, PMATH_TYPE_INTEGER)
    || pmath_number_sign(item) <= 0){
      pmath_message(
        PMATH_SYMBOL_TRANSPOSE, "perm1", 2, 
        item,
        pmath_ref(perm_in));
      return FALSE;
    }
    
    if(pmath_integer_fits_ui(item)){
      size_t j = pmath_integer_get_ui(item);
      
      if(j <= depth){
        perm_out[j] = i;
        pmath_unref(item);
        continue;
      }
    }
    
    pmath_message(
      PMATH_SYMBOL_TRANSPOSE, "perm2", 2, 
      item,
      pmath_ref(perm_in));
    return FALSE;
  }
  
  return TRUE;
}*/

pmath_t builtin_transpose(pmath_expr_t expr){
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 2){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  return expr;
}
