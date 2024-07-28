#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers.h>
#include <pmath-core/packed-arrays.h>

#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols-private.h>

#include <string.h>


extern pmath_symbol_t pmath_System_List;

#define ceildiv(x, y) (((x) + (y) - 1) / (y))

static pmath_bool_t next(
  long       *in_i,    // all 0-indexed
  long       *out_i,   // all 0-indexed
  const long *pad,
  const long *n,       // all >= 0
  const long *d,       // all >= 0
  const long *blocks,
  long        depth
) {
  long k = depth - 1;
  do {
    ++in_i[k];
    ++out_i[k];
    
    if(out_i[k] % n[k] == 0)
      in_i[k] += d[k] - n[k];
      
    if(out_i[k] < blocks[k] * n[k])
      return TRUE;
      
    in_i[k] = -pad[k];
    out_i[k] = 0;
    --k;
  } while(k > 0);
  
  return FALSE;
}

static pmath_t part(
  pmath_expr_t array,  // will be freed
  const long *in_i,    // all 0-indexed
  long depth
) {
  pmath_expr_t subarray;
  long k, l;
  
  while(depth > 0) {
    k = (long)pmath_expr_length(array);
    
    if(k > 0) {
      l = 1 + (*in_i) % k;
      
      if(l <= 0)
        l += k;
    }
    else {
      pmath_unref(array);
      return PMATH_UNDEFINED;
    }
    
    subarray = pmath_expr_get_item(array, (size_t)l);
    pmath_unref(array);
    
    array = subarray;
    ++in_i;
    --depth;
  }
  
  return array;
}

static pmath_bool_t is_inside(
  const long *i,    // all 0-indexed
  const long *dim,
  long depth
) {
  while(depth > 0) {
    if(*i < 0 || *i >= *dim)
      return FALSE;
      
    ++i;
    ++dim;
    --depth;
  }
  
  return TRUE;
}

static pmath_expr_t set_divmod_part(
  pmath_expr_t array, // will be freed
  const long *out_i,  // all 0-indexed
  const long *n,
  long k,
  long depth,
  pmath_t item        // will be freed
) {
  pmath_expr_t subarray;
  long l;
  
  if(k >= 2 * depth) {
    pmath_unref(array);
    return item;
  }
  
  if(k < depth)
    l = 1 + out_i[k] / n[k];
  else
    l = 1 + out_i[k - depth] % n[k - depth];
    
  subarray = pmath_expr_extract_item(array, (size_t)l);
  subarray = set_divmod_part(subarray, out_i, n, k + 1, depth, item);
  return pmath_expr_set_item(array, (size_t)l, subarray);
}

static pmath_expr_t make_similar_deep_array(
  pmath_t arr, // wont be freed
  const long *blocks,
  const long *n,
  long k,
  long depth
) {
  pmath_expr_t result;
  
  if(k >= 2 * depth)
    return PMATH_NULL;
    
  if(pmath_is_packed_array(arr)) {
    pmath_packed_type_t elem_type = pmath_packed_array_get_element_type(arr);
    size_t dims = (size_t)(2 * depth - k);
    size_t *sizes = pmath_mem_calloc(dims, sizeof(size_t));
    if(!sizes)
      return PMATH_NULL;
    
    for(long i = k; i < depth; ++i)
      sizes[i - k] = (size_t)blocks[i];
    for(long i = k > depth ? k : depth; i < 2 * depth; ++i)
      sizes[i - k] = (size_t)n[i - depth];
    
    result = pmath_packed_array_new(PMATH_NULL, elem_type, dims, sizes, NULL, 0);
    pmath_mem_free(sizes);
    return result;
  }
  
  if(k < depth)
    result = pmath_expr_new(pmath_expr_get_item(arr, 0), (size_t)blocks[k]);
  else
    result = pmath_expr_new(pmath_expr_get_item(arr, 0), (size_t)n[k - depth]);
    
  if(k < 2 * depth) {
    for(size_t i = pmath_expr_length(result); i > 0; --i) {
      result = pmath_expr_set_item(
                 result, i,
                 make_similar_deep_array(arr, blocks, n, k + 1, depth));
    }
  }
  
  return result;
}

