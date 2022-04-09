#include <graphics/symbolic-length.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_None;

//{ class Length ...

float Length::resolve(float em, float auto_factor) const {
  if(is_explicit())
    return raw_value();
  
  if(is_symbolic()) {
    switch(symblic_value()) {
      case SymbolicSize::Automatic: return em * auto_factor;
      
      case SymbolicSize::Invalid: break;
    }
  }
  
  return 0.0f;
}

Length Length::from_pmath(Expr obj) {
  if(obj == richmath_System_Automatic)
    return SymbolicSize::Automatic;
  
  if(obj.is_number())
    return Length(obj.to_double());
  
  return SymbolicSize::Invalid;
}

Expr Length::to_pmath() const {
  if(is_explicit())
    return Number(raw_value());
  
  switch(symblic_value()) {
    case SymbolicSize::Automatic: return Symbol(richmath_System_Automatic);
    
    case SymbolicSize::Invalid: break;
  }
  
  return Symbol(richmath_System_None);
}

//} ... class Length
