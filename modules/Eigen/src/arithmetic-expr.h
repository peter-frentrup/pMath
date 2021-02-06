#ifndef P4E__ARITHMETIC_EXPR_H__INCLUDED
#define P4E__ARITHMETIC_EXPR_H__INCLUDED

#include <pmath-cpp.h>
#include <complex>


namespace pmath4eigen {
  class UnevaluatedExpr;
  class ComplexExpr;
  class RealExpr;
  
  inline UnevaluatedExpr abs(UnevaluatedExpr x);
  inline RealExpr        abs(ComplexExpr     x);
  
  inline UnevaluatedExpr real(UnevaluatedExpr x);
  inline RealExpr        real(ComplexExpr     x);
  
  inline UnevaluatedExpr imag(UnevaluatedExpr x);
  inline RealExpr        imag(ComplexExpr     x);
  
  inline UnevaluatedExpr acos(UnevaluatedExpr x);
  inline ComplexExpr     acos(ComplexExpr     x);
  inline RealExpr        acos(RealExpr        x);
  
  inline UnevaluatedExpr asin(UnevaluatedExpr x);
  inline ComplexExpr     asin(ComplexExpr     x);
  inline RealExpr        asin(RealExpr        x);
  
  inline UnevaluatedExpr cos(UnevaluatedExpr x);
  inline ComplexExpr     cos(ComplexExpr     x);
  inline RealExpr        cos(RealExpr        x);
  
  inline UnevaluatedExpr sin(UnevaluatedExpr x);
  inline ComplexExpr     sin(ComplexExpr     x);
  inline RealExpr        sin(RealExpr        x);
  
  inline UnevaluatedExpr tan(UnevaluatedExpr x);
  inline ComplexExpr     tan(ComplexExpr     x);
  inline RealExpr        tan(RealExpr        x);
  
  inline UnevaluatedExpr sqrt(UnevaluatedExpr x);
  inline ComplexExpr     sqrt(ComplexExpr     x);
  
  inline UnevaluatedExpr conj(UnevaluatedExpr x);
  inline ComplexExpr     conj(ComplexExpr     x);
  
  inline UnevaluatedExpr exp(UnevaluatedExpr x);
  inline ComplexExpr     exp(ComplexExpr     x);
  inline RealExpr        exp(RealExpr        x);
  
  inline UnevaluatedExpr log(UnevaluatedExpr x);
  inline ComplexExpr     log(ComplexExpr     x);
  
  inline UnevaluatedExpr pow(UnevaluatedExpr a, UnevaluatedExpr b);
  inline ComplexExpr     pow(ComplexExpr     a, ComplexExpr     b);
  inline RealExpr        pow(RealExpr        a, RealExpr        b);
}

#include <Eigen/Core>

/* ============================= Implementation ============================= */

extern pmath_symbol_t p4e_System_Abs;
extern pmath_symbol_t p4e_System_ArcCos;
extern pmath_symbol_t p4e_System_ArcSin;
extern pmath_symbol_t p4e_System_ArcTan;
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
extern pmath_symbol_t p4e_System_Unequal;

namespace pmath4eigen {
  class UnevaluatedExpr : public pmath::Expr {
    public:
      explicit UnevaluatedExpr(pmath::Expr e) : Expr(e) {}
  };
  
  class RealExpr : public pmath::Expr {
    public:
      explicit RealExpr(const pmath::Expr &src) : Expr(src) {}
      RealExpr() : Expr(0) {}
      RealExpr(int i) : Expr(i) {}
      RealExpr(int64_t i) : Expr(i) {}
      RealExpr(double d) : Expr(d) {}
      
      RealExpr &operator+=(const RealExpr &other);
      RealExpr &operator-=(const RealExpr &other);
      RealExpr &operator*=(const RealExpr &other);
      RealExpr &operator/=(const RealExpr &other);
      
      RealExpr real() const { return *this; }
      RealExpr imag() const { return RealExpr(0); }
  };
  
