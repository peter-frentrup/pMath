#ifndef RICHMATH__UTIL__DOUBLE_MATRIX_H__INCLUDED
#define RICHMATH__UTIL__DOUBLE_MATRIX_H__INCLUDED


#include <util/pmath-extra.h>

namespace richmath {
  /// A matrix of double values
  class DoubleMatrix {
    public:
      DoubleMatrix() 
        : _expr(), _rows(0), _cols(0), _row_stride(0), _raw_data(nullptr)
      {
      }
      
      static DoubleMatrix const_from_expr(Expr expr) {
        if(!expr.is_packed_array())
          return DoubleMatrix{};
        
        if(pmath_packed_array_get_element_type(expr.get()) != PMATH_PACKED_DOUBLE)
          return DoubleMatrix{};
        
        if(pmath_packed_array_get_dimensions(expr.get()) != 2)
          return DoubleMatrix{};
        
        const size_t *sizes = pmath_packed_array_get_sizes(expr.get());
        const size_t *steps = pmath_packed_array_get_steps(expr.get());
        
        assert(steps[0] % sizeof(double) == 0);
        
        size_t rows = sizes[0];
        size_t cols = sizes[1];
        size_t row_stride = steps[0] / sizeof(double);
        
        const void *data = pmath_packed_array_read(expr.get(), nullptr, 0);
        
        return DoubleMatrix{std::move(expr), rows, cols, row_stride, (double*)data};
      }
      
      explicit operator bool() const { return _raw_data != nullptr; }
      bool is_empty() const { return _rows == 0 || _cols == 0; }
      
      size_t rows() const { return _rows; }
      size_t cols() const { return _cols; }
      
      double get(size_t row0, size_t col0) const {
        assert(row0 < rows() || col0 < cols());
        
        return _raw_data[row0 * _row_stride + col0];
      }
      
      friend void swap(DoubleMatrix &left, DoubleMatrix &right) {
        using std::swap;
        
        swap(left._expr,       right._expr);
        swap(left._rows,       right._rows);
        swap(left._cols,       right._cols);
        swap(left._row_stride, right._row_stride);
        swap(left._raw_data,   right._raw_data);
      }
      
    private:
      DoubleMatrix(Expr expr, size_t rows, size_t cols, size_t row_stride, double *data)
        : _expr(expr), _rows(rows), _cols(cols), _row_stride(row_stride), _raw_data(data)
      {
      }
      
    protected:
      Expr _expr;
      size_t _rows;
      size_t _cols;
      size_t _row_stride;
      double *_raw_data;
  };
  
  class WriteableDoubleMatrix : public DoubleMatrix {
    public:
      WriteableDoubleMatrix(size_t rows, size_t cols) 
        : DoubleMatrix()
      {
        size_t sizes[2] = { rows, cols };
        pmath_packed_array_t obj = pmath_packed_array_new(PMATH_NULL, PMATH_PACKED_DOUBLE, 2, sizes, nullptr, 0);
        
        _raw_data = (double*)pmath_packed_array_begin_write(&obj, nullptr, 0);
        _expr = Expr{obj};
        
        if(_raw_data) {
          _rows = rows;
          _cols = cols;
          _row_stride = cols;
          assert(pmath_packed_array_get_steps(obj)[0] == _row_stride * sizeof(double));
        }
      }
    
      void set(size_t row0, size_t col0, double value) {
        assert(row0 < rows() || col0 < cols());
        
        _raw_data[row0 * _row_stride + col0] = value;
      }
  };
}

#endif // RICHMATH__UTIL__DOUBLE_MATRIX_H__INCLUDED
