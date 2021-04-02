#include <boxes/graphics/pointbox.h>

#include <graphics/context.h>

#include <cmath>
#include <limits>


#ifdef _MSC_VER
namespace std {
  static bool isnan(double d) {return _isnan(d);}
}
#endif

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif


using namespace richmath;
using namespace std;

extern pmath_symbol_t richmath_System_CompressedData;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_PointBox;

//{ class DoublePoint ...

bool DoublePoint::load_point(DoublePoint &point, Expr coords) {
  if(coords[0] != richmath_System_List)
    return false;
    
  if(coords.expr_length() != 2)
    return false;
    
  point.x = coords[1].to_double(NAN);
  point.y = coords[2].to_double(NAN);
  
  return !std::isnan(point.x) && !std::isnan(point.y);
}

bool DoublePoint::load_point_or_points(DoubleMatrix &points, Expr coords) {
  DoublePoint pt;
  if(load_point(pt, coords)) {
    WriteableDoubleMatrix the_point {1, 2};
    if(the_point) {
      the_point.set(0, 0, pt.x);
      the_point.set(0, 1, pt.y);
      points = std::move(the_point);
      return true;
    }
    return false;
  }
  
  return load_line(points, std::move(coords));
}

bool DoublePoint::load_line(DoubleMatrix &line, Expr coords) {
  if(coords.is_packed_array()) {
    line = DoubleMatrix::const_from_expr(coords);
    if(line) {
      if(line.cols() != 2) {
        line = DoubleMatrix{};
        return false;
      }
      
      return true;
    }
  }
  
  if(coords[0] != richmath_System_List)
    return false;
  
  size_t rows = coords.expr_length();
  WriteableDoubleMatrix new_points{rows, 2};
  if(new_points.is_empty() && rows > 0)
    return false;
  
  DoublePoint pt;
  for(size_t i = 0; i < rows; ++i) {
    if(!load_point(pt, coords[i + 1])) {
      line = DoubleMatrix{};
      return false;
    }
    
    new_points.set(i, 0, pt.x);
    new_points.set(i, 1, pt.y);
  }
  
  line = std::move(new_points);
  return true;
}

bool DoublePoint::load_line_or_lines(Array< DoubleMatrix > &lines, Expr coords) {
  if(coords[0] != richmath_System_List)
    return false;
    
  lines.length(1);
  if(load_line(lines[0], coords))
    return true;
    
  lines.length((int)coords.expr_length());
  for(int i = 0; i < lines.length(); ++i) {
    if(!load_line(lines[i], coords[i + 1])) {
      lines.length(0);
      return false;
    }
  }
  
  return true;
}

//} ... class DoublePoint

//{ class PointBox ...

PointBox::PointBox()
  : GraphicsElement()
{
}

PointBox::~PointBox() {
}

bool PointBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_PointBox)
    return false;
    
  if(expr.expr_length() != 1)
    return false;
    
  if(_uncompressed_expr == expr) {
    finish_load_from_object(std::move(expr));
    return true;
  }
    
  Expr data = expr[1];
  if(data[0] == richmath_System_CompressedData && data[1].is_string()) {
    data = Expr{ pmath_decompress_from_string(data[1].release()) };
    if(data.is_expr())
      expr.set(1, data);
  }
  
  if(DoublePoint::load_point_or_points(_points, data)) {
    _uncompressed_expr = expr;
    finish_load_from_object(std::move(expr));
    return true;
  }
  
  _uncompressed_expr = Expr();
  return false;
}

PointBox *PointBox::try_create(Expr expr, BoxInputFlags opts) {
  PointBox *box = new PointBox;
  
  if(!box->try_load_from_object(expr, opts)) {
    delete box;
    return nullptr;
  }
  
  return box;
}

void PointBox::find_extends(GraphicsBounds &bounds) {
  for(size_t i = 0; i < _points.rows(); ++i) 
    bounds.add_point(_points.get(i, 0), _points.get(i, 1));
}

void PointBox::paint(GraphicsBox *owner, Context &context) {
  context.canvas().save();
  {
    cairo_matrix_t mat = context.canvas().get_matrix();
    
    context.canvas().reset_matrix();
    
    for(size_t i = 0; i < _points.rows(); ++i) {
      DoublePoint pt{ _points.get(i, 0), _points.get(i, 1) };
      
      cairo_matrix_transform_point(&mat, &pt.x, &pt.y);
      
      context.canvas().new_sub_path();
      context.canvas().arc(
        pt.x, pt.y,
        2,
        0.0,
        2 * M_PI,
        false);
    }
  }
  context.canvas().restore();
  
  context.canvas().fill();
}

Expr PointBox::to_pmath(BoxOutputFlags flags) { 
  if(_uncompressed_expr.expr_length() != 1)
    return _uncompressed_expr;
    
  Expr data = _uncompressed_expr[1];
  bool should_compress = false;
  if(data[0] == richmath_System_List && data.expr_length() > 1) {
    if(data.is_packed_array() && pmath_packed_array_get_element_type(data.get()) == PMATH_PACKED_DOUBLE) {
      should_compress = true;
    }
  }
  
  if(!should_compress) {
    size_t size = pmath_object_bytecount(data.get());
    should_compress = size > 4096;
  }
  
  if(should_compress) {
    data = Call(
      Symbol(richmath_System_CompressedData), 
      Expr{ pmath_compress_to_string(data.release()) });
    return Call(_uncompressed_expr[0], data);
  }
  
  return _uncompressed_expr;
}

//} ... class PointBox
