#include "classification.h"
#include "conversion.h"

#include <Eigen/LU>


using namespace pmath;
using namespace pmath4eigen;
using namespace Eigen;



/*template<typename RealType>
struct is_arithmetic_expr{
  enum{
    Value = 0
  };
};

template<>
struct is_arithmetic_expr< ArithmeticExpr > {
  enum{
    Value = 1
  };
};


template<typename MatrixType>
class ConditionNumber {
  public:
    typedef typename Eigen::internal::traits<MatrixType>::Scalar  ScalarType;
    typedef typename NumTraits< ScalarType >::Real                RealType;
    
    static RealType approx_Kinf(const FullPivLU<MatrixType> &lu, const MatrixType &original_matrix);
    
    static RealType approx_Kinf_if_needed(const FullPivLU<MatrixType> &lu, const MatrixType &original_matrix) {
      if(is_arithmetic_expr<RealType>::Value)
        return 1;
      else
        return approx_Kinf(lu, original_matrix);
    }
};

template<typename MatrixType>
inline typename ConditionNumber<MatrixType>::RealType 
ConditionNumber<MatrixType>
::approx_Kinf(const FullPivLU<MatrixType> &lu, const MatrixType &original_matrix)
{
  typedef Matrix<ScalarType, MatrixType::RowsAtCompileTime, 1>      VectorType;
  
  // K_inf = |A|_inf * |A^-1|_inf >= |A| * |y| / |Ay| -> max for
  //   y = eigenvector of A corrwsponding to smallest eigenvalue
  //
  // y(i+1) = A^-1 y(i) / |y(i)|_inf converges to that eigenvector.
  
  VectorType y = VectorType::Random(original_matrix.rows());
  
  for(int i = 0; i < 5; ++i) {
    y = lu.solve(y / y.template lpNorm<Infinity>());
  }
  
  return original_matrix.cwiseAbs().rowwise().sum().maxCoeff() * y.template lpNorm<Infinity>();
}*/


/*
template<typename MatrixType>
static bool is_symbolic_matrix(Expr &matrix_expr)
{
  return false;
}

template<>
bool is_symbolic_matrix<ArithmeticExpr>(Expr &matrix_expr)
{
  return MatrixKind::is_symbolic_matrix(matrix_expr);
}


template<typename MatrixType>
static void set_threshold(FullPivLU<MatrixType> &lu, const Expr &matrix_expr)
{
  // TODO: determine epsilon and call lu.setThreashold( lu.matrixLU().diagonalSize() * epsilon )
}
*/


template<typename MatrixType>
static Expr ludecomposition(Expr &matrix_expr, size_t rows, size_t cols)
{
  MatrixType matrix(rows, cols);
  
  Converter::to_eigen(matrix, matrix_expr);
  
  FullPivLU<MatrixType> lu(matrix);
  
//  set_threshold(lu, matrix_expr);
//  if(rows == cols && !lu.isInvertible()) {
//    if(!is_symbolic_matrix<MatrixType>(matrix_expr)){
//      pmath_message(
//        PMATH_NULL, "sing", 1, matrix_expr.release());
//    }
//  }
  
  Expr lu_expr   = Converter::from_eigen(lu.matrixLU());
  Expr p_expr    = Converter::list_from_permutation(lu.permutationP());
  Expr q_expr    = Converter::list_from_permutation(lu.permutationQ());
  
//  if(rows == cols){
//    Expr cond_expr = ConditionNumber<MatrixType>::approx_Kinf_if_needed(lu, matrix);
//    
//    return List(lu_expr, p_expr, q_expr, cond_expr);
//  }
  
  return List(lu_expr, p_expr, q_expr);
}


pmath_t p4e_builtin_fullpivlu(pmath_expr_t _expr)
{
// Eigen`FullPivLU(M) gives {lu, p, q} such that Dot(P, M, Q) = Dot(L, U)
//   - M may be rectangular or non-invertibale.
//   - lu is the combined LU-matrix.
//   - p and q are permutation index vectors representing the permutation 
//     matrices P and Q
// Note that this is not suitable for symbolic matrices because of the used 
// pivoting stategy! => causes divisions...

  Expr expr(_expr);
  
  if(expr.expr_length() != 1) {
    pmath_message_argxxx(expr.expr_length(), 1, 1);
    return expr.release();
  }
  
  Expr matrix = expr[1];
  
  size_t rows, cols;
  if( !MatrixKind::get_matrix_dimensions(matrix, rows, cols) || rows == 0) {
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
      return ludecomposition<MatrixXa>(matrix, rows, cols).release();
      
    case MatrixKind::MachineReal:
      return ludecomposition<MatrixXd>(matrix, rows, cols).release();
      
    case MatrixKind::MachineComplex:
      return ludecomposition<MatrixXcd>(matrix, rows, cols).release();
      
  }
  
  pmath_debug_print("[Eigen`LUDecomposition: unexpected classification %d]\n", (int)type);
  return pmath_ref(PMATH_SYMBOL_FAILED);
}
