#include "classification.h"

using namespace pmath;
using namespace pmath4eigen;


bool MatrixKind::get_matrix_dimensions(
  const Expr &matrix,
  size_t     &rows,
  size_t     &cols
) {
  rows = matrix.expr_length();
  cols = 0;
  
  if(rows == 0)
    return false;
    
  if(matrix[0] != PMATH_SYMBOL_LIST)
    return false;
    
  Expr matrix_row = matrix[1];
  cols = matrix_row.expr_length();
  
  if(matrix_row[0] != PMATH_SYMBOL_LIST)
    return false;
    
  for(size_t i = rows; i > 1; --i) {
    matrix_row = matrix[i];
    
    if(matrix_row.expr_length() != cols)
      return false;
      
    if(matrix_row[0] != PMATH_SYMBOL_LIST)
      return false;
    
  }
  
  return true;
}

MatrixKind::Type MatrixKind::classify(const Expr &matrix) {
  bool has_complex_double = false;
  
  for(size_t r = matrix.expr_length();r > 0;--r){
    Expr row = matrix[r];
    
    for(size_t c = row.expr_length();c > 0;--c){
      Expr elem = row[c];
      
      if(elem.is_double())
        continue;
      
      if(elem.expr_length() == 2 && elem[0] == PMATH_SYMBOL_COMPLEX){
        Expr re = elem[1];
        Expr im = elem[2];
        
        has_complex_double = true;
        
        if(re.is_double()){
          if(im.is_double())
            continue;
            
          if(im.is_int32())
            continue;
        }
        else if(re.is_int32()){
          if(im.is_double())
            continue;
        }
        
        return MatrixKind::General;
      }
      
      return MatrixKind::General;
    }
  }
  
  if(has_complex_double)
    return MatrixKind::MachineComplex;
  
  return MatrixKind::MachineReal;
}

bool MatrixKind::is_symbolic_matrix(const pmath::Expr &matrix){
  for(size_t r = matrix.expr_length();r > 0;--r){
    Expr row = matrix[r];
    
    for(size_t c = row.expr_length();c > 0;--c){
      Expr elem = row[c];
      
      if(elem.is_number())
        continue;
      
      if(pmath_is_numeric(elem.get()))
        continue;
        
      return true;
    }
  }
  
  return false;
}
