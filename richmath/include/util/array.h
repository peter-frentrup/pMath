#ifndef RICHMATH__UTIL__ARRAY_H__INCLUDED
#define RICHMATH__UTIL__ARRAY_H__INCLUDED

#include <cassert>
#include <cstring>
#include <utility>

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
  
  template<typename T> class ArrayView {
    public:
      using self_type = ArrayView<T>;
      using value_type = T;
      using iterator = value_type*;
      using const_iterator = const value_type*;
    
    public:
      explicit ArrayView(int length, T *items) : _items(items), _length(length) { ARRAY_ASSERT(length >= 0); }
      
      template<int N>
      ArrayView(T items[N]) : _items(items), _length(N) {}
      
      int length() const { return _length; }
      
      iterator begin() { return iterator(_items); }
      iterator end() {   return iterator(_items + _length); }
      const_iterator begin() const { return const_iterator(_items); }
      const_iterator end() const {   return const_iterator(_items + _length); }
      
      T &get(int i) const {
        ARRAY_ASSERT(i >= 0);
        ARRAY_ASSERT(i < _length);
        return _items[i];
      }
      
      template<class S>
      T &set(int i, const S &t) {
        ARRAY_ASSERT(i >= 0);
        ARRAY_ASSERT(i < _length);
        return _items[i] = t;
      }
      
      T &set(int i, T &&t) {
        ARRAY_ASSERT(i >= 0);
        ARRAY_ASSERT(i < _length);
        return _items[i] = std::move(t);
      }
      
      T &operator[](int i) const { return get(i); }
      
      T *items() {             return _items; }
      const T *items() const { return _items; }
      
    private:
      T *_items;
      int _length;
  };
  
  template<typename T> class Array {
    public:
      using self_type = Array<T>;
      using value_type = T;
      using iterator = value_type*;
      using const_iterator = const value_type*;
    public:
      Array(ArrayView<T> src)
        : _length(src.length()),
        _capacity(best_capacity(src.length())),
        _items(nullptr)
      {
        if(_length > 0)
          _items = new T[_capacity];
          
        const T *src_items = src.items();
        for(int i = 0; i < _length; ++i)
          _items[i] = src_items[i];
      }
      
      Array(const Array &src)
        : _length{0}, _capacity{0}, _items{nullptr}
      {
        *this = ArrayView<const T>(src);
      }
      
      Array(Array &&src)
        : _length{0}, _capacity{0}, _items{nullptr}
      {
        swap(*this, src);
      }
      
      explicit Array(int length = 0)
        : _length(length),
        _capacity(best_capacity(length)),
        _items(nullptr)
      {
        ARRAY_ASSERT(_length >= 0);
        if(_capacity > 0)
          _items = new T[(size_t)_capacity];
      }
      
      Array(int length, T *src)
        : _length(length),
        _capacity(best_capacity(length)),
        _items(nullptr)
      {
        ARRAY_ASSERT(_length >= 0);
        if(_length > 0)
          _items = new T[_capacity];
        memcpy(_items, src, _length * sizeof(T));
      }
      
      ~Array() {
        delete[] _items;
      }
      
      Array &operator=(Array other) {
        swap(*this, other);
        return *this;
      }
      
      Array &operator=(ArrayView<const T> other) {
        length(other.length());
        const T *other_items = other.items();
        for(int i = 0; i < _length; ++i)
          _items[i] = other_items[i];
        return *this;
      }
      
      operator       ArrayView<T>() {             return ArrayView<T>(      _length, _items); }
      operator const ArrayView<const T>() const { return ArrayView<const T>(_length, _items); }
      
      int length()   const { return _length;   }
      int capacity() const { return _capacity; }
      
      iterator begin() { return iterator(_items); }
      iterator end() {   return iterator(_items + _length); }
      const_iterator begin() const { return const_iterator(_items); }
      const_iterator end() const {   return const_iterator(_items + _length); }
      
      Array<T> &length(int newlen) {
        using std::swap;
        
        ARRAY_ASSERT(newlen >= 0);
        if(newlen > _capacity) {
          //while(newlen > _capacity)
          //  _capacity*= 2;
          _capacity = best_capacity(newlen);
          T *newitems = new T[_capacity];
          for(int i = 0; i < _length; ++i)
            swap(newitems[i], _items[i]);
            
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
      
      T &operator[](int i) const { return get(i); }
      
//      const T &operator[](int i) const { return get(i); }

      ArrayView<T> operator[](const Range &range) const {
        ARRAY_ASSERT(range.start >= 0);
        ARRAY_ASSERT(range.end < _length);
        ARRAY_ASSERT(range.end - range.start + 1 >= 0);
        return ArrayView<T>(range.end - range.start + 1, _items + range.start);
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
      
      template<class S>
      Array<T> &set(int i, const S &t) {
        ARRAY_ASSERT(i >= 0);
        ARRAY_ASSERT(i < _length);
        _items[i] = t;
        return *this;
      }
      
      Array<T> &set(int i, T &&t) {
        ARRAY_ASSERT(i >= 0);
        ARRAY_ASSERT(i < _length);
        _items[i] = std::move(t);
        return *this;
      }
      
      template<class S>
      Array<T> &add(const S &t) {
        length(_length + 1);
        _items[_length - 1] = t;
        return *this;
      }
      
      Array<T> &add(T &&t) {
        length(_length + 1);
        _items[_length - 1] = std::move(t);
        return *this;
      }
      
//      template<class S>
//      Array<T> &add(int inslen, const S *insitems) {
//        ARRAY_ASSERT(inslen >= 0);
//        length(_length + inslen);
//        for(int i = 0; i < inslen; ++i)
//          _items[_length - inslen + i] = insitems[i];
//        return *this;
//      }
      
      template<class S>
      Array<T> &add_all(ArrayView<S> ins) {
        length(_length + ins.length());
        for(int i = 0; i < ins.length(); ++i)
          _items[_length - ins.length() + i] = ins[i];
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
      
      template<class S>
      Array<T> &insert(int start, int inslen, const S *insitems) {
        using std::swap;
        
        ARRAY_ASSERT(start >= 0);
        ARRAY_ASSERT(start <= _length);
        ARRAY_ASSERT(inslen >= 0);
        if(inslen == 0)
          return *this;
          
        length(_length + inslen);
        
        for(int i = _length - 1; i >= start + inslen; --i)
          swap(_items[i], _items[i - inslen]);
          
        for(int i = 0; i < inslen; ++i)
          _items[start + i] = insitems[i];
          
        return *this;
      }
      
      template<class S>
      Array<T> &insert(int start, ArrayView<S> ins) {
        return insert(start, ins.length(), ins.items());
      }
      
      template<class S>
      Array<T> &insert_swap(int start, int inslen, S *insitems) {
        using std::swap;
        
        ARRAY_ASSERT(start >= 0);
        ARRAY_ASSERT(start <= _length);
        ARRAY_ASSERT(inslen >= 0);
        if(inslen == 0)
          return *this;
          
        length(_length + inslen);
        
        for(int i = _length - 1; i >= start + inslen; --i)
          swap(_items[i], _items[i - inslen]);
          
        for(int i = 0; i < inslen; ++i)
          swap(_items[start + i], insitems[i]);
          
        return *this;
      }
      
      Array<T> &remove(int start, int remlen) {
        using std::swap;
        
        ARRAY_ASSERT(start >= 0);
        ARRAY_ASSERT(remlen >= 0);
        ARRAY_ASSERT(start + remlen <= _length);
        
        for(int i = start + remlen; i < _length; ++i)
          swap(_items[i - remlen], _items[i]);
          
        return length(_length - remlen);
      }
      
      Array<T> &remove(const Range &range) {
        return remove(range.start, range.end -  range.start + 1);
      }
      
      T *items() {             return _items; }
      const T *items() const { return _items; }
      
      static int best_capacity(int min) {
        if(min <= 0)
          return 0;
        int result = 1;
        while(result < min)
          result *= 2;
          
        return result;
      }
      
      friend void swap(Array &left, Array &right) {
        using std::swap;
        
        swap(left._length,   right._length);
        swap(left._capacity, right._capacity);
        swap(left._items,    right._items);
      }
    private:
      int  _length;
      int  _capacity;
      T   *_items;
  };
}

#endif // RICHMATH__UTIL__ARRAY_H__INCLUDED
