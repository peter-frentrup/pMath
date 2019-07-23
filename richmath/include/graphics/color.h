#ifndef RICHMATH__GRAPHICS__COLOR_H__INCLUDED
#define RICHMATH__GRAPHICS__COLOR_H__INCLUDED


#include <util/pmath-extra.h>


namespace richmath {
  class Color final {
    public:
      Color() : _value(-1) {}
      
      static Color decode(int value) { return Color{value}; }
      int encode() const { return _value; }
      
      static Color from_rgb(double red, double green, double blue);
      
      static Color from_rgb24(int rgb24) { 
        return from_rgb24((rgb24 & 0xFF0000) >> 16, (rgb24 & 0xFF00) >> 8, rgb24 & 0xFF) ;
      }
      static Color from_rgb24(int red, int green, int blue) { 
        return decode(((red & 0xFF) << 16) | ((green & 0xFF) << 8) | (blue & 0xFF));
      }
      int to_rgb24() const {
        if(!is_valid())
          return 0;
        
        return _value & 0xFFFFFF;
      }
      static Color from_bgr24(int bgr24) { 
        return from_rgb24(bgr24 & 0xFF, (bgr24 & 0xFF00) >> 8, (bgr24 & 0xFF0000) >> 16) ;
      }
      
      static Color from_pmath(Expr pmath);
      Expr to_pmath() const;
      
      bool is_valid() const { return _value >= 0; }
      
      double red() const { 
        if(!is_valid())
          return 0.0;
        
        return ((_value & 0xFF0000) >> 16) / 255.0;
      }
      double green() const { 
        if(!is_valid())
          return 0.0;
        
        return ((_value & 0xFF00) >> 8) / 255.0;
      }
      double blue() const { 
        if(!is_valid())
          return 0.0;
        
        return (_value & 0xFF) / 255.0;
      }
      
      friend void swap(Color &left, Color &right) {
        using std::swap;
        swap(left._value, right._value);
      }
      
      friend bool operator==(Color left, Color right) {
        return left._value == right._value;
      }
      friend bool operator!=(Color left, Color right) {
        return left._value != right._value;
      }
      
      static const Color None;
      static const Color Black;
      static const Color White;
    
    private:
      Color(int value) : _value(value) {}
      
    private:
      int _value;
  };
}

#endif // RICHMATH__GRAPHICS__COLOR_H__INCLUDED
