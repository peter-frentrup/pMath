#include <graphics/color.h>

#include <math.h>


using namespace richmath;

static double round_to_prec(double x, int p);

//{ class Color ...

const Color Color::None = Color::decode(-1);
const Color Color::Black = Color::from_rgb24(0x000000);
const Color Color::White = Color::from_rgb24(0xFFFFFF);

Color Color::from_rgb(double red, double green, double blue) {
  if(!(red   >= 0.0)) red = 0.0;
  if(!(green >= 0.0)) green = 0.0;
  if(!(blue  >= 0.0)) blue = 0.0;
  if(!(red   <= 1.0)) red = 1.0;
  if(!(green <= 1.0)) green = 1.0;
  if(!(blue  <= 1.0)) blue = 1.0;
  
  int red8   = (int)(red   * 255 + 0.5);
  int green8 = (int)(green * 255 + 0.5);
  int blue8  = (int)(blue  * 255 + 0.5);
  
  return Color::from_rgb24(red8, green8, blue8);
}

Color Color::from_pmath(Expr expr) {
  if(expr == PMATH_SYMBOL_NONE)
    return Color::None;
    
  if(expr.is_expr()) {
    if(expr[0] == PMATH_SYMBOL_RGBCOLOR) {
      if( expr.expr_length() == 1 &&
          expr[1][0] == PMATH_SYMBOL_LIST)
      {
        expr = expr[1];
      }
      
      if( expr.expr_length() == 3 &&
          expr[1].is_number() &&
          expr[2].is_number() &&
          expr[3].is_number())
      {
        return Color::from_rgb(expr[1].to_double(), expr[2].to_double(), expr[3].to_double());
      }
    }
    
    if(expr[0] == PMATH_SYMBOL_HUE) {
      if( expr.expr_length() == 1 &&
          expr[1][0] == PMATH_SYMBOL_LIST)
      {
        expr = expr[1];
      }
      
      if(expr.expr_length() >= 1 && expr.expr_length() <= 3) {
        for(auto item : expr.items())
          if(!item.is_number())
            return -1;
            
        double h, s = 1, v = 1;
        
        h = expr[1].to_double();
        h = fmod(h, 1.0);
        if(h < 0)
          h += 1.0;
          
        if(!(h >= 0 && h <= 1))
          return Color::None;
          
        if(expr.expr_length() >= 2) {
          s = expr[2].to_double();
          if(s < 0) s = 0;
          else if(!(s <= 1)) s = 1;
          
          if(expr.expr_length() >= 3) {
            v = expr[3].to_double();
            v = fmod(v, 1.0);
            if(v < 0) v = 0;
            else if(!(v <= 1)) v = 1;
          }
        }
        
        h *= 360;
        int hi = (int)(h / 60);
        double f = h / 60 - hi;
        double p = v * (1 - s);
        double q = v * (1 - s * f);
        double t = v * (1 - s * (1 - f));
        
        switch(hi) {
          case 0:
          case 6: return Color::from_rgb(v, t, p);
          case 1: return Color::from_rgb(q, v, p);
          case 2: return Color::from_rgb(p, v, t);
          case 3: return Color::from_rgb(p, q, v);
          case 4: return Color::from_rgb(t, p, v);
          case 5: return Color::from_rgb(v, p, q);
        }
      }
    }
    
    if( expr[0] == PMATH_SYMBOL_GRAYLEVEL &&
        expr.expr_length() == 1 &&
        expr[1].is_number())
    {
      double l = expr[1].to_double();
      if(l < 0) l = 0;
      else if(!(l <= 1)) l = 1;
      
      return Color::from_rgb(l, l, l);
    }
  }
  
  return Color::decode(-2);
}

Expr Color::to_pmath() const {
  if(!is_valid())
    return Symbol(PMATH_SYMBOL_NONE);
    
  int r = (_value & 0xFF0000) >> 16;
  int g = (_value & 0x00FF00) >>  8;
  int b =  _value & 0x0000FF;
  
  if(r == g && r == b) {
    return Call(
             Symbol(PMATH_SYMBOL_GRAYLEVEL),
             Number(round_to_prec(r / 255.0, 255)));
  }
  
  return Call(
           Symbol(PMATH_SYMBOL_RGBCOLOR),
           Number(round_to_prec(r / 255.0, 255)),
           Number(round_to_prec(g / 255.0, 255)),
           Number(round_to_prec(b / 255.0, 255)));
}

Color Color::blend(Color a, Color b, float scale) {
  if(scale <= 0) return a;
  if(scale >= 1) return b;
  
  if(!a) return b;
  if(!b) return a;
  
  auto a_red   = a.red();
  auto a_green = a.green();
  auto a_blue  = a.blue();
  auto b_red   = b.red();
  auto b_green = b.green();
  auto b_blue  = b.blue();
  
  return Color::from_rgb(
           a_red   + (b_red   - a_red) * scale,
           a_green + (b_green - a_green) * scale,
           a_blue  + (b_blue  - a_blue) * scale);
}

//} ... class Color

static double round_factor(double x, double f) {
  x = floor(x * f + 0.5);
  x = x / f;
  return x;
}

static double round_to_prec(double x, int p) {
  double y = 0.0;
  double f = 1.0;
  
  for(int dmax = 10; dmax > 0; --dmax) {
    y = round_factor(x, f);
    if(fabs(y * p - x * p) < 0.5)
      return y;
    f *= 10;
  }
  
  return y;
}
