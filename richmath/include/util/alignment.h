#ifndef RICHMATH__UTIL__ALIGNMENT_H__INCLUDED
#define RICHMATH__UTIL__ALIGNMENT_H__INCLUDED


#include <util/pmath-extra.h>


namespace richmath {
  class SimpleAlignment {
    class Impl;
  public:
    float horizontal; // 0 = left .. 1 = right
    float vertical;   // 0 = bottom .. 1 = top
    
    static const SimpleAlignment TopLeft;
    static const SimpleAlignment Center;
    
    float interpolate_left_to_right(float left, float right) { return left + horizontal * (right - left); }
    float interpolate_bottom_to_top(float bottom, float top) { return bottom + vertical * (top - bottom); }
  
    static SimpleAlignment from_pmath(Expr expr, SimpleAlignment fallback);
  };
  
}

#endif // RICHMATH__UTIL__ALIGNMENT_H__INCLUDED