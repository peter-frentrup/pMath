#include <boxes/graphics/linebox.h>

#include <graphics/context.h>

#include <cmath>
#include <limits>


#ifdef _MSC_VER

#define isnan  _isnan

#endif

#ifndef NAN
#define NAN numeric_limits<double>::quiet_NaN()
#endif


using namespace richmath;
using namespace std;

//{ class LineBox ...

LineBox::LineBox()
  : GraphicsElement()
{
}

LineBox::~LineBox() {
}

static bool load_point(LineBox::Point &point, Expr coords) {
  if(coords[0] != PMATH_SYMBOL_LIST)
    return false;
    
  if(coords.expr_length() != 2)
    return false;
    
  point.x = coords[1].to_double(NAN);
  point.y = coords[2].to_double(NAN);
  
  return !isnan(point.x) && !isnan(point.y);
}

static bool load_line(Array<LineBox::Point> &line, Expr coords) {
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

static bool load_lines(Array< Array<LineBox::Point> > &lines, Expr coords) {
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

LineBox *LineBox::create(Expr expr, int opts) {
  LineBox *box = new LineBox;
  box->_expr = expr;
  
  load_lines(box->_lines, expr[1]);
  
  return box;
}

void LineBox::find_extends(GraphicsBounds &bounds) {
  for(int i = 0; i < _lines.length(); ++i) {
    Array<Point> &line = _lines[i];
    
    for(int j = 0; j < line.length(); ++j) {
      Point &pt = line[j];
      bounds.add_point(pt.x, pt.y);
    }
  }
}

void LineBox::paint(Context *context) {
  for(int i = 0; i < _lines.length(); ++i) {
    Array<Point> &line = _lines[i];
    
    if(line.length() > 1) {
      Point &pt = line[0];
      cairo_move_to(context->canvas->cairo(), pt.x, pt.y);
      
      for(int j = 1; j < line.length(); ++j) {
        Point &pt = line[j];
        cairo_line_to(context->canvas->cairo(), pt.x, pt.y);
      }
    }
  }
  
  context->canvas->hair_stroke();
}

//} ... class LineBox
