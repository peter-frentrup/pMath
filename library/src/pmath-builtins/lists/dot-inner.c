#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers.h>

#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

#include <string.h>


PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_tensor_get(
  pmath_t       tensor,  // will be freed
  size_t        depth, 
  const size_t *idx
){ 
  size_t i;
  pmath_t cur, tmp;
  
  cur = pmath_ref(tensor);
  for(i = 0;i < depth;++i){
    tmp = cur;
    cur = pmath_expr_get_item(tmp, idx[i]);
    pmath_unref(tmp);
  }
  
  return cur;
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_tensor_set(
  pmath_t       tensor,  // will be freed
  size_t        depth, 
  const size_t *idx, 
  pmath_t       obj      // will be freed
){
  pmath_t ti;
  if(depth == 0){
    pmath_unref(tensor);
    return obj;
  }
  
  ti = pmath_expr_get_item(tensor, idx[0]);
  tensor = pmath_expr_set_item(tensor, idx[0], PMATH_NULL);
  
  return pmath_expr_set_item(
    tensor, idx[0], 
    _pmath_tensor_set(
      ti, 
      depth-1, 
      idx+1, 
      obj));
}

static pmath_bool_t prev(size_t depth, size_t *idx, const size_t *lens){
  size_t d = depth;
  while(d-- > 0){
    if(idx[d] > 1){
      size_t i;
      for(i = d+1;i < depth;++i)
        idx[i] = lens[i];
      
      --idx[d];
      return TRUE;
    }
  }
  
  return FALSE;
}

// head wont be freed
static pmath_t make_tensor(pmath_t head, size_t dims, const size_t *lens){
  pmath_t result, t;
  size_t i;
  
  if(dims == 0)
    return PMATH_NULL;
  
  result = pmath_expr_new(pmath_ref(head), lens[0]);
  t = make_tensor(head, dims-1, lens+1);
  for(i = lens[0];i > 0;--i){
    result = pmath_expr_set_item(result, i, pmath_ref(t));
  }
  pmath_unref(t);
  return result;
}

struct _inner_info_t{
  pmath_t f;
  pmath_t g;
  size_t n;
  
  size_t dim1;
  size_t dim2;
  size_t dims;
  size_t *idx;
  size_t *lens;
};

// t1, t2 wont be freed
static pmath_bool_t init(struct _inner_info_t *info, pmath_t t1, pmath_t t2){
  pmath_t d1 = _pmath_dimensions(t1, SIZE_MAX);
  pmath_t d2 = _pmath_dimensions(t2, SIZE_MAX);
  pmath_t obj1, obj2;
  size_t i;
  
  info->dim1 = pmath_expr_length(d1);
  info->dim2 = pmath_expr_length(d2);
  
  if(info->n == SIZE_MAX)
    info->n = info->dim1;
  
  if(info->dim1 < info->n){
    pmath_message(PMATH_NULL, "nolev", 3,
      pmath_integer_new_uiptr(info->n),
      pmath_ref(t1),
      d1);
    pmath_unref(d2);
    return FALSE;
  }
  
  if(info->dim1 < 1 || info->dim2 < 1){
    pmath_unref(d1);
    pmath_unref(d2);
    return FALSE;
  }
  
  obj1 = pmath_expr_get_item(d1, info->n);
  obj2 = pmath_expr_get_item(d2, 1);
  if(!pmath_equals(obj1, obj2)){
    pmath_message(PMATH_NULL, "shape", 5,
      obj1,
      pmath_integer_new_uiptr(info->n),
      pmath_ref(t1),
      obj2,
      pmath_ref(t2));
    pmath_unref(d1);
    pmath_unref(d2);
    return FALSE;
  }
  
  if(!pmath_is_integer(obj1)){
    pmath_unref(d1);
    pmath_unref(d2);
    pmath_unref(obj1);
    pmath_unref(obj2);
    return FALSE;
  }
  pmath_unref(obj1);
  pmath_unref(obj2);
  
  info->dims = info->dim1 + info->dim2 - 2;
  info->idx  = pmath_mem_alloc(info->dims * sizeof(size_t));
  info->lens = pmath_mem_alloc(info->dims * sizeof(size_t));
  if(info->dims > 0 && (!info->idx || !info->lens)){
    pmath_unref(d1);
    pmath_unref(d2);
    pmath_mem_free(info->idx);
    pmath_mem_free(info->lens);
    return FALSE;
  }
  
  for(i = 1;i < info->n;++i){
    obj1 = pmath_expr_get_item(d1, i);
    if(!pmath_is_int32(obj1) || PMATH_AS_INT32(obj1) < 0){
      pmath_unref(obj1);
      pmath_unref(d1);
      pmath_unref(d2);
      pmath_mem_free(info->idx);
      pmath_mem_free(info->lens);
      return FALSE;
    }
    
    info->lens[i-1] = (unsigned)PMATH_AS_INT32(obj1);
  }
  
  for(i = info->n + 1;i <= info->dim1;++i){
    obj1 = pmath_expr_get_item(d1, i);
    if(!pmath_is_int32(obj1) || PMATH_AS_INT32(obj1) < 0){
      pmath_unref(obj1);
      pmath_unref(d1);
      pmath_unref(d2);
      pmath_mem_free(info->idx);
      pmath_mem_free(info->lens);
      return FALSE;
    }
    
    info->lens[i-2] = (unsigned)PMATH_AS_INT32(obj1);
  }
  
  for(i = 2;i <= info->dim2;++i){
    obj2 = pmath_expr_get_item(d2, i);
    if(!pmath_is_int32(obj2) || PMATH_AS_INT32(obj2) < 0){
      pmath_unref(obj2);
      pmath_unref(d1);
      pmath_unref(d2);
      pmath_mem_free(info->idx);
      pmath_mem_free(info->lens);
      return FALSE;
    }
    
    info->lens[info->dim1 + i - 3] = (unsigned)PMATH_AS_INT32(obj2);
  }

  pmath_unref(d1);
  pmath_unref(d2);
  
  return TRUE;
}

// t1, t2 wont be freed
static pmath_t inner(struct _inner_info_t *info, pmath_t t1, pmath_t t2){
  size_t k;
  size_t sumlen = pmath_expr_length(t2);
  pmath_t obj    = pmath_expr_get_item(t1, 0);
  pmath_t result = make_tensor(obj, info->dims, info->lens);
  pmath_unref(obj);
  
  /* result[i_1,...,i_(n-1), i_(n+1), ..., i_dim1, j_2, ..., j_dim2] =
       g @@ Table(f(t1[i_1, ..., i_(n-1), k, i_(n+1), ..., t_dim1],
                    t2[k, j_2, ..., j_dim2]), {k, sumlen})
   */
  if(info->dims == 0 || info->lens[0] > 0){
    memcpy(info->idx, info->lens, sizeof(size_t) * info->dims);
    do{
      pmath_t sum = pmath_expr_new(pmath_ref(info->g), sumlen);
      
      for(k = sumlen;k > 0;--k){
        pmath_t t1a, t1b, t1c, t2a, t2b;
        
        t1a = _pmath_tensor_get(t1, info->n - 1, info->idx);
        t1b = pmath_expr_get_item(t1a, k);
        t1c = _pmath_tensor_get(t1b, info->dim1 - info->n, info->idx + info->n - 1);
        
        t2a = pmath_expr_get_item(t2, k);
        t2b = _pmath_tensor_get(t2a, info->dim2 - 1, info->idx + info->dim1 - 1);
        
        pmath_unref(t1a);
        pmath_unref(t1b);
        pmath_unref(t2a);
        
        obj = pmath_expr_new_extended(
          pmath_ref(info->f), 2,
          t1c,
          t2b);
        
        sum = pmath_expr_set_item(sum, k, obj);
      }
      
      result = _pmath_tensor_set(result, info->dims, info->idx, sum);
    }while(prev(info->dims, info->idx, info->lens));
  }
  
  return result;
}

PMATH_PRIVATE pmath_t builtin_dot(pmath_expr_t expr){
  struct _inner_info_t info;
  size_t exprlen, ia, ib;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen == 0)
    return expr;
  
  info.f = pmath_ref(PMATH_SYMBOL_TIMES);
  info.g = pmath_ref(PMATH_SYMBOL_PLUS);
  
  ia = 1;
  while(ia < exprlen){
    pmath_t a = pmath_expr_get_item(expr, ia);
    
    if(pmath_is_expr_of(a, PMATH_SYMBOL_LIST)){
      ib = ia+1;
      
      while(ib <= exprlen){
        pmath_t b = pmath_expr_get_item(expr, ib);
        
        if(pmath_is_expr_of(b, PMATH_SYMBOL_LIST)){
          info.n = SIZE_MAX;
          
          if(init(&info, a, b)){
            pmath_t c = inner(&info, a, b);
            
            pmath_unref(a);
            pmath_unref(b);
            expr = pmath_expr_set_item(expr, ib, PMATH_UNDEFINED);
            a = c;
            
            pmath_mem_free(info.idx);
            pmath_mem_free(info.lens);
            ++ib;
            continue;
          }
        }
        
        pmath_unref(b);
        break;
      }
      
      expr = pmath_expr_set_item(expr, ia, a);
      ia = ib;
    }
    else{
      pmath_unref(a);
      ++ia;
    }
  }
  
  pmath_unref(info.f);
  pmath_unref(info.g);
  
  return _pmath_expr_shrink_associative(expr, PMATH_UNDEFINED);
}

