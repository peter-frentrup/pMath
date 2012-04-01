#include "classification.h"
#include "conversion.h"

#include <Eigen/Cholesky>


using namespace pmath;
using namespace pmath4eigen;
using namespace Eigen;


template<typename MatrixType>
static Expr llt(Expr &expr, Expr &matrix_expr, size_t rows_and_cols)
{
  MatrixType matrix(rows_and_cols, rows_and_cols);
  
  Converter::to_eigen(matrix, matrix_expr);
  
  // Todo: specify tolerance...
  if(!MatrixKind::is_hermetian_matrix(matrix)) {
    pmath_message(
      PMATH_NULL,
      "herm", 1,
      matrix_expr.release());
    return expr;
  }
  
  LLT<MatrixType> llt(matrix);
  
  if(llt.info() != Success){
    pmath_message(
      PMATH_NULL,
      "posdef", 1,
      matrix_expr.release());
    return expr;
  }
  
  expr = Expr();
  Converter::from_eigen(matrix_expr, llt.matrixL());
  
  return matrix_expr;
}


pmath_t p4e_builtin_llt(pmath_expr_t _expr)
{
/* Eigen`LLT(M) performs a Cholesky decomposition and gives L such that 
   M = Dot(L, L.Conjugate().Transpose()))
   Therefor, M must be positive definite.
 */

  Expr expr(_expr);
  
  if(expr.expr_length() != 1) {
    pmath_message_argxxx(expr.expr_length(), 1, 1);
    return expr.release();
  }
  
  Expr matrix = expr[1];
  
  size_t rows, cols;
  if( !MatrixKind::get_matrix_dimensions(matrix, rows, cols) || 
      rows != cols ||
      rows == 0) 
  {
    pmath_message(
      PMATH_NULL,
      "matsq", 2,
      matrix.release(),
      PMATH_FROM_INT32(1));
      
    return expr.release();
  }
  
  MatrixKind::Type type = MatrixKind::classify(matrix);
  
  switch(type) {
    case MatrixKind::General:
      return llt<MatrixXa>(expr, matrix, rows).release();
      
    case MatrixKind::MachineReal:
      return llt<MatrixXd>(expr, matrix, rows).release();
      
    case MatrixKind::MachineComplex:
      return llt<MatrixXcd>(expr, matrix, rows).release();
      
  }
  
  pmath_debug_print("[%s: unexpected classification %d]\n", __func__, (int)type);
  return pmath_ref(PMATH_SYMBOL_FAILED);
}
