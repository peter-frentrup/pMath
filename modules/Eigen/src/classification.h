#ifndef __P4E__CLASSIFICATION_H__
#define __P4E__CLASSIFICATION_H__

#include "arithmetic-expr.h"
#include <Eigen/Core>


namespace pmath4eigen {

  class MatrixKind {
    public:
    
      typedef enum {
        General,
        MachineReal,
        MachineComplex
      } Type;
      
      static bool get_matrix_dimensions(
        const pmath::Expr &matrix,
        size_t            &rows,
        size_t            &cols);
        
      static Type classify(const pmath::Expr &matrix);
      
      static bool is_symbolic_matrix(const pmath::Expr &matrix);
      
      template<typename Derived>
      static bool is_hermetian_matrix(const Eigen::MatrixBase<Derived> &m) {
        return Eigen::internal::isApprox(
                 (m.conjugate() - m.transpose()).cwiseAbs().maxCoeff(), 0);
      }
  };
  
}


#endif // __P4E__CLASSIFICATION_H__
