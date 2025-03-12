#include <util/alignment.h>

#include <util/style.h> // for get_factor_of_scaled()

#include <math.h>


#ifdef _MSC_VER
#  define isfinite(x)  (_finite(x))
#endif


namespace richmath {
  class SimpleAlignment::Impl {
    public:
      static float decode_horizontal(Expr horizontal_alignment, float fallback);
      static float decode_vertical(Expr vertical_alignment, float fallback);
  };      
}

using namespace richmath;

extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_Axis;
extern pmath_symbol_t richmath_System_Baseline;
extern pmath_symbol_t richmath_System_Bottom;
extern pmath_symbol_t richmath_System_Center;
extern pmath_symbol_t richmath_System_Left;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Right;
extern pmath_symbol_t richmath_System_Scaled;
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

//{ class SimpleBoxBaselinePositioning ...

static const float NormalTextAxisFactor    = 0.25f; // TODO: use actual math axis from font
static const float NormalTextDescentFactor = 0.25f;
static const float NormalTextAscentFactor  = 0.75f;

float SimpleBoxBaselinePositioning::calculate_baseline(float em, Expr baseline_pos) const {
  if(baseline_pos == richmath_System_Automatic) return 0.0f;
  if(baseline_pos == richmath_System_Bottom)    return scaled_baseline(0);
  if(baseline_pos == richmath_System_Top)       return scaled_baseline(1);
  if(baseline_pos == richmath_System_Center)    return scaled_baseline(0.5);
  
  if(baseline_pos == richmath_System_Axis) 
    return NormalTextAxisFactor * em - cy; // TODO: use actual math axis from font
    
  if(baseline_pos == richmath_System_Baseline) 
    return -cy;
  
  if(baseline_pos.item_equals(0, richmath_System_Scaled)) {
    double factor = 0.0;
    if(get_factor_of_scaled(baseline_pos, &factor) && isfinite(factor)) 
      return scaled_baseline(factor);
  }
  else if(baseline_pos.is_rule()) {
    float lhs_y = calculate_baseline(em, baseline_pos[1]);
    Expr rhs = baseline_pos[2];
    
    if(rhs == richmath_System_Axis) {
      float ref_pos = NormalTextAxisFactor * em; // TODO: use actual math axis from font
      return lhs_y - ref_pos;
    }
    else if(rhs == richmath_System_Baseline) {
      float ref_pos = cy;
      return lhs_y - ref_pos;
    }
    else if(rhs == richmath_System_Bottom) {
      float ref_pos = - NormalTextDescentFactor * em;
      return lhs_y - ref_pos;
    }
    else if(rhs == richmath_System_Center) {
      float ref_pos = (NormalTextAscentFactor - NormalTextDescentFactor) * em; // (bottom + top)/2
      return lhs_y - ref_pos;
    }
    else if(rhs == richmath_System_Top) {
      float ref_pos = NormalTextAscentFactor * em;
      return lhs_y - ref_pos;
    }
    else if(rhs.item_equals(0, richmath_System_Scaled)) {
      double factor = 0.0;
      if(get_factor_of_scaled(rhs, &factor) && isfinite(factor)) {
        //float ref_pos = NormalTextAscentFactor * em * factor - NormalTextDescentFactor * em * (1 - factor);
        float ref_pos = ((NormalTextAscentFactor + NormalTextDescentFactor) * factor - NormalTextDescentFactor) * em;
        return lhs_y - ref_pos;
      }
    }
  }
  
  return 0; // as if baseline_pos == richmath_System_Automatic
}

//} ... class SimpleBoxBaselinePositioning

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
