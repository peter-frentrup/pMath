#include <util/alignment.h>


namespace richmath {
  class SimpleAlignment::Impl {
    public:
      static float decode_horizontal(Expr horizontal_alignment, float fallback);
      static float decode_vertical(Expr vertical_alignment, float fallback);
  };      
}

using namespace richmath;

extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_Bottom;
extern pmath_symbol_t richmath_System_Center;
extern pmath_symbol_t richmath_System_Left;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Right;
extern pmath_symbol_t richmath_System_Top;

//{ class SimpleAlignment ...

const SimpleAlignment SimpleAlignment::TopLeft { 0.0f, 1.0f };
const SimpleAlignment SimpleAlignment::Center {  0.5f, 0.5f };

SimpleAlignment SimpleAlignment::from_pmath(Expr expr, SimpleAlignment fallback) {
  if(expr.is_symbol()) {
    return {
      Impl::decode_horizontal(expr, fallback.horizontal), 
      Impl::decode_vertical(  expr, fallback.vertical)};
  }
  else if(expr.is_number()) {
    return { Impl::decode_horizontal(expr, fallback.horizontal), fallback.vertical};
  }
  else if(expr.expr_length() == 2 && expr.item_equals(0, richmath_System_List)) {
    return {
      Impl::decode_horizontal(expr[1], fallback.horizontal),
      Impl::decode_vertical(  expr[2], fallback.vertical)};
  }
  return fallback;
}

//} ... class SimpleAlignment

//{ class SimpleAlignment::Impl ...

float SimpleAlignment::Impl::decode_horizontal(Expr horizontal_alignment, float fallback) {
  if(horizontal_alignment.is_symbol()) {
    if(horizontal_alignment == richmath_System_Automatic) return 0;
    if(horizontal_alignment == richmath_System_Left)      return 0;
    if(horizontal_alignment == richmath_System_Right)     return 1;
    if(horizontal_alignment == richmath_System_Center)    return 0.5f;
  }
  else if(horizontal_alignment.is_number()) {
    double val = horizontal_alignment.to_double();
    if(-1.0 <= val && val <= 1.0)
      return (float)((val - (-1.0)) / 2);
    
    if(val < -1.0)
      return 0;
    if(val > 1.0)
      return 1;
  }
  
  return fallback;
}

float SimpleAlignment::Impl::decode_vertical(Expr vertical_alignment, float fallback) {
  if(vertical_alignment.is_symbol()) {
    if(vertical_alignment == richmath_System_Automatic) return 1;
    if(vertical_alignment == richmath_System_Center)    return 0.5f;
    if(vertical_alignment == richmath_System_Top)       return 1;
    if(vertical_alignment == richmath_System_Bottom)    return 0;
  }
  else if(vertical_alignment.is_number()) {
    double val = vertical_alignment.to_double();
    if(-1.0 <= val && val <= 1.0)
      return (float)((val - (-1.0)) / 2);
    
    if(val < -1.0)
      return 0;
    if(val > 1.0)
      return 1;
  }
  
  return fallback;
}

//} ... class SimpleAlignment::Impl
