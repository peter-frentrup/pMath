#include <boxes/graphics/pointbox.h>

#include <boxes/graphics/graphicsbox.h>
#include <graphics/canvas.h>

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

void PointBox::paint(GraphicsDrawingContext &gc) {
  gc.canvas().save();
  {
    cairo_matrix_t mat = gc.canvas().get_matrix();
    
    cairo_matrix_t init_mat = gc.initial_matrix();
    gc.canvas().set_matrix(gc.initial_matrix());
    //gc.canvas().reset_matrix();
    cairo_matrix_invert(&init_mat);
    //cairo_matrix_multiply(&mat, &mat, &init_mat);
    
    float radius = 0.5f * gc.point_size.resolve(1.0f, LengthConversionFactors::PointSize, gc.plot_range_width);
    
    bool round_to_odd = false;
    bool round_to_even = false;
    
    if(gc.canvas().pixel_device) {
      double px_x = 0.5;
      double px_y = 0;
      
      cairo_matrix_transform_distance(&init_mat, &px_x, &px_y);
      double px_diam = radius * pow(px_x * px_x + px_y * px_y, -0.5);
      
      if(px_diam >= 1 && px_diam < 20) {
        int ipx_diam = (int)(px_diam + 0.5);
        if(ipx_diam & 1)
          round_to_odd = true;
        else
          round_to_even = true;
      }
    }
    
    for(size_t i = 0; i < _points.rows(); ++i) {
      DoublePoint pt{ _points.get(i, 0), _points.get(i, 1) };
      
      cairo_matrix_transform_point(&mat, &pt.x, &pt.y);
      if(round_to_odd) {
        pt.x = ceil(pt.x) - 0.5f;
        pt.y = ceil(pt.y) - 0.5f;
      }
      else if(round_to_even) {
        pt.x = round(pt.x);
        pt.y = round(pt.y);
      }
      cairo_matrix_transform_point(&init_mat, &pt.x, &pt.y);
      
      gc.canvas().new_sub_path();
      gc.canvas().arc(
        pt.x, pt.y,
        radius,
        0.0,
        2 * M_PI,
        false);
    }
  }
  gc.canvas().restore();
  
  gc.canvas().fill();
}

Expr PointBox::to_pmath_impl(BoxOutputFlags flags) { 
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