  class ComplexExpr : public pmath::Expr {
    public:
      explicit ComplexExpr(const pmath::Expr &src) : Expr(src) {}
      ComplexExpr() : Expr(0) {}
      ComplexExpr(int i) : Expr(i) {}
      ComplexExpr(int64_t i) : Expr(i) {}
      ComplexExpr(double d) : Expr(d) {}
      ComplexExpr(RealExpr e) : Expr(e.release()) {}
      
      explicit ComplexExpr(RealExpr re, RealExpr im)
        : Expr((im == 0) ? Expr(std::move(re)) : pmath::Complex(std::move(re), std::move(im)))
      {
      }

      explicit ComplexExpr(std::complex<double> c)
        : Expr(
          (std::imag(c) == 0.0)
          ? Expr(std::real(c))
          : Expr(pmath_build_value("Cff", std::real(c), std::imag(c))))
      {
      }
      
      ComplexExpr &operator+=(const ComplexExpr &other);
      ComplexExpr &operator-=(const ComplexExpr &other);
      ComplexExpr &operator*=(const ComplexExpr &other);
      ComplexExpr &operator/=(const ComplexExpr &other);
      
      RealExpr real() const { return pmath4eigen::real(*this); }
      RealExpr imag() const { return pmath4eigen::imag(*this); }
  };
  
  typedef Eigen::Matrix< ComplexExpr, Eigen::Dynamic, Eigen::Dynamic > MatrixXa;
  
  inline UnevaluatedExpr pow(UnevaluatedExpr a, UnevaluatedExpr b) { return UnevaluatedExpr(pmath::Call(pmath::Symbol(p4e_System_Power), a, b)); }
  
  #define P4E_IMPL_BINARY_SYM( NAME, SYM ) \
    inline UnevaluatedExpr NAME(UnevaluatedExpr a, UnevaluatedExpr b) { \
      return UnevaluatedExpr(pmath::Call(pmath::Symbol(SYM), a, b));    \
    }
  
  inline UnevaluatedExpr operator-(UnevaluatedExpr x) {
    return UnevaluatedExpr(pmath::Call(pmath::Symbol(p4e_System_Times), pmath::Expr(-1), std::move(x)));
  }
  
  P4E_IMPL_BINARY_SYM( operator+, p4e_System_Plus )
  P4E_IMPL_BINARY_SYM( operator*, p4e_System_Times )
  P4E_IMPL_BINARY_SYM( operator==, p4e_System_Equal )
  P4E_IMPL_BINARY_SYM( operator!=, p4e_System_Unequal )
  P4E_IMPL_BINARY_SYM( operator<,  p4e_System_Less )
  P4E_IMPL_BINARY_SYM( operator<=, p4e_System_LessEqual )
  P4E_IMPL_BINARY_SYM( operator>,  p4e_System_Greater )
  P4E_IMPL_BINARY_SYM( operator>=, p4e_System_GreaterEqual )
  
  inline UnevaluatedExpr operator-(UnevaluatedExpr left, UnevaluatedExpr right) { return left + (-right); }
  inline UnevaluatedExpr operator/(UnevaluatedExpr left, UnevaluatedExpr right) { return left * pow(right, UnevaluatedExpr(pmath::Expr(-1))); }
  
  #define P4E_IMPL_UNARY_SYM( NAME, SYM ) \
    inline UnevaluatedExpr NAME(UnevaluatedExpr a) {              \
      return UnevaluatedExpr(pmath::Call(pmath::Symbol(SYM), a)); \
    }
	
