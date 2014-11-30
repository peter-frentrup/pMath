#ifndef __P4E__ARITHMETIC_EXPR_H__
#define __P4E__ARITHMETIC_EXPR_H__

#include <pmath-cpp.h>
#include <complex>


namespace pmath4eigen {
  class ArithmeticExpr;

#define P4E_DECL_FUNC(NAME)       \
  inline const ArithmeticExpr     \
  NAME(const ArithmeticExpr &x);

  P4E_DECL_FUNC( abs  )
  P4E_DECL_FUNC( acos )
  P4E_DECL_FUNC( asin )
  P4E_DECL_FUNC( conj )
  P4E_DECL_FUNC( cos  )
  P4E_DECL_FUNC( exp  )
  P4E_DECL_FUNC( imag )
  P4E_DECL_FUNC( log  )
  P4E_DECL_FUNC( real )
  P4E_DECL_FUNC( sin  )
  P4E_DECL_FUNC( sqrt )
  P4E_DECL_FUNC( tan  )

  inline const ArithmeticExpr
  pow(const ArithmeticExpr &a, const ArithmeticExpr &b);
}

#include <Eigen/Core>

/* ============================= Implementation ============================= */

namespace pmath4eigen {

  class ArithmeticExpr: public pmath::Expr {
    public:
      explicit ArithmeticExpr(const pmath::Expr &src)
        : Expr(src)
      {
      }

      ArithmeticExpr()
        : Expr(0)
      {
      }

      ArithmeticExpr(int i)
        : Expr(i)
      {
      }

      ArithmeticExpr(int64_t i)
        : Expr(i)
      {
      }

      ArithmeticExpr(double d)
        : Expr(d)
      {
      }

      explicit ArithmeticExpr(const ArithmeticExpr &re, const ArithmeticExpr &im)
        : Expr((im == 0) ? Expr(re) : pmath::Complex(re, im))
      {
      }

      explicit ArithmeticExpr(std::complex<double> c)
        : Expr(
          (std::imag(c) == 0.0)
          ? Expr(std::real(c))
          : Expr(pmath_build_value("Cff", std::real(c), std::imag(c))))
      {
      }

      ArithmeticExpr &operator+=(const ArithmeticExpr &other);
      ArithmeticExpr &operator-=(const ArithmeticExpr &other);
      ArithmeticExpr &operator*=(const ArithmeticExpr &other);
      ArithmeticExpr &operator/=(const ArithmeticExpr &other);
  };

  typedef Eigen::Matrix< ArithmeticExpr, Eigen::Dynamic, Eigen::Dynamic > MatrixXa;

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

  inline bool
  operator==(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    if(left.is_number() && right.is_number())
      return pmath_equals(left.get(), right.get());

    return pmath::Evaluate(
             pmath::Call(
               pmath::Symbol(PMATH_SYMBOL_EQUAL),
               left,
               right)
           ) == PMATH_SYMBOL_TRUE;
  }

  // not that (x != y) and (x == y) both may return false!
  inline bool
  operator!=(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    if(left.is_number() && right.is_number())
      return !pmath_equals(left.get(), right.get());

    return pmath::Evaluate(
             pmath::Call(
               pmath::Symbol(PMATH_SYMBOL_EQUAL),
               left,
               right)
           ) == PMATH_SYMBOL_FALSE;
  }

  inline bool
  operator<(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    if(left.is_number() && right.is_number())
      return left.compare(right) < 0;

    return pmath::Evaluate(
             pmath::Call(
               pmath::Symbol(PMATH_SYMBOL_LESS),
               left,
               right)
           ) == PMATH_SYMBOL_TRUE;
  }

  inline bool
  operator<=(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    if(left.is_number() && right.is_number())
      return left.compare(right) <= 0;

    return pmath::Evaluate(
             pmath::Call(
               pmath::Symbol(PMATH_SYMBOL_LESSEQUAL),
               left,
               right)
           ) == PMATH_SYMBOL_TRUE;
  }

  inline bool
  operator>(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    if(left.is_number() && right.is_number())
      return left.compare(right) > 0;

    return pmath::Evaluate(
             pmath::Call(
               pmath::Symbol(PMATH_SYMBOL_GREATER),
               left,
               right)
           ) == PMATH_SYMBOL_TRUE;
  }

  inline bool
  operator>=(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    if(left.is_number() && right.is_number())
      return left.compare(right) >= 0;

    return pmath::Evaluate(
             pmath::Call(
               pmath::Symbol(PMATH_SYMBOL_GREATEREQUAL),
               left,
               right)
           ) == PMATH_SYMBOL_TRUE;
  }


  inline ArithmeticExpr &
  ArithmeticExpr::operator+=(const ArithmeticExpr &other)
  {
    return *this = *this + other;
  }

  inline ArithmeticExpr &
  ArithmeticExpr::operator-=(const ArithmeticExpr &other)
  {
    return *this = *this - other;
  }

