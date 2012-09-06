#include <pmath-core/numbers.h>
#include <pmath-core/expressions-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>
#include <pmath-builtins/lists-private.h>


static pmath_bool_t is_quant_list(pmath_t q) {
  int q_class;
  
  if(pmath_is_expr_of(q, PMATH_SYMBOL_LIST)) {
    size_t i;
    
    for(i = pmath_expr_length(q); i > 0; --i) {
      pmath_t qi = pmath_expr_get_item(q, i);
      
      if(!is_quant_list(qi)) {
        pmath_unref(qi);
        return FALSE;
      }
      
      pmath_unref(qi);
    }
    
    return TRUE;
  }
  
  q_class = _pmath_number_class(q);
  
  return 0 != (q_class & (PMATH_CLASS_ZERO | PMATH_CLASS_POSSMALL | PMATH_CLASS_POSONE));
}

static pmath_bool_t is_real(pmath_t q) {
  int q_class = _pmath_number_class(q);
  return 0 != (q_class & (PMATH_CLASS_RINF | PMATH_CLASS_REAL));
}

static pmath_bool_t is_real_vector(pmath_t x) {
  size_t i;
  
  if(!pmath_is_expr_of(x, PMATH_SYMBOL_LIST))
    return FALSE;
    
  for(i = pmath_expr_length(x); i > 0; --i) {
    pmath_t xi = pmath_expr_get_item(x, i);
    
    if(!is_real(xi)) {
      pmath_unref(xi);
      return FALSE;
    }
    
    pmath_unref(xi);
  }
  
  return TRUE;
}

static pmath_bool_t is_real_matrix(pmath_t x, size_t *rows, size_t *cols) {
  size_t i;
  pmath_t xi;
  
  *rows = *cols = 0;
  
  if(!pmath_is_expr_of(x, PMATH_SYMBOL_LIST))
    return FALSE;
    
  *rows = pmath_expr_length(x);
  if(*rows == 0)
    return FALSE;
    
  xi = pmath_expr_get_item(x, *rows);
  if(!is_real_vector(xi)) {
    pmath_unref(xi);
    return FALSE;
  }
  
  *cols = pmath_expr_length(xi);
  pmath_unref(xi);
  
  for(i = *rows - 1; i > 0; --i) {
    xi = pmath_expr_get_item(x, i);
    
    if( !pmath_is_expr_of_len(xi, PMATH_SYMBOL_LIST, *cols) ||
        !is_real_vector(xi))
    {
      pmath_unref(xi);
      return FALSE;
    }
    
    pmath_unref(xi);
  }
  
  return TRUE;
}

static int cmp_less(const pmath_t *a, const pmath_t *b) {
  pmath_t tmp;
  
  // fast path
  if(pmath_is_double(*a) && pmath_is_double(*b)) {
    if(PMATH_AS_DOUBLE(*a) < PMATH_AS_DOUBLE(*b))
      return -1;
    if(PMATH_AS_DOUBLE(*a) > PMATH_AS_DOUBLE(*b))
      return 1;
    return 0;
  }
  
  if(pmath_equals(*a, *b))
    return 0;
    
  tmp = FUNC2(pmath_ref(PMATH_SYMBOL_LESS), pmath_ref(*a), pmath_ref(*b));
  tmp = pmath_evaluate(tmp);
  pmath_unref(tmp);
  
  if(pmath_same(tmp, PMATH_SYMBOL_TRUE))
    return -1;
  return 1;
}

static pmath_t get_item(pmath_expr_t list, pmath_t idx) { // int_index will be freed
  size_t n = pmath_expr_length(list);
  
  if(pmath_is_int32(idx)) {
    int i = PMATH_AS_INT32(idx);
    
    if(i < 1)
      return pmath_expr_get_item(list, 1);
      
    if((size_t)i > n)
      return pmath_expr_get_item(list, n);
      
    return pmath_expr_get_item(list, (size_t)i);
  }
  
  if(pmath_is_expr_of(idx, PMATH_SYMBOL_LIST)) {
    size_t i;
    for(i = pmath_expr_length(idx); i > 0; --i) {
      pmath_t idx_i = pmath_expr_extract_item(idx, i);
      
      idx_i = get_item(list, idx_i);
      
      idx = pmath_expr_set_item(idx, i, idx_i);
    }
    
    return idx;
  }
  
  if(pmath_is_integer(idx)) {
    if(pmath_compare(idx, INT(1)) <= 0) {
      pmath_unref(idx);
      return pmath_expr_get_item(list, 1);
    }
    
    if(pmath_compare(idx, INT(n)) >= 0) {
      pmath_unref(idx);
      return pmath_expr_get_item(list, n);
    }
  }
  
  pmath_unref(idx);
  return pmath_ref(PMATH_SYMBOL_UNDEFINED);
}

