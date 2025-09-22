#include <boxes/graphics/graphicselement.h>

#include <boxes/graphics/graphicsdirective.h>
#include <boxes/graphics/graphicsdrawingcontext.h>
#include <boxes/graphics/beziercurvebox.h>
#include <boxes/graphics/circleordiskbox.h>
#include <boxes/graphics/linebox.h>
#include <boxes/graphics/pointbox.h>
#include <boxes/box.h>

#include <graphics/canvas.h>

#include <util/autovaluereset.h>

#include <cmath>


#ifdef _MSC_VER
#  define isfinite(x)  (_finite(x))
#endif


using namespace richmath;
using namespace std;

extern pmath_symbol_t richmath_System_BezierCurveBox;
extern pmath_symbol_t richmath_System_CircleBox;
extern pmath_symbol_t richmath_System_DiskBox;
extern pmath_symbol_t richmath_System_LineBox;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_PointBox;

namespace {
  class DummyGraphicsElement: public GraphicsElement {
    public:
      DummyGraphicsElement(Expr expr)
        : GraphicsElement(),
          _expr(expr)
      {
      }
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override {
        return false;
      }
      
      virtual void find_extends(GraphicsBounds &bounds) override {
      }
      
      virtual void paint(GraphicsDrawingContext &gc) override {
      }
      
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override { return _expr; }
      
    private:
      Expr _expr;
  };
}

//{ class GraphicsBounds ...

GraphicsBounds::GraphicsBounds()
: x_range{HUGE_VAL, -HUGE_VAL},
  y_range{HUGE_VAL, -HUGE_VAL}
{
  cairo_matrix_init_identity(&elem_to_container);
}

bool GraphicsBounds::is_finite() const {
  return isfinite(x_range.from) && isfinite(x_range.to) && isfinite(y_range.from) && isfinite(y_range.to);
}

void GraphicsBounds::add_point(double elem_x, double elem_y) {
  bool add_x = isfinite(elem_x);
  bool add_y = isfinite(elem_y);
  
  if(!add_x)
    elem_x = 0.0;
  
  if(!add_y)
    elem_y = 0.0;
  
  cairo_matrix_transform_point(&elem_to_container, &elem_x, &elem_y);
  
  if(add_x) {
    if(elem_x < x_range.from) x_range.from = elem_x;
    if(elem_x > x_range.to)   x_range.to   = elem_x;
  }
  
  if(add_y) {
    if(elem_y < y_range.from) y_range.from = elem_y;
    if(elem_y > y_range.to)   y_range.to   = elem_y;
  }
}

//} ... class GraphicsBounds

//{ class GraphicsElement ...

GraphicsElement::GraphicsElement()
  : StyledObject()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

GraphicsElement::~GraphicsElement() {
}

GraphicsElement *GraphicsElement::create(Expr expr, BoxInputFlags opts) {
  Expr head = expr[0];
  
  if(head == richmath_System_BezierCurveBox) {
    if(auto ge = BezierCurveBox::try_create(expr, opts))
      return ge;
  }
  
  if(head == richmath_System_CircleBox || head == richmath_System_DiskBox) {
    if(auto ge = CircleOrDiskBox::try_create(expr, opts))
      return ge;
  }
  
  if(head == richmath_System_LineBox) {
    if(auto ge = LineBox::try_create(expr, opts))
      return ge;
  }
  
  if(head == richmath_System_List) {
    auto coll = new GraphicsElementCollection(nullptr);
    coll->load_from_object(expr, opts);
    return coll;
  }
  
  if(head == richmath_System_PointBox) {
    if(auto ge = PointBox::try_create(expr, opts))
      return ge;
  }
  
  if(auto dir = GraphicsDirective::try_create(expr, opts)) 
    return dir;
  
  return new DummyGraphicsElement(expr);
}

Expr GraphicsElement::to_pmath(BoxOutputFlags flags) {
  if(has(flags, BoxOutputFlags::LimitedDepth)) {
    if(Box::max_box_output_depth <= 0)
      return id().to_pmath();
    
    AutoValueReset<int> auto_mbod(Box::max_box_output_depth);
    --Box::max_box_output_depth;
    
    return to_pmath_impl(flags);
  }
  return to_pmath_impl(flags);
}

void GraphicsElement::request_repaint_all() {
  if(Box *owner = Box::find_nearest_box(this)) {
    owner->request_repaint_all();
  }
}

Expr GraphicsElement::prepare_dynamic(Expr expr) {
  StyledObject *prev = style_parent();
  
  while(dynamic_cast<GraphicsDirective*>(prev))
    prev = prev->style_parent();
  
  if(prev)
    return prev->prepare_dynamic(PMATH_CPP_MOVE(expr));
  
  return expr;
}

