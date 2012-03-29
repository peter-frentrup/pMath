#ifndef __P4E__GENERAL_MATRIX_CONVERSION_H__
#define __P4E__GENERAL_MATRIX_CONVERSION_H__

#include "arithmetic-expr.h"


namespace pmath4eigen {

  class Converter {
    public:
      // matrix must already be properly sized.
      // see MatrixKind::get_matrix_dimensions()
      template<typename Derived>
      static void to_eigen(Eigen::MatrixBase<Derived> const &matrix, const pmath::Expr &expr);
      
      static pmath::Expr const_matrix(size_t rows, size_t cols, const pmath::Expr &val);
      
      // expr must already be properly sized.
      static void const_matrix(pmath::Expr &expr, size_t rows, size_t cols, const pmath::Expr &val);
      
      template<typename Derived>
      static pmath::Expr from_eigen(const Eigen::MatrixBase<Derived> &matrix);
      
      // expr must already be properly sized.
      template<typename Derived>
      static void from_eigen(pmath::Expr &expr, const Eigen::MatrixBase<Derived> &matrix);
      
      // expr must already be properly sized. and initialized with 0
      template<typename Derived>
      static pmath::Expr from_eigen(const Eigen::TriangularBase<Derived> &matrix);
      
      // expr must already be properly sized. and initialized with 0
      template<typename Derived>
      static void from_eigen(pmath::Expr &expr, const Eigen::TriangularBase<Derived> &matrix);
      
      template<typename ScalarType>
      static ScalarType to_scalar(const pmath::Expr &e);
      
      template<typename Derived>
      static pmath::Expr list_from_permutation(const Eigen::PermutationBase<Derived> &perm);
      
      template<typename Derived>
      static size_t diagonalSize(const Eigen::EigenBase<Derived> &matrix){
        return std::min(matrix.rows(), matrix.cols());
      }
      
    protected:
      template<unsigned int Mode>
      struct Triangle {
        template<typename Derived>
        static void from_eigen(pmath::Expr &expr, const Eigen::EigenBase<Derived> &matrix);
      };
  };
  
  
  template<typename Derived>
  inline void Converter::to_eigen(Eigen::MatrixBase<Derived> const &matrix, const pmath::Expr &expr)
  {
    Eigen::MatrixBase<Derived> &matrix_ = const_cast< Eigen::MatrixBase<Derived>& >(matrix);
    
    for(size_t r = matrix_.rows(); r > 0; --r) {
      pmath::Expr row_expr = expr[r];
      
      for(size_t c = matrix_.cols(); c > 0; --c) {
        matrix_(r - 1, c - 1) = Converter::to_scalar<typename Eigen::internal::traits<Derived>::Scalar>(row_expr[c]);
      }
    }
  }
  
  inline pmath::Expr Converter::const_matrix(size_t rows, size_t cols, const pmath::Expr &val) {
    pmath::Expr expr = pmath::MakeList(rows);
    
    for(size_t r = rows; r > 0; --r) {
      pmath::Expr row_expr = pmath::MakeList(cols);
      
      for(size_t c = cols; c > 0; --c) {
        row_expr.set(c, val);
      }
      
      expr.set(r, row_expr);
    }
    
    return expr;
  }
  
  inline void Converter::const_matrix(pmath::Expr &expr, size_t rows, size_t cols, const pmath::Expr &val) {
    for(size_t r = rows; r > 0; --r) {
      pmath::Expr row_expr(pmath_expr_extract_item(expr.get(), r));
      
      for(size_t c = cols; c > 0; --c) {
        row_expr.set(c, val);
      }
      
      expr.set(r, row_expr);
    }
  }
  
  template<typename Derived>
  inline pmath::Expr Converter::from_eigen(const Eigen::MatrixBase<Derived> &matrix)
  {
    pmath::Expr expr = pmath::MakeList(matrix.rows());
    
    for(size_t r = matrix.rows(); r > 0; --r) {
      pmath::Expr row_expr = pmath::MakeList(matrix.cols());
      
      for(size_t c = matrix.cols(); c > 0; --c) {
        row_expr.set(c, ArithmeticExpr(matrix.derived()(r - 1, c - 1)));
      }
      
      expr.set(r, row_expr);
    }
    
    return expr;
  }
  
  template<typename Derived>
  inline void Converter::from_eigen(pmath::Expr &expr, const Eigen::MatrixBase<Derived> &matrix)
  {
    for(size_t r = matrix.rows(); r > 0; --r) {
      pmath::Expr row_expr(pmath_expr_extract_item(expr.get(), r));
      
      for(size_t c = matrix.cols(); c > 0; --c) {
        row_expr.set(c, ArithmeticExpr(matrix.derived()(r - 1, c - 1)));
      }
      
      expr.set(r, row_expr);
    }
  }
  
  template<typename Derived>
  inline pmath::Expr Converter::from_eigen(const Eigen::TriangularBase<Derived> &matrix)
  {
    typedef typename Eigen::internal::traits<Derived>::Scalar ScalarType;
    
    pmath::Expr expr = Converter::const_matrix(
                         matrix.rows(),
                         matrix.cols(),
                         ArithmeticExpr(ScalarType(0)));
                         
    from_eigen(expr, matrix);
    
    return expr;
  }
  
