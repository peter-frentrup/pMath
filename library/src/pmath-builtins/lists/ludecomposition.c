#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>


static pmath_bool_t is_zero(pmath_t x) {
  return pmath_is_number(x) && pmath_number_sign(x) == 0;
}

PMATH_PRIVATE
void _pmath_matrix_is_triangular( // in ludecomposition
  pmath_expr_t  m, // wont be freed
  pmath_bool_t *lower_has_nonzeros,
  pmath_bool_t *diagonal_has_nonzeros,
  pmath_bool_t *diagonal_has_zeros,
  pmath_bool_t *upper_has_nonzeros
) {
  const size_t N = pmath_expr_length(m);
  size_t i, j;
  int diag;
  
  *lower_has_nonzeros = FALSE;
  *upper_has_nonzeros = FALSE;
  
  diag = 0;
  
  for(i = N; i > 0; --i) {
    pmath_t row  = pmath_expr_get_item(m, i);
    pmath_t item = pmath_expr_get_item(row, i);
    pmath_unref(row);
    
    if(is_zero(item))
      diag |= 2;
    else
      diag |= 1;
      
    pmath_unref(item);
    if(diag == 3)
      break;
  }
  
  *diagonal_has_nonzeros = diag & 1;
  *diagonal_has_zeros    = diag & 2;
  
  // lower:
  for(i = N; i > 1; --i) {
    pmath_t row = pmath_expr_get_item(m, i);
    
    for(j = i - 1; j > 0; --j) {
      pmath_t item = pmath_expr_get_item(row, j);
      
      if(!is_zero(item)) {
        pmath_unref(row);
        pmath_unref(item);
        *lower_has_nonzeros = TRUE;
        goto UPPER;
      }
      pmath_unref(item);
    }
    
    pmath_unref(row);
  }
  
UPPER:
  for(i = N - 1; i > 0; --i) {
    pmath_t row = pmath_expr_get_item(m, i);
    
    for(j = i + 1; j <= N; ++j) {
      pmath_t item = pmath_expr_get_item(row, j);
      
      if(!is_zero(item)) {
        pmath_unref(row);
        pmath_unref(item);
        *upper_has_nonzeros = TRUE;
        return;
      }
      pmath_unref(item);
    }
    
    pmath_unref(row);
  }
}


static pmath_bool_t greater(pmath_t a, pmath_t b) {
  pmath_t tmp;
  
  if(pmath_is_number(a) && pmath_is_number(b)) {
    return pmath_compare(a, b) > 0;
  }
  
  tmp = pmath_evaluate(GREATER(pmath_ref(a), pmath_ref(b)));
  pmath_unref(tmp);
  return pmath_same(tmp, PMATH_SYMBOL_TRUE);
}

static pmath_t calc_abs(pmath_t x) { // will be freed
  if(pmath_is_number(x)) {
    if(pmath_number_sign(x) < 0)
      return pmath_number_neg(x);
    return x;
  }
  
  return pmath_evaluate(ABS(x));
}

PMATH_PRIVATE
pmath_t _pmath_matrix_get(
  pmath_expr_t matrix,
  size_t       r,
  size_t       c
) {
  pmath_t row  = pmath_expr_get_item(matrix, r);
  pmath_t item = pmath_expr_get_item(row, c);
  
  pmath_unref(row);
  return item;
}

PMATH_PRIVATE
pmath_expr_t _pmath_matrix_set( // return: new matrix
  pmath_expr_t matrix, // will be freed
  size_t       r,
  size_t       c,
  pmath_t      value   // will be freed
) {
  pmath_t row = pmath_expr_extract_item(matrix, r);
  row         = pmath_expr_set_item(row,    c, value);
  return        pmath_expr_set_item(matrix, r, row);
}

/* This uses the LU Decomposition method described in
     William H. Press et al,
     NUMERICAL RECIPES IN C
     pp 40-47
   with adapted permutation vector.
   
   @param [in/out] matrix          Combined LU decomposition of input value on 
                                   exit.
   @param [out]    indx            Permutaion vector.
   @param [in]     sing_fast_exit  Whether to stop when matrix is found to be
                                   (almost) singular.
   @param [in]     tiny            A small positive constant used in a 
                                   divide-by-zero scenario (through numeric 
                                   roundoff)
 */