static pmath_expr_t trim_undef(pmath_expr_t list, int depth) {
  size_t a, b;
  
  if(depth <= 0 || !pmath_is_expr(list))
    return list;
    
  b = pmath_expr_length(list);
  while(b > 0 && pmath_expr_item_equals(list, b, PMATH_UNDEFINED)) {
    --b;
  }
  
  a = 1;
  while(a < b && pmath_expr_item_equals(list, a, PMATH_UNDEFINED)) {
    ++a;
  }
  
  if(a > 1 || b < pmath_expr_length(list)) {
    pmath_expr_t newlist;
    
    if(a <= b) {
      newlist = pmath_expr_get_item_range(list, a, b - a + 1);
    }
    else
      newlist = pmath_expr_set_item(
                  pmath_ref(_pmath_object_emptylist), 0,
                  pmath_expr_get_item(list, 0));
                  
    pmath_unref(list);
    list = newlist;
  }
  
  for(a = pmath_expr_length(list); a > 0; --a) {
    list = pmath_expr_set_item(
             list, a,
             trim_undef(
               pmath_expr_get_item(list, a), depth - 1));
  }
  
  return list;
}

static pmath_expr_t partition(
  pmath_expr_t array,   // wont be freed
  pmath_expr_t padding, // wont be freed
  const long *dim,
  const long *n,
  const long *d,
  long       *left,
  const long *right,
  long depth
) {
  pmath_expr_t result = PMATH_NULL;
  pmath_t item;
  long *in_i;
  long *out_i;
  long *blocks;
  
  pmath_bool_t have_undef = FALSE;
  
  if(depth <= 0)
    return pmath_ref(array);
  
  in_i   = (long *)pmath_mem_calloc(depth, sizeof(long));
  out_i  = (long *)pmath_mem_calloc(depth, sizeof(long));
  blocks = (long *)pmath_mem_calloc(depth, sizeof(long));
  
  if(dim && n && d && left && right && in_i && out_i && blocks) {
    long k;
    for(k = 0; k < depth; ++k) {
      long r;
      long rr;
      
      if(left[k] < 0)
        left[k] = n[k] + left[k];
      else
        --left[k];
        
      in_i[k] = -left[k];
      
      blocks[k] = 1 + ceildiv(left[k] + dim[k] - n[k], d[k]);
      
      r = (blocks[k] - 1) * d[k] + n[k] - dim[k] - left[k];
      
      if(right[k] < 0)
        rr = -1 - right[k];
      else
        rr = n[k] - right[k];
        
      if(rr >= r)
        blocks[k] += rr / d[k];
      else if(r > 0)
        --blocks[k];
    }
    
    result = make_similar_deep_array(array, blocks, n, 0, depth);
    
    memset(out_i, 0, depth * sizeof(long));
    
    do {
      if(is_inside(in_i, dim, depth))
        item = part(pmath_ref(array), in_i, depth);
      else
        item = part(pmath_ref(padding), in_i, depth);
        
      have_undef = have_undef || pmath_same(item, PMATH_UNDEFINED);
      
      result = set_divmod_part(result, out_i, n, 0, depth, item);
    } while(next(in_i, out_i, left, n, d, blocks, depth));
  }
  
  pmath_mem_free(in_i);
  pmath_mem_free(out_i);
  pmath_mem_free(blocks);
  
  if(have_undef)
    return trim_undef(result, 2 * depth);
    
  return result;
}

