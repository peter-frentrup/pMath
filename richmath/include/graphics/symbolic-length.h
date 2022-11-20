#ifndef RICHMATH__GRAPHICS__SYMBOLIC_SIZE_H__INCLUDED
#define RICHMATH__GRAPHICS__SYMBOLIC_SIZE_H__INCLUDED


#include <util/pmath-extra.h>

namespace richmath {
  enum class SymbolicSize {
    Invalid = 0,
    
    Automatic,
    None,
    Tiny,
    Small,
    Medium,
    Large,
    
  };
  
  struct LengthConversionFactors {
    static const LengthConversionFactors Zero;
    static const LengthConversionFactors FontSizeInPt;
    static const LengthConversionFactors SectionMargins;
    static const LengthConversionFactors GraphicsSize;
    static const LengthConversionFactors NormalDashingInPt;
    static const LengthConversionFactors PointSizeInPt;
    static const LengthConversionFactors SimpleDashingOnInPt;
    static const LengthConversionFactors SimpleDashingOffInPt;
    static const LengthConversionFactors SliderLengthScale;
    static const LengthConversionFactors SliderThicknessScale;
    static const LengthConversionFactors ThicknessInPt;
    static const LengthConversionFactors ToggleSwitchLengthScale;
    static const LengthConversionFactors ToggleSwitchThicknessScale;
    static const LengthConversionFactors PlotRangePadding;
    
    float Automatic;
    float Tiny;
    float Small;
    float Medium;
    float Large;
  };

  class Length {
    public:
      Length() { _raw_value.as_uint32 = QuietNanMask; }
      
      Length(SymbolicSize sym) { _raw_value.as_uint32 = QuietNanMask | (uint32_t)sym; }
      explicit Length(float raw_value) : _raw_value{raw_value} {}
      
      static Length Absolute(float f) { Length len( f); return len.is_explicit_abs() ? len : Length(); }
      static Length Relative(float f) { Length len(-f); return len.is_explicit_rel() ? len : Length(); }
      
      float raw_value() const { return _raw_value.as_float; }
      float explicit_abs_value() const { return  _raw_value.as_float; }
      float explicit_rel_value() const { return -_raw_value.as_float; }
      SymbolicSize symblic_value() const { return (SymbolicSize)(_raw_value.as_uint32 & QuietNanPayloadMask); }
      
      Length resolve_scaled(float rel_scale) const;
      float resolve(float em, const LengthConversionFactors &factors, float rel_scale) const;
      
      //bool is_abs_rel_nonzero() const { return _raw_value.as_float > 0.0f || as_float.as_float < 0.0f; }
      bool is_explicit_abs_positive() const { return is_explicit_abs() && explicit_abs_value() > 0; }
      bool is_explicit_abs() const { return (_raw_value.as_uint32 & ExponentMask) != ExponentMask && (_raw_value.as_uint32 & SignMask) == 0; }
      bool is_explicit_rel() const { return (_raw_value.as_uint32 & ExponentMask) != ExponentMask && (_raw_value.as_uint32 & SignMask) == SignMask; }
      bool is_symbolic() const { return (_raw_value.as_uint32 & ExponentMask) == ExponentMask; } // NaN or Infinity
      bool is_valid()    const { return _raw_value.as_uint32 != QuietNanMask; } // not NaN
      
      explicit operator bool() const { return is_valid(); }
      
      friend bool operator==(Length left, Length right) { return left._raw_value.as_uint32 == right._raw_value.as_uint32; }
      friend bool operator!=(Length left, Length right) { return left._raw_value.as_uint32 != right._raw_value.as_uint32; }
      
      static Length from_pmath(Expr obj);
      Expr to_pmath() const;
      
      explicit operator Expr() const { return to_pmath(); }
      
    private:
      union {
        float    as_float;
        uint32_t as_uint32;
      } _raw_value;
      
      static const uint32_t SignMask            = 0x80000000u; // 10000000_00000000_00000000_00000000
      static const uint32_t ExponentMask        = 0x7F800000u; // 01111111_10000000_00000000_00000000
      static const uint32_t QuietNanMask        = 0x7FC00000u; // 01111111_11000000_00000000_00000000
      static const uint32_t QuietNanPayloadMask = 0x003FFFFFu; // 00000000_00111111_11111111_11111111
  };
}

#endif // RICHMATH__GRAPHICS__SYMBOLIC_SIZE_H__INCLUDED
