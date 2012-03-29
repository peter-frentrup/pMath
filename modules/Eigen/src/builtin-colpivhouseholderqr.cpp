#include "classification.h"
#include "conversion.h"

#include <Eigen/Dense>


using namespace pmath;
using namespace pmath4eigen;
using namespace Eigen;


template<typename MatrixType>
static Expr colpivhouseholderqr(Expr &matrix_expr, size_t rows, size_t cols)
{
  typedef typename Eigen::internal::traits<MatrixType>::Scalar ScalarType;
  
  MatrixType matrix(rows, cols);
  
  Converter::to_eigen(matrix, matrix_expr);
  
  ColPivHouseholderQR<MatrixType> qr(matrix);
  
  matrix = qr.householderQ();
  Converter::from_eigen(matrix_expr, matrix);
  Expr r_expr = Converter::from_eigen(qr.matrixQR().template triangularView<Upper>());
  Expr p_expr = Converter::list_from_permutation(qr.colsPermutation());
  
  return List(matrix_expr, r_expr);
}


pmath_t p4e_builtin_colpivhouseholderqr(pmath_expr_t _expr)
{
/* Eigen`ColPivHouseholderQR(M) gives {Q, R, p} such that Dot(M, P) = Dot(Q, R)
   Note that this is not suitable for symbolic matrices. Givens Rotations would 
   be are more appropriate than Householder transformations
 */

  Expr expr(_expr);
  
  if(expr.expr_length() != 1) {
    pmath_message_argxxx(expr.expr_length(), 1, 1);
    return expr.release();
  }
  
  Expr matrix = expr[1];
  
  size_t rows, cols;
  if( !MatrixKind::get_matrix_dimensions(matrix, rows, cols) || 
      rows == 0 ||
      cols == 0) 
  {
    pmath_message(
      PMATH_NULL,
      "mat", 2,
      matrix.release(),
      PMATH_FROM_INT32(1));
      
    return expr.release();
  }
  
  expr = Expr();
  MatrixKind::Type type = MatrixKind::classify(matrix);
  
  switch(type) {
    case MatrixKind::General:
      return colpivhouseholderqr<MatrixXa>(matrix, rows, cols).release();
      
    case MatrixKind::MachineReal:
      return colpivhouseholderqr<MatrixXd>(matrix, rows, cols).release();
      
    case MatrixKind::MachineComplex:
      return colpivhouseholderqr<MatrixXcd>(matrix, rows, cols).release();
      
  }
  
  pmath_debug_print("[%s: unexpected classification %d]\n", __FUNC__, (int)type);
  return pmath_ref(PMATH_SYMBOL_FAILED);
}
