#include <gui/gtk/mgtk-css.h>

#include <eval/cubic-bezier-easing-function.h>
#include <graphics/color.h>


using namespace richmath;


namespace {
  class MathGtkCssParser {
    public:
      MathGtkCssParser(const char *buffer);
      
      double parse_duration();
      Expr parse_text_shadow_value();
      bool parse_timing_function(CubicBezierEasingFunction &fun);
      
    private:
      Expr parse_next_text_shadow();
      Color parse_color();
      double parse_percent(double no_percent_divisor);
      double parse_length(double px);
      double parse_next_number();
      
      int skip_hex();
      int skip_hyphen_identifier();
      int skip_word();
      int skip_whitespace();
      int skip_whitespace_or_comma();
      
      bool starts_number() { starts_number(*buffer); }
      static bool starts_number(char ch);
      static int hex_digit(char ch, int err = 0);
      
      static Color color_from_3hex(const char *hex);
      static Color color_from_4hex(const char *hex);
      static Color color_from_6hex(const char *hex);
      static Color color_from_8hex(const char *hex);
      
    public:
      const char *buffer;
  };
}

extern pmath_symbol_t richmath_System_Reverse;

//{ class MathGtkCss ...

void MathGtkCss::init() {
}

void MathGtkCss::done() {
}

double MathGtkCss::parse_transition_delay(const char *css, double fallback) {
  static const char name[] = "transition-delay:";
  if(const char *sub = strstr(css, name)) {
    return MathGtkCssParser(sub + sizeof(name) - 1).parse_duration();
  }
  return fallback;
}

double MathGtkCss::parse_transition_duration(const char *css, double fallback) {
  static const char name[] = "transition-duration:";
  if(const char *sub = strstr(css, name)) {
    return MathGtkCssParser(sub + sizeof(name) - 1).parse_duration();
  }
  return fallback;
}

bool MathGtkCss::parse_transition_timing_function(CubicBezierEasingFunction &fun, const char *css) {
  static const char name[] = "transition-timing-function:";
  if(const char *sub = strstr(css, name)) {
    return MathGtkCssParser(sub + sizeof(name) - 1).parse_timing_function(fun);
  }
  
  return false;
}

Expr MathGtkCss::parse_text_shadow(const char *css) {
  static const char name[] = "text-shadow:";
  if(const char *sub = strstr(css, name)) {
    return MathGtkCssParser(sub + sizeof(name) - 1).parse_text_shadow_value();
  }
  return Expr();
}

#if GTK_MAJOR_VERSION >= 3
Expr MathGtkCss::parse_text_shadow(GtkStyleContext *ctx) {
# if GTK_CHECK_VERSION(3, 20, 0)
  {
    char *utf8 = gtk_style_context_to_string(ctx, GTK_STYLE_CONTEXT_PRINT_SHOW_STYLE);
    
    Expr text_shadow = parse_text_shadow(utf8);
    
    g_free(utf8);
    
    return text_shadow;
  }      
# endif
  return Expr();
}
#endif

//} ... class MathGtkCss

//{ class MathGtkCssParser ...

MathGtkCssParser::MathGtkCssParser(const char *buffer)
: buffer{buffer} 
{
}

double MathGtkCssParser::parse_duration() {
  skip_whitespace();
  
  double value = parse_next_number();
  
  const char *unit = buffer;
  int unit_len = skip_word();
  
  if(unit_len == 2 && 0 == memcmp(unit, "ms", 2))
    value /= 1000;
    
  // else: assume seconds

  skip_whitespace();
  return value;
}

