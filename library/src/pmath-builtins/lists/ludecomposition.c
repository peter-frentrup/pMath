#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>


static pmath_bool_t greater(pmath_t a, pmath_t b){
  pmath_t tmp;
  
  if(pmath_is_number(a) && pmath_is_number(b)){
    return pmath_compare(a, b) > 0;
  }
  
  tmp = pmath_evaluate(GREATER(pmath_ref(a), pmath_ref(b)));
  pmath_unref(tmp);
  return pmath_same(tmp, PMATH_SYMBOL_TRUE);
}

static pmath_bool_t is_zero(pmath_t x){
  return pmath_is_number(x) && pmath_number_sign(x) == 0;
}

static pmath_t calc_abs(pmath_t x){ // will be freed
  if(pmath_is_number(x)){
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
){
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
){
  pmath_t row = pmath_expr_get_item(matrix, r);
  matrix = pmath_expr_set_item(matrix, r, NULL);
  row = pmath_expr_set_item(row, c, value);
  return pmath_expr_set_item(matrix, r, row);
}

/* This uses the LU Decomposition method described in
     William H. Press, et al,
     NUMERICAL RECIPES IN C
     pp 40-47
 */
static int numeric_ludecomp(
  pmath_expr_t *matrix,
  size_t       *perm,
  pmath_bool_t  sing_fast_exit,
  pmath_t       tiny
){
  pmath_expr_t A = *matrix;
  const size_t n = pmath_expr_length(*matrix);
  size_t i,j,k,imax = 1;
  int sign = 1;
  pmath_expr_t vv = pmath_expr_new(NULL, n);
  
  perm = perm-1; // perm[1] ... perm[n]
  
  for(i = n;i > 0;--i)
    perm[i] = i;
  
  for(i = 1;i <= n;++i){
    pmath_t big = pmath_integer_new_si(0);
    for(j = 1;j <= n;++j){
      if(is_zero(big)){
        pmath_unref(big);
        big = calc_abs(_pmath_matrix_get(A, i,j));
      }
      else{
        pmath_t tmp = calc_abs(_pmath_matrix_get(A, i,j));
        
        if(greater(tmp, big)){
          pmath_unref(big);
          big = tmp;
        }
        else
          pmath_unref(tmp);
      }
    }
    
    if(is_zero(big)){
      pmath_unref(big);
      if(sing_fast_exit){
        *matrix = A;
        pmath_unref(vv);
        return 0;
      }
      
      big = pmath_ref(tiny);
    }
    
    vv = pmath_expr_set_item(vv, i, pmath_evaluate(INV(big)));
  }
  
  for(j = 1;j <= n;++j){
    for(i = 1;i < j;++i){
      pmath_t sum = _pmath_matrix_get(A, i,j);
      for(k = 1;k < i;++k){
        sum = pmath_evaluate(
          PLUS(
            sum, 
            TIMES3(
              INT(-1), 
              _pmath_matrix_get(A, i,k),
              _pmath_matrix_get(A, k,j))));
      }
      A = _pmath_matrix_set(A, i, j, sum);
    }
    
    {
      pmath_t big = pmath_integer_new_si(0);
      for(i = j;i <= n;++i){
        pmath_t sum = _pmath_matrix_get(A, i,j);
        for(k = 1;k < j;++k){
          sum = pmath_evaluate(
            PLUS(
              sum, 
              TIMES3(
                INT(-1), 
                _pmath_matrix_get(A, i,k), 
                _pmath_matrix_get(A, k,j))));
        }
        A = _pmath_matrix_set(A, i, j, pmath_ref(sum));
        
        if(is_zero(big)){
          pmath_unref(big);
          big = pmath_evaluate(TIMES(pmath_expr_get_item(vv, i), ABS(sum)));
          imax = i;
        }
        else{
          sum = pmath_evaluate(TIMES(pmath_expr_get_item(vv, i), ABS(sum)));
          if(greater(sum, big)){
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
    
    if(j != imax){
      pmath_t temp = pmath_expr_get_item(A, j);
      A = pmath_expr_set_item(A, j, pmath_expr_get_item(A, imax));
      A = pmath_expr_set_item(A, imax, temp);
      
      vv = pmath_expr_set_item(vv, imax, pmath_expr_get_item(vv, j));
      
      k          = perm[j];
      perm[j]    = perm[imax];
      perm[imax] = k;
      
      sign = -sign;
    }
    
    {
      pmath_t dum = _pmath_matrix_get(A, j,j);
      if(is_zero(dum)){
        pmath_unref(dum);
//        if(sing_fast_exit){
//          pmath_unref(vv);
//          *matrix = A;
//          return 0;
//        }
//        else{
          dum = pmath_ref(tiny);
          A = _pmath_matrix_set(A, j,j, pmath_ref(dum));
//        }
      }
      
      if(j != n){
        dum = pmath_evaluate(INV(dum));
        for(i = j+1;i <= n;++i){
          A = _pmath_matrix_set(A, i,j, 
            pmath_evaluate(TIMES(_pmath_matrix_get(A, i,j), pmath_ref(dum))));
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
  size_t       *perm,
  pmath_bool_t  sing_fast_exit
){
  pmath_expr_t A = *matrix;
  const size_t n = pmath_expr_length(*matrix);
  size_t i,imax,j,k;
  int sign = 1;
  
  perm = perm-1; // perm[1] ... perm[n]
  
  for(i = n;i > 0;--i)
    perm[i] = i;
  
  if(sing_fast_exit){
    for(i = 1;i <= n;++i){
      pmath_t big = pmath_integer_new_si(0);
      for(j = 1;j <= n;++j){
        if(is_zero(big)){
          pmath_unref(big);
          big = _pmath_matrix_get(A, i,j);
        }
        else
          break;
      }
      
      if(is_zero(big)){
        *matrix = A;
        pmath_unref(big);
        return 0;
      }
      
      pmath_unref(big);
    }
  }
  
  for(j = 1;j <= n;++j){
    for(i = 1;i < j;++i){
      pmath_t sum = _pmath_matrix_get(A, i,j);
      for(k = 1;k < i;++k){
        sum = pmath_evaluate(
          PLUS(
            sum, 
            TIMES3(
              INT(-1), 
              _pmath_matrix_get(A, i,k), 
              _pmath_matrix_get(A, k,j))));
      }
      A = _pmath_matrix_set(A, i, j, sum);
    }
    
    {
      imax = j;
//      pmath_t big = pmath_integer_new_si(0);
      for(i = j;i <= n;++i){
        pmath_t sum = _pmath_matrix_get(A, i,j);
        for(k = 1;k < j;++k){
          sum = pmath_evaluate(
            PLUS(
              sum, 
              TIMES3(
                INT(-1), 
                _pmath_matrix_get(A, i,k), 
                _pmath_matrix_get(A, k,j))));
        }
        A = _pmath_matrix_set(A, i, j, pmath_ref(sum));
        
        if(imax == j && !is_zero(sum)){
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
    
    if(j != imax){
      pmath_t temp = pmath_expr_get_item(A, j);
      A = pmath_expr_set_item(A, j, pmath_expr_get_item(A, imax));
      A = pmath_expr_set_item(A, imax, temp);
      
      k          = perm[j];
      perm[j]    = perm[imax];
      perm[imax] = k;
      
      sign = -sign;
    }
    
    {
      pmath_t dum = _pmath_matrix_get(A, j,j);
      if(is_zero(dum)){
        pmath_unref(dum);
        *matrix = A;
        return 0;
      }
      
      if(j != n){
        dum = pmath_evaluate(INV(dum));
        for(i = j+1;i <= n;++i){
          A = _pmath_matrix_set(
            A, i,j, 
            pmath_evaluate(TIMES(_pmath_matrix_get(A, i,j), pmath_ref(dum))));
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
  size_t       *perm,
  pmath_bool_t  sing_fast_exit
){
  double prec = pmath_precision(pmath_ref(*matrix));
  
  if(isfinite(prec)){
    pmath_t tiny = pmath_evaluate(
      POW(INT(2), pmath_set_precision(pmath_float_new_d(-prec), prec)));
    
    int sgn = numeric_ludecomp(matrix, perm, sing_fast_exit, tiny);
    
    pmath_unref(tiny);
    return sgn;
  }
  
  if(prec < 0){
    pmath_t tiny = pmath_float_new_d(DBL_EPSILON);
    
    int sgn = numeric_ludecomp(matrix, perm, sing_fast_exit, tiny);
    
    pmath_unref(tiny);
    return sgn;
  }
  
  return symbolic_ludecomp(matrix, perm, sing_fast_exit);
}

PMATH_PRIVATE pmath_t builtin_ludecomposition(pmath_expr_t expr){
  pmath_t matrix;
  pmath_t lu_mat;
  size_t rows, cols;
  size_t *perm;
  int sgn;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  matrix = pmath_expr_get_item(expr, 1);
  if(!_pmath_is_matrix(matrix, &rows, &cols)
  || rows != cols
  || rows == 0){
    pmath_message(NULL, "matsq", 2, matrix, pmath_integer_new_si(1));
    return expr;
  }
  
  pmath_unref(expr);
  perm = pmath_mem_alloc(sizeof(size_t) * rows);
  if(!perm){
    pmath_unref(matrix);
    return NULL;
  }
  
  lu_mat = pmath_ref(matrix);
  sgn = _pmath_matrix_ludecomp(&lu_mat, perm, FALSE);
  if(sgn == 0){
    pmath_message(NULL, "sing", 1, matrix);
  }
  else
    pmath_unref(matrix);
  
  matrix = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), rows);
  for(cols = rows;cols > 0;--cols){
    matrix = pmath_expr_set_item(
      matrix, cols,
      pmath_integer_new_size(
        perm[cols-1]));
  }
  
  pmath_mem_free(perm);
  return pmath_build_value("(oo)", lu_mat, matrix);
}
