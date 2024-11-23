#ifndef RICHMATH__UTIL__ALIGNMENT_H__INCLUDED
#define RICHMATH__UTIL__ALIGNMENT_H__INCLUDED


#include <util/pmath-extra.h>


namespace richmath {
  class SimpleAlignment {
    class Impl;
  public:
    float horizontal; // 0 = left .. 1 = right
    float vertical;   // 0 = bottom .. 1 = top
  
    static SimpleAlignment from_pmath(Expr expr);
  };
  
}

#endif // RICHMATH__UTIL__ALIGNMENT_H__INCLUDED