  P4E_IMPL_UNARY_SYM( abs,   p4e_System_Abs       )
  P4E_IMPL_UNARY_SYM( acos,  p4e_System_ArcCos    )
  P4E_IMPL_UNARY_SYM( asin,  p4e_System_ArcSin    )
  P4E_IMPL_UNARY_SYM( atan,  p4e_System_ArcTan    )
  P4E_IMPL_UNARY_SYM( conj,  p4e_System_Conjugate )
  P4E_IMPL_UNARY_SYM( cos,   p4e_System_Cos       )
  P4E_IMPL_UNARY_SYM( exp,   p4e_System_Exp       )
  P4E_IMPL_UNARY_SYM( imag,  p4e_System_Im        )
  P4E_IMPL_UNARY_SYM( log,   p4e_System_Log       )
  P4E_IMPL_UNARY_SYM( real,  p4e_System_Re        )
  P4E_IMPL_UNARY_SYM( sin,   p4e_System_Sin       )
  P4E_IMPL_UNARY_SYM( sqrt,  p4e_System_Sqrt      )
  P4E_IMPL_UNARY_SYM( tan,   p4e_System_Tan       )
  
  #define P4E_IMPL_UNARY(TYPE, NAME) \
    inline TYPE NAME(TYPE a) { return TYPE( pmath::Evaluate( NAME(UnevaluatedExpr(a)) ) ); }
  #define P4E_IMPL_UNARY_C_AND_R(NAME) \
    P4E_IMPL_UNARY(ComplexExpr, NAME) \
    P4E_IMPL_UNARY(RealExpr,    NAME)
  
  P4E_IMPL_UNARY_C_AND_R( operator- )
  
  #define P4E_IMPL_BINARY(TYPE, NAME) \
    inline TYPE NAME(TYPE a, TYPE b) { return TYPE( UnevaluatedExpr(pmath::Evaluate( NAME(UnevaluatedExpr(a), UnevaluatedExpr(b)) ) )); }
  #define P4E_IMPL_BINARY_C_AND_R(NAME) \
    P4E_IMPL_BINARY(ComplexExpr, NAME) \
    P4E_IMPL_BINARY(RealExpr,    NAME)
  
  P4E_IMPL_BINARY_C_AND_R( pow )
  P4E_IMPL_BINARY_C_AND_R( operator+ )
  P4E_IMPL_BINARY_C_AND_R( operator- )
  P4E_IMPL_BINARY_C_AND_R( operator* )
  P4E_IMPL_BINARY_C_AND_R( operator/ )
  
  inline RealExpr abs(ComplexExpr z) {  return RealExpr( pmath::Evaluate( abs(UnevaluatedExpr(z)) ) ); }
  inline RealExpr imag(ComplexExpr z) { return RealExpr( pmath::Evaluate( imag(UnevaluatedExpr(z)) ) ); }
  inline RealExpr real(ComplexExpr z) { return RealExpr( pmath::Evaluate( real(UnevaluatedExpr(z)) ) ); }
  P4E_IMPL_UNARY_C_AND_R( acos )
  P4E_IMPL_UNARY_C_AND_R( asin )
  P4E_IMPL_UNARY_C_AND_R( atan )
  P4E_IMPL_UNARY_C_AND_R( conj )
  P4E_IMPL_UNARY_C_AND_R( cos  )
  P4E_IMPL_UNARY_C_AND_R( exp  )
  P4E_IMPL_UNARY_C_AND_R( log  )
  P4E_IMPL_UNARY_C_AND_R( sin  )
  P4E_IMPL_UNARY_C_AND_R( sqrt )
  P4E_IMPL_UNARY_C_AND_R( tan  )
  
  #define P4E_IMPL_EQ( TYPE, NAME, NUM_EQ ) \
    inline bool NAME(TYPE left, TYPE right) {   \
      if(left.is_number() && right.is_number()) \
        return NUM_EQ(left.get(), right.get()); \
      return pmath::Evaluate( NAME( UnevaluatedExpr(left), UnevaluatedExpr(right) ) ) == p4e_System_True; \
    }
  
  // not that (x != y) and (x == y) both may return false!
  
  P4E_IMPL_EQ( ComplexExpr, operator==, pmath_equals )
  P4E_IMPL_EQ( RealExpr,    operator==, pmath_equals )
  
