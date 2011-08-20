#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/number-theory-private.h>


/* The Berkowitz algorithm.
   input: an n*n matrix A.
   output: a list with the n+1 coefficients of its characteristic polynomial
           Det(A-xI). The coefficient (-1)^n of x^n comes first.
 */
static pmath_t berkowitz(pmath_expr_t A) { // A wont be freed
  pmath_t C, S, Q, Vect, sum;
  size_t i, j, k, r, mi;
  const size_t n = pmath_expr_length(A);
  
  C    = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), n + 1);
  S    = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), n + 1);
  Q    = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), n + 1);
  Vect = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), n + 1);
  
  // C[1]:= Vect[1]:= -1;   Vect[2]:= A[1,1]
  C    = pmath_expr_set_item(C,    1, PMATH_FROM_INT32(-1));
  Vect = pmath_expr_set_item(Vect, 1, PMATH_FROM_INT32(-1));
  Vect = pmath_expr_set_item(Vect, 2, _pmath_matrix_get(A, 1, 1));
  
  for(r = 2; r <= n; ++r) {
    // S[1..r-1]:= A[1..r-1, r]
    for(i = 1; i < r; ++i) {
      S = pmath_expr_set_item(S, i, _pmath_matrix_get(A, i, r));
    }
    
    // C[2]:= A[r,r]
    C = pmath_expr_set_item(C, 2, _pmath_matrix_get(A, r, r));
    
    for(i = 1; i <= r - 2; ++i) {
      // C[i+2]:= Sum(A[r,k] * S[k], k->1..r-1)
      sum = pmath_expr_new(pmath_ref(PMATH_SYMBOL_PLUS), r - 1);
      for(k = 1; k < r; ++k) {
        sum = pmath_expr_set_item(sum, k,
                                  TIMES(_pmath_matrix_get(A, r, k), pmath_expr_get_item(S, k)));
      }
      sum = pmath_evaluate(sum);
      C = pmath_expr_set_item(C, i + 2, sum);
      
      for(j = 1; j < r; ++j) {
        // Q[j]:= Sum(A[j,k] * S[k], k->1..r-1)
        sum = pmath_expr_new(pmath_ref(PMATH_SYMBOL_PLUS), r - 1);
        for(k = 1; k <= r - 1; ++k) {
          sum = pmath_expr_set_item(sum, k,
                                    TIMES(_pmath_matrix_get(A, j, k), pmath_expr_get_item(S, k)));
        }
        sum = pmath_evaluate(sum);
        Q = pmath_expr_set_item(Q, j, sum);
      }
      
      // S[1..r-1]:= Q[1..r-1]
      for(j = 1; j < r; ++j) {
        S = pmath_expr_set_item(S, j, pmath_expr_get_item(Q, j));
      }
    }
    
    // C[r+1]:= Sum(A[r,j] * S[j], j->1..r-1)
    sum = pmath_expr_new(pmath_ref(PMATH_SYMBOL_PLUS), r - 1);
    for(j = 1; j < r; ++j) {
      sum = pmath_expr_set_item(sum, j,
                                TIMES(_pmath_matrix_get(A, r, j), pmath_expr_get_item(S, j)));
    }
    sum = pmath_evaluate(sum);
    C = pmath_expr_set_item(C, r + 1, sum);
    
    for(i = 1; i <= r + 1; ++i) {
      // Q[i]:= Sum(C[i+1-j] * Vect[j], j->1..Min(i,r))
      mi = i < r ? i : r;
      sum = pmath_expr_new(pmath_ref(PMATH_SYMBOL_PLUS), mi);
      for(j = 1; j <= mi; ++j) {
        sum = pmath_expr_set_item(sum, j,
                                  TIMES(pmath_expr_get_item(C, i + 1 - j), pmath_expr_get_item(Vect, j)));
      }
      sum = pmath_evaluate(sum);
      Q = pmath_expr_set_item(Q, i, sum);
    }
    
    // Vect[1..r+1]:= Q[1..r+1]
    for(i = 1; i <= r + 1; ++i) {
      Vect = pmath_expr_set_item(Vect, i, pmath_expr_get_item(Q, i));
    }
  }
  
  pmath_unref(C);
  pmath_unref(S);
  pmath_unref(Q);
  return Vect;
}

/*berkowitz(~M)::=
  Local({A,Vect,i,j,r,C,S,Q,n,k},
    A:= M;
    n:= Length(M);
    C:= Array(n+1);
    Vect:= S:= Q:= Array(1..n+1);
    C[1]:= Vect[1]:= -1;
    Vect[2]:= A[1,1];
    Do(
      S:= S.ReplacePart(~i /? i < r :> A[i,r]);
      C[2]:= A[r,r];
      Do(
        C[i+2]:= Plus @@ (A[r,1..r-1] * S[1..r-1]);
        Do(
          Q[j]:= Plus @@ (A[j,1..r-1] * S[1..r-1]);
        ,j->1..r-1);
        S:= S.ReplacePart(~j /? j<r :> Q[j]);
      ,i->1..r-2);
      C[r+1]:= Plus @@ (A[r,1..r-1] * S[1..r-1]);
      Do(
        Q[i]:= Plus @@ (C[i..max(i+1-r,1)] * Vect[1..min(r,i)])
      ,i->1..r+1);
      Vect:= Vect.ReplacePart(~i /? i<=r+1 :> Q[i]);
    ,r->2..n);
    Vect
    )*/

static pmath_t symbolic_det(pmath_expr_t matrix) { // matrix will be freed
  size_t n = pmath_expr_length(matrix);
  pmath_t char_poly = berkowitz(matrix);
  pmath_t det = pmath_expr_get_item(char_poly, n + 1);
  pmath_unref(char_poly);
  pmath_unref(matrix);
  
  return det;
}