void GraphicsElement::next_in_limbo(ObjectWithLimbo *next) { 
  RICHMATH_ASSERT( _style_parent_or_limbo_next.is_normal() );
  _style_parent_or_limbo_next.set_to_tinted(next);
}

//} ...class GraphicsElement

//{ class GraphicsElementCollection ...

GraphicsElementCollection::GraphicsElementCollection(StyledObject *owner)
  : base()
{
  style_parent(owner);
}

GraphicsElementCollection::~GraphicsElementCollection()
{
  for(int i = 0; i < _items.length(); ++i)
    delete_owned(_items[i]);
}

bool GraphicsElementCollection::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_List))
    return false;
  
  int oldlen = _items.length();
  int newlen = (int)expr.expr_length();
  
  StyledObject *sty_par = this;
  for(int i = 0; i < newlen && i < oldlen; ++i) {
    Expr             elem_expr = expr[i + 1];
    GraphicsElement *elem      = _items[i];
    
    if(!elem->try_load_from_object(elem_expr, opts)) {
      delete_owned(elem);
      elem = GraphicsElement::create(elem_expr, opts);
      elem->style_parent(sty_par);
      _items.set(i, elem);
    }
    
    if(dynamic_cast<GraphicsDirectiveBase*>(elem))
      sty_par = elem;
  }
  
  for(int i = newlen; i < oldlen; ++i)
    delete_owned(_items[i]);
    
  _items.length(newlen);
  
  for(int i = oldlen; i < newlen; ++i) {
    Expr             elem_expr = expr[i + 1];
    GraphicsElement *elem      = GraphicsElement::create(elem_expr, opts);
    elem->style_parent(sty_par);
    _items.set(i, elem);
    
    if(dynamic_cast<GraphicsDirectiveBase*>(elem))
      sty_par = elem;
  }
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

void GraphicsElementCollection::load_from_object(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_List))
    expr = List(expr);
    
  try_load_from_object(PMATH_CPP_MOVE(expr), opts);
}

void GraphicsElementCollection::add(GraphicsElement *g) {
  RICHMATH_ASSERT(g != nullptr);
  RICHMATH_ASSERT(g->style_parent() == nullptr);
  
  StyledObject *sty_par;
  if(int len = _items.length()) {
    auto last_elem = _items[len - 1];
    sty_par = dynamic_cast<GraphicsDirectiveBase*>(last_elem) ? last_elem : last_elem->style_parent();
  }
  else
    sty_par = this;
  
  g->style_parent(sty_par);
  _items.add(g);
}

void GraphicsElementCollection::insert(int i, GraphicsElement *g) {
  RICHMATH_ASSERT(0 <= i);
  RICHMATH_ASSERT(i <= count());
  RICHMATH_ASSERT(g != nullptr);
  RICHMATH_ASSERT(g->style_parent() == nullptr);
  
  StyledObject *sty_par;
  if(i > 0) {
    auto last_elem = _items[i - 1];
    sty_par = dynamic_cast<GraphicsDirectiveBase*>(last_elem) ? last_elem : last_elem->style_parent();
  }
  else
    sty_par = this;
  
  g->style_parent(sty_par);
  _items.insert(i, 1, &g);
  
  if(i + 1 < _items.length()) {
    if(dynamic_cast<GraphicsDirectiveBase*>(g))
      _items[i + 1]->style_parent(g);
  }
}

void GraphicsElementCollection::remove(int i) {
  RICHMATH_ASSERT(0 <= i);
  RICHMATH_ASSERT(i < count());
  
  auto elem = _items[i];
  if(i + 1 < _items.length()) {
    auto next_elem = _items[i+1];
    
    if(next_elem->style_parent() == elem)
      next_elem->style_parent(elem->style_parent());
  }
  
  delete_owned(elem);
  _items.remove(i, 1);
}

void GraphicsElementCollection::find_extends(GraphicsBounds &bounds) {
  for(int i = 0; i < count(); ++i)
    item(i)->find_extends(bounds);
}

void GraphicsElementCollection::paint(GraphicsDrawingContext &gc) {
  gc.canvas().save();
  Color old_color       = gc.canvas().get_color();
  Length old_point_size = gc.point_size;
  
  for(int i = 0; i < count(); ++i)
    item(i)->paint(gc);
  
  gc.point_size = old_point_size;
  gc.canvas().set_color(old_color);
  gc.canvas().restore();
}

Expr GraphicsElementCollection::to_pmath_impl(BoxOutputFlags flags) {
  Expr result = MakeCall(Symbol(richmath_System_List), (size_t)count());
  
  for(int i = 0; i < count(); ++i)
    result.set(i+1, item(i)->to_pmath(flags));
    
  return result;
}

//} ... class GraphicsElementCollection
