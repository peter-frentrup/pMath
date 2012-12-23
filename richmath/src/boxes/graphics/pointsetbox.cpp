#include <boxes/graphics/pointsetbox.h>

#include <graphics/context.h>

#include <cmath>
#include <limits>


#ifdef _MSC_VER
#  define isnan  _isnan
#endif

#ifndef NAN
#  define NAN numeric_limits<double>::quiet_NaN()
#endif


using namespace richmath;
using namespace std;

static bool load_point(PointSetBox::Point &point, Expr coords) {
  if(coords[0] != PMATH_SYMBOL_LIST)
    return false;
    
  if(coords.expr_length() != 2)
    return false;
    
  point.x = coords[1].to_double(NAN);
  point.y = coords[2].to_double(NAN);
  
  return !isnan(point.x) && !isnan(point.y);
}

static bool load_line(Array<PointSetBox::Point> &line, Expr coords) {
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

static bool load_lines(Array< Array<PointSetBox::Point> > &lines, Expr coords) {
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

//{ class PointSetBox ...

PointSetBox::PointSetBox()
  : GraphicsElement()
{
}

PointSetBox::~PointSetBox() {
}

bool PointSetBox::try_load_from_object(Expr expr, int opts) {
  if(expr[0] != PMATH_SYMBOL_LINEBOX && expr[0] != PMATH_SYMBOL_POINTBOX)
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
  
  if(load_lines(_lines, data)) {
    _uncompressed_expr = expr;
    return true;
  }
  
  _uncompressed_expr = Expr();
  return false;
}

PointSetBox *PointSetBox::create(Expr expr, int opts) {
  PointSetBox *box = new PointSetBox;
  
  if(!box->try_load_from_object(expr, opts)) {
    delete box;
    return 0;
  }
  
  return box;
}

void PointSetBox::find_extends(GraphicsBounds &bounds) {
  for(int i = 0; i < _lines.length(); ++i) {
    Array<Point> &line = _lines[i];
    
    for(int j = 0; j < line.length(); ++j) {
      Point &pt = line[j];
      bounds.add_point(pt.x, pt.y);
    }
  }
}

void PointSetBox::paint(GraphicsBoxContext *context) {
  if(is_drawing_points()) {
    context->ctx->canvas->save();
    {
      cairo_matrix_t mat;
      cairo_get_matrix(context->ctx->canvas->cairo(), &mat);
      
      cairo_matrix_t idmat;
      cairo_matrix_init_identity(&idmat);
      cairo_set_matrix(context->ctx->canvas->cairo(), &idmat);
      
      for(int i = 0; i < _lines.length(); ++i) {
        Array<Point> &line = _lines[i];
        
        if(line.length() > 1) {
          for(int j = 0; j < line.length(); ++j) {
            Point pt = line[j];
            
            cairo_matrix_transform_point(&mat, &pt.x, &pt.y);
            
            cairo_new_sub_path(context->ctx->canvas->cairo());
            cairo_arc(
              context->ctx->canvas->cairo(),
              pt.x, pt.y,
              2,
              0.0,
              2 * M_PI);
          }
        }
      }
    }
    context->ctx->canvas->restore();
    
    context->ctx->canvas->fill();
  }
  
  if(is_drawing_lines()) {
    for(int i = 0; i < _lines.length(); ++i) {
      Array<Point> &line = _lines[i];
      
      if(line.length() > 1) {
        const Point &pt = line[0];
        cairo_move_to(context->ctx->canvas->cairo(), pt.x, pt.y);
        
        for(int j = 1; j < line.length(); ++j) {
          const Point &pt = line[j];
          cairo_line_to(context->ctx->canvas->cairo(), pt.x, pt.y);
        }
      }
    }
    
    context->ctx->canvas->hair_stroke();
  }
}

Expr PointSetBox::to_pmath(int flags) {  // BoxFlagXXX
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

//} ... class LineBox