  inline ArithmeticExpr &
  ArithmeticExpr::operator*=(const ArithmeticExpr &other)
  {
    return *this = *this * other;
  }

  inline ArithmeticExpr &
  ArithmeticExpr::operator/=(const ArithmeticExpr &other)
  {
    return *this = *this / other;
  }

#define P4E_IMPL_FUNC(NAME, PMCPP_NAME)         \
  inline const ArithmeticExpr                   \
  NAME(const ArithmeticExpr &x)                 \
  {                                             \
    return ArithmeticExpr(                      \
           pmath::Evaluate( PMCPP_NAME(x) ) );  \
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

  inline const ArithmeticExpr
  conj(const ArithmeticExpr &x)
  {
    return ArithmeticExpr(
             pmath::Evaluate(
               pmath::Call(
                 pmath::Symbol(PMATH_SYMBOL_CONJUGATE),
                 x)));
  }

  inline const ArithmeticExpr
  pow(const ArithmeticExpr &x, const ArithmeticExpr &y)
  {
    return ArithmeticExpr(
             pmath::Evaluate(
               pmath::Power(
                 x,
                 y)));
  }
}

namespace Eigen {

  template<>
  struct NumTraits< pmath4eigen::ArithmeticExpr >
  {
    typedef pmath4eigen::ArithmeticExpr  Real;
    typedef pmath4eigen::ArithmeticExpr  NonInteger;
    typedef pmath4eigen::ArithmeticExpr  Nested;

    enum {
      IsInteger = 0,
      IsComplex = 1,
      IsSigned = 1,
      RequireInitialization = 1,
      ReadCost = 1,
      AddCost = 1,
      MulCost = 1
    };

    static pmath4eigen::ArithmeticExpr epsilon() {
      // TODO: use thread-local default values...
      return pmath4eigen::ArithmeticExpr(1e-8);
    }
  };
}

namespace Eigen {

  namespace internal {

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



    template<>
    struct conj_helper<pmath4eigen::ArithmeticExpr, pmath4eigen::ArithmeticExpr, false, true>
    {
      typedef pmath4eigen::ArithmeticExpr Scalar;

      EIGEN_STRONG_INLINE Scalar pmadd(const Scalar &x, const Scalar &y, const Scalar &c) const
      {
        return Scalar(
                 pmath::Evaluate(
                   pmath::Plus(
                     c,
                     pmath::Times(
                       x,
                       pmath::Call(pmath::Symbol(PMATH_SYMBOL_CONJUGATE), y)))));
      }

      EIGEN_STRONG_INLINE Scalar pmul(const Scalar &x, const Scalar &y) const
      {
        return Scalar(
                 pmath::Evaluate(
                   pmath::Times(
                     x,
                     pmath::Call(pmath::Symbol(PMATH_SYMBOL_CONJUGATE), y))));
      }
    };

    template<>
    struct conj_helper<pmath4eigen::ArithmeticExpr, pmath4eigen::ArithmeticExpr, true, false>
    {
      typedef pmath4eigen::ArithmeticExpr Scalar;

      EIGEN_STRONG_INLINE Scalar pmadd(const Scalar &x, const Scalar &y, const Scalar &c) const
      {
        return Scalar(
                 pmath::Evaluate(
                   pmath::Plus(
                     c,
                     pmath::Times(
                       pmath::Call(pmath::Symbol(PMATH_SYMBOL_CONJUGATE), x),
                       y))));
      }

      EIGEN_STRONG_INLINE Scalar pmul(const Scalar &x, const Scalar &y) const
      {
        return Scalar(
                 pmath::Evaluate(
                   pmath::Times(
                     pmath::Call(pmath::Symbol(PMATH_SYMBOL_CONJUGATE), x),
                     y)));
      }
    };

    template<>
    struct conj_helper<pmath4eigen::ArithmeticExpr, pmath4eigen::ArithmeticExpr, true, true>
    {
      typedef pmath4eigen::ArithmeticExpr Scalar;

      EIGEN_STRONG_INLINE Scalar pmadd(const Scalar &x, const Scalar &y, const Scalar &c) const
      {
        return Scalar(
                 pmath::Evaluate(
                   pmath::Plus(
                     c,
                     pmath::Times(
                       pmath::Call(pmath::Symbol(PMATH_SYMBOL_CONJUGATE), x),
                       pmath::Call(pmath::Symbol(PMATH_SYMBOL_CONJUGATE), y)))));
      }

      EIGEN_STRONG_INLINE Scalar pmul(const Scalar &x, const Scalar &y) const
      {
        return Scalar(
                 pmath::Evaluate(
                   pmath::Times(
                     pmath::Call(pmath::Symbol(PMATH_SYMBOL_CONJUGATE), x),
                     pmath::Call(pmath::Symbol(PMATH_SYMBOL_CONJUGATE), y))));
      }
    };
  }
}

#endif // __P4E__ARITHMETIC_EXPR_H__