// all arguments will be freed
static pmath_t quantile(pmath_expr_t sorted_list, pmath_t qi, pmath_t c, pmath_t d) {
  pmath_t fi, ci, val_f, val_c;
  
  assert(pmath_is_expr_of(sorted_list, PMATH_SYMBOL_LIST));
  
  //list = _pmath_expr_sort_ex(list, cmp);
  //qi = PLUS(pmath_ref(a), pmath_ref(q), PLUS(INT(n) + pmath_ref(b)));
  //qi = pmath_evaluate(x);
  
  if(pmath_is_int32(qi)) {
    val_c = get_item(sorted_list, qi);
    pmath_unref(sorted_list);
    return val_c;
  }
  
  fi = pmath_evaluate(FUNC(pmath_ref(PMATH_SYMBOL_FLOOR),   pmath_ref(qi)));
  ci = pmath_evaluate(FUNC(pmath_ref(PMATH_SYMBOL_CEILING), pmath_ref(qi)));
  
  val_f = get_item(sorted_list, fi);
  val_c = get_item(sorted_list, ci);
  pmath_unref(sorted_list);
  
  if(!pmath_same(d, INT(0)))
    c = PLUS(c, TIMES(d, FUNC(pmath_ref(PMATH_SYMBOL_FRACTIONALPART), pmath_ref(qi))));
    
  pmath_unref(qi);
  
  val_c = MINUS(val_c, pmath_ref(val_f));
  val_f = PLUS(val_f, TIMES(val_c, c));
  val_f = pmath_evaluate(val_f);
  return val_f;
}

pmath_t builtin_quantile(pmath_expr_t expr) {
  /* Quantile(list, q)
     Quantile(list, {q1, q2, ...})
     Quantile(list, q, {{a, b}, {c, d}})
     Quantile({{x1, y1, ...}, {x2, y2, ...}, ...}, q)
                          = {Quantile({x1,x2,...}, q), Quantile({y1,y2,...}, q)}
  
     default {{a,b},{c,d} = {{0,0},{1,0}}
  
     For n:= Length(list), set
     s:= Sort(list, Less),
     x:= a + q*(n+b).
  
     If x is an integer, return s[x].
     Otherwise return
     s[Floor(x)] + (s[Ceiling(x)] - s[Floor[x]]) * (c + d FractionalPart(x)),
     with the indices clipped to 1..n.
  
     Messages:
       General::shlen
       General::vecmatn
       Quantile::nquan: The Quantile specification `1` should be a number or a
                        list of numbers between 0 and 1.
       Quantile::parm:  The Quantile parameters `1` should be given as a 2 x 2
                        matrix of real numbers {{a,b},{c,d}} or as a pair of
                        real plot point parameters {a,b}.
   */
  pmath_expr_t list, qs, a, b, c, d;
  size_t rows, cols, exprlen;
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2 || exprlen > 3) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 3);
    return expr;
  }
  
  qs = pmath_expr_get_item(expr, 2);
  if(!is_quant_list(qs)) {
    pmath_message(PMATH_NULL, "nquan", 1, qs);
    return expr;
  }
  
  if(exprlen >= 3) {
    pmath_t params = pmath_expr_get_item(expr, 3);
    
    if(is_real_matrix(params, &rows, &cols) && rows == 2 && cols == 2) {
      a = _pmath_matrix_get(params, 1, 1);
      b = _pmath_matrix_get(params, 1, 2);
      c = _pmath_matrix_get(params, 2, 1);
      d = _pmath_matrix_get(params, 2, 2);
      pmath_unref(params);
    }
    else if(is_real_vector(params) && pmath_expr_length(params) == 2) {
      a = pmath_expr_get_item(params, 1);
      a = pmath_expr_get_item(params, 2);
      c = INT(1);
      d = INT(0);
      pmath_unref(params);
    }
    else {
      pmath_unref(qs);
      pmath_message(PMATH_NULL, "parm", 1, params);
      return expr;
    }
  }
  else {
    a = INT(0);
    b = INT(0);
    c = INT(1);
    d = INT(0);
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(is_real_matrix(list, &rows, &cols)) {
    if(rows >= 2) {
      pmath_t qi = PLUS(a, TIMES(qs, PLUS(INT(rows), b)));
      size_t j;
      
      qi = pmath_evaluate(qi);
      
      pmath_unref(expr);
      expr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), cols);
      for(j = cols; j > 0; --j) {
        pmath_expr_t sub = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), rows);
        size_t i;
        
        for(i = rows; i > 0; --i)
          sub = pmath_expr_set_item(sub, i, _pmath_matrix_get(list, i, j));
          
        sub = _pmath_expr_sort_ex(sub, cmp_less);
        
        a = quantile(sub, pmath_ref(qi), pmath_ref(c), pmath_ref(d));
        expr = pmath_expr_set_item(expr, j, a);
      }
      
      pmath_unref(list);
      pmath_unref(qi);
      pmath_unref(c);
      pmath_unref(d);
      return expr;
    }
    
    pmath_message(PMATH_NULL, "shlen", 1, list);
    list = PMATH_UNDEFINED;
  }
  else if(is_real_vector(list)) {
    size_t n = pmath_expr_length(list);
    if(n >= 2) {
      pmath_t qi = PLUS(a, TIMES(qs, PLUS(INT(n), b)));
      
      qi = pmath_evaluate(qi);
      
      pmath_unref(expr);
      list = _pmath_expr_sort_ex(list, cmp_less);
      
      expr = quantile(list, qi, c, d);
      return expr;
    }
    
    pmath_message(PMATH_NULL, "shlen", 1, list);
    list = PMATH_UNDEFINED;
  }
  else {
    pmath_message(PMATH_NULL, "vecmatn", 1, list);
    list = PMATH_UNDEFINED;
  }
  
  pmath_unref(list);
  pmath_unref(qs);
  pmath_unref(a);
  pmath_unref(b);
  pmath_unref(c);
  pmath_unref(d);
  return expr;
}