// result must be freed
static long *get_n( 
  pmath_t n_obj,    // wont be freed
  long *depth       // -1 on error
) {
  long *n;
  
  if(pmath_is_int32(n_obj)) {
    if(PMATH_AS_INT32(n_obj) == 0) {
      *depth = -1;
      return NULL;
    }
    
    *depth = 1;
    n = (long *)pmath_mem_alloc(sizeof(long));
    
    n[0] = PMATH_AS_INT32(n_obj);
    return n;
  }
  
  if(pmath_is_expr_of(n_obj, pmath_System_List)) {
    *depth = (long)pmath_expr_length(n_obj);
    
    if(*depth > 0) {
      long i;
      
      n = (long *)pmath_mem_alloc((size_t) * depth * sizeof(long));
      if(!n) {
        *depth = -1;
        return NULL;
      }
      
      for(i = 0; i < *depth; ++i) {
        pmath_t item = pmath_expr_get_item(n_obj, 1 + (size_t)i);
        
        if(pmath_is_int32(item) && PMATH_AS_INT32(item) != 0) {
          n[i] = PMATH_AS_INT32(item);
        }
        else {
          pmath_unref(item);
          pmath_mem_free(n);
          
          *depth = -1;
          return NULL;
        }
      }
      
      return n;
    }
    
    if(*depth == 0)
      return NULL;
  }
  
  *depth = -1;
  return NULL;
}

static pmath_bool_t get_d(
  pmath_t  d_obj, // wont be freed
  long    *d,
  long     depth
) {
  if(pmath_is_int32(d_obj) && depth == 1) {
    if(PMATH_AS_INT32(d_obj) <= 0)
      return FALSE;
      
    d[0] = PMATH_AS_INT32(d_obj);
    return TRUE;
  }
  
  if(pmath_is_expr_of_len(d_obj, pmath_System_List, (size_t)depth)) {
    long i;
    
    for(i = 0; i < depth; ++i) {
      pmath_t item = pmath_expr_get_item(d_obj, 1 + (size_t)i);
      
      if(pmath_is_int32(item)) {
        if(PMATH_AS_INT32(item) <= 0)
          return FALSE;
          
        d[i] = PMATH_AS_INT32(item);
      }
      else {
        pmath_unref(item);
        
        return FALSE;
      }
    }
    
    return TRUE;
  }
  
  return FALSE;
}

static pmath_bool_t set_overhang(
  pmath_t all_obj,  // wont be freed
  pmath_t overhang, // will be freed
  long *result,
  long depth
) {
  long i;
  
  if(pmath_is_int32(overhang)) {
    long val = PMATH_AS_INT32(overhang);
    
    if(val == 0) {
      pmath_message(PMATH_NULL, "ohp", 1, pmath_ref(all_obj));
      pmath_unref(overhang);
      return FALSE;
    }
    
    if(depth == 0) {
      pmath_message(PMATH_NULL, "ohpdm", 2,
                    PMATH_FROM_INT32(1),
                    PMATH_FROM_INT32(0));
      pmath_unref(overhang);
      return FALSE;
    }
    
    for(i = 0; i < depth; ++i) {
      result[i] = val;
    }
    
    pmath_unref(overhang);
    return TRUE;
  }
  
  if(pmath_is_expr_of(overhang, pmath_System_List)) {
    if(pmath_expr_length(overhang) != (size_t)depth) {
      pmath_message(PMATH_NULL, "ohpdm", 2,
                    pmath_integer_new_uiptr(pmath_expr_length(overhang)),
                    pmath_integer_new_slong(depth));
                    
      pmath_unref(overhang);
      return FALSE;
    }
    
    for(i = 0; i < (long)pmath_expr_length(overhang); ++i) {
      pmath_t item = pmath_expr_get_item(overhang, 1 + (size_t)i);
      long val = 0;
      
      if(pmath_is_int32(item))
        val = PMATH_AS_INT32(item);
        
      pmath_unref(item);
      
      if(val == 0) {
        pmath_message(PMATH_NULL, "ohp", 1, pmath_ref(all_obj));
        pmath_unref(overhang);
        return FALSE;
      }
      
      result[i] = val;
    }
    
    pmath_unref(overhang);
    return TRUE;
  }
  
  pmath_message(PMATH_NULL, "ohp", 1, pmath_ref(all_obj));
  pmath_unref(overhang);
  return FALSE;
}

