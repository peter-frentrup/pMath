#ifndef RICHMATH__UTIL__MATRIX_H__INCLUDED
#define RICHMATH__UTIL__MATRIX_H__INCLUDED

#include <util/array.h>


namespace richmath {
  template<typename T> class Matrix {
    public:
      explicit Matrix(int rows = 0, int cols = 0)
        : _items(rows *cols),
        _cols(cols)
      {
        ARRAY_ASSERT(rows >= 0);
        ARRAY_ASSERT(cols >= 0);
      }
      
      ~Matrix() {
      }
      
      int length() { return _items.length(); }
      int capacity() const { return _items.capacity(); }
      int rows() const { return _items.length() / _cols; }
      int cols() const { return _cols; }
      
      const T &get(int i) const {
        return _items[i];
      }
      
      T &get(int i) {
        return _items[i];
      }
      
      const T &get(int y, int x) const {
        ARRAY_ASSERT(x >= 0 && x < _cols);
        ARRAY_ASSERT(y >= 0 && y < _items.length() / _cols);
        
        return _items[x + y * _cols];
      }
      
      T &get(int y, int x) {
        ARRAY_ASSERT(x >= 0 && x < _cols);
        ARRAY_ASSERT(y >= 0 && y < _items.length() / _cols);
        
        return _items[x + y * _cols];
      }
      
      void set(int i, const T &t) {
        _items.set(i, t);
      }
      
      void set(int y, int x, const T &t) {
        ARRAY_ASSERT(x >= 0 && x < _cols);
        ARRAY_ASSERT(y >= 0 && y < _items.length() / _cols);
        
        _items.set(x + y * _cols, t);
      }
      
      void insert_rows(int yindex, int extra_rows) {
        ARRAY_ASSERT(extra_rows >= 0);
        
        _items.length(_items.length() + extra_rows * _cols);
        for(int x = _cols - 1; x >= 0; --x)
          for(int y = rows() - 1; y >= yindex + extra_rows; --y)
            _items.set(x + y * _cols, _items[x + (y - extra_rows) * _cols]);
      }
      
      void insert_rows(int yindex, int extra_rows, const T &t) {
        insert_rows(yindex, extra_rows);
        for(int x = 0; x < _cols; ++x)
          for(int y = yindex; y < yindex + extra_rows; ++y)
            _items.set(x + y * _cols, t);
      }
      
      void insert_cols(int xindex, int extra_cols) {
        ARRAY_ASSERT(extra_cols >= 0);
        
        _items.length(_items.length() + extra_cols * rows());
        int old_cols = _cols;
        _cols += extra_cols;
        for(int y = rows() - 1; y >= 0; --y) {
          for(int x = _cols - 1; x >= xindex + extra_cols; --x)
            _items.set(x + y * _cols, _items[x - extra_cols + y * old_cols]);
            
          for(int x = xindex; x >= 0; --x)
            _items.set(x + y * _cols, _items[x + y * old_cols]);
        }
      }
      
      void insert_cols(int xindex, int extra_cols, const T &t) {
        insert_cols(xindex, extra_cols);
        for(int y = rows() - 1; y >= 0; --y)
          for(int x = xindex; x < xindex + extra_cols; ++x)
            _items.set(x + y * _cols, t);
      }
      
      void remove_rows(int yindex, int rem_rows) {
        ARRAY_ASSERT(yindex >= 0);
        ARRAY_ASSERT(rem_rows >= 0);
        ARRAY_ASSERT(yindex + rem_rows <= rows());
        
        int new_rows = rows() - rem_rows;
        for(int y = yindex; y < new_rows; ++y)
          for(int x = 0; x < _cols; ++x)
            _items.set(x + y * _cols, _items[x + (y + rem_rows) * _cols]);
            
        _items.length(new_rows * _cols);
      }
      
      void remove_cols(int xindex, int rem_cols) {
        ARRAY_ASSERT(xindex >= 0);
        ARRAY_ASSERT(rem_cols >= 0);
        ARRAY_ASSERT(xindex + rem_cols <= _cols);
        
        for(int y = 0; y < rows(); ++y) {
          for(int x = 0; x < xindex; ++x)
            _items.set(x + y *(_cols - rem_cols), _items[x + y * _cols]);
          for(int x = xindex; x < _cols - rem_cols; ++x)
            _items.set(x + y *(_cols - rem_cols), _items[x + rem_cols + y * _cols]);
        }
        
        _items.length(rows() *(_cols - rem_cols));
        _cols -= rem_cols;
      }
      
      const T &operator[](int i) const {
        return get(i);
      }
      
      T &operator[](int i) {
        return get(i);
      }
      
      const Array<T> &as_array() { return _items; }
      
      int yx_to_index(int y, int x) const {
        return x + y * _cols;
      }
      void index_to_yx(int index, int *y, int *x) const {
        *x = index % _cols;
        *y = index / _cols;
      }
    private:
      Array<T> _items;
      int      _cols;
  };
}

#endif // RICHMATH__UTIL__MATRIX_H__INCLUDED
