#ifndef __P4E__ARITHMETIC_EXPR_H__
#define __P4E__ARITHMETIC_EXPR_H__

#include <pmath-cpp.h>
#include <Eigen/Core>


namespace pmath4eigen {

  class ArithmeticExpr: public pmath::Expr {
    public:
      explicit ArithmeticExpr(const pmath::Expr &src)
        : pmath::Expr(src)
      {
      }
      
      ArithmeticExpr()
        : Expr((size_t)0)
      {
      }
      
      ArithmeticExpr(int i)
        : pmath::Expr(i)
      {
      }
  };
  
  typedef Eigen::Matrix< ArithmeticExpr, Eigen::Dynamic, Eigen::Dynamic > ArithmeticExprMatrix;
  
  inline const ArithmeticExpr
  operator+(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    return ArithmeticExpr(
             pmath::Evaluate(
               pmath::Plus(
                 left,
                 right)));
  }
  
  inline const ArithmeticExpr
  operator-(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    return ArithmeticExpr(
             pmath::Evaluate(
               pmath::Minus(
                 left,
                 right)));
  }
  
  inline const ArithmeticExpr
  operator-(const ArithmeticExpr &arg)
  {
    return ArithmeticExpr(
             pmath::Evaluate(
               pmath::Minus(
                 arg)));
  }
  
  inline const ArithmeticExpr
  operator*(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    return ArithmeticExpr(
             pmath::Evaluate(
               pmath::Times(
                 left,
                 right)));
  }
  
  inline const ArithmeticExpr
  operator/(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    return ArithmeticExpr(
             pmath::Evaluate(
               pmath::Divide(
                 left,
                 right)));
  }
  
}

namespace Eigen {

  template<> struct NumTraits< pmath4eigen::ArithmeticExpr >
  {
    typedef pmath4eigen::ArithmeticExpr Real;
    typedef pmath4eigen::ArithmeticExpr NonInteger;
    typedef pmath4eigen::ArithmeticExpr Nested;
    
    enum {
      IsInteger = 0,
      IsComplex = 1,
      IsSigned = 1,
      RequireInitialization = 1,
      ReadCost = 1,
      AddCost = 1,
      MulCost = 1
    };
  };
  
}

namespace Eigen {

  namespace internal {
  
#define P4E_IMPL_FUNC(NAME, PMCPP_NAME)                                     \
  inline const pmath4eigen::ArithmeticExpr                                  \
  NAME(const pmath4eigen::ArithmeticExpr &x)                                \
  {                                                                         \
    return pmath4eigen::ArithmeticExpr( pmath::Evaluate( PMCPP_NAME(x) ) ); \
  }
  
    P4E_IMPL_FUNC( abs,  pmath::Abs    )
    P4E_IMPL_FUNC( acos, pmath::ArcCos )
    P4E_IMPL_FUNC( asin, pmath::ArcSin )
    P4E_IMPL_FUNC( cos,  pmath::Cos    )
    P4E_IMPL_FUNC( exp,  pmath::Exp    )
    P4E_IMPL_FUNC( imag, pmath::Im     )
    P4E_IMPL_FUNC( log,  pmath::Log    )
    P4E_IMPL_FUNC( real, pmath::Re     )
    P4E_IMPL_FUNC( sin,  pmath::Sin    )
    P4E_IMPL_FUNC( sqrt, pmath::Sqrt   )
    P4E_IMPL_FUNC( tan,  pmath::Tan    )
    
    inline const pmath4eigen::ArithmeticExpr
    conj(const pmath4eigen::ArithmeticExpr &x)
    {
      return pmath4eigen::ArithmeticExpr(
               pmath::Evaluate(
                 pmath::Call(
                   pmath::Symbol(PMATH_SYMBOL_CONJUGATE),
                   x)));
    }
    
    inline const pmath4eigen::ArithmeticExpr
    pow(const pmath4eigen::ArithmeticExpr &x, const pmath4eigen::ArithmeticExpr &y)
    {
      return pmath4eigen::ArithmeticExpr(
               pmath::Evaluate(
                 pmath::Power(
                   x,
                   y)));
    }
    
    inline bool
    isApprox(
      const pmath4eigen::ArithmeticExpr &x,
      const pmath4eigen::ArithmeticExpr &y)
    {
      // x == y  calls pmath_equals(x, y)
      // If x and y are numbers, x.compare(y) == 0 could be used, but that does
      // not do apprximate comparision for doubles.
      return pmath::Evaluate(
               pmath::Call(
                 pmath::Symbol(PMATH_SYMBOL_EQUAL),
                 x,
                 y)
             ) == PMATH_SYMBOL_TRUE;
    }
    
    inline bool
    isApproxOrLessThan(
      const pmath4eigen::ArithmeticExpr &x,
      const pmath4eigen::ArithmeticExpr &y)
    {
      // If x and y are numbers, x.compare(y) <= 0 could be used, but that does
      // not do apprximate comparision for doubles.
      return pmath::Evaluate(
               pmath::Call(
                 pmath::Symbol(PMATH_SYMBOL_LESSEQUAL),
                 x,
                 y)
             ) == PMATH_SYMBOL_TRUE;
    }
  }
  
}

#endif // __P4E__ARITHMETIC_EXPR_H__
