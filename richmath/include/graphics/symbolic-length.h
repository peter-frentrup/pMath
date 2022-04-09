#ifndef RICHMATH__GRAPHICS__SYMBOLIC_SIZE_H__INCLUDED
#define RICHMATH__GRAPHICS__SYMBOLIC_SIZE_H__INCLUDED


#include <util/pmath-extra.h>

namespace richmath {
  enum class SymbolicSize {
    Automatic = -1,
    //Tiny = -4,
    //Small = -5,
    //Medium = -6,
    //Large = -7,
    
    Invalid = -100,
  };
  
  struct LengthConversionFactors {
    static const LengthConversionFactors Zero;
    static const LengthConversionFactors FontSizeInPt;
    static const LengthConversionFactors SectionMargins;
    
    float Automatic;
  };

  class Length {
    public:
      Length() : _raw_value{(float)(int)SymbolicSize::Invalid} {}
      
      Length(SymbolicSize sym) : _raw_value{(float)(int)sym} {}
      explicit Length(float raw_value) : _raw_value{raw_value} {}
      
      float raw_value() const { return _raw_value; }
      SymbolicSize symblic_value() const { return (SymbolicSize)(int)_raw_value; }
      
      float resolve(float em, const LengthConversionFactors &factors) const;
      
      bool is_positive() const { return _raw_value > 0.0f; }
      bool is_explicit() const { return _raw_value >= 0.0f; }
      bool is_symbolic() const { return _raw_value < 0.0f; }
      bool is_valid()    const { return _raw_value >= 0.0f || (_raw_value < 0.0f && _raw_value > (int)SymbolicSize::Invalid && _raw_value == (int)_raw_value); }
      
      explicit operator bool() const { return is_valid(); }
      
      friend bool operator==(Length left, Length right) { return left._raw_value == right._raw_value; }
      friend bool operator!=(Length left, Length right) { return left._raw_value != right._raw_value; }
      
      static Length from_pmath(Expr obj);
      Expr to_pmath() const;
      
      explicit operator Expr() const { return to_pmath(); }
      
    private:
      float _raw_value;
  };
}

#endif // RICHMATH__GRAPHICS__SYMBOLIC_SIZE_H__INCLUDED
