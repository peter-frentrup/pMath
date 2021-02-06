#ifndef __P4E__GENERAL_MATRIX_CONVERSION_H__
#define __P4E__GENERAL_MATRIX_CONVERSION_H__

#include "arithmetic-expr.h"

#ifdef min
#  undef min
#endif

extern pmath_symbol_t p4e_System_Complex;
extern pmath_symbol_t p4e_System_List;

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
      static pmath::Expr list_from_transpositions(const Eigen::TranspositionsBase<Derived> &perm);

      template<typename Derived>
      static pmath::Expr list_from_vector(const Eigen::MatrixBase<Derived> &vec);

      template<typename Derived>
      static size_t diagonalSize(const Eigen::EigenBase<Derived> &matrix) {
        return std::min(matrix.rows(), matrix.cols());
      }

    protected:
      template<unsigned int Mode>
      struct Triangle {
        template<typename Derived>
        static void from_eigen(pmath::Expr &expr, const Eigen::EigenBase<Derived> &matrix);
      };
  };

  template<typename Scalar>
  struct PackedArrays{
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> MatrixType;

    typedef Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic> StrideType;

    typedef Eigen::Map<const MatrixType, Eigen::Unaligned, StrideType> MapTypeConst;

    // precondition: expr is a packed array of Scalar type and 2 dimensions
    static MapTypeConst make_const_map(pmath_packed_array_t array){
      const Scalar *data  = (const Scalar*)pmath_packed_array_read(array, 0, 0);
      const size_t *sizes = pmath_packed_array_get_sizes(array);
      const size_t *steps = pmath_packed_array_get_steps(array);

      return MapTypeConst(data, sizes[0], sizes[1], StrideType(steps[0] / steps[1], steps[1] / sizeof(Scalar)));
    }
  };

  template<typename Derived>
  inline void Converter::to_eigen(Eigen::MatrixBase<Derived> const &matrix, const pmath::Expr &expr) {
    Eigen::MatrixBase<Derived> &matrix_ = const_cast< Eigen::MatrixBase<Derived>& >(matrix);

    if(expr.is_packed_array() && pmath_packed_array_get_dimensions(expr.get()) == 2){
      switch(pmath_packed_array_get_element_type(expr.get())) {
        case PMATH_PACKED_DOUBLE:
          matrix_ = PackedArrays<double>::make_const_map(expr.get()).cast<typename Derived::Scalar>();
          return;

        case PMATH_PACKED_INT32:
          matrix_ = PackedArrays<int32_t>::make_const_map(expr.get()).cast<typename Derived::Scalar>();
          break;

        default: break;
      }
    }

    for(size_t r = matrix_.rows(); r > 0; --r) {
      pmath::Expr row_expr = expr[r];

      for(size_t c = matrix_.cols(); c > 0; --c) {
        matrix_(r - 1, c - 1) = Converter::to_scalar<typename Eigen::internal::traits<Derived>::Scalar>(row_expr[c]);
      }
    }
  }

  inline pmath::Expr Converter::const_matrix(size_t rows, size_t cols, const pmath::Expr &val) {
    pmath::Expr expr = pmath::MakeCall(pmath::Symbol(p4e_System_List), rows);

    for(size_t r = rows; r > 0; --r) {
      pmath::Expr row_expr = pmath::MakeCall(pmath::Symbol(p4e_System_List), cols);

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
    pmath::Expr expr = pmath::MakeCall(pmath::Symbol(p4e_System_List), matrix.rows());

    for(size_t r = matrix.rows(); r > 0; --r) {
      pmath::Expr row_expr = pmath::MakeCall(pmath::Symbol(p4e_System_List), matrix.cols());

      for(size_t c = matrix.cols(); c > 0; --c) {
        row_expr.set(c, ComplexExpr(matrix.derived()(r - 1, c - 1)));
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
        row_expr.set(c, ComplexExpr(matrix.derived()(r - 1, c - 1)));
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
                         ComplexExpr(ScalarType(0)));

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
          row_expr.set(c, ComplexExpr(matrix.derived()(r - 1, c - 1)));
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
        expr.set(r, r, ComplexExpr(matrix.derived()(r - 1, r - 1)));
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
          row_expr.set(c, ComplexExpr(matrix.derived()(r - 1, c - 1)));
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
        expr.set(r, r, ComplexExpr(matrix.derived()(r - 1, r - 1)));
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

    if(e.expr_length() == 2 && e[0] == p4e_System_Complex)
      return std::complex<double>(to_scalar<double>(e[1]), to_scalar<double>(e[2]));

    return std::numeric_limits<double>::quiet_NaN();
  }
  
  #define P4E_IMPL_Converter__to_scalar(TYPE) \
    template<> inline TYPE Converter::to_scalar< TYPE >(const pmath::Expr &e) { return TYPE(e); }
    
  P4E_IMPL_Converter__to_scalar(ComplexExpr)
  P4E_IMPL_Converter__to_scalar(RealExpr)

  template<typename Derived>
  inline pmath::Expr Converter::list_from_permutation(const Eigen::PermutationBase<Derived> &perm)
  {
    pmath::Expr list = pmath::MakeCall(pmath::Symbol(p4e_System_List), perm.size());

    for(size_t i = perm.size(); i > 0; --i)
      list.set(i, 1 + (size_t)perm.indices()[i - 1]);

    return list;
  }

  template<typename Derived>
  inline pmath::Expr Converter::list_from_transpositions(const Eigen::TranspositionsBase<Derived> &perm)
  {
    pmath::Expr list = pmath::MakeCall(pmath::Symbol(p4e_System_List), perm.size());

    for(size_t i = perm.size(); i > 0; --i)
      list.set(i, 1 + (size_t)perm.indices()[i - 1]);

    return list;
  }

  template<typename Derived>
  inline pmath::Expr Converter::list_from_vector(const Eigen::MatrixBase<Derived> &vec)
  {
    pmath::Expr list = pmath::MakeCall(pmath::Symbol(p4e_System_List), vec.size());

    for(size_t i = vec.size(); i > 0; --i)
      list.set(i, ComplexExpr(vec(i - 1)));

    return list;
  }

}

#endif // __P4E__GENERAL_MATRIX_CONVERSION_H__
