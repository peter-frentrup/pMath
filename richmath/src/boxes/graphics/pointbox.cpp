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


//{ class DoublePoint ...

bool DoublePoint::load_point(DoublePoint &point, Expr coords) {
  if(coords[0] != PMATH_SYMBOL_LIST)
    return false;
    
  if(coords.expr_length() != 2)
    return false;
    
  point.x = coords[1].to_double(NAN);
  point.y = coords[2].to_double(NAN);
  
  return !std::isnan(point.x) && !std::isnan(point.y);
}

bool DoublePoint::load_point_or_points(Array<DoublePoint> &points, Expr coords) {
  if(coords[0] != PMATH_SYMBOL_LIST)
    return false;
    
  points.length(1);
  if(load_point(points[0], coords))
    return true;
    
  points.length((int)coords.expr_length());
  for(int i = 0; i < points.length(); ++i) {
    if(!load_point(points[i], coords[i + 1])) {
      points.length(0);
      return false;
    }
  }
  
  return true;
}

bool DoublePoint::load_line(Array<DoublePoint> &line, Expr coords) {
  if(coords[0] != PMATH_SYMBOL_LIST)
    return false;
    
  line.length((int)coords.expr_length());
  for(int i = 0; i < line.length(); ++i) {
    if(!load_point(line[i], coords[i + 1])) {
      line.length(0);
      return false;
    }
  }
  
  return true;
}

bool DoublePoint::load_line_or_lines(Array< Array<DoublePoint> > &lines, Expr coords) {
  if(coords[0] != PMATH_SYMBOL_LIST)
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
  if(expr[0] != PMATH_SYMBOL_POINTBOX)
    return false;
    
  if(expr.expr_length() != 1)
    return false;
    
  if(_uncompressed_expr == expr)
    return true;
    
  Expr data = expr[1];
  if(data[0] == PMATH_SYMBOL_UNCOMPRESS && data[1].is_string()) {
    data = Call(Symbol(PMATH_SYMBOL_UNCOMPRESS), data[1], Symbol(PMATH_SYMBOL_HOLDCOMPLETE));
    data = Evaluate(data);
    if(data[0] == PMATH_SYMBOL_HOLDCOMPLETE && data.expr_length() == 1) {
      data = data[1];
      expr.set(1, data);
    }
  }
  
  if(DoublePoint::load_point_or_points(_points, data)) {
    _uncompressed_expr = expr;
    return true;
  }
  
  _uncompressed_expr = Expr();
  return false;
}

PointBox *PointBox::create(Expr expr, BoxInputFlags opts) {
  PointBox *box = new PointBox;
  
  if(!box->try_load_from_object(expr, opts)) {
    delete box;
    return nullptr;
  }
  
  return box;
}

void PointBox::find_extends(GraphicsBounds &bounds) {
  for(int i = 0; i < _points.length(); ++i) {
    DoublePoint &pt = _points[i];
    bounds.add_point(pt.x, pt.y);
  }
}

void PointBox::paint(GraphicsBoxContext *context) {
  context->ctx->canvas->save();
  {
    cairo_matrix_t mat = context->ctx->canvas->get_matrix();
    
    cairo_matrix_t idmat;
    cairo_matrix_init_identity(&idmat);
    context->ctx->canvas->set_matrix(idmat);
    
    for(int i = 0; i < _points.length(); ++i) {
      DoublePoint pt = _points[i];
      
      cairo_matrix_transform_point(&mat, &pt.x, &pt.y);
      
      context->ctx->canvas->new_sub_path();
      context->ctx->canvas->arc(
        pt.x, pt.y,
        2,
        0.0,
        2 * M_PI,
        false);
    }
  }
  context->ctx->canvas->restore();
  
  context->ctx->canvas->fill();
}

Expr PointBox::to_pmath(BoxOutputFlags flags) { 
  if(_uncompressed_expr.expr_length() != 1)
    return _uncompressed_expr;
    
  Expr data = _uncompressed_expr[1];
  size_t size = pmath_object_bytecount(data.get());
  
  if(size > 4096) {
    data = Evaluate(Call(Symbol(PMATH_SYMBOL_COMPRESS), data));
    data = Call(Symbol(PMATH_SYMBOL_UNCOMPRESS), data);
    return Call(_uncompressed_expr[0], data);
  }
  
  return _uncompressed_expr;
}

//} ... class PointBox
