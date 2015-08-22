#include <pmath-core/numbers-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>


static pmath_bool_t is_zero(pmath_t x) {
  if(pmath_is_expr_of(x, PMATH_SYMBOL_LIST)) {
    size_t i = pmath_expr_length(x);

    for(; i > 0; --i) {
      pmath_t item = pmath_expr_get_item(x, i);

      if(!pmath_is_number(x) || pmath_number_sign(x) != 0) {
        pmath_unref(item);
        return FALSE;
      }

      pmath_unref(item);
    }

    return TRUE;
  }

  return pmath_is_number(x) && pmath_number_sign(x) == 0;
}

static pmath_t expand_times(pmath_t factor, pmath_t sum) {
  if(pmath_is_expr_of(sum, PMATH_SYMBOL_PLUS)) {
    size_t i = pmath_expr_length(sum);

    for(; i > 0; --i) {
      pmath_t item = pmath_expr_extract_item(sum, i);
      item = pmath_evaluate(TIMES(pmath_ref(factor), item));
      sum = pmath_expr_set_item(sum, i, item);
    }

    pmath_unref(factor);
    return sum;
  }

  return TIMES(factor, sum);
}


/* This uses the LU Decomposition backsubstitution method described in
     William H. Press et al,
     NUMERICAL RECIPES IN C
     pp 40-47
   with adapted permutation vector.
 */
PMATH_PRIVATE
void _pmath_matrix_lubacksubst(
  pmath_expr_t  lumatrix,
  pmath_expr_t *vector
) {
  const size_t n = pmath_expr_length(lumatrix);
  size_t i, ii = 0, j;
  pmath_t sum;

  for(i = 1; i <= n; ++i) {
    sum = pmath_expr_get_item(*vector, i);

    if(ii) {
      for(j = ii; j < i; ++j) {
        sum = pmath_evaluate(MINUS(sum, expand_times( // sum-= a[i][j] * b[j]
                                     _pmath_matrix_get(lumatrix, i, j),
                                     pmath_expr_get_item(*vector, j))));
      }
    }
    else if(!is_zero(sum))
      ii = i;

    *vector = pmath_expr_set_item(*vector, i, sum);
  }

  for(i = n; i > 0; --i) {
    sum = pmath_expr_get_item(*vector, i);
    for(j = i + 1; j <= n; ++j) {
      sum = pmath_evaluate(MINUS(sum, expand_times( // sum-= a[i][j] * b[j]
                                   _pmath_matrix_get(lumatrix, i, j),
                                   pmath_expr_get_item(*vector, j))));
    }

    *vector = pmath_expr_set_item(*vector, i,
                                  pmath_evaluate(expand_times(INV(_pmath_matrix_get(lumatrix, i, i)), sum)));
  }
}


PMATH_PRIVATE pmath_t builtin_linearsolve(pmath_expr_t expr) {
  /* LinearSolve(A, b) = LinearSolve(A)(b)  ~~>  LinearSolveFunction(...)(b)

     TODO: support rectangular matrices; e.g. via PseudoInverse
   */
  size_t exprlen = pmath_expr_length(expr);
  pmath_t matrix;
  pmath_t vector;
  size_t rows, cols, i;
  size_t *indx;
  int sgn;

  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }

  matrix = pmath_expr_get_item(expr, 1);
  if(!_pmath_is_matrix(matrix, &rows, &cols, TRUE)) {
    if(!pmath_is_expr(matrix) && !pmath_is_symbol(matrix))
      pmath_message(PMATH_NULL, "matsq", 2, matrix, PMATH_FROM_INT32(1));
    else
      pmath_unref(matrix);

    return expr;
  }

  if(rows != cols || rows == 0) {
    pmath_message(PMATH_NULL, "matsq", 2, matrix, PMATH_FROM_INT32(1));
    return expr;
  }

  if(exprlen == 2) {
    vector = pmath_expr_get_item(expr, 2);
    if(!pmath_is_expr_of_len(vector, PMATH_SYMBOL_LIST, cols)) {
      if(!pmath_is_expr(vector) && !pmath_is_symbol(vector))
        pmath_message(PMATH_NULL, "lslc", 0);

      pmath_unref(matrix);
      pmath_unref(vector);
      return expr;
    }
  }
  else {
    vector = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), rows);

    for(i = rows; i > 0; --i)
      vector = pmath_expr_set_item(vector, i, pmath_integer_new_siptr(i));
  }


  indx = pmath_mem_alloc(sizeof(size_t) * rows);
  if(!indx) {
    pmath_unref(matrix);
    pmath_unref(vector);
    return expr;
  }

  sgn = _pmath_matrix_ludecomp(&matrix, indx, FALSE);
  if(sgn == 0) {
    pmath_message(PMATH_NULL, "sing", 1, pmath_expr_get_item(expr, 1));
    pmath_unref(matrix);
    pmath_unref(vector);
    pmath_mem_free(indx);
    return expr;
  }
  pmath_unref(expr);

  // apply permutation:
  for(i = 1; i <= rows; ++i) {
    if(indx[i - 1] != i) {
      pmath_t a = pmath_expr_get_item(vector, i);
      pmath_t b = pmath_expr_get_item(vector, indx[i - 1]);
      vector    = pmath_expr_set_item(vector, i,         b);
      vector    = pmath_expr_set_item(vector, indx[i - 1], a);
    }
  }
  pmath_mem_free(indx);

  if(exprlen == 1) {
    return pmath_expr_new_extended(
             pmath_ref(PMATH_SYMBOL_LINEARSOLVEFUNCTION), 2,
             pmath_build_value("(NN)", rows, cols),
             pmath_build_value("(boo)", TRUE, matrix, vector));
  }

  _pmath_matrix_lubacksubst(matrix, &vector);
  pmath_unref(matrix);
  return vector;
}


