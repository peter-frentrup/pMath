#include <boxes/graphics/linebox.h>

#include <graphics/context.h>

#include <cmath>
#include <limits>


using namespace richmath;
using namespace std;

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
  
  if(DoublePoint::load_line_or_lines(_lines, data)) {
    _uncompressed_expr = expr;
    return true;
  }
  
  _uncompressed_expr = Expr();
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
    Array<DoublePoint> &line = _lines[i];
    
    for(int j = 0; j < line.length(); ++j) {
      DoublePoint &pt = line[j];
      bounds.add_point(pt.x, pt.y);
    }
  }
}

void LineBox::paint(GraphicsBoxContext *context) {
  for(int i = 0; i < _lines.length(); ++i) {
    Array<DoublePoint> &line = _lines[i];
    
    if(line.length() > 1) {
      const DoublePoint &pt = line[0];
      cairo_move_to(context->ctx->canvas->cairo(), pt.x, pt.y);
      
      for(int j = 1; j < line.length(); ++j) {
        const DoublePoint &pt = line[j];
        cairo_line_to(context->ctx->canvas->cairo(), pt.x, pt.y);
      }
    }
    else if(line.length() == 1){
      const DoublePoint &pt = line[0];
      cairo_move_to(context->ctx->canvas->cairo(), pt.x, pt.y);
      cairo_line_to(context->ctx->canvas->cairo(), pt.x, pt.y);
    }
  }
  
  cairo_line_cap_t cap = cairo_get_line_cap(context->ctx->canvas->cairo());
  cairo_set_line_cap(context->ctx->canvas->cairo(), CAIRO_LINE_CAP_ROUND);
  
  context->ctx->canvas->hair_stroke();
  
  cairo_set_line_cap(context->ctx->canvas->cairo(), cap);
}

Expr LineBox::to_pmath(int flags) {  // BoxFlagXXX
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