static pmath_bool_t set_leftright_overhang(
  pmath_t  obj, // wont be freed
  long    *left,
  long    *right,
  long     depth
) {
  if(pmath_is_expr_of_len(obj, pmath_System_List, 2)) {
    if(!set_overhang(obj, pmath_expr_get_item(obj, 1), left, depth))
      return FALSE;
      
    if(!set_overhang(obj, pmath_expr_get_item(obj, 2), right, depth))
      return FALSE;
      
    return TRUE;
  }
  
  if( !set_overhang(obj, pmath_ref(obj), left,  depth) ||
      !set_overhang(obj, pmath_ref(obj), right, depth))
  {
    return FALSE;
  }
  
  return TRUE;
}

static pmath_expr_t embed(
  pmath_t head, // wont be freed
  pmath_t item, // will be freed
  long depth
) {
  while(depth-- > 0)
    item = pmath_expr_new_extended(pmath_ref(head), 1, item);
    
  return item;
}

static pmath_expr_t embed_at(
  pmath_t     head, // wont be freed
  pmath_expr_t item, // will be freed
  long level,
  long embed_depth
) {
  size_t i;
  
  if(level == 0)
    return embed(head, item, embed_depth);
    
  if(pmath_expr_length(item) == 0)
    return embed(head, item, level + embed_depth);
    
  for(i = pmath_expr_length(item); i > 0; --i) {
    pmath_t subitem = pmath_expr_get_item(item, i);
    
    item = pmath_expr_set_item(item, i, PMATH_NULL);
    
    subitem = embed_at(head, subitem, level - 1, embed_depth);
    
    item = pmath_expr_set_item(item, i, subitem);
  }
  
  return item;
}

static pmath_expr_t make_padding(
  pmath_t pad,  // will be freed
  long depth
) {
  pmath_t dim_obj;
  
  if(!pmath_is_expr_of(pad, pmath_System_List)) {
    pad = embed(pmath_System_List, pad, depth);
    return pad;
  }
  
  dim_obj = _pmath_dimensions(pad, (size_t)depth);
  
  if(!pmath_is_expr_of(dim_obj, pmath_System_List)) {
    pmath_unref(dim_obj);
    pad = embed(pmath_System_List, pad, depth);
    return pad;
  }
  
  if(pmath_expr_length(dim_obj) < (size_t)depth) {
    long level = (long)pmath_expr_length(dim_obj);
    
    pad = embed_at(
            pmath_System_List,
            pad,
            level,
            depth - level);
  }
  
  pmath_unref(dim_obj);
  return pad;
}

// < 0: error, return expr
// = 0: return array
// > 0: ok
static int get_dimensions(
  pmath_t array, // wont be freed
  long *dim,
  long depth
) {
  pmath_t dim_obj = _pmath_dimensions(array, (size_t)depth);
  size_t i;
  
  if(!pmath_is_expr_of_len(dim_obj, pmath_System_List, (size_t)depth)) {
    pmath_message(PMATH_NULL, "pdep", 2,
                  pmath_integer_new_slong(depth),
                  dim_obj);
    return -1;
  }
  
  for(i = 1; i <= (size_t)depth; ++i) {
    pmath_t len = pmath_expr_get_item(dim_obj, i);
    
    if(pmath_is_int32(len)) {
      dim[i - 1] = PMATH_AS_INT32(len);
      
      if(dim[i - 1] == 0) {
        pmath_message(PMATH_NULL, "pdep", 2,
                      pmath_integer_new_slong(depth),
                      dim_obj);
        return -1;
      }
    }
    else {
      pmath_unref(len);
      pmath_unref(dim_obj);
      return 0;
    }
  }
  
  pmath_unref(dim_obj);
  return 1;
}

