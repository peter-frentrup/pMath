#include <boxes/graphics/rectanglebox.h>

#include <boxes/graphics/graphicsdrawingcontext.h>
#include <graphics/canvas.h>


namespace richmath {
  class RectangleBox::Impl {
    public:
      explicit Impl(RectangleBox &self);
      
    private:
      RectangleBox &self;
  };
}

using namespace richmath;

extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Range;
extern pmath_symbol_t richmath_System_RectangleBox;

//{ class RectangleBox ...

RectangleBox::RectangleBox()
  : GraphicsElement(),
    p0{0.0, 0.0},
    p1{1.0, 1.0}
{
}

RectangleBox::~RectangleBox() {
}

bool RectangleBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_RectangleBox))
    return false;
    
  if(expr.expr_length() > 2)
    return false;
    
  if(_expr == expr) {
    finish_load_from_object(PMATH_CPP_MOVE(expr));
    return true;
  }
  
  _expr = expr;
  p0 = DoublePoint{0.0, 0.0};
  if(expr.expr_length() >= 1) {
    DoublePoint::load_point(p0, expr[1]); // TODO: message if load_point() returns false
  }
  
  p1 = DoublePoint{p0.x + 1.0, p0.y + 1.0};
  
  if(expr.expr_length() >= 2) {
    DoublePoint::load_point(p1, expr[2]); // TODO: message if load_point() returns false
  }
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

RectangleBox *RectangleBox::try_create(Expr expr, BoxInputFlags opts) {
  RectangleBox *box = new RectangleBox;
  
  if(!box->try_load_from_object(expr, opts)) {
    delete box;
    return nullptr;
  }
  
  return box;
}

void RectangleBox::find_extends(GraphicsBounds &bounds) {
  bounds.add_point(p0.x, p0.y);
  bounds.add_point(p0.x, p1.y);
  bounds.add_point(p1.x, p1.y);
  bounds.add_point(p1.x, p0.y);
}

void RectangleBox::paint(GraphicsDrawingContext &gc) {
  auto mat = gc.canvas().get_matrix();
  
  gc.canvas().move_to(p0.x, p0.y);
  gc.canvas().line_to(p0.x, p1.y);
  gc.canvas().line_to(p1.x, p1.y);
  gc.canvas().line_to(p1.x, p0.y);
  gc.canvas().close_path();
  
  gc.canvas().set_matrix(gc.initial_matrix());
  gc.fill_with_edgeform();
  gc.canvas().set_matrix(mat);
}

Expr RectangleBox::to_pmath_impl(BoxOutputFlags flags) {
  return _expr;
}

//} ... class RectangleBox

//{ class RectangleBox::Impl ...

RectangleBox::Impl::Impl(RectangleBox &self)
: self{self}
{
}

//} ... class RectangleBox::Impl
