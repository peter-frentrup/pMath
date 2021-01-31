#ifndef P4E__ARITHMETIC_EXPR_H__INCLUDED
#define P4E__ARITHMETIC_EXPR_H__INCLUDED

#include <pmath-cpp.h>
#include <complex>


namespace pmath4eigen {
  class ArithmeticExpr;

#define P4E_DECL_FUNC(NAME)   inline const ArithmeticExpr NAME(ArithmeticExpr x);

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

  inline const ArithmeticExpr pow(ArithmeticExpr a, ArithmeticExpr b);
}

#include <Eigen/Core>

/* ============================= Implementation ============================= */

extern pmath_symbol_t p4e_System_Abs;
extern pmath_symbol_t p4e_System_ArcCos;
extern pmath_symbol_t p4e_System_ArcSin;
extern pmath_symbol_t p4e_System_Conjugate;
extern pmath_symbol_t p4e_System_Cos;
extern pmath_symbol_t p4e_System_Equal;
extern pmath_symbol_t p4e_System_Exp;
extern pmath_symbol_t p4e_System_False;
extern pmath_symbol_t p4e_System_Greater;
extern pmath_symbol_t p4e_System_GreaterEqual;
extern pmath_symbol_t p4e_System_Im;
extern pmath_symbol_t p4e_System_Less;
extern pmath_symbol_t p4e_System_LessEqual;
extern pmath_symbol_t p4e_System_Log;
extern pmath_symbol_t p4e_System_Plus;
extern pmath_symbol_t p4e_System_Power;
extern pmath_symbol_t p4e_System_Re;
extern pmath_symbol_t p4e_System_Sin;
extern pmath_symbol_t p4e_System_Sqrt;
extern pmath_symbol_t p4e_System_Tan;
extern pmath_symbol_t p4e_System_Times;
extern pmath_symbol_t p4e_System_True;

namespace pmath4eigen {

