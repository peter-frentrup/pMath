#ifndef __UTIL__BASE_H__
#define __UTIL__BASE_H__

#include <pmath-util/concurrency/atomic.h>
#include <stdint.h>

namespace richmath{
  void assert_failed();
  
  static inline int boole(bool b){ return b ? 1 : 0; }
  
  extern const double Infinity;

  class Base{
    public:
      Base();
      ~Base();
      
    private:
      Base(const Base &src){}
      Base const &operator=(Base const &src){ return *this; }
  };
  
  class Shareable: public Base{
    public:
      Shareable();
      virtual ~Shareable();
      
      void ref() const;
      void unref() const;
      
      intptr_t refcount() const { return pmath_atomic_read_aquire(&_refcount); };
    private:
      mutable pmath_atomic_t _refcount;
  };
}

#endif // __UTIL__BASE_H__
