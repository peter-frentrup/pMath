#include <pmath-core/expressions.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

static pmath_bool_t part(
  pmath_expr_t  *list, 
  pmath_expr_t   position, 
  size_t         position_start
){
  pmath_t pos;
  size_t i, listlen, max_position_start;
  
  max_position_start = pmath_expr_length(position);
  
  for(;;){
    if(position_start > max_position_start)
      return TRUE;
    
    if(!pmath_instance_of(*list, PMATH_TYPE_EXPRESSION)){
      pmath_message(NULL, "partd", 1, pmath_ref(position));
      return FALSE;
    }
    
    listlen = pmath_expr_length(*list);
    
    pos = pmath_expr_get_item(position, position_start);
    if(!pmath_instance_of(pos, PMATH_TYPE_INTEGER))
      break;
    
    i = SIZE_MAX;
    if(!extract_number(pos, listlen, &i)){
      pmath_message(NULL, "pspec", 1, pos);
      return FALSE;
    }
    if(i > listlen){
      pmath_message(NULL, "partw", 2, pmath_ref(*list), pos);
      return FALSE;
    }
    
    pmath_unref(pos);
    {
      pmath_expr_t tmp = *list;
      *list = (pmath_expr_t)pmath_expr_get_item(tmp, i);
      pmath_unref(tmp);
    }
    
    // end recursion: return part(list, position, position_start + 1);
    ++position_start;
  }
  
  if(pmath_is_expr_of(pos, PMATH_SYMBOL_RANGE)){
    pmath_t old;
    pmath_bool_t reverse = FALSE;
    size_t start_index = 1;
    size_t end_index = listlen;
    
    if(!extract_range(pos, &start_index, &end_index, TRUE)){
      pmath_message(NULL, "pspec", 1, pos);
      return FALSE;
    }
    
    if(start_index > listlen || end_index > listlen){
      pmath_message(NULL, "partw", 2, pmath_ref(*list), pos);
      return FALSE;
    }
    
    if(start_index > end_index){
      size_t t = start_index;
      start_index = end_index;
      end_index = t;
      
      reverse = TRUE;
    }
    
    old = *list;
    *list = pmath_expr_get_item_range(
      old, start_index, end_index + 1 - start_index);
    
    pmath_unref(old);
    
    if(reverse){
      listlen = pmath_expr_length(*list);
      
      for(i = 1;i <= listlen/2;i++){
        pmath_t first = pmath_expr_get_item(*list, i);
        pmath_t last  = pmath_expr_get_item(*list, listlen-i+1);
        *list = pmath_expr_set_item(*list, i,           last);
        *list = pmath_expr_set_item(*list, listlen-i+1, first);
      }
    }
  }
  else if(pmath_is_expr_of(pos, PMATH_SYMBOL_LIST)){
    size_t poslen = pmath_expr_length((pmath_expr_t)pos);
    
    for(i = 1;i <= poslen;++i){
      pmath_t subpos = pmath_expr_get_item((pmath_expr_t)pos, i);
      size_t index = SIZE_MAX;
      
      if(!extract_number(subpos, listlen, &index)){
        pmath_message(NULL, "pspec", 1, subpos);
        pmath_unref(pos);
        return FALSE;
      }
      
      if(index > listlen){
        pmath_message(NULL, "partw", 2, 
          pmath_ref(*list), 
          subpos);
        pmath_unref(pos);
        return FALSE;
      }
      
      pmath_unref(subpos);
      
      pos = pmath_expr_set_item((pmath_expr_t)pos, i, 
        pmath_expr_get_item(*list, index));
    }
    pos = pmath_expr_set_item((pmath_expr_t)pos, 0, 
      pmath_expr_get_item(*list, 0));
    pmath_unref(*list);
    *list = pos;
    pos = NULL;
  }
  else if(pos != PMATH_SYMBOL_ALL){
    pmath_message(NULL, "pspec", 1, pos);
    return FALSE;
  }
  
  pmath_unref(pos);
  
  if(position_start < pmath_expr_length(position)){
    listlen = pmath_expr_length(*list);
    ++position_start;
    
    for(i = 1;i <= listlen;++i){
      pmath_expr_t item = 
        (pmath_expr_t)pmath_expr_get_item(*list, i);
      
      if(!part(&item, position, position_start)){
        pmath_unref(item);
        return FALSE;
      }
      
      *list = pmath_expr_set_item(*list, i, item);
    }
  }
  
  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_extract(pmath_expr_t expr){
  pmath_expr_t list, part_spec;
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2 || exprlen > 3){
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  part_spec = pmath_expr_get_item(expr, 2);
  
  if(!pmath_is_expr_of(part_spec, PMATH_SYMBOL_LIST))
    part_spec = pmath_build_value("(o)", part_spec);
  
  list = pmath_expr_get_item(expr, 1);
  if(!part(&list, part_spec, 1)){
    pmath_unref(list);
    pmath_unref(part_spec);
    return expr;
  }
  
  pmath_unref(part_spec);
  
  if(exprlen == 3){
    list = pmath_expr_new_extended(
      pmath_expr_get_item(expr, 3), 1,
      list);
  }
  
  pmath_unref(expr);
  return list;
}

PMATH_PRIVATE pmath_t builtin_part(pmath_expr_t expr){
  pmath_expr_t list;
  
  if(pmath_expr_length(expr) < 1)
    return expr;
  
  list = pmath_expr_get_item(expr, 1);
  if(!part(&list, expr, 2)){
    pmath_unref(list);
    return expr;
  }
  
  pmath_unref(expr);
  return list;
}
