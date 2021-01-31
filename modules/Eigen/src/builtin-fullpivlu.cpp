#include "classification.h"
#include "conversion.h"

#include <Eigen/LU>


using namespace pmath;
using namespace pmath4eigen;
using namespace Eigen;

extern pmath_symbol_t p4e_System_DollarFailed;

template<typename MatrixType>
static Expr fullpivlu(Expr &matrix_expr, size_t rows, size_t cols)
{
  MatrixType matrix(rows, cols);
  
  Converter::to_eigen(matrix, matrix_expr);
  
  FullPivLU<MatrixType> lu(matrix);
  
  Converter::from_eigen(matrix_expr, lu.matrixLU());
  Expr p_expr = Converter::list_from_permutation(lu.permutationP());
  Expr q_expr = Converter::list_from_permutation(lu.permutationQ());
  
  return List(matrix_expr, p_expr, q_expr);
}


pmath_t p4e_builtin_fullpivlu(pmath_expr_t _expr)
{
/* Eigen`FullPivLU(M) gives {lu, p, q} such that Dot(P, M, Q) = Dot(L, U)
     - M may be rectangular or non-invertibale.
     - lu is the combined LU-matrix.
     - p and q are permutation index vectors representing the permutation 
       matrices P and Q
   Note that this is not suitable for symbolic matrices because of the used 
   pivoting stategy! => causes divisions...
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
      return fullpivlu<MatrixXa>(matrix, rows, cols).release();
      
    case MatrixKind::MachineReal:
      return fullpivlu<MatrixXd>(matrix, rows, cols).release();
      
    case MatrixKind::MachineComplex:
      return fullpivlu<MatrixXcd>(matrix, rows, cols).release();
      
  }
  
  pmath_debug_print("[%s: unexpected classification %d]\n", __func__, (int)type);
  return pmath_ref(p4e_System_DollarFailed);
}
