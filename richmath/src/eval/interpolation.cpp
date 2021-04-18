#include <eval/interpolation.h>

#include <algorithm>
#include <cmath>
#include <limits>


#ifdef _MSC_VER
namespace std {
  static bool isnan(double d) {return _isnan(d);}
}
#endif

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif


using namespace richmath;

//{ class Interpolation ...


float Interpolation::interpolation_index(const Array<float> &values, float val, bool clip) {
  float prev = NAN;
  for(int i = 0; i < values.length(); ++i) {
    float cur = values[i];
    if(val == cur)
      return i;
    
    float rel = (cur - val) / (cur - prev);
    if(0 <= rel && rel <= 1) 
      return i - rel;
    
    prev = cur;
  }
  
  if(clip) {
    if(values.length() >= 2) {
      if(values[0] < values[1]) {
        if(val < values[0])
          return 0;
        else
          return values.length() - 1;
      }
      else {
        if(val > values[0])
          return values.length() - 1;
        else
          return 0;
      }
    }
  }
  return NAN;
}

//} ... class Interpolation