PMATH_PRIVATE pmath_t builtin_inner(pmath_expr_t expr){
/* Inner(f, t1, t2, g, k) takes an m1 x m2 x ... x mr dimensional tensor t1
   and an n1 x n2 x ... x ns dimensional tensor t2 with mk = n1 and returns an
   m1 x m2 x ... m(k-1) x m(k+1) x ... x mr x n2 x ... x ns dimesional tensor
 */
  struct _inner_info_t info;
  pmath_t t1, t2;
  pmath_t head1, head2;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 3 || exprlen > 5){
    pmath_message_argxxx(exprlen, 3, 5);
    return expr;
  }
  
  info.f = pmath_expr_get_item(expr, 1);
  t1 = pmath_expr_get_item(expr, 2);
  t2 = pmath_expr_get_item(expr, 3);
  if(exprlen >= 4)
    info.g = pmath_expr_get_item(expr, 4);
  else
    info.g = pmath_ref(PMATH_SYMBOL_PLUS);
  
  if(exprlen == 5){
    pmath_t obj = pmath_expr_get_item(expr, 5);
    
    if(!pmath_is_int32(obj) || PMATH_AS_INT32(obj) < 0){
      pmath_message(PMATH_NULL, "intpm", 2,
        pmath_ref(expr),
        PMATH_FROM_INT32(5));
      
      pmath_unref(info.f);
      pmath_unref(info.g);
      pmath_unref(t1);
      pmath_unref(t2);
      pmath_unref(obj);
      return expr;
    }
    
    info.n = (unsigned)PMATH_AS_INT32(obj);
  }
  else
    info.n = SIZE_MAX;
  
  if(!pmath_is_expr(t1)){
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(2), pmath_ref(expr));
    pmath_unref(info.f);
    pmath_unref(info.g);
    pmath_unref(t1);
    pmath_unref(t2);
    return expr;
  }
  
  if(!pmath_is_expr(t2)){
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(3), pmath_ref(expr));
    pmath_unref(info.f);
    pmath_unref(info.g);
    pmath_unref(t1);
    pmath_unref(t2);
    return expr;
  }
  
  head1 = pmath_expr_get_item(t1, 0);
  head2 = pmath_expr_get_item(t2, 0);
  if(!pmath_equals(head1, head2)){
    pmath_message(PMATH_NULL, "heads", 4,
      head1, 
      head2,
      PMATH_FROM_INT32(2),
      PMATH_FROM_INT32(3));
    pmath_unref(info.f);
    pmath_unref(info.g);
    pmath_unref(t1);
    pmath_unref(t2);
    return expr;
  }
  
  pmath_unref(head1);
  pmath_unref(head2);
  
  if(!init(&info, t1, t2)){
    pmath_unref(info.f);
    pmath_unref(info.g);
    pmath_unref(t1);
    pmath_unref(t2);
    return expr;
  }
  
  pmath_unref(expr);
  expr = inner(&info, t1, t2);
  
  pmath_unref(info.f);
  pmath_unref(info.g);
  pmath_unref(t1);
  pmath_unref(t2);
  pmath_mem_free(info.idx);
  pmath_mem_free(info.lens);
  return expr;
}
