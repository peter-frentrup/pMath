#include "classification.h"
#include "conversion.h"

#include <Eigen/SVD>


using namespace pmath;
using namespace pmath4eigen;
using namespace Eigen;

extern pmath_symbol_t p4e_System_DollarFailed;

template<typename MatrixType, int QRPreconditioner>
static Expr jacobisvd(Expr &expr, Expr &matrix_expr, size_t rows, size_t cols)
{
  MatrixType matrix(rows, cols);
  
  Converter::to_eigen(matrix, matrix_expr);
  
  JacobiSVD<MatrixType, QRPreconditioner> svd(matrix, ComputeFullU | ComputeFullV);
  
  expr        = Expr();
  matrix_expr = Expr();
  
  Expr s_expr = Converter::list_from_vector(svd.singularValues());
  Expr u_expr = Converter::from_eigen(svd.matrixU());
  Expr v_expr = Converter::from_eigen(svd.matrixV());
  
  return List(s_expr, u_expr, v_expr);
}


pmath_t p4e_builtin_jacobisvd(pmath_expr_t _expr)
{
/* Eigen`JacobiSVD(M) gives {s,U,V} 
   such that M = Dot(U, DiagonalMatrix(s), V.Conjugate().Transpose())
 */

  Expr expr(_expr);
  
  if(expr.expr_length() != 1) {
    pmath_message_argxxx(expr.expr_length(), 1, 1);
    return expr.release();
  }
  
  Expr matrix = expr[1];
  
  size_t rows, cols;
  if( !MatrixKind::get_matrix_dimensions(matrix, rows, cols) || 
      rows == 0) 
  {
    pmath_message(
      PMATH_NULL,
      "mat", 2,
      matrix.release(),
      PMATH_FROM_INT32(1));
      
    return expr.release();
  }
  
  MatrixKind::Type type = MatrixKind::classify(matrix);
  
  switch(type) {
    case MatrixKind::General:
      return jacobisvd<MatrixXa, ColPivHouseholderQRPreconditioner>(expr, matrix, rows, cols).release();
      
    case MatrixKind::MachineReal:
      return jacobisvd<MatrixXd, ColPivHouseholderQRPreconditioner>(expr, matrix, rows, cols).release();
      
    case MatrixKind::MachineComplex:
      return jacobisvd<MatrixXcd, ColPivHouseholderQRPreconditioner>(expr, matrix, rows, cols).release();
      
  }
  
  pmath_debug_print("[%s: unexpected classification %d]\n", __func__, (int)type);
  return pmath_ref(p4e_System_DollarFailed);
}
