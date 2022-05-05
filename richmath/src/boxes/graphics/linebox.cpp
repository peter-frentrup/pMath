#include <boxes/graphics/linebox.h>

#include <graphics/context.h>

#include <cmath>
#include <limits>


using namespace richmath;
using namespace std;

extern pmath_symbol_t richmath_System_CompressedData;
extern pmath_symbol_t richmath_System_LineBox;
extern pmath_symbol_t richmath_System_List;

//{ class LineBox ...

LineBox::LineBox()
  : GraphicsElement()
{
}

LineBox::~LineBox() {
}

bool LineBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_LineBox)
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
  
  if(DoublePoint::load_line_or_lines(_lines, data)) {
    _uncompressed_expr = expr;
    finish_load_from_object(std::move(expr));
    return true;
  }
  
  _uncompressed_expr = Expr();
  return false;
}

LineBox *LineBox::try_create(Expr expr, BoxInputFlags opts) {
  LineBox *box = new LineBox;
  
  if(!box->try_load_from_object(expr, opts)) {
    delete box;
    return nullptr;
  }
  
  return box;
}

void LineBox::find_extends(GraphicsBounds &bounds) {
  for(int i = 0; i < _lines.length(); ++i) {
    DoubleMatrix &line = _lines[i];
    
    for(int j = 0; j < line.rows(); ++j) 
      bounds.add_point(line.get(j, 0), line.get(j, 1));
  }
}

void LineBox::paint(GraphicsBox *owner, Context &context) {
  for(int i = 0; i < _lines.length(); ++i) {
    DoubleMatrix &line = _lines[i];
    
    if(line.rows() > 1) {
      context.canvas().move_to(line.get(0, 0), line.get(0, 1));
      
      for(int j = 1; j < line.rows(); ++j) 
        context.canvas().line_to(line.get(j, 0), line.get(j, 1));
    }
    else if(line.rows() == 1){
      DoublePoint pt { line.get(0, 0), line.get(0, 1) };
      context.canvas().move_to(pt.x, pt.y);
      context.canvas().line_to(pt.x, pt.y);
    }
  }
  
  cairo_line_cap_t cap = cairo_get_line_cap(context.canvas().cairo());
  cairo_set_line_cap(context.canvas().cairo(), CAIRO_LINE_CAP_ROUND);
  
  context.canvas().hair_stroke();
  
  cairo_set_line_cap(context.canvas().cairo(), cap);
}

Expr LineBox::to_pmath_impl(BoxOutputFlags flags) { 
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

//} ... class LineBox