/*deprecated: the naive approach (O(n!))

static void swap(size_t *arr, size_t a, size_t b){
  size_t tmp = arr[a];
  arr[a] = arr[b];
  arr[b] = tmp;
}

static pmath_bool_t next_permutation(size_t *arr, size_t len, int *sign){
  size_t i, j, k;

  if(len < 2)
    return FALSE;

  i = len - 2;
  while(i != 0 && arr[i] > arr[i+1])
    --i;

  if(i == 0 && arr[0] > arr[1])
    return FALSE;

  k = i + 1;
  for(j = i+2;j < len;++j){
    if(arr[i] < arr[j] && arr[j] < arr[k])
      k = j;
  }

  swap(arr, i,k);
  *sign = - *sign;

  for(j = i+1;j <= (len+i) / 2;++j){
    swap(arr, j, len+i-j);
    if(j != len+i-j)
      *sign = - *sign;
  }

  return TRUE;
}

static pmath_t symbolic_det(pmath_expr_t matrix){ // matrix will be freed
  const size_t n = pmath_expr_length(matrix);
  size_t i, j, k, sumlen;
  pmath_t sum;
  size_t *perm;
  int sign;

#define EVAL_TRIGGER  16

  perm = pmath_mem_alloc(sizeof(size_t) * n);
  if(!perm){
    pmath_unref(matrix);
    return PMATH_NULL;
  }

  for(i = 0;i < n;++i)
    perm[i] = i+1;
  sign = 1;

  sumlen = EVAL_TRIGGER; k = 1;
  sum = pmath_expr_new(pmath_ref(PMATH_SYMBOL_PLUS), sumlen);
  for(j = 1;!pmath_aborting();++j){
    pmath_t mul;

    if(sign < 0){
      mul = pmath_expr_new(pmath_ref(PMATH_SYMBOL_TIMES), n+1);
      mul = pmath_expr_set_item(mul, n+1, PMATH_FROM_INT32(-1));
    }
    else
      mul = pmath_expr_new(pmath_ref(PMATH_SYMBOL_TIMES), n);

    for(i = 0;i < n;++i){
      mul = pmath_expr_set_item(
        mul, i+1,
        _pmath_matrix_get(matrix, i+1, perm[i]));
    }

    sum = pmath_expr_set_item(sum, k, mul);
    if(k == sumlen){
      sum = pmath_evaluate(sum);

      if(pmath_is_expr_of(sum, PMATH_SYMBOL_PLUS)){
        k = pmath_expr_length(sum) + 1;
        sumlen = 2 * k;// + EVAL_TRIGGER;
        sum = pmath_expr_resize(sum, sumlen);
      }
      else{
        pmath_t old = sum;
        sumlen = EVAL_TRIGGER;
        k = 2;
        sum = pmath_expr_new(pmath_ref(PMATH_SYMBOL_PLUS), sumlen);
        sum = pmath_expr_set_item(sum, 1, old);
      }
    }
    else
      ++k;

    if(!next_permutation(perm, n, &sign))
      break;
  }

  pmath_mem_free(perm);
  pmath_unref(matrix);

  return pmath_expr_resize(sum, k-1);
}
*/

static pmath_bool_t use_symbolic_det(pmath_expr_t matrix) { // wont be freed
  pmath_bool_t inexact = FALSE;
  size_t i, j;
  
  for(i = pmath_expr_length(matrix); i > 0; --i) {
    pmath_t row = pmath_expr_get_item(matrix, i);
    
    for(j = pmath_expr_length(row); j > 0; --j) {
      pmath_t obj = pmath_expr_get_item(row, j);
      
      if(pmath_is_float(obj)) {
        inexact = TRUE;
      }
      else if(!pmath_is_number(obj)) {
        if(!_pmath_is_nonreal_complex(obj)) {
          if(!_pmath_is_numeric(obj)) { // non-numeric entity found
            pmath_unref(obj);
            pmath_unref(row);
            return TRUE;
          }
        }
      }
      
      pmath_unref(obj);
    }
    
    pmath_unref(row);
  }
  
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_det(pmath_expr_t expr) {
  pmath_t matrix;
  size_t rows, cols;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  matrix = pmath_expr_get_item(expr, 1);
  if(!_pmath_is_matrix(matrix, &rows, &cols, TRUE)
      || rows != cols
      || rows == 0) {
    pmath_message(PMATH_NULL, "matsq", 2, matrix, PMATH_FROM_INT32(1));
    return expr;
  }
  
  pmath_unref(expr);
  if(use_symbolic_det(matrix)) { // berkowitz-algorithm (no divisions needed)
    expr = symbolic_det(matrix);
  }
  else { // lu-decomposition
    size_t *perm;
    int sgn;
    
    perm = pmath_mem_alloc(sizeof(size_t) * rows);
    if(!perm) {
      pmath_unref(matrix);
      return PMATH_NULL;
    }
    
    sgn = _pmath_matrix_ludecomp(&matrix, perm, TRUE);
    if(sgn == 0) {
      pmath_unref(matrix);
      pmath_mem_free(perm);
      return PMATH_FROM_INT32(0);
    }
    
    pmath_mem_free(perm);
    pmath_gather_begin(PMATH_NULL);
    if(sgn < 0)
      pmath_emit(PMATH_FROM_INT32(-1), PMATH_NULL);
      
    for(cols = 1; cols <= rows; ++cols) {
      pmath_emit(_pmath_matrix_get(matrix, cols, cols), PMATH_NULL);
    }
    
    pmath_unref(matrix);
    expr = pmath_expr_set_item(
             pmath_gather_end(), 0,
             pmath_ref(PMATH_SYMBOL_TIMES));
  }
  
  return pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_EXPAND), 1,
           expr);
}