  P4E_IMPL_EQ( ComplexExpr, operator!=, !pmath_equals )
  P4E_IMPL_EQ( RealExpr,    operator!=, !pmath_equals )
  
  P4E_IMPL_EQ( ComplexExpr, operator<,  0 >  pmath_compare )
  P4E_IMPL_EQ( RealExpr,    operator<,  0 >  pmath_compare )
  P4E_IMPL_EQ( ComplexExpr, operator<=, 0 >= pmath_compare )
  P4E_IMPL_EQ( RealExpr,    operator<=, 0 >= pmath_compare )
  P4E_IMPL_EQ( ComplexExpr, operator>,  0 <  pmath_compare )
  P4E_IMPL_EQ( RealExpr,    operator>,  0 <  pmath_compare )
  P4E_IMPL_EQ( ComplexExpr, operator>=, 0 <= pmath_compare )
  P4E_IMPL_EQ( RealExpr,    operator>=, 0 <= pmath_compare )
  
  #define P4E_IMPL_OPASSIGN(TYPE, OP) \
    inline TYPE &TYPE::operator OP##=(const TYPE &other) { return *this = *this OP other; }
  
  #define P4E_IMPL_OPASSIGN_C_AND_R(OP) \
    P4E_IMPL_OPASSIGN( ComplexExpr, OP ) \
    P4E_IMPL_OPASSIGN( RealExpr,    OP )
  
  P4E_IMPL_OPASSIGN_C_AND_R( + )
  P4E_IMPL_OPASSIGN_C_AND_R( - )
  P4E_IMPL_OPASSIGN_C_AND_R( * )
  P4E_IMPL_OPASSIGN_C_AND_R( / )
}

namespace Eigen {

  template<>
  struct NumTraits< pmath4eigen::ComplexExpr >
  {
    using Literal    = pmath4eigen::ComplexExpr;
    using Real       = pmath4eigen::RealExpr;
    using NonInteger = pmath4eigen::ComplexExpr;
    using Nested     = pmath4eigen::ComplexExpr;

    enum {
      IsInteger = 0,
      IsComplex = 1,
      IsSigned = 1,
      RequireInitialization = 1,
      ReadCost = 1,
      AddCost = 1,
      MulCost = 1
    };

    static Real epsilon() {
      // TODO: use thread-local default values...
      return Real(0);
    }

    static Real dummy_precision() {
      return epsilon();
    }
  };
  
  template<>
  struct NumTraits< pmath4eigen::RealExpr >
  {
    using Literal    = pmath4eigen::RealExpr;
    using Real       = pmath4eigen::RealExpr;
    using NonInteger = pmath4eigen::ComplexExpr;
    using Nested     = pmath4eigen::ComplexExpr;

    enum {
      IsInteger = 0,
      IsComplex = 0,
      IsSigned = 1,
      RequireInitialization = 1,
      ReadCost = 1,
      AddCost = 10,
      MulCost = 10
    };

    static Real epsilon() {
      // TODO: use thread-local default values...
      return Real(0);
    }

    static Real dummy_precision() {
      return epsilon();
    }
  };
}

namespace Eigen {

  namespace internal {
    #define P4E_IMPL_isApprox(TYPE) \
      inline bool isApprox(TYPE x, TYPE y) {                                \
        using namespace pmath4eigen;                                        \
        return pmath::Evaluate(                                             \
            abs(UnevaluatedExpr(x) - UnevaluatedExpr(y))                    \
              <= UnevaluatedExpr(Eigen::NumTraits<TYPE>::dummy_precision()) \
          ) ==  p4e_System_True;                                            \
      }
    
    P4E_IMPL_isApprox(pmath4eigen::ComplexExpr)
    P4E_IMPL_isApprox(pmath4eigen::RealExpr)
    
    inline bool isApprox(double x, int y) { return isApprox(x, double(y)); }

