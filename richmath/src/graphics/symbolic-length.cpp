#include <graphics/symbolic-length.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_Large;
extern pmath_symbol_t richmath_System_Medium;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_Small;
extern pmath_symbol_t richmath_System_Tiny;

//                                                                    Automatic  Tiny   Small  Medium  Large
const LengthConversionFactors LengthConversionFactors::Zero {             0,       0,     0,      0,     0 };
const LengthConversionFactors LengthConversionFactors::FontSizeInPt {   12.0f,     6,     9,     12,    24 };
const LengthConversionFactors LengthConversionFactors::SectionMargins {  0.0f,   0.125,   0.25,   0.5,   1 };
const LengthConversionFactors LengthConversionFactors::GraphicsSize {    0,        5,    10,     20,    40 };


//{ class Length ...

float Length::resolve(float em, const LengthConversionFactors &factors) const {
  if(is_explicit())
    return raw_value();
  
  if(is_symbolic()) {
    switch(symblic_value()) {
      case SymbolicSize::Automatic: return em * factors.Automatic;
      case SymbolicSize::Large:     return em * factors.Large;
      case SymbolicSize::Medium:    return em * factors.Medium;
      case SymbolicSize::Small:     return em * factors.Small;
      case SymbolicSize::Tiny:      return em * factors.Tiny;
      
      case SymbolicSize::Invalid: break;
    }
  }
  
  return 0.0f;
}

Length Length::from_pmath(Expr obj) {
  if(obj == richmath_System_Automatic) return SymbolicSize::Automatic;
  if(obj == richmath_System_Large)     return SymbolicSize::Large;
  if(obj == richmath_System_Medium)    return SymbolicSize::Medium;
  if(obj == richmath_System_Small)     return SymbolicSize::Small;
  if(obj == richmath_System_Tiny)      return SymbolicSize::Tiny;
  
  if(obj.is_number())
    return Length(obj.to_double());
  
  return SymbolicSize::Invalid;
}

Expr Length::to_pmath() const {
  if(is_explicit())
    return Number(raw_value());
  
  switch(symblic_value()) {
    case SymbolicSize::Automatic: return Symbol(richmath_System_Automatic);
    case SymbolicSize::Large:     return Symbol(richmath_System_Large);
    case SymbolicSize::Medium:    return Symbol(richmath_System_Medium);
    case SymbolicSize::Small:     return Symbol(richmath_System_Small);
    case SymbolicSize::Tiny:      return Symbol(richmath_System_Tiny);
    
    case SymbolicSize::Invalid: break;
  }
  
  return Symbol(richmath_System_None);
}

//} ... class Length
