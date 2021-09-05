#ifndef RICHMATH__UTIL__ARRAY_H__INCLUDED
#define RICHMATH__UTIL__ARRAY_H__INCLUDED

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
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
    
    public:
      explicit ArrayView(int length, T *items) : _items(items), _length(length) { ARRAY_ASSERT(length >= 0); }
      
      template<int N>
      ArrayView(T items[N]) : _items(items), _length(N) {}
      
      int length() { return _length; }
      
      iterator begin() { return iterator(_items); }
      iterator end() {   return iterator(_items + _length); }
      
      T &get(int i) { 
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
      
      T &operator[](int i) { return get(i); }
      
      T *items() { return _items; }
      
      ArrayView<T> part(int start) {
        ARRAY_ASSERT(start >= 0);
        ARRAY_ASSERT(start < _length);
        return ArrayView<T>(_length - start, _items + start);
      }
      
    private:
      T *_items;
      int _length;
  };
  
  template<typename T> class Array {
    private:
      struct DataHeader {
        int capacity;
        int length;
        T items[1];
      };
    public:
      using self_type = Array<T>;
      using value_type = T;
      using iterator = value_type*;
      using const_iterator = const value_type*;
    public:
      Array(ArrayView<const T> src)
        : _items(nullptr)
      {
        if(src.length() > 0)
          length(src.length());
          
        const T *src_items = src.items();
        for(int i = 0; i < src.length(); ++i)
          _items[i] = src_items[i];
      }
      
      Array(const Array &src)
        : _items{nullptr}
      {
        *this = ArrayView<const T>(src);
      }
      
      Array(Array &&src)
        : _items{nullptr}
      {
        swap(*this, src);
      }
      
      explicit Array(int length = 0)
        : _items(nullptr)
      {
        ARRAY_ASSERT(length >= 0);
        if(length > 0) {
          this->length(length);
        }
      }
      
      Array(int length, const T *src)
        : _items(nullptr)
      {
        ARRAY_ASSERT(length >= 0);
        if(length > 0) {
          this->length(length);
          for(int i = 0; i < length; ++i)
            _items[i] = src[i];
        }
      }
      
      ~Array() {
        if(_items)
          deallocate(header());
      }
      
      Array &operator=(Array other) {
        swap(*this, other);
        return *this;
      }
      
      Array &operator=(ArrayView<const T> other) {
        const T *other_items = other.items();
        if((uintptr_t)_items < (uintptr_t)&other_items[other.length()] && (uintptr_t)other_items < (uintptr_t)&_items[length()]) {
          *this = Array(other);
          return *this;
        }
        
        length(other.length());
        for(int i = 0; i < other.length(); ++i)
          _items[i] = other_items[i];
        return *this;
      }
      
      operator ArrayView<T>() {             return ArrayView<T>(      length(), _items); }
      operator ArrayView<const T>() const { return ArrayView<const T>(length(), _items); }
      
      int length()   const { return _items ? header()->length : 0;   }
      int capacity() const { return _items ? header()->capacity : 0; }
      
      iterator begin() { return iterator(_items); }
      iterator end() {   return iterator(_items + length()); }
      const_iterator begin() const { return const_iterator(_items); }
      const_iterator end() const {   return const_iterator(_items + length()); }
      
      Array<T> &length(int newlen) {
        using std::swap;
        
        ARRAY_ASSERT(newlen >= 0);
        if(newlen > capacity()) {
          DataHeader *new_header = allocate(best_capacity(newlen));
          ARRAY_ASSERT(new_header != nullptr);
          
          new_header->length = newlen;
          int oldlen = length();
          for(int i = 0; i < oldlen; ++i)
            swap(new_header->items[i], _items[i]);
          
          deallocate(header());
          _items = &new_header->items[0];
          
          ARRAY_ASSERT(header() == new_header);
          ARRAY_ASSERT(length() == newlen);
        }
        else if(auto head = header()) {
          head->length = newlen;
        }
        else {
          ARRAY_ASSERT(newlen == 0);
        }
        return *this;
      }
      
      Array<T> &length(int newlen, const T &default_value) {
        int oldlen = length();
        length(newlen);
        for(int i = oldlen; i < length(); ++i)
          _items[i] = default_value;
        return *this;
      }
      
      Array<T> &zeromem() {
        memset(_items, 0, length() * sizeof(T));
        return *this;
      }
      
      T &operator[](int i) const { return get(i); }
      
//      const T &operator[](int i) const { return get(i); }

      ArrayView<const T> operator[](const Range &range) const {
        ARRAY_ASSERT(range.start >= 0);
        ARRAY_ASSERT(range.end < length());
        ARRAY_ASSERT(range.end - range.start + 1 >= 0);
        return ArrayView<const T>(range.end - range.start + 1, _items + range.start);
      }
      
      ArrayView<T> operator[](const Range &range) {
        ARRAY_ASSERT(range.start >= 0);
        ARRAY_ASSERT(range.end < length());
        ARRAY_ASSERT(range.end - range.start + 1 >= 0);
        return ArrayView<T>(range.end - range.start + 1, _items + range.start);
      }
      
//      const T &get(int i) const {
//        ARRAY_ASSERT(i >= 0);
//        ARRAY_ASSERT(i < length());
//        return _items[i];
//      }

      T &get(int i) const {
        ARRAY_ASSERT(i >= 0);
        ARRAY_ASSERT(i < length());
        return _items[i];
      }
      
      template<class S>
      Array<T> &set(int i, const S &t) {
        ARRAY_ASSERT(i >= 0);
        ARRAY_ASSERT(i < length());
        _items[i] = t;
        return *this;
      }
      
      Array<T> &set(int i, T &&t) {
        ARRAY_ASSERT(i >= 0);
        ARRAY_ASSERT(i < length());
        _items[i] = std::move(t);
        return *this;
      }
      
      template<class S>
      Array<T> &add(const S &t) {
        length(length() + 1);
        _items[length() - 1] = t;
        return *this;
      }
      
      Array<T> &add(T &&t) {
        length(length() + 1);
        _items[length() - 1] = std::move(t);
        return *this;
      }
      
      template<class S>
      Array<T> &add_all(ArrayView<S> ins) {
        length(length() + ins.length());
        for(int i = 0; i < ins.length(); ++i)
          _items[length() - ins.length() + i] = ins[i];
        return *this;
      }
      
      Array<T> &insert(int start, int inslen) {
        ARRAY_ASSERT(start >= 0);
        ARRAY_ASSERT(start <= length());
        ARRAY_ASSERT(inslen >= 0);
        if(inslen == 0)
          return *this;
          
        length(length() + inslen);
        
        for(int i = length() - 1; i >= start + inslen; --i)
          _items[i] = _items[i - inslen];
          
        return *this;
      }
      
      template<class S>
      Array<T> &insert(int start, int inslen, const S *insitems) {
        using std::swap;
        
        ARRAY_ASSERT(start >= 0);
        ARRAY_ASSERT(start <= length());
        ARRAY_ASSERT(inslen >= 0);
        if(inslen == 0)
          return *this;
          
        length(length() + inslen);
        
        for(int i = length() - 1; i >= start + inslen; --i)
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
        ARRAY_ASSERT(start <= length());
        ARRAY_ASSERT(inslen >= 0);
        if(inslen == 0)
          return *this;
          
        length(length() + inslen);
        
        for(int i = length() - 1; i >= start + inslen; --i)
          swap(_items[i], _items[i - inslen]);
          
        for(int i = 0; i < inslen; ++i)
          swap(_items[start + i], insitems[i]);
          
        return *this;
      }
      
      Array<T> &remove(int start, int remlen) {
        using std::swap;
        
        ARRAY_ASSERT(start >= 0);
        ARRAY_ASSERT(remlen >= 0);
        ARRAY_ASSERT(start + remlen <= length());
        
        for(int i = start + remlen; i < length(); ++i)
          swap(_items[i - remlen], _items[i]);
          
        return length(length() - remlen);
      }
      
      Array<T> &remove(const Range &range) {
        return remove(range.start, range.end -  range.start + 1);
      }
      
      T *items() {             return _items; }
      const T *items() const { return _items; }
      
      static int best_capacity(int min) {
        if(min <= 0)
          return 0;
        
        int header = offsetof(DataHeader, items) / sizeof(T);
        
        int result = 1;
        while(result < header + min)
          result *= 2;
          
        return result - header;
      }
      
      friend void swap(Array &left, Array &right) {
        using std::swap;
        
        swap(left._items, right._items);
      }
    
    private:
      DataHeader *header() const { return _items ? (DataHeader*)((uintptr_t)(void*)_items - offsetof(DataHeader, items)) : nullptr; }
      
      static DataHeader *allocate(int capacity) {
        ARRAY_ASSERT(capacity > 0);
        if(auto header = (DataHeader*)malloc(offsetof(DataHeader, items) + sizeof(T) * capacity)) {
          header->capacity = capacity;
          header->length = capacity;
          for(int i = 0; i < capacity; ++i)
            new(&header->items[i]) T;
          
          return header;
        }
        return nullptr;
      }
      
      static void deallocate(DataHeader *header) {
        if(!header)
          return;
        
        for(int i = 0; i < header->capacity; ++i) {
          // cope for non-class T
          struct Wrapper { T elem; };
          ((Wrapper*)(&header->items[i]))->~Wrapper();
        }
        
        free(header);
      }
      
    private:
      T   *_items;
  };
}

#endif // RICHMATH__UTIL__ARRAY_H__INCLUDED
