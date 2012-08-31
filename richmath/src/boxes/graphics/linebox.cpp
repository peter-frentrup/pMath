#include <boxes/graphics/linebox.h>

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

//{ class LineBox ...

LineBox::LineBox()
  : GraphicsElement()
{
}

LineBox::~LineBox() {
}

bool LineBox::try_load_from_object(Expr expr, int opts) {
  if(expr[0] != PMATH_SYMBOL_LINEBOX)
    return false;
    
  if(expr.expr_length() != 1)
    return false;
  
  if(_expr == expr)
    return true;
  
  Expr data = expr[1];
  if(data[0] == PMATH_SYMBOL_UNCOMPRESS && data[1].is_string()){
    data = Call(Symbol(PMATH_SYMBOL_UNCOMPRESS), data[1], Symbol(PMATH_SYMBOL_HOLDCOMPLETE));
    data = Evaluate(data);
    if(data[0] == PMATH_SYMBOL_HOLDCOMPLETE && data.expr_length() == 1) {
      data = data[1];
      expr.set(1, data);
    }
  }
  
  if(load_lines(_lines, data)) {
    _expr = expr;
    return true;
  }
  
  _expr = Expr();
  return false;
}
    
LineBox *LineBox::create(Expr expr, int opts) {
  LineBox *box = new LineBox;
  
  if(!box->try_load_from_object(expr, opts)) {
    delete box;
    return 0;
  }
  
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

Expr LineBox::to_pmath(int flags) {  // BoxFlagXXX
  if(_expr[0] != PMATH_SYMBOL_LINEBOX || _expr.expr_length() != 1)
    return _expr;
  
  Expr data = _expr[1];
  size_t size = pmath_object_bytecount(data.get());
  
  if(size > 4096) {
    data = Evaluate(Call(Symbol(PMATH_SYMBOL_COMPRESS), data));
    data = Call(Symbol(PMATH_SYMBOL_UNCOMPRESS), data);
    return Call(Symbol(PMATH_SYMBOL_LINEBOX), data);
  }
  
  return _expr; 
}

//} ... class LineBox
