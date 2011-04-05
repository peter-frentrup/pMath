#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers.h>

#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


struct make_rect_t{
  int           depth;
  size_t       *lengths;
  pmath_bool_t  padleft;
};

static int estimate_rect_depth(pmath_t array, pmath_bool_t *maybe_bigger){
  size_t i;
  int depth = 0;
  
  *maybe_bigger = FALSE;
  if(!pmath_is_expr_of(array, PMATH_SYMBOL_LIST))
    return 0;
  
  if(pmath_expr_length(array) == 0){
    *maybe_bigger = TRUE;
    return 1;
  }
  
  for(i = pmath_expr_length(array);i > 0;--i){
    pmath_t obj = pmath_expr_get_item(array, i);
    int depth2 = estimate_rect_depth(obj, maybe_bigger);
    pmath_unref(obj);
    
    if(!*maybe_bigger)
      return 1 + depth2;
    
    if(depth < depth2)
       depth = depth2;
  }
  
  return 1 + depth;
}

static void init_rect_length(struct make_rect_t *info, pmath_t array, int level){
  size_t i;
  
  if(level >= info->depth)
    return;
  
  if(!pmath_is_expr_of(array, PMATH_SYMBOL_LIST)){
    info->depth = level;
    return;
  }
  
  if(info->lengths[level] < pmath_expr_length(array))
     info->lengths[level] = pmath_expr_length(array);
  
  ++level;
  for(i = pmath_expr_length(array);i > 0 && level < info->depth;--i){
    pmath_t item = pmath_expr_get_item(array, i);
    init_rect_length(info, item, level);
    pmath_unref(item);
  }
}

static pmath_bool_t init_make_rect(struct make_rect_t *info, pmath_t array){
  pmath_bool_t maybe_bigger;
  
  assert(info != NULL);
  
  info->depth = estimate_rect_depth(array, &maybe_bigger);
  info->lengths = pmath_mem_alloc(sizeof(intptr_t) * info->depth);
  if(info->depth && !info->lengths)
    return FALSE;
  
  memset(info->lengths, 0, sizeof(intptr_t) * info->depth);
  
  init_rect_length(info, array, 0);
  return TRUE;
}

static pmath_t simple_fill_rect(const struct make_rect_t *info, pmath_t array, int level){
  size_t len, delta, i;
  
  if(level >= info->depth)
    return array;
  
  len = pmath_expr_length(array);
  if(len < info->lengths[level])
    array = pmath_expr_resize(array, info->lengths[level]);
    
  delta = info->lengths[level] - len;
  
  if(info->padleft){
    for(i = len;i > 0;--i){
      pmath_t item = pmath_expr_extract_item(array, i);
      
      item = simple_fill_rect(info, item, level + 1);
      
      array = pmath_expr_set_item(array, i + delta, item);
    }
    
    if(level + 1 == info->depth){
      pmath_t zero = PMATH_FROM_INT32(0);
      
      for(i = delta;i > 0;--i){
        array = pmath_expr_set_item(array, i, zero);
      }
    }
    else{
      pmath_t item = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), 0);
      item = simple_fill_rect(info, item, level+1);
      
      for(i = delta;i > 0;--i){
        array = pmath_expr_set_item(array, i, pmath_ref(item));
      }
      
      pmath_unref(item);
    }
  }
  else{
    for(i = len;i > 0;--i){
      pmath_t item = pmath_expr_extract_item(array, i);
      
      item = simple_fill_rect(info, item, level + 1);
      
      array = pmath_expr_set_item(array, i, item);
    }
    
    if(level + 1 == info->depth){
      pmath_t zero = PMATH_FROM_INT32(0);
      
      for(i = delta;i > 0;--i){
        array = pmath_expr_set_item(array, len + i, zero);
      }
    }
    else{
      pmath_t item = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), 0);
      item = simple_fill_rect(info, item, level+1);
      
      for(i = delta;i > 0;--i){
        array = pmath_expr_set_item(array, len + i, pmath_ref(item));
      }
      
      pmath_unref(item);
    }
  }
  
  return array;
}

