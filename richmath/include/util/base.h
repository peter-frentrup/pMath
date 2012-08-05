#ifndef __UTIL__BASE_H__
#define __UTIL__BASE_H__

#include <pmath-util/concurrency/atomic.h>
#include <stdint.h>


namespace richmath {
  void assert_failed();
  
  static inline int boole(bool b) { return b ? 1 : 0; }
  
  extern const double Infinity;
  
  class Base {
    public:
      Base();
      ~Base();
      
    private:
      Base(const Base &src) {}
      Base const &operator=(Base const &src) { return *this; }
  };
  
  class Shareable: public Base {
    public:
      Shareable();
      virtual ~Shareable();
      
      void ref() const;
      void unref() const;
      
      intptr_t refcount() const { return pmath_atomic_read_aquire(&_refcount); };
    private:
      mutable pmath_atomic_t _refcount;
  };
  
  template <
  typename T,
           T *(*ref_func)(T *),
           void (*unref_func)(T *)
           >
  class AutoRefBase {
    public:
      AutoRefBase(T *ptr = 0)
        : _ptr(ptr)
      { }
      
      AutoRefBase(const AutoRefBase<T, ref_func, unref_func> &src)
        : _ptr(src._ptr ? ref_func(src._ptr) : 0)
      { }
      
      ~AutoRefBase() {
        if(_ptr)
          unref_func(_ptr);
      }
      
      AutoRefBase &operator=(const AutoRefBase<T, ref_func, unref_func> &src) {
        T *old = _ptr;
        
        _ptr = src._ptr ? ref_func(src._ptr) : 0;
        
        if(old)
          unref_func(old);
        
        return *this;
      }
      
      T *ptr() { return _ptr; }
      
    private:
      T *_ptr;
  };
}

#endif // __UTIL__BASE_H__