    inline bool isApproxOrLessThan(pmath4eigen::RealExpr x, pmath4eigen::RealExpr y) {
      using namespace pmath4eigen;
      return pmath::Evaluate(
          UnevaluatedExpr(x) - UnevaluatedExpr(y) 
            <= UnevaluatedExpr(Eigen::NumTraits<RealExpr>::dummy_precision())
        ) == p4e_System_True;
    }
    

//    template<>
//    struct conj_helper<pmath4eigen::ComplexExpr, pmath4eigen::ComplexExpr, false, true>
//    {
//      typedef pmath4eigen::ArithmeticExpr Scalar;
//
//      EIGEN_STRONG_INLINE Scalar pmadd(const Scalar &x, const Scalar &y, const Scalar &c) const
//      {
//        return Scalar(
//                 pmath::Evaluate(
//                   pmath::Call(pmath::Symbol(p4e_System_Plus),
//                     c,
//                     pmath::Call(pmath::Symbol(p4e_System_Times),
//                       x,
//                       pmath::Call(pmath::Symbol(p4e_System_Conjugate), y)))));
//      }
//
//      EIGEN_STRONG_INLINE Scalar pmul(const Scalar &x, const Scalar &y) const
//      {
//        return Scalar(
//                 pmath::Evaluate(
//                   pmath::Call(pmath::Symbol(p4e_System_Times),
//                     x,
//                     pmath::Call(pmath::Symbol(p4e_System_Conjugate), y))));
//      }
//    };
//
//    template<>
//    struct conj_helper<pmath4eigen::ArithmeticExpr, pmath4eigen::ArithmeticExpr, true, false>
//    {
//      typedef pmath4eigen::ArithmeticExpr Scalar;
//
//      EIGEN_STRONG_INLINE Scalar pmadd(const Scalar &x, const Scalar &y, const Scalar &c) const
//      {
//        return Scalar(
//                 pmath::Evaluate(
//                   pmath::Call(pmath::Symbol(p4e_System_Plus),
//                     c,
//                     pmath::Call(pmath::Symbol(p4e_System_Times),
//                       pmath::Call(pmath::Symbol(p4e_System_Conjugate), x),
//                       y))));
//      }
//
//      EIGEN_STRONG_INLINE Scalar pmul(const Scalar &x, const Scalar &y) const
//      {
//        return Scalar(
//                 pmath::Evaluate(
//                   pmath::Call(pmath::Symbol(p4e_System_Times),
//                     pmath::Call(pmath::Symbol(p4e_System_Conjugate), x),
//                     y)));
//      }
//    };
//
//    template<>
//    struct conj_helper<pmath4eigen::ArithmeticExpr, pmath4eigen::ArithmeticExpr, true, true>
//    {
//      typedef pmath4eigen::ArithmeticExpr Scalar;
//
//      EIGEN_STRONG_INLINE Scalar pmadd(const Scalar &x, const Scalar &y, const Scalar &c) const
//      {
//        return Scalar(
//                 pmath::Evaluate(
//                   pmath::Call(pmath::Symbol(p4e_System_Plus),
//                     c,
//                     pmath::Call(pmath::Symbol(p4e_System_Times),
//                       pmath::Call(pmath::Symbol(p4e_System_Conjugate), x),
//                       pmath::Call(pmath::Symbol(p4e_System_Conjugate), y)))));
//      }
//
//      EIGEN_STRONG_INLINE Scalar pmul(const Scalar &x, const Scalar &y) const
//      {
//        return Scalar(
//                 pmath::Evaluate(
//                   pmath::Call(pmath::Symbol(p4e_System_Times),
//                     pmath::Call(pmath::Symbol(p4e_System_Conjugate), x),
//                     pmath::Call(pmath::Symbol(p4e_System_Conjugate), y))));
//      }
//    };
  }
}

#endif // P4E__ARITHMETIC_EXPR_H__INCLUDED