static pmath_t make_rectangular(pmath_t array, pmath_bool_t padleft){
  struct make_rect_t info;
  
  if(!init_make_rect(&info, array))
    return array;
  
  info.padleft = padleft;
  array = simple_fill_rect(&info, array, 0);
  
  pmath_mem_free(info.lengths);
  return array;
}


struct make_rect_ex_t{
  struct make_rect_t   inherited;
  intptr_t            *margins;
};

static pmath_t next_pad(pmath_t pad, intptr_t i){
  if(pmath_is_expr_of(pad, PMATH_SYMBOL_LIST)
  && pmath_expr_length(pad) > 0){
    intptr_t len = (intptr_t)pmath_expr_length(pad);
    
    i = i % len;
    if(i < 0)
      i+= len;
    if(i == 0)
      i = len;
    return pmath_expr_get_item(pad, (size_t)i);
  }
  
  return pmath_ref(pad);
}

static pmath_t advanced_make_rect(const struct make_rect_ex_t *info, pmath_t array, int level, pmath_t pad){
  intptr_t oldlen, len, mar, i, base;
  intptr_t delta, left, right;
  
  if(level >= info->inherited.depth){
    pmath_unref(pad);
    return array;
  }
  
  if(!pmath_is_expr_of(array, PMATH_SYMBOL_LIST)){
    pmath_unref(array);
    // message ...
    return pad;
  }
  
  len    = (intptr_t)info->inherited.lengths[level];
  mar    = info->margins[level];
  oldlen = (intptr_t)pmath_expr_length(array);
  if(oldlen < len)
    array = pmath_expr_resize(array, len);
  
  if(info->inherited.padleft){
    base = oldlen;
    delta = len - mar - oldlen;
  }
  else{
    base = 0;
    delta = mar;
  }
  
  left = 1;
  right = oldlen;
  if(delta < 0)
     left = 1 - delta;
  if(right + delta > len)
     right = len - delta;
  
  if(delta < 0){
    for(i = left;i <= right;++i){
      pmath_t item = pmath_expr_extract_item(array, i);
      item = advanced_make_rect(info, item, level+1, next_pad(pad, i - base));
      array = pmath_expr_set_item(array, i + delta, item);
    }
  }
  else{
    for(i = right;i >= left;--i){
      pmath_t item = pmath_expr_extract_item(array, i);
      item = advanced_make_rect(info, item, level+1, next_pad(pad, i - base));
      array = pmath_expr_set_item(array, i + delta, item);
    }
  }
  
  if(oldlen > len)
    array = pmath_expr_resize(array, len);
  
  if(level + 1 == info->inherited.depth){
  
    for(i = 1-delta;i < left;++i){
      array = pmath_expr_set_item(array, i + delta, next_pad(pad, i - base));
    }
    
    for(i = right+1;i <= len-delta;++i){
      array = pmath_expr_set_item(array, i + delta, next_pad(pad, i - base));
    }
  }
  else{
    for(i = 1-delta;i < left;++i){
      pmath_t item = pmath_ref(_pmath_object_emptylist);
      item = advanced_make_rect(info, item, level+1, next_pad(pad, i - base));
      array = pmath_expr_set_item(array, i + delta, item);
    }
    
    for(i = right+1;i <= len-delta;++i){
      pmath_t item = pmath_ref(_pmath_object_emptylist);
      item = advanced_make_rect(info, item, level+1, next_pad(pad, i - base));
      array = pmath_expr_set_item(array, i + delta, item);
    }
  }
  
  pmath_unref(pad);
  return array;
}


