#include <eval/cubic-bezier-easing-function.h>
#include <util/pmath-extra.h>

#include <algorithm>
#include <cmath>

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

using namespace richmath;

#define NEWTON_ITERATIONS            (4)
#define NEWTON_MIN_SLOPE             (0.001)
#define SUBDIVISION_PRECISION        (0.0000001)
#define SUBDIVISION_MAX_ITERATIONS   (10)

static const double SampleStep = 1.0 / (CubicBezierEasingFunction::SplineTableSize - 1.0);

extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_List;

Expr richmath_eval_FrontEnd_EvaluateCubicBezierFunction(Expr expr);

static double cubic_bezier_poly3(double b1, double b2) {
  return 3 * b1 - 3 * b2 + 1; // -b0
}

static double cubic_bezier_poly2(double b1, double b2) {
  return 3 * b2 - 6 * b1; // + 3 * b0
}

static double cubic_bezier_poly1(double b1) {
  return 3 * b1; // -3 * b0
}

static double cubic_bezier(double t, double b1, double b2) { // b0=0, b3=1
  return ((cubic_bezier_poly3(b1, b2) * t + cubic_bezier_poly2(b1, b2)) * t + cubic_bezier_poly1(b1)) * t;
}

static double cubic_bezier_slope(double t, double b1, double b2) { // b0=0, b3=1
  return 3 * cubic_bezier_poly3(b1, b2) * t * t + 2 * cubic_bezier_poly2(b1, b2) * t + cubic_bezier_poly1(b1);
}

static double binary_subdivide_t_for_x(double x, double tA, double tB, double x1, double x2) {
  double x_err;
  double mid_t;
  int i = 0;
  do {
    mid_t = tA + (tB - tA)/2;
    x_err = cubic_bezier(mid_t, x1, x2) - x;
    if(x_err > 0) 
      tB = mid_t;
    else 
      tA = mid_t;
  } while(fabs(x_err) > SUBDIVISION_PRECISION && ++i < SUBDIVISION_MAX_ITERATIONS);
  return mid_t;
}

static double newton_iterate_t_for_x(double x, double best_t, double x1, double x2) {
  for(int i = 0; i < NEWTON_ITERATIONS; ++i) {
    double slope = cubic_bezier_slope(best_t, x1, x2);
    if(slope == 0.0)
      return best_t;
    
    double x_err = cubic_bezier(best_t, x1, x2) - x;
    best_t -= x_err / slope;
  }
  
  return best_t;
}

//{ class CubicBezierEasingFunction ...

const CubicBezierEasingFunction CubicBezierEasingFunction::Linear    = CubicBezierEasingFunction(0.0f,  0.0f, 1.0f,  1.0f);
const CubicBezierEasingFunction CubicBezierEasingFunction::Ease      = CubicBezierEasingFunction(0.25f, 0.1f, 0.25f, 1.0f);
const CubicBezierEasingFunction CubicBezierEasingFunction::EaseIn    = CubicBezierEasingFunction(0.42f, 0.0f, 1.0f,  1.0f);
const CubicBezierEasingFunction CubicBezierEasingFunction::EaseInOut = CubicBezierEasingFunction(0.42f, 0.0f, 0.58f, 1.0f);
const CubicBezierEasingFunction CubicBezierEasingFunction::EaseOut   = CubicBezierEasingFunction(0.0f,  0.0f, 0.58f, 1.0f);

CubicBezierEasingFunction::CubicBezierEasingFunction(float x1, float y1, float x2, float y2)
: _x1(std::max(0.0f, std::min(x1, 1.0f))), 
  _y1(y1), 
  _x2(std::max(0.0f, std::min(x2, 1.0f))), 
  _y2(y2) 
{
  for(int i = 0; i < SplineTableSize; ++i) {
    _spline_table[i] = cubic_bezier(i * SampleStep, _x1, _x2);
  }
}

double CubicBezierEasingFunction::get_t_for_x(double x) const {
  int i = 0;
  while(i < SplineTableSize - 1 && _spline_table[i] <= x) {
    ++i;
  }
  --i;
  double tA = i * SampleStep;
  
  double dist = (x - _spline_table[i]) / (_spline_table[i + 1] - _spline_table[i]);
  double best_t = tA + dist * SampleStep;
  
  double slope = cubic_bezier_slope(best_t, _x1, _x2);
  if(fabs(slope) > NEWTON_MIN_SLOPE) 
    return newton_iterate_t_for_x(x, best_t, _x1, _x2);
  else if (slope == 0.0)
    return best_t;
  else
    return binary_subdivide_t_for_x(x, tA, tA + SampleStep, _x1, _x2);
}

double CubicBezierEasingFunction::operator()(double x) const {
  if(is_linear())
    return x;
  else if(0.0 <= x && x <= 1.0)
    return cubic_bezier(get_t_for_x(x), _y1, _y2);
  else
    return x;
}

//} ... class CubicBezierEasingFunction

template<typename F>
static void map_double_func(Expr &args, F func) {
  if(args.is_packed_array()) {
    if(pmath_packed_array_get_element_type(args.get()) == PMATH_PACKED_DOUBLE) {
      if(pmath_packed_array_get_non_continuous_dimensions(args.get()) == 0) {
        pmath_t pa = args.release();
        size_t total_count = *pmath_packed_array_get_sizes(pa);
        
        if(double *data = (double*)pmath_packed_array_begin_write(&pa, nullptr, 0)) {
          for(size_t i = 0; i < total_count; ++i)
            data[i] = func(data[i]);
          
          args = Expr{pa};
          return;
        }
        args = Expr{pa};
      }
    }
  }
  
  if(args[0] == richmath_System_List) {
    pmath_t obj = args.release();
    size_t len = pmath_expr_length(obj);
    
    for(size_t i = 1; i <= len; ++i) {
      Expr item {pmath_expr_extract_item(obj, i)};
      map_double_func(item, func);
      obj = pmath_expr_set_item(obj, i, item.release());
    }
    
    args = Expr{obj};
    return;
  }
  
  args = func(args.to_double());
}

Expr richmath_eval_FrontEnd_EvaluateCubicBezierFunction(Expr expr) {
  /*  FrontEnd`EvaluateCubicBezierFunction(x1, y1, x2, y2, x)
   */
  
  if(expr.expr_length() != 5)
    return Symbol(richmath_System_DollarFailed);
  
  double x1 = expr[1].to_double();
  double y1 = expr[2].to_double();
  double x2 = expr[3].to_double();
  double y2 = expr[4].to_double();
  
  CubicBezierEasingFunction func(x1, y1, x2, y2);
  Expr arg = expr[5];
  expr = Expr();
  
  map_double_func(arg, func);
  return arg;
}
