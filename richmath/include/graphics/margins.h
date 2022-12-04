#ifndef RICHMATH__GRAPHICS__MARGINS_H__INCLUDED
#define RICHMATH__GRAPHICS__MARGINS_H__INCLUDED

namespace richmath {
  template<typename T>
  struct Margins {
    T left;
    T right;
    T top;
    T bottom;
    
    explicit Margins(T all) : left{all}, right{all}, top{all}, bottom{all} {}
    explicit Margins(T horizontal, T vertical) : left{horizontal}, right{horizontal}, top{vertical}, bottom{vertical} {}
    explicit Margins(T left, T right, T top, T bottom) : left{left}, right{right}, top{top}, bottom{bottom} {}
    
    template<typename U>
    Margins &operator=(const Margins<U> &other) {
      left   = other.left;
      right  = other.right;
      top    = other.top;
      bottom = other.bottom;
      return *this;
    }
    
    Margins &operator+=(T all) {
      left   += all; 
      right  += all;
      top    += all;
      bottom += all;
      return *this;
    }
    Margins &operator-=(T all) {
      left   -= all; 
      right  -= all;
      top    -= all;
      bottom -= all;
      return *this;
    }
    Margins &operator*=(T factor) {
      left   *= factor; 
      right  *= factor;
      top    *= factor;
      bottom *= factor;
      return *this;
    }
    Margins &operator/=(T quotient) {
      left   *= quotient; 
      right  *= quotient;
      top    *= quotient;
      bottom *= quotient;
      return *this;
    }
    Margins &operator+=(const Margins &other) {
      left   += other.left;
      right  += other.right;
      top    += other.top;
      bottom += other.bottom;
      return *this;
    }
    Margins &operator-=(const Margins &other) {
      left   -= other.left;
      right  -= other.right;
      top    -= other.top;
      bottom -= other.bottom;
      return *this;
    }
    
    Margins operator+(const Margins &other) const { Margins result = *this; return result += other; }
    Margins operator-(const Margins &other) const { Margins result = *this; return result -= other; }
    Margins operator-() const { return Margins(-left, -right, -top, -bottom); }
  };
  
  template<typename T, typename U>
  inline bool operator==(const Margins<T> &a, const Margins<T> &b) { return a.left == b.left && a.right == b.right && a.top == b.top && a.bottom == b.bottom; }
  
  template<typename T, typename U>
  inline bool operator!=(const Margins<T> &a, const Margins<T> &b) { return a.left != b.left || a.right != b.right || a.top != b.top || a.bottom != b.bottom; }
  
  template<typename T>
  inline Margins<T> operator*(T factor, Margins<T> m) { return m *= factor; }
  
  template<typename T>
  inline Margins<T> operator*(Margins<T> m, T factor) { return m *= factor; }
  
  template<typename T>
  inline Margins<T> operator/(Margins<T> m, T quotient) { return m /= quotient; }
}

#endif // RICHMATH__GRAPHICS__MARGINS_H__INCLUDED
