#ifndef RICHMATH__EVAL__INTERPOLATION_H__INCLUDED
#define RICHMATH__EVAL__INTERPOLATION_H__INCLUDED

#include <util/array.h>

namespace richmath {
  class Interpolation {
    public:
      static float interpolation_index(const Array<float> &values, float val, bool clip);
      //static float interpolation_value(const Array<float> &values, float index);
  };
}

#endif // RICHMATH__EVAL__INTERPOLATION_H__INCLUDED