  template<>
  struct Converter::Triangle<Eigen::StrictlyUpper> {
  
    template<typename Derived>
    static void from_eigen(pmath::Expr &expr, const Eigen::EigenBase<Derived> &matrix)
    {
      for(size_t r = Converter::diagonalSize(matrix); r > 0; --r) {
        pmath::Expr row_expr(pmath_expr_extract_item(expr.get(), r));
        
        for(size_t c = matrix.cols(); c > r; --c) {
          row_expr.set(c, ArithmeticExpr(matrix.derived()(r - 1, c - 1)));
        }
        
        expr.set(r, row_expr);
      }
    }
    
  };
  
  template<>
  struct Converter::Triangle<Eigen::Upper> {
  
    template<typename Derived>
    static void from_eigen(pmath::Expr &expr, const Eigen::EigenBase<Derived> &matrix)
    {
      Converter::Triangle<Eigen::StrictlyUpper>::from_eigen(expr, matrix);
      
      for(size_t r = Converter::diagonalSize(matrix); r > 0; --r) {
        expr.set(r, r, ArithmeticExpr(matrix.derived()(r - 1, r - 1)));
      }
    }
    
  };
  
  template<>
  struct Converter::Triangle<Eigen::UnitUpper> {
  
    template<typename Derived>
    static void from_eigen(pmath::Expr &expr, const Eigen::EigenBase<Derived> &matrix)
    {
      Converter::Triangle<Eigen::StrictlyUpper>::from_eigen(expr, matrix);
      
      for(size_t r = Converter::diagonalSize(matrix); r > 0; --r) {
        expr.set(r, r, pmath::Expr(1));
      }
    }
    
  };
  
  template<>
  struct Converter::Triangle<Eigen::StrictlyLower> {
  
    template<typename Derived>
    static void from_eigen(pmath::Expr &expr, const Eigen::EigenBase<Derived> &matrix)
    {
      for(size_t r = Converter::diagonalSize(matrix); r > 0; --r) {
        pmath::Expr row_expr(pmath_expr_extract_item(expr.get(), r));
        
        for(size_t c = r; c > 0; --c) {
          row_expr.set(c, ArithmeticExpr(matrix.derived()(r - 1, c - 1)));
        }
        
        expr.set(r, row_expr);
      }
    }
    
  };
  
  template<>
  struct Converter::Triangle<Eigen::Lower> {
  
    template<typename Derived>
    static void from_eigen(pmath::Expr &expr, const Eigen::EigenBase<Derived> &matrix)
    {
      Converter::Triangle<Eigen::StrictlyLower>::from_eigen(expr, matrix);
      
      for(size_t r = Converter::diagonalSize(matrix); r > 0; --r) {
        expr.set(r, r, ArithmeticExpr(matrix.derived()(r - 1, r - 1)));
      }
    }
    
  };
  
  
  template<>
  struct Converter::Triangle<Eigen::UnitLower> {
  
    template<typename Derived>
    static void from_eigen(pmath::Expr &expr, const Eigen::EigenBase<Derived> &matrix)
    {
      Converter::Triangle<Eigen::StrictlyLower>::from_eigen(expr, matrix);
      
      for(size_t r = Converter::diagonalSize(matrix); r > 0; --r) {
        expr.set(r, r, pmath::Expr(1));
      }
    }
    
  };
  
  template<typename Derived>
  inline void Converter::from_eigen(pmath::Expr &expr, const Eigen::TriangularBase<Derived> &matrix)
  {
    Converter::Triangle< Eigen::internal::traits<Derived>::Mode >::from_eigen(expr, matrix);
  }
  
  template<>
  inline double Converter::to_scalar<double>(const pmath::Expr &e)
  {
    if(e.is_number())
      return e.to_double();
      
    return std::numeric_limits<double>::quiet_NaN();
  }
  
  template<>
  inline std::complex<double> Converter::to_scalar< std::complex<double> >(const pmath::Expr &e)
  {
    if(e.is_number())
      return e.to_double();
      
    if(e.expr_length() == 2 && e[0] == PMATH_SYMBOL_COMPLEX)
      return std::complex<double>(to_scalar<double>(e[1]), to_scalar<double>(e[2]));
      
    return std::numeric_limits<double>::quiet_NaN();
  }
  
  template<>
  inline ArithmeticExpr Converter::to_scalar< ArithmeticExpr >(const pmath::Expr &e)
  {
    return ArithmeticExpr(e);
  }
  
  template<typename Derived>
  inline pmath::Expr Converter::list_from_permutation(const Eigen::PermutationBase<Derived> &perm)
  {
    pmath::Expr list = pmath::MakeList(perm.size());
    
    for(size_t i = perm.size(); i > 0; --i)
      list.set(i, 1 + (size_t)perm.indices()[i - 1]);
      
    return list;
  }
  
}

#endif // __P4E__GENERAL_MATRIX_CONVERSION_H__
