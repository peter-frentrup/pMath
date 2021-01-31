#include "classification.h"
#include "conversion.h"

#include <Eigen/LU>


using namespace pmath;
using namespace pmath4eigen;
using namespace Eigen;

extern pmath_symbol_t p4e_System_DollarFailed;

template<typename MatrixType>
static Expr partialpivlu(Expr &matrix_expr, size_t rows_and_cols)
{
  typedef typename Eigen::internal::traits<MatrixType>::Scalar ScalarType;
  
  MatrixType matrix(rows_and_cols, rows_and_cols);
  
  Converter::to_eigen(matrix, matrix_expr);
  
  PartialPivLU<MatrixType> lu(matrix);
  
  for(size_t i = 0;i < rows_and_cols;++i){
    if(Eigen::internal::isApprox(lu.matrixLU()(i, i), ScalarType(0))){
      pmath_message(
        PMATH_NULL,
        "sing", 1,
        pmath_ref(matrix_expr.get()));
      
      break;
    }
  }
  
  Converter::from_eigen(matrix_expr, lu.matrixLU());
  Expr p_expr = Converter::list_from_permutation(lu.permutationP());
  
  return List(matrix_expr, p_expr);
}


pmath_t p4e_builtin_partialpivlu(pmath_expr_t _expr)
{
/* Eigen`PartialPivLU(M) gives {lu, p} such that Dot(P, M) = Dot(L, U)
     - M must be square and invertible.
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
  
  expr = Expr();
  MatrixKind::Type type = MatrixKind::classify(matrix);
  
  switch(type) {
    case MatrixKind::General:
      return partialpivlu<MatrixXa>(matrix, rows).release();
      
    case MatrixKind::MachineReal:
      return partialpivlu<MatrixXd>(matrix, rows).release();
      
    case MatrixKind::MachineComplex:
      return partialpivlu<MatrixXcd>(matrix, rows).release();
      
  }
  
  pmath_debug_print("[%s: unexpected classification %d]\n", __func__, (int)type);
  return pmath_ref(p4e_System_DollarFailed);
}