bool MathGtkCssParser::parse_timing_function(CubicBezierEasingFunction &fun) {
  skip_whitespace();
  
  const char *word = buffer;
  
  int word_len = skip_hyphen_identifier();
  if(word_len == 4 && 0 == memcmp(word, "ease", 4)) {
    fun = CubicBezierEasingFunction::Ease;
    return true;
  }
  if(word_len == 7 && 0 == memcmp(word, "ease-in", 7)) {
    fun = CubicBezierEasingFunction::EaseIn;
    return true;
  }
  if(word_len == 11 && 0 == memcmp(word, "ease-in-out", 11)) {
    fun = CubicBezierEasingFunction::EaseInOut;
    return true;
  }
  if(word_len == 8 && 0 == memcmp(word, "ease-out", 8)) {
    fun = CubicBezierEasingFunction::EaseOut;
    return true;
  }
  if(word_len == 6 && 0 == memcmp(word, "linear", 6)) {
    fun = CubicBezierEasingFunction::Linear;
    return true;
  }
  if(word_len == 12 && 0 == memcmp(word, "cubic-bezier", 12)) {
    if(*buffer == '(') {
      ++buffer;
      double x1 = parse_next_number();
      skip_whitespace_or_comma();
      double y1 = parse_next_number();
      skip_whitespace_or_comma();
      double x2 = parse_next_number();
      skip_whitespace_or_comma();
      double y2 = parse_next_number();
      skip_whitespace();
      if(*buffer == ')') {
        ++buffer;
        
        fun = CubicBezierEasingFunction(x1, y1, x2, y2);
        return true;
      }
    }
    return false;
  }
  if(word_len == 10 && 0 == memcmp(word, "step-start", 10)) {
    //fun = CubicBezierEasingFunction::Linear;
    //return true;
    return false;
  }
  if(word_len == 8 && 0 == memcmp(word, "step-end", 8)) {
    //fun = CubicBezierEasingFunction::Linear;
    //return true;
    return false;
  }
  
  return false;
}

Expr MathGtkCssParser::parse_text_shadow_value() {
  Gather g;
  
  skip_whitespace();
  while(*buffer && *buffer != ';') {
    Expr shadow = parse_next_text_shadow();
    if(0 == skip_whitespace_or_comma()) {
      // possibly a parse error
      break;
    }
    
    if(shadow)
      g.emit(shadow);
  }
  
  return Evaluate(Call(Symbol(richmath_System_Reverse), g.end()));
}

Expr MathGtkCssParser::parse_next_text_shadow() {
  skip_whitespace();
  if(starts_number()) {
    double dx = parse_length(0.75);
    
    skip_whitespace();
    if(starts_number()) {
      double dy = parse_length(0.75);
      
      double blur_radius = 0;
      if(starts_number())
        blur_radius = parse_length(0.75);
      
      Color col = parse_color();
      
      if(col) {
        if(blur_radius > 0)
          return List(Expr(dx), Expr(dy), col.to_pmath(), Expr(blur_radius));
        else
          return List(Expr(dx), Expr(dy), col.to_pmath());
      }
    }
  }
  
  return Expr();
}

Color MathGtkCssParser::parse_color() {
  if(buffer[0] == '#') {
    ++buffer;
    
    int hexlen = skip_hex();
    switch(hexlen) {
      case 3: return color_from_3hex(buffer - hexlen);
      case 4: return color_from_4hex(buffer - hexlen);
      case 6: return color_from_6hex(buffer - hexlen);
      case 8: return color_from_8hex(buffer - hexlen);
      default: return Color::None;
    }
  }
  
  const char *name_start = buffer;
  int name_len = skip_word();
  
  if(*buffer == '(') {
    ++buffer;
    if((name_len == 4 || name_len == 3) && memcmp(name_start, "rgba", name_len) == 0) {
      double red = parse_percent(255);
      skip_whitespace_or_comma();
      double green = parse_percent(255);
      skip_whitespace_or_comma();
      double blue = parse_percent(255);
      skip_whitespace_or_comma();
      
      double alpha = 1.0;
      if(starts_number()) {
        alpha = parse_percent(1);
      }
      
      while(*buffer && *buffer != ')')
        ++buffer;
      
      if(*buffer == ')')
        ++buffer;
      
      if(alpha == 0)
        return Color::None;
      
      return Color::from_rgb(red, green, blue);
    }
    
    while(*buffer && *buffer != ')')
      ++buffer;
    
    if(*buffer == ')')
      ++buffer;
      
    return Color::None;
  }
  
  return Color::None;
}

Color MathGtkCssParser::color_from_3hex(const char *hex) {
  int red   = hex_digit(hex[0]);
  int green = hex_digit(hex[1]);
  int blue  = hex_digit(hex[2]);
  return Color::from_rgb24(red * 0x110000 + green * 0x1100 + blue * 0x11);
}