static int numeric_ludecomp(
  pmath_expr_t *matrix,
  size_t       *indx,
  pmath_bool_t  sing_fast_exit,
  pmath_t       tiny
) {
  pmath_expr_t A = *matrix;
  const size_t n = pmath_expr_length(*matrix);
  size_t i, j, k, imax = 1;
  int sign = 1;
  pmath_expr_t vv = pmath_expr_new(PMATH_NULL, n);
  
  indx = indx - 1; // indx[1] ... indx[n]
  
  for(i = 1; i <= n; ++i) {
    pmath_t big = PMATH_FROM_INT32(0);
    for(j = 1; j <= n; ++j) {
      if(is_zero(big)) {
        pmath_unref(big);
        big = calc_abs(_pmath_matrix_get(A, i, j));
      }
      else {
        pmath_t tmp = calc_abs(_pmath_matrix_get(A, i, j));
        
        if(greater(tmp, big)) {
          pmath_unref(big);
          big = tmp;
        }
        else
          pmath_unref(tmp);
      }
    }
    
    if(is_zero(big)) {
      pmath_unref(big);
      if(sing_fast_exit) {
        *matrix = A;
        pmath_unref(vv);
        for(j = n; j > 0; --j)
          indx[j] = j;
        return 0;
      }
      
      big = pmath_ref(tiny);
    }
    
    vv = pmath_expr_set_item(vv, i, pmath_evaluate(INV(big)));
  }
  
  for(j = 1; j <= n; ++j) {
    for(i = 1; i < j; ++i) {
      pmath_t sum = _pmath_matrix_get(A, i, j);
      for(k = 1; k < i; ++k) {
        sum = pmath_evaluate(
                PLUS(
                  sum,
                  TIMES3(
                    INT(-1),
                    _pmath_matrix_get(A, i, k),
                    _pmath_matrix_get(A, k, j))));
      }
      A = _pmath_matrix_set(A, i, j, sum);
    }
    
    {
      pmath_t big = PMATH_FROM_INT32(0);
      for(i = j; i <= n; ++i) {
        pmath_t sum = _pmath_matrix_get(A, i, j);
        for(k = 1; k < j; ++k) {
          sum = pmath_evaluate(
                  PLUS(
                    sum,
                    TIMES3(
                      INT(-1),
                      _pmath_matrix_get(A, i, k),
                      _pmath_matrix_get(A, k, j))));
        }
        A = _pmath_matrix_set(A, i, j, pmath_ref(sum));
        
        if(is_zero(big)) {
          pmath_unref(big);
          big = pmath_evaluate(TIMES(pmath_expr_get_item(vv, i), ABS(sum)));
          imax = i;
        }
        else {
          sum = pmath_evaluate(TIMES(pmath_expr_get_item(vv, i), ABS(sum)));
          if(greater(sum, big)) {
            pmath_unref(big);
            big = sum;
            imax = i;
          }
          else
            pmath_unref(sum);
        }
      }
      pmath_unref(big);
    }
    
    if(j != imax) {
      pmath_t temp = pmath_expr_get_item(A, j);
      A = pmath_expr_set_item(A, j, pmath_expr_get_item(A, imax));
      A = pmath_expr_set_item(A, imax, temp);
      
      vv = pmath_expr_set_item(vv, imax, pmath_expr_get_item(vv, j));
      
      sign = -sign;
    }
    indx[j] = imax;
    
    {
      pmath_t dum = _pmath_matrix_get(A, j, j);
      if(is_zero(dum)) {
        pmath_unref(dum);
//        if(sing_fast_exit){
//          pmath_unref(vv);
//          *matrix = A;
//          for(++j;j <= n;++j)
//            indx[j] = j;
//          return 0;
//        }
//        else{
        dum = pmath_ref(tiny);
        A = _pmath_matrix_set(A, j, j, pmath_ref(dum));
//        }
      }
      
      if(j != n) {
        dum = pmath_evaluate(INV(dum));
        for(i = j + 1; i <= n; ++i) {
          A = _pmath_matrix_set(A, i, j,
                                pmath_evaluate(TIMES(_pmath_matrix_get(A, i, j), pmath_ref(dum))));
        }
      }
      pmath_unref(dum);
    }
  }
  
  *matrix = A;
  pmath_unref(vv);
  return sign;
}

