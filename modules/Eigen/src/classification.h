#ifndef __P4E__CLASSIFICATION_H__
#define __P4E__CLASSIFICATION_H__

#include <pmath-cpp.h>


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
      
  };
  
}


#endif // __P4E__CLASSIFICATION_H__