PMATH_PRIVATE pmath_t builtin_call_linearsolvefunction(pmath_expr_t expr) {
  /* LinearSolveFunction({row, cols}, {True, lumatrix, permvector})(b)
   */
  pmath_t head, vector, headitem, lumatrix, perm_vector, obj1, obj2;
  size_t rows, cols, i, j;

  if(pmath_expr_length(expr) != 1)
    return expr;

  head   = pmath_expr_get_item(expr, 0);
  vector = pmath_expr_get_item(expr, 1);

  if(!pmath_is_expr_of_len(head, PMATH_SYMBOL_LINEARSOLVEFUNCTION, 2)) {
    pmath_unref(head);
    pmath_unref(vector);
    return expr;
  }

  if(!_pmath_is_vector(vector) && !_pmath_is_matrix(vector, &i, &j, TRUE)) {
    pmath_message(PMATH_SYMBOL_LINEARSOLVEFUNCTION, "vecmat1", 1, vector);

    pmath_unref(head);
    return expr;
  }

  headitem = pmath_expr_get_item(head, 2);
  if(!pmath_is_expr_of_len(headitem, PMATH_SYMBOL_LIST, 3)) {
    pmath_unref(headitem);
    pmath_unref(head);
    pmath_unref(vector);
    return expr;
  }

  obj1 = pmath_expr_get_item(headitem, 1);
  pmath_unref(obj1);
  if(!pmath_same(obj1, PMATH_SYMBOL_TRUE)) {
    // no LU decomposition, but original matrix given
    // never emited by current version of LinearSolve, so no need to handle here
    pmath_unref(headitem);
    pmath_unref(head);
    pmath_unref(vector);
    return expr;
  }

  lumatrix    = pmath_expr_get_item(headitem, 2);
  perm_vector = pmath_expr_get_item(headitem, 3);
  pmath_unref(headitem);
  headitem = pmath_expr_get_item(head, 1);
  pmath_unref(head);

  if( !pmath_is_expr_of_len(headitem, PMATH_SYMBOL_LIST, 2) ||
      !_pmath_is_matrix(lumatrix, &rows, &cols, TRUE)       ||
      rows == 0                                             ||
      cols != rows                                          ||
      rows >= (size_t)INT_MAX                               ||
      pmath_expr_length(vector) != cols)
  {
    pmath_unref(lumatrix);
    pmath_unref(perm_vector);
    pmath_unref(headitem);
    pmath_unref(vector);
    return expr;
  }

  obj1 = pmath_expr_get_item(headitem, 1);
  obj2 = pmath_expr_get_item(headitem, 2);
  pmath_unref(headitem);

  if( !pmath_same(obj1, PMATH_FROM_INT32((int)rows)) ||
      !pmath_same(obj2, PMATH_FROM_INT32((int)cols)))
  {
    pmath_unref(obj1);
    pmath_unref(obj2);
    pmath_unref(lumatrix);
    pmath_unref(perm_vector);
    pmath_unref(vector);
    return expr;
  }

  head = pmath_ref(vector);
  for(i = 1; i <= rows; ++i) {
    obj1 = pmath_expr_get_item(perm_vector, i);

    if( !pmath_is_int32(obj1)     ||
        PMATH_AS_INT32(obj1) <= 0 ||
        (unsigned)PMATH_AS_INT32(obj1) > rows)
    {
      pmath_unref(obj1);
      pmath_unref(lumatrix);
      pmath_unref(perm_vector);
      pmath_unref(vector);
      pmath_unref(head);
      return expr;
    }

    j = (unsigned)PMATH_AS_INT32(obj1);
    if(j != i) {
      obj1   = pmath_expr_get_item(head, i);
      obj2   = pmath_expr_get_item(head, j);
      vector = pmath_expr_set_item(vector, i, obj2);
      vector = pmath_expr_set_item(vector, j, obj1);
    }
    else {
      obj1   = pmath_expr_get_item(head,   i);
      vector = pmath_expr_set_item(vector, i, obj1);
    }
  }

  pmath_unref(head);
  pmath_unref(perm_vector);

  pmath_unref(expr);
  _pmath_matrix_lubacksubst(lumatrix, &vector);
  pmath_unref(lumatrix);
  return vector;
}