PMATH_PRIVATE pmath_t builtin_padleft_and_padright(pmath_expr_t expr){
/* PadLeft(list, n, xs, m)
   PadLeft(list, n, {x1, x2, ...})  == PadLeft(list, n, {x1, x2, ...}, 0)
   PadLeft(list, n, x)  == PadLeft(list, n, {x})
   PadLeft(list, n)     == PadLeft(list, n, 0)
   
   PadLeft(array, {n1, n2, ...}, ...) for multipe dimensions
   PadLeft(array)  makes the array rectangular
   
   PadRight(...)
 */
  struct make_rect_ex_t info;
  size_t exprlen = pmath_expr_length(expr);
  pmath_t list, n_obj, padding, margin;
  size_t i;
  
  if(exprlen < 1 || exprlen > 4){
    pmath_message_argxxx(exprlen, 1, 4);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of(list, PMATH_SYMBOL_LIST)){
    if(!pmath_is_expr(list))
      pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    pmath_unref(list);
    return expr;
  }
  
  info.inherited.padleft = pmath_is_expr_of(expr, PMATH_SYMBOL_PADLEFT);
  if(exprlen == 1){
    pmath_unref(expr);
    list = make_rectangular(list, info.inherited.padleft);
    return list;
  }
  
  n_obj = pmath_expr_get_item(expr, 2);
  if(exprlen >= 3){
    padding = pmath_expr_get_item(expr, 3);
  }
  else
    padding = PMATH_FROM_INT32(0);
  
  if(!pmath_is_expr_of(n_obj, PMATH_SYMBOL_LIST))
    n_obj = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, n_obj);
  
  if(exprlen >= 4){
    margin = pmath_expr_get_item(expr, 4);
  }
  else
    margin = PMATH_FROM_INT32(0);
  
  if(!pmath_is_expr_of(margin, PMATH_SYMBOL_LIST)){
    pmath_t tmp = margin;
    margin = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, pmath_expr_length(n_obj));
    for(i = pmath_expr_length(n_obj);i > 0;--i)
      margin = pmath_expr_set_item(margin, i, pmath_ref(tmp));
    
    pmath_unref(tmp);
  }
  
  info.inherited.lengths = NULL;
  info.margins           = NULL;
  
  if(pmath_expr_length(margin) != pmath_expr_length(n_obj)){
    pmath_message(PMATH_NULL, "margin", 4, 
      pmath_ref(margin),
      pmath_ref(expr),
      pmath_integer_new_uiptr(pmath_expr_length(n_obj)),
      pmath_ref(n_obj));
    goto FINISH;
  }
  
  info.inherited.depth = pmath_expr_length(n_obj);
  info.inherited.lengths = pmath_mem_alloc(sizeof(intptr_t) * info.inherited.depth);
  info.margins           = pmath_mem_alloc(sizeof(intptr_t) * info.inherited.depth);
  if(!info.inherited.lengths || !info.margins)
    goto FINISH;
  
  for(i = pmath_expr_length(n_obj);i > 0;--i){
    pmath_t n = pmath_expr_get_item(n_obj, i);
    
    if(!pmath_is_integer(n) || !pmath_integer_fits_siptr(n) || pmath_number_sign(n) < 0){
      pmath_unref(n);
      pmath_message(PMATH_NULL, "intnm", 2, pmath_ref(expr), PMATH_FROM_INT32(2));
      goto FINISH;
    }
    
    info.inherited.lengths[i-1] = pmath_integer_get_siptr(n);
    pmath_unref(n);
  }
  
  for(i = pmath_expr_length(margin);i > 0;--i){
    pmath_t m = pmath_expr_get_item(margin, i);
    
    if(!pmath_is_integer(m) || !pmath_integer_fits_siptr(m)){
      pmath_unref(m);
      pmath_message(PMATH_NULL, "intm", 2, pmath_ref(expr), PMATH_FROM_INT32(4));
      goto FINISH;
    }
    
    info.margins[i-1] = pmath_integer_get_siptr(m);
    pmath_unref(m);
  }
  
  pmath_unref(expr);
  expr = advanced_make_rect(&info, list, 0, padding);
  list    = PMATH_NULL;
  padding = PMATH_NULL;
  
 FINISH:
  pmath_mem_free(info.inherited.lengths);
  pmath_mem_free(info.margins);
  pmath_unref(list);
  pmath_unref(n_obj);
  pmath_unref(padding);
  pmath_unref(margin);
  return expr;
}