PMATH_PRIVATE pmath_t builtin_partition(pmath_expr_t expr) {
  /* Partition(list, n)                   = Partition(list, n, n, {1, -1}, list)
     Partition(list, n, d)                = Partition(list, n, d, {1, -1}, list)
     Partition(list, n, d, {kL, kR})      = Partition(list, n, d, {kL, kR}, list)
     Partition(list, n, d, {kL, kR}, pad)
  
     list . . . expression with Length(Dimensions(list)) == Length(n)
     n  . . . . positive machine size integer or list of those. {x} is same as x
     d  . . . . ditto, same dimensions as n
     kL, kR . . machine size integer or lists of those, != 0
     pad  . . . padding element(s)
  
     Partitions list into sublists of length n with offset d.
     The first element of list should appear at position kL in the first sublist.
     The last element of list should appear at or after position kR in the last
     sublist.
     If additional elements are needed, they are taken from pad cyclically.
     If pad = {}, no padding is done, and so the sublists might have different
     lengths.
   */
  pmath_t list;
  pmath_t padding;
  pmath_t obj;
  size_t exprlen;
  long *dim;
  long *n;
  long *d;
  long *left;
  long *right;
  long depth;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2 || exprlen > 5) {
    pmath_message_argxxx(exprlen, 2, 5);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 2);
  n = get_n(obj, &depth);
  pmath_unref(obj);
  
  if(depth < 0) {
    pmath_message(PMATH_NULL, "ilsmp", 2, PMATH_FROM_INT32(2), pmath_ref(expr));
    return expr;
  }
  
  dim   = (long *)pmath_mem_alloc((size_t)depth * sizeof(long));
  d     = (long *)pmath_mem_alloc((size_t)depth * sizeof(long));
  left  = (long *)pmath_mem_alloc((size_t)depth * sizeof(long));
  right = (long *)pmath_mem_alloc((size_t)depth * sizeof(long));
  
  if(depth == 0 || (n && dim && d && left && right)) {
    int res;
    list = pmath_expr_get_item(expr, 1);
    
    res = get_dimensions(list, dim, depth);
    
    if(res < 0) {
      pmath_unref(list);
      goto CLEANUP;
    }
    
    if(res == 0) {
      pmath_unref(expr);
      expr = list;
      goto CLEANUP;
    }
    
    if(exprlen >= 3) {
      obj = pmath_expr_get_item(expr, 3);
      
      if(!get_d(obj, d, depth)) {
        pmath_message(PMATH_NULL, "ilsmp", 2, PMATH_FROM_INT32(3), pmath_ref(expr));
        
        pmath_unref(obj);
        pmath_unref(list);
        goto CLEANUP;
      }
      
      pmath_unref(obj);
    }
    else {
      memcpy(d, n, (size_t)depth * sizeof(long));
    }
    
    if(exprlen >= 4) {
      obj = pmath_expr_get_item(expr, 4);
      
      if(!set_leftright_overhang(obj, left, right, depth)) {
        pmath_unref(obj);
        pmath_unref(list);
        goto CLEANUP;
      }
      
      pmath_unref(obj);
    }
    else {
      long i;
      for(i = 0; i < depth; ++i) {
        left[i] = 1;
        right[i] = -1;
      }
    }
    
    if(exprlen >= 5) {
      padding = make_padding(
                  pmath_expr_get_item(expr, 5),
                  depth);
    }
    else
      padding = pmath_ref(list);
      
    pmath_unref(expr);
    expr = partition(list, padding, dim, n, d, left, right, depth);
    
    pmath_unref(list);
    pmath_unref(padding);
  }
  
CLEANUP:
  pmath_mem_free(n);
  pmath_mem_free(dim);
  pmath_mem_free(d);
  pmath_mem_free(left);
  pmath_mem_free(right);
  
  return expr;
}