/* This is based on the above algorithm, but pivoting is different
   (first nonzero element A[i,i] is used)
 */
static int symbolic_ludecomp(
  pmath_expr_t *matrix,
  size_t       *indx,
  pmath_bool_t  sing_fast_exit
) {
  pmath_expr_t A = *matrix;
  const size_t n = pmath_expr_length(*matrix);
  size_t i, imax, j, k;
  int sign = 1;
  
  indx = indx - 1; // indx[1] ... indx[n]
  
  if(sing_fast_exit) {
    for(i = 1; i <= n; ++i) {
      pmath_t big = PMATH_FROM_INT32(0);
      for(j = 1; j <= n; ++j) {
        if(is_zero(big)) {
          pmath_unref(big);
          big = _pmath_matrix_get(A, i, j);
        }
        else
          break;
      }
      
      if(is_zero(big)) {
        *matrix = A;
        pmath_unref(big);
        for(j = n; j > 0; --j)
          indx[j] = j;
        return 0;
      }
      
      pmath_unref(big);
    }
  }
  
  for(j = 1; j <= n; ++j) {
    for(i = 1; i < j; ++i) {
      pmath_t sum = _pmath_matrix_get(A, i, j);
      for(k = 1; k < i; ++k) {
        sum = pmath_evaluate(
                PLUS(
                  sum,
                  TIMES3(
                    INT(-1),
                    _pmath_matrix_get(A, i, k),
                    _pmath_matrix_get(A, k, j))));
      }
      A = _pmath_matrix_set(A, i, j, sum);
    }
    
    {
      imax = j;
//      pmath_t big = PMATH_FROM_INT32(0);
      for(i = j; i <= n; ++i) {
        pmath_t sum = _pmath_matrix_get(A, i, j);
        for(k = 1; k < j; ++k) {
          sum = pmath_evaluate(
                  PLUS(
                    sum,
                    TIMES3(
                      INT(-1),
                      _pmath_matrix_get(A, i, k),
                      _pmath_matrix_get(A, k, j))));
        }
        A = _pmath_matrix_set(A, i, j, pmath_ref(sum));
        
        if(imax == j && !is_zero(sum)) {
          imax = i;
        }
        pmath_unref(sum);
//        if(is_zero(big)){
//          pmath_unref(big);
//          big = sum;
//          imax = i;
//        }
//        else
//          pmath_unref(sum);
      }
//      pmath_unref(big);
    }
    
    if(j != imax) {
      pmath_t temp = pmath_expr_get_item(A, j);
      A = pmath_expr_set_item(A, j, pmath_expr_get_item(A, imax));
      A = pmath_expr_set_item(A, imax, temp);
      
      sign = -sign;
    }
    indx[j] = imax;
    
    {
      pmath_t dum = _pmath_matrix_get(A, j, j);
      if(is_zero(dum)) {
        pmath_unref(dum);
        *matrix = A;
        for(++j; j <= n; ++j)
          indx[j] = j;
        return 0;
      }
      
      if(j != n) {
        dum = pmath_evaluate(INV(dum));
        for(i = j + 1; i <= n; ++i) {
          A = _pmath_matrix_set(
                A, i, j,
                pmath_evaluate(TIMES(_pmath_matrix_get(A, i, j), pmath_ref(dum))));
        }
      }
      pmath_unref(dum);
    }
  }
  
  *matrix = A;
  return sign;
}

