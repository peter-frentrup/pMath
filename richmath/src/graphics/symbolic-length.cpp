#include <graphics/symbolic-length.h>

#include <util/style.h> // for convert_float_to_nice_double()


using namespace richmath;

extern pmath_symbol_t richmath_System_All;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_Large;
extern pmath_symbol_t richmath_System_Medium;
extern pmath_symbol_t richmath_System_NCache;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_Scaled;
extern pmath_symbol_t richmath_System_Small;
extern pmath_symbol_t richmath_System_Tiny;

//                                                                                            Automatic  Tiny   Small  Medium  Large
const LengthConversionFactors LengthConversionFactors::Zero {                                    0,        0,     0,      0,     0 };
const LengthConversionFactors LengthConversionFactors::FontSizeInPt {                           12.0f,     6,     9,     12,    24 };
const LengthConversionFactors LengthConversionFactors::SectionMargins {                          0.0f,   0.125,   0.25,   0.5,   1 };
const LengthConversionFactors LengthConversionFactors::GraphicsSize {                            0,        8,    14,     22,    40 };
const LengthConversionFactors LengthConversionFactors::NormalDashingInPt {                      3.0,      1.5,   3.0,    6.0,  12.0 };
const LengthConversionFactors LengthConversionFactors::PointSizeInPt {                          2.5,       1,    1.75,   3.0,  5.25 };
const LengthConversionFactors LengthConversionFactors::ProgressIndicatorLengthScale {            1,       0.5,   0.75,    1,    2 };
const LengthConversionFactors LengthConversionFactors::ProgressIndicatorThicknessScale   {       1,       0.5,   0.75,    1,    1.25 };
const LengthConversionFactors LengthConversionFactors::SimpleDashingOnInPt {                    1.75,     0.75,  1.75,   3.75,  7.5 };
const LengthConversionFactors LengthConversionFactors::SimpleDashingOffInPt {                   3.5,      2.25,  3.5,    5.25,  9.75 };
const LengthConversionFactors LengthConversionFactors::SliderLengthScale {                       1,       0.5,   0.75,    1,    2 };
const LengthConversionFactors LengthConversionFactors::SliderThicknessScale   {                  1,       0.5,   0.75,    1,    1.25 };
const LengthConversionFactors LengthConversionFactors::ThicknessInPt {                          1.0,      0.5,   0.75,   1.5,   3.0 };
const LengthConversionFactors LengthConversionFactors::ToggleSwitchLengthScale {                 1,       0.67,  0.8,     1,    1.25 };
const LengthConversionFactors LengthConversionFactors::ToggleSwitchThicknessScale {              1,       0.67,  0.8,     1,    1.25 };
const LengthConversionFactors LengthConversionFactors::PlotRangePadding {                       0.04,     0.01,  0.02,   0.04,  0.08 };
const LengthConversionFactors LengthConversionFactors::ControlWidth {                            0,        4,    6,       8,     12 };
const LengthConversionFactors LengthConversionFactors::ControlHeight {                           0,        1,    1.2,    1.5,     2 };

static double convert_float_to_nice_double(float f);

//{ class Length ...

Length Length::resolve_scaled(float rel_scale) const {
  if(is_explicit_abs())
    return *this;
  
  if(is_explicit_rel() && rel_scale >= 0)
    return Length::Absolute(rel_scale * explicit_rel_value());
  
  return SymbolicSize::Automatic;
}

float Length::resolve(float em, const LengthConversionFactors &factors, float rel_scale) const {
  if(is_explicit_abs())
    return explicit_abs_value();
  
  if(is_explicit_rel()) {
    if(rel_scale >= 0) {
      auto abs = Length::Absolute(explicit_rel_value() * rel_scale);
      if(abs.is_explicit_abs())
        return abs.explicit_abs_value();
    }
    return em * factors.Automatic;
  }
  
  if(is_symbolic()) {
    switch(symblic_value()) {
      case SymbolicSize::All:
      case SymbolicSize::Automatic: return em * factors.Automatic;
      case SymbolicSize::Large:     return em * factors.Large;
      case SymbolicSize::Medium:    return em * factors.Medium;
      case SymbolicSize::Small:     return em * factors.Small;
      case SymbolicSize::Tiny:      return em * factors.Tiny;
      case SymbolicSize::None:      return 0.0f;
      
      case SymbolicSize::Invalid: break;
    }
  }
  
  return 0.0f;
}

Length Length::from_pmath(Expr obj) {
  if(obj == richmath_System_All)       return SymbolicSize::All;
  if(obj == richmath_System_Automatic) return SymbolicSize::Automatic;
  if(obj == richmath_System_None)      return SymbolicSize::None;
  if(obj == richmath_System_Large)     return SymbolicSize::Large;
  if(obj == richmath_System_Medium)    return SymbolicSize::Medium;
  if(obj == richmath_System_Small)     return SymbolicSize::Small;
  if(obj == richmath_System_Tiny)      return SymbolicSize::Tiny;
  
  if(obj.item_equals(0, richmath_System_NCache))
    obj = obj[2];
  
  if(obj.is_number())
    return Length::Absolute(obj.to_double());
  
  if(obj.item_equals(0, richmath_System_Scaled) && obj.expr_length() == 1) {
    Expr scale = obj[1];
      
    if(scale.item_equals(0, richmath_System_NCache))
      scale = scale[2];
    
    if(scale.is_number())
      return Length::Relative(scale.to_double());
  }
  
  return SymbolicSize::Invalid;
}

Expr Length::to_pmath() const {
  if(is_explicit_abs())
    return Number(convert_float_to_nice_double(explicit_abs_value()));
  
  if(is_explicit_rel())
    return Call(Symbol(richmath_System_Scaled), Number(convert_float_to_nice_double(explicit_rel_value())));
  
  switch(symblic_value()) {
    case SymbolicSize::All:       return Symbol(richmath_System_All);
    case SymbolicSize::Automatic: return Symbol(richmath_System_Automatic);
    case SymbolicSize::None:      return Symbol(richmath_System_None);
    case SymbolicSize::Large:     return Symbol(richmath_System_Large);
    case SymbolicSize::Medium:    return Symbol(richmath_System_Medium);
    case SymbolicSize::Small:     return Symbol(richmath_System_Small);
    case SymbolicSize::Tiny:      return Symbol(richmath_System_Tiny);
    
    case SymbolicSize::Invalid: break;
  }
  
  return Symbol(richmath_System_None);
}

//} ... class Length