Color MathGtkCssParser::color_from_4hex(const char *hex) {
  int red   = hex_digit(hex[0]);
  int green = hex_digit(hex[1]);
  int blue  = hex_digit(hex[2]);
  int alpha = hex_digit(hex[3]);
  if(alpha == 0)
    return Color::None;
  
  return Color::from_rgb24(red * 0x110000 + green * 0x1100 + blue * 0x11);
}

Color MathGtkCssParser::color_from_6hex(const char *hex) {
  int red   = (hex_digit(hex[0]) << 8) | hex_digit(hex[1]);
  int green = (hex_digit(hex[2]) << 8) | hex_digit(hex[3]);
  int blue  = (hex_digit(hex[4]) << 8) | hex_digit(hex[5]);
  return Color::from_rgb24((red << 16) | (green << 8) | blue);
}

Color MathGtkCssParser::color_from_8hex(const char *hex) {
  int red   = (hex_digit(hex[0]) << 8) | hex_digit(hex[1]);
  int green = (hex_digit(hex[2]) << 8) | hex_digit(hex[3]);
  int blue  = (hex_digit(hex[4]) << 8) | hex_digit(hex[5]);
  int alpha = (hex_digit(hex[6]) << 8) | hex_digit(hex[7]);
  if(alpha == 0)
    return Color::None;
  return Color::from_rgb24((red << 16) | (green << 8) | blue);
}

double MathGtkCssParser::parse_percent(double no_percent_divisor) {
  double value = parse_next_number();
  
  if(*buffer == '%') {
    ++buffer;
    return value / 100;
  }
  
  return value / no_percent_divisor;
} 

double MathGtkCssParser::parse_length(double px) {
  double value = parse_next_number();
  
  skip_word();
  // ignore the unit. assume it is Pixels (px)

  skip_whitespace();
  return value * px;
}

bool MathGtkCssParser::starts_number(char ch) {
  return ch == '+' || ch == '-' || (ch >= '0' && ch <= '9');
}

double MathGtkCssParser::parse_next_number() {
  bool negative = false;
  double value = 0.0;
  
  if(*buffer == '-') {
    negative = true;
    ++buffer;
  }
  else if(*buffer == '+')
    ++buffer;
  
  while(*buffer >= '0' && *buffer <= '9') {
    value = 10 * value + (*buffer - '0');
    ++buffer;
  }
  
  if(*buffer == '.') {
    double divisor = 1.0;
    ++buffer;
    
    while(*buffer >= '0' && *buffer <= '9') {
      value = 10 * value + (*buffer - '0');
      divisor*= 10;
      ++buffer;
    }
    
    value /= divisor;
  }
  
  return value;
}

int MathGtkCssParser::hex_digit(char ch, int err) {
  if(ch >= '0' && ch <= '9')
    return ch - '0';
  
  if(ch >= 'a' && ch <= 'f')
    return (ch - 'a') + 10;
  
  if(ch >= 'A' && ch <= 'F')
    return (ch - 'A') + 10;
  
  return err;
}

int MathGtkCssParser::skip_hex() {
  const char *start = buffer;
  
  while(hex_digit(*buffer, -1) >= 0)
    ++buffer;
    
  return buffer - start;
}

int MathGtkCssParser::skip_hyphen_identifier() {
  const char *start = buffer;
  
  while((*buffer >= 'a' && *buffer <= 'z') || (*buffer >= 'A' && *buffer <= 'Z') || *buffer == '-')
    ++buffer;
    
  return buffer - start;
}

int MathGtkCssParser::skip_word() {
  const char *start = buffer;
  
  while((*buffer >= 'a' && *buffer <= 'z') || (*buffer >= 'A' && *buffer <= 'Z'))
    ++buffer;
    
  return buffer - start;
}

int MathGtkCssParser::skip_whitespace() {
  const char *start = buffer;
  
  while(*buffer && *buffer <= ' ')
    ++buffer;
  
  return buffer - start;
}

int MathGtkCssParser::skip_whitespace_or_comma() {
  const char *start = buffer;
  
  skip_whitespace();
  if(*buffer == ',') {
    ++buffer;
    skip_whitespace();
  }
  
  return buffer - start;
}

  
//} ... class MathGtkCssParser
