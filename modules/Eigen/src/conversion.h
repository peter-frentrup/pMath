#ifndef __P4E__GENERAL_MATRIX_CONVERSION_H__
#define __P4E__GENERAL_MATRIX_CONVERSION_H__

#include "arithmetic-expr.h"


namespace pmath4eigen {
  
  class Converter{
    public:
    
      // matrix must already be properly sized.
      // see MatrixKind::get_matrix_dimensions()
      static bool toEigen(ArithmeticExprMatrix &matrix, const pmath::Expr &expr);
      
      // matrix must already be properly sized.
      // expr must be classfied as MachineComplex or MachineReal
      // see MatrixKind::classify()
      static bool toEigen(Eigen::MatrixXcd &matrix, const pmath::Expr &expr);
      
      // matrix must already be properly sized.
      // expr must be classfied as MachineReal
      // see MatrixKind::classify()
      static bool toEigen(Eigen::MatrixXd &matrix, const pmath::Expr &expr);
      
      static pmath::Expr fromEigen(const ArithmeticExprMatrix &matrix);
      static pmath::Expr fromEigen(const Eigen::MatrixXcd &matrix);
      static pmath::Expr fromEigen(const Eigen::MatrixXd &matrix);
  };
  
}

#endif // __P4E__GENERAL_MATRIX_CONVERSION_H__
