#ifndef __UTIL__BASE_H__
#define __UTIL__BASE_H__

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
      
      intptr_t refcount() const { return _refcount; };
    private:
      mutable volatile intptr_t _refcount;
  };
}

#endif // __UTIL__BASE_H__