  class ArithmeticExpr: public pmath::Expr {
    public:
      explicit ArithmeticExpr(const pmath::Expr &src) : Expr(src) {}
      ArithmeticExpr() : Expr(0) {}
      ArithmeticExpr(int i) : Expr(i) {}
      ArithmeticExpr(int64_t i) : Expr(i) {}
      ArithmeticExpr(double d) : Expr(d) {      }

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
               pmath::Call(pmath::Symbol(p4e_System_Plus),
                 left,
                 right)));
  }

  inline const ArithmeticExpr
  operator-(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    return ArithmeticExpr(
             pmath::Evaluate(
               pmath::Call(pmath::Symbol(p4e_System_Plus),
                 left,
                 pmath::Call(pmath::Symbol(p4e_System_Times), pmath::Expr(-1), right))));
  }

  inline const ArithmeticExpr
  operator-(const ArithmeticExpr &arg)
  {
    return ArithmeticExpr(
             pmath::Evaluate(
               pmath::Call(pmath::Symbol(p4e_System_Times), pmath::Expr(-1), arg)));
  }

  inline const ArithmeticExpr
  operator*(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    return ArithmeticExpr(
             pmath::Evaluate(
               pmath::Call(pmath::Symbol(p4e_System_Times),
                 left,
                 right)));
  }

  inline const ArithmeticExpr
  operator/(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    return ArithmeticExpr(
             pmath::Evaluate(
               pmath::Call(pmath::Symbol(p4e_System_Times),
                 left,
                 pmath::Call(pmath::Symbol(p4e_System_Power), right, pmath::Expr(-1)))));
  }

  inline bool
  operator==(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    if(left.is_number() && right.is_number())
      return pmath_equals(left.get(), right.get());

    return pmath::Evaluate(
             pmath::Call(
               pmath::Symbol(p4e_System_Equal),
               left,
               right)
           ) == p4e_System_True;
  }

  // not that (x != y) and (x == y) both may return false!
  inline bool
  operator!=(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    if(left.is_number() && right.is_number())
      return !pmath_equals(left.get(), right.get());

    return pmath::Evaluate(
             pmath::Call(
               pmath::Symbol(p4e_System_Equal),
               left,
               right)
           ) == p4e_System_False;
  }

  inline bool
  operator<(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    if(left.is_number() && right.is_number())
      return left.compare(right) < 0;

    return pmath::Evaluate(
             pmath::Call(
               pmath::Symbol(p4e_System_Less),
               left,
               right)
           ) == p4e_System_True;
  }

  inline bool
  operator<=(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    if(left.is_number() && right.is_number())
      return left.compare(right) <= 0;

    return pmath::Evaluate(
             pmath::Call(
               pmath::Symbol(p4e_System_LessEqual),
               left,
               right)
           ) == p4e_System_True;
  }

  inline bool
  operator>(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    if(left.is_number() && right.is_number())
      return left.compare(right) > 0;

    return pmath::Evaluate(
             pmath::Call(
               pmath::Symbol(p4e_System_Greater),
               left,
               right)
           ) == p4e_System_True;
  }

  inline bool
  operator>=(const ArithmeticExpr &left, const ArithmeticExpr &right)
  {
    if(left.is_number() && right.is_number())
      return left.compare(right) >= 0;

    return pmath::Evaluate(
             pmath::Call(
               pmath::Symbol(p4e_System_GreaterEqual),
               left,
               right)
           ) == p4e_System_True;
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

#define P4E_IMPL_EVAL(NAME, X, FX)                     \
  inline const ArithmeticExpr NAME(ArithmeticExpr X) { \
    return ArithmeticExpr(pmath::Evaluate( FX ) );     \
  }

  P4E_IMPL_EVAL( abs,  x, pmath::Call(pmath::Symbol(p4e_System_Abs),    std::move(x)) )
  P4E_IMPL_EVAL( acos, x, pmath::Call(pmath::Symbol(p4e_System_ArcCos), std::move(x)) )
  P4E_IMPL_EVAL( asin, x, pmath::Call(pmath::Symbol(p4e_System_ArcSin), std::move(x)) )
  P4E_IMPL_EVAL( cos,  x, pmath::Call(pmath::Symbol(p4e_System_Cos),    std::move(x)) )
  P4E_IMPL_EVAL( exp,  x, pmath::Call(pmath::Symbol(p4e_System_Exp),    std::move(x)) )
  P4E_IMPL_EVAL( imag, x, pmath::Call(pmath::Symbol(p4e_System_Im),     std::move(x)) )
  P4E_IMPL_EVAL( log,  x, pmath::Call(pmath::Symbol(p4e_System_Log),    std::move(x)) )
  P4E_IMPL_EVAL( real, x, pmath::Call(pmath::Symbol(p4e_System_Re),     std::move(x)) )
  P4E_IMPL_EVAL( sin,  x, pmath::Call(pmath::Symbol(p4e_System_Sin),    std::move(x)) )
  P4E_IMPL_EVAL( sqrt, x, pmath::Call(pmath::Symbol(p4e_System_Sqrt),   std::move(x)) )
  P4E_IMPL_EVAL( tan,  x, pmath::Call(pmath::Symbol(p4e_System_Tan),    std::move(x)) )

  inline const ArithmeticExpr conj(ArithmeticExpr x) {
    return ArithmeticExpr(
             pmath::Evaluate(
               pmath::Call(
                 pmath::Symbol(p4e_System_Conjugate), std::move(x))));
  }

  inline const ArithmeticExpr pow(ArithmeticExpr x, ArithmeticExpr y) {
    return ArithmeticExpr(
             pmath::Evaluate(
               pmath::Call(pmath::Symbol(p4e_System_Power), std::move(x), std::move(y))));
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

    // What is dummy_precision() for ?
    static pmath4eigen::ArithmeticExpr dummy_precision() {
      return epsilon();
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
      // FIXME: do approximate comparison for reals
      
      // x == y  calls pmath_equals(x, y)
      // If x and y are numbers, x.compare(y) == 0 could be used, but that does
      // not do approximate comparision for doubles.
      return pmath::Evaluate(
               pmath::Call(
                 pmath::Symbol(p4e_System_Equal),
                 x,
                 y)
             ) == p4e_System_True;
    }

    inline bool
    isApproxOrLessThan(
      const pmath4eigen::ArithmeticExpr &x,
      const pmath4eigen::ArithmeticExpr &y)
    {
      // FIXME: do approximate comparison for reals
      
      // If x and y are numbers, x.compare(y) <= 0 could be used, but that does
      // not do approximate comparision for doubles.
      return pmath::Evaluate(
               pmath::Call(
                 pmath::Symbol(p4e_System_LessEqual),
                 x,
                 y)
             ) == p4e_System_True;
    }



    template<>
    struct conj_helper<pmath4eigen::ArithmeticExpr, pmath4eigen::ArithmeticExpr, false, true>
    {
      typedef pmath4eigen::ArithmeticExpr Scalar;

      EIGEN_STRONG_INLINE Scalar pmadd(const Scalar &x, const Scalar &y, const Scalar &c) const
      {
        return Scalar(
                 pmath::Evaluate(
                   pmath::Call(pmath::Symbol(p4e_System_Plus),
                     c,
                     pmath::Call(pmath::Symbol(p4e_System_Times),
                       x,
                       pmath::Call(pmath::Symbol(p4e_System_Conjugate), y)))));
      }

      EIGEN_STRONG_INLINE Scalar pmul(const Scalar &x, const Scalar &y) const
      {
        return Scalar(
                 pmath::Evaluate(
                   pmath::Call(pmath::Symbol(p4e_System_Times),
                     x,
                     pmath::Call(pmath::Symbol(p4e_System_Conjugate), y))));
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
                   pmath::Call(pmath::Symbol(p4e_System_Plus),
                     c,
                     pmath::Call(pmath::Symbol(p4e_System_Times),
                       pmath::Call(pmath::Symbol(p4e_System_Conjugate), x),
                       y))));
      }

      EIGEN_STRONG_INLINE Scalar pmul(const Scalar &x, const Scalar &y) const
      {
        return Scalar(
                 pmath::Evaluate(
                   pmath::Call(pmath::Symbol(p4e_System_Times),
                     pmath::Call(pmath::Symbol(p4e_System_Conjugate), x),
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
                   pmath::Call(pmath::Symbol(p4e_System_Plus),
                     c,
                     pmath::Call(pmath::Symbol(p4e_System_Times),
                       pmath::Call(pmath::Symbol(p4e_System_Conjugate), x),
                       pmath::Call(pmath::Symbol(p4e_System_Conjugate), y)))));
      }

      EIGEN_STRONG_INLINE Scalar pmul(const Scalar &x, const Scalar &y) const
      {
        return Scalar(
                 pmath::Evaluate(
                   pmath::Call(pmath::Symbol(p4e_System_Times),
                     pmath::Call(pmath::Symbol(p4e_System_Conjugate), x),
                     pmath::Call(pmath::Symbol(p4e_System_Conjugate), y))));
      }
    };
  }
}

#endif // P4E__ARITHMETIC_EXPR_H__INCLUDED
