#include "conversion.h"

#include <cmath>


using namespace pmath;
using namespace pmath4eigen;
using namespace Eigen;

bool Converter::toEigen(ArithmeticExprMatrix &matrix, const Expr &expr)
{
  for(size_t r = matrix.rows(); r > 0; --r) {
    Expr row_expr = expr[r];
    
    for(size_t c = matrix.cols(); c > 0; --c) {
      matrix(r - 1, c - 1) = ArithmeticExpr(row_expr[c]);
    }
  }
  
  return true;
}

bool Converter::toEigen(MatrixXcd &matrix, const Expr &expr)
{
  for(size_t r = matrix.rows(); r > 0; --r) {
    Expr row_expr = expr[r];
    
    for(size_t c = matrix.cols(); c > 0; --c) {
      Expr elem = row_expr[c];
      
      if(elem.is_double()) {
        matrix(r - 1, c - 1) = PMATH_AS_DOUBLE(elem.get());
        continue;
      }
      
      if(elem[0] == PMATH_SYMBOL_COMPLEX) {
        Expr re = elem[1];
        Expr im = elem[2];
        
        double re_value;
        double im_value;
        
        if(re.is_double())
          re_value = PMATH_AS_DOUBLE(re.get());
        else if(re.is_int32())
          re_value = PMATH_AS_INT32(re.get());
        else
          return false;
          
        if(im.is_double())
          im_value = PMATH_AS_DOUBLE(im.get());
        else if(im.is_int32())
          im_value = PMATH_AS_INT32(im.get());
        else
          return false;
          
        matrix(r - 1, c - 1) = std::complex<double>(re_value, im_value);
      }
      
      return false;
    }
  }
  
  return true;
}

bool Converter::toEigen(MatrixXd &matrix, const Expr &expr)
{
  for(size_t r = matrix.rows(); r > 0; --r) {
    Expr row_expr = expr[r];
    
    for(size_t c = matrix.cols(); c > 0; --c) {
      Expr elem = row_expr[c];
      
      if(elem.is_double()) {
        matrix(r - 1, c - 1) = PMATH_AS_DOUBLE(elem.get());
        continue;
      }
      
      return false;
    }
  }
  
  return true;
}

Expr Converter::fromEigen(const ArithmeticExprMatrix &matrix)
{
  Expr expr = MakeList(matrix.rows());
  
  for(size_t r = matrix.rows(); r > 0; --r) {
    Expr row_expr = MakeList(matrix.cols());
    
    for(size_t c = matrix.cols(); c > 0; --c) {
      row_expr.set(c, matrix(r - 1, c - 1));
    }
    
    expr.set(r, row_expr);
  }
  
  return expr;
}

Expr Converter::fromEigen(const MatrixXcd &matrix)
{
  Expr expr = MakeList(matrix.rows());
  
  for(size_t r = matrix.rows(); r > 0; --r) {
    Expr row_expr = MakeList(matrix.cols());
    
    for(size_t c = matrix.cols(); c > 0; --c) {
      const std::complex<double> &value = matrix(r - 1, c - 1);
      
      if(std::imag(value) == 0){
        row_expr.set(c, std::real(value));
        continue;
      }
      
      row_expr.set(c, Complex(std::real(value), std::imag(value)));
    }
    
    expr.set(r, row_expr);
  }
  
  return expr;
}

Expr Converter::fromEigen(const MatrixXd &matrix)
{
  Expr expr = MakeList(matrix.rows());
  
  for(size_t r = matrix.rows(); r > 0; --r) {
    Expr row_expr = MakeList(matrix.cols());
    
    for(size_t c = matrix.cols(); c > 0; --c) {
      row_expr.set(c, matrix(r - 1, c - 1));
    }
    
    expr.set(r, row_expr);
  }
  
  return expr;
}
