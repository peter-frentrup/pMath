#include <graphics/color.h>

#include <util/style.h> // pmath_to_color(), color_to_pmath()

#include <math.h>


using namespace richmath;

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
  return Color::decode(pmath_to_color(std::move(expr)));
}

Expr Color::to_pmath() const {
  return color_to_pmath(_value);
}

//} ... class Color
