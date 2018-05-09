#ifndef __UTIL__ARRAY_H__
#define __UTIL__ARRAY_H__

#include <cassert>
#include <cstring>

#define ARRAY_ASSERT(a) \
  do{if(!(a)){ \
      assert_failed(); \
      assert(a); \
    }}while(0)


namespace richmath {
  void assert_failed();
  
  class Range {
    public:
      Range(int _start, int _end)
        : start(_start),
        end(_end)
      {
      }
      
    public:
      int start;
      int end;
  };
  
  template<typename T> class Array {
    public:
      typedef Array<T>          self_type;
      typedef T                 value_type;
      typedef value_type       *iterator;
      typedef const value_type *const_iterator;
    public:
      Array(const Array &src)
        : _length(src._length),
        _capacity(best_capacity(src._length)),
        _items(0)
      {
        if(_length > 0)
          _items = new T[_capacity];
          
        for(int i = 0; i < _length; ++i)
          _items[i] = src._items[i];
      }
      
      explicit Array(int length = 0)
        : _length(length),
        _capacity(best_capacity(length)),
        _items(0)
      {
        ARRAY_ASSERT(_length >= 0);
        if(_length > 0)
          _items = new T[(size_t)_capacity];
      }
      
      Array(int length, T *src)
        : _length(length),
        _capacity(best_capacity(length)),
        _items(0)
      {
        ARRAY_ASSERT(_length >= 0);
        if(_length > 0)
          _items = new T[_capacity];
        memcpy(_items, src, _length * sizeof(T));
      }
      
      ~Array() {
        delete[] _items;
      }
      
      Array &operator=(const Array &src) {
        if(this != &src) {
          delete[] _items;
          
          _length   = src._length;
          _capacity = best_capacity(src._length);
          _items    = 0;
          
          if(_length > 0)
            _items = new T[_capacity];
            
          for(int i = 0; i < _length; ++i)
            _items[i] = src._items[i];
        }
        return *this;
      }
      
      int length()   const { return _length;   }
      int capacity() const { return _capacity; }
      
      iterator begin() {
        return iterator(_items);
      }
      iterator end() {
        return iterator(_items + _length);
      }
      const_iterator begin() const {
        return const_iterator(_items);
      }
      const_iterator end() const {
        return const_iterator(_items + _length);
      }
      
      Array<T> &length(int newlen) {
        ARRAY_ASSERT(newlen >= 0);
        if(newlen > _capacity) {
          //while(newlen > _capacity)
          //  _capacity*= 2;
          _capacity = best_capacity(newlen);
          T *newitems = new T[_capacity];
          for(int i = 0; i < _length; ++i)
            newitems[i] = _items[i];
            
          delete[] _items;
          _items = newitems;
        }
        _length = newlen;
        return *this;
      }
      
      Array<T> &length(int newlen, const T &default_value) {
        int oldlen = _length;
        length(newlen);
        for(int i = oldlen; i < _length; ++i)
          _items[i] = default_value;
        return *this;
      }
      
      Array<T> &zeromem() {
        memset(_items, 0, _length * sizeof(T));
        return *this;
      }
      
      T &operator[](int i) const {
        return get(i);
      }
      
//      const T &operator[](int i) const {
//        return get(i);
//      }

      Array<T> operator[](const Range &range) const {
        return Array<T>(range.end - range.start + 1, _items + range.start);
      }
      
//      const T &get(int i) const {
//        ARRAY_ASSERT(i >= 0);
//        ARRAY_ASSERT(i < _length);
//        return _items[i];
//      }

      T &get(int i) const {
        ARRAY_ASSERT(i >= 0);
        ARRAY_ASSERT(i < _length);
        return _items[i];
      }
      
      Array<T> &set(int i, const T &t) {
        ARRAY_ASSERT(i >= 0);
        ARRAY_ASSERT(i < _length);
        _items[i] = t;
        return *this;
      }
      
      Array<T> &add(const T &t) {
        length(_length + 1);
        _items[_length - 1] = t;
        return *this;
      }
      
      Array<T> &add(int inslen, const T *insitems) {
        ARRAY_ASSERT(inslen >= 0);
        length(_length + inslen);
        for(int i = 0; i < inslen; ++i)
          _items[_length - inslen + i] = insitems[i];
        return *this;
      }
      
      Array<T> &insert(int start, int inslen) {
        ARRAY_ASSERT(start >= 0);
        ARRAY_ASSERT(start <= _length);
        ARRAY_ASSERT(inslen >= 0);
        if(inslen == 0)
          return *this;
          
        length(_length + inslen);
        
        for(int i = _length - 1; i >= start + inslen; --i)
          _items[i] = _items[i - inslen];
          
        return *this;
      }
      
      Array<T> &insert(int start, int inslen, const T *insitems) {
        ARRAY_ASSERT(start >= 0);
        ARRAY_ASSERT(start <= _length);
        ARRAY_ASSERT(inslen >= 0);
        if(inslen == 0)
          return *this;
          
        length(_length + inslen);
        
        for(int i = _length - 1; i >= start + inslen; --i)
          _items[i] = _items[i - inslen];
          
        for(int i = 0; i < inslen; ++i)
          _items[start + i] = insitems[i];
          
        return *this;
      }
      
      Array<T> &remove(int start, int remlen) {
        ARRAY_ASSERT(start >= 0);
        ARRAY_ASSERT(remlen >= 0);
        ARRAY_ASSERT(start + remlen <= _length);
        
        for(int i = start + remlen; i < _length; ++i)
          _items[i - remlen] = _items[i];
          
        return length(_length - remlen);
      }
      
      Array<T> &remove(const Range &range) {
        return remove(range.start, range.end -  range.start + 1);
      }
      
      T *items() {              return _items; }
      const T *items() const { return _items; }
      
      static int best_capacity(int min) {
        if(min <= 0)
          return 0;
        int result = 1;
        while(result < min)
          result *= 2;
          
        return result;
      }
      
      void swap(Array<T> &other) {
        int  l  = _length;
        int  c  = _capacity;
        T   *is = _items;
        _length   = other._length;
        _capacity = other._capacity;
        _items    = other._items;
        
        other._length   = l;
        other._capacity = c;
        other._items    = is;
      }
    private:
      int  _length;
      int  _capacity;
      T   *_items;
  };
}

#endif // __UTIL__ARRAY_H__