PMATH_PRIVATE
int _pmath_matrix_ludecomp(
  pmath_expr_t *matrix,
  size_t       *indx,
  pmath_bool_t  sing_fast_exit
) {
  double prec;
  pmath_bool_t lower_nz, diag_nz, diag_z, upper_nz;
  
  //{ fast paths for triagonal matrices ...
  _pmath_matrix_is_triangular(*matrix, &lower_nz, &diag_nz, &diag_z, &upper_nz);
  if(!lower_nz) { // upper triagonal matrix
    size_t i;
    for(i = pmath_expr_length(*matrix); i > 0; --i)
      indx[i - 1] = i;
      
    return diag_z ? 0 : 1;
  }
  
  if(!upper_nz) { // lower triagonal matrix
    size_t i;
    if(diag_z) { // 0 on diagonal => singular
      for(i = pmath_expr_length(*matrix); i > 0; --i)
        indx[i - 1] = i;
        
      return 0;
    }
    
    for(i = pmath_expr_length(*matrix); i > 0; --i) {
      pmath_t item = _pmath_matrix_get(*matrix, i, i);
      
      if(pmath_compare(item, PMATH_FROM_INT32(1)) != 0) {
        pmath_unref(item);
        goto NO_LOWER_DIAG1;
      }
      
      pmath_unref(item);
    }
    
    // diag = (1,1,...,1)
    for(i = pmath_expr_length(*matrix); i > 0; --i)
      indx[i - 1] = i;
    return 1;
    
  NO_LOWER_DIAG1: ; // diag != (1,1,...,1)
  }
  //} ... fast paths for triagonal matrices
  
  prec = pmath_precision(pmath_ref(*matrix));
  
  if(isfinite(prec)) {
    pmath_t tiny = pmath_evaluate(
                     POW(INT(2), pmath_set_precision(PMATH_FROM_DOUBLE(-prec), prec)));
                     
    int sgn = numeric_ludecomp(matrix, indx, sing_fast_exit, tiny);
    
    pmath_unref(tiny);
    return sgn;
  }
  
  if(prec < 0) { // -HUGE_VAL = MachinePrecision
    pmath_t tiny = PMATH_FROM_DOUBLE(DBL_EPSILON);
    
    int sgn = numeric_ludecomp(matrix, indx, sing_fast_exit, tiny);
    
    pmath_unref(tiny);
    return sgn;
  }
  
  return symbolic_ludecomp(matrix, indx, sing_fast_exit);
}


PMATH_PRIVATE pmath_t builtin_ludecomposition(pmath_expr_t expr) {
  /* {LU, Pvec, 1} := LUDecomposition(A)
   */
  pmath_t matrix, perm;
  size_t rows, cols;
  size_t *indx;
  int sgn;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  matrix = pmath_expr_get_item(expr, 1);
  if(!_pmath_is_matrix(matrix, &rows, &cols, TRUE) ||
      rows != cols                                 ||
      rows == 0)
  {
    pmath_message(PMATH_NULL, "matsq", 2, matrix, PMATH_FROM_INT32(1));
    return expr;
  }
  
  pmath_unref(expr);
  indx = pmath_mem_alloc(sizeof(size_t) * rows);
  if(!indx) {
    pmath_unref(matrix);
    return PMATH_NULL;
  }
  
  sgn = _pmath_matrix_ludecomp(&matrix, indx, FALSE);
  if(sgn == 0) {
    pmath_message(PMATH_NULL, "sing", 1, pmath_ref(matrix));
  }
  
  perm = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), rows);
  for(cols = rows; cols > 0; --cols) {
    perm = pmath_expr_set_item(perm, cols, pmath_integer_new_uiptr(cols));
  }
  
  for(cols = 1; cols <= rows; ++cols) {
    if(indx[cols - 1] != cols) {
      pmath_t a = pmath_expr_get_item(perm, cols);
      pmath_t b = pmath_expr_get_item(perm, indx[cols - 1]);
      perm      = pmath_expr_set_item(perm, cols,         b);
      perm      = pmath_expr_set_item(perm, indx[cols - 1], a);
    }
  }
  
  pmath_mem_free(indx);
  // todo set third element to condition number estimate
  return pmath_build_value("(ooi)", matrix, perm, 1);
}
