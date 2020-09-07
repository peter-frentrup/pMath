#include <boxes/graphics/graphicselement.h>

#include <boxes/graphics/graphicsdirective.h>
#include <boxes/graphics/pointbox.h>
#include <boxes/graphics/linebox.h>

#include <graphics/context.h>

#include <cmath>


#ifdef _MSC_VER
#  define isfinite(x)  (_finite(x))
#endif


using namespace richmath;
using namespace std;

extern pmath_symbol_t richmath_System_LineBox;
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
      
      virtual void paint(GraphicsBox *owner, Context &context) override {
      }
      
      virtual Expr to_pmath(BoxOutputFlags flags) override {
        return _expr;
      }
      
    private:
      Expr _expr;
  };
}

//{ class GraphicsBounds ...

GraphicsBounds::GraphicsBounds() {
  cairo_matrix_init_identity(&elem_to_container);
  
  xmin = ymin =  HUGE_VAL;
  xmax = ymax = -HUGE_VAL;
}

bool GraphicsBounds::is_finite() {
  return isfinite(xmin) && isfinite(ymin) && isfinite(xmax) && isfinite(ymax);
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
    if(elem_x < xmin) xmin = elem_x;
    if(elem_x > xmax) xmax = elem_x;
  }
  
  if(add_y) {
    if(elem_y < ymin) ymin = elem_y;
    if(elem_y > ymax) ymax = elem_y;
  }
}

//} ... class GraphicsBounds

//{ class GraphicsElement ...

GraphicsElement::GraphicsElement()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

GraphicsElement::~GraphicsElement() {
}

GraphicsElement *GraphicsElement::create(Expr expr, BoxInputFlags opts) {
  Expr head = expr[0];
  
  if(head == richmath_System_PointBox) {
    if(auto ge = PointBox::try_create(expr, opts))
      return ge;
  }
  
  if(head == richmath_System_LineBox) {
    if(auto ge = LineBox::try_create(expr, opts))
      return ge;
  }
  
  if(head == PMATH_SYMBOL_LIST) {
    auto coll = new GraphicsElementCollection;
    coll->load_from_object(expr, opts);
    return coll;
  }
  
  if(auto dir = GraphicsDirective::try_create(expr, opts)) 
    return dir;
  
  return new DummyGraphicsElement(expr);
}

//} ...class GraphicsElement

//{ class GraphicsElementCollection ...

GraphicsElementCollection::GraphicsElementCollection()
  : base()
{
}

GraphicsElementCollection::~GraphicsElementCollection()
{
  for(int i = 0; i < _items.length(); ++i)
    delete_owned(_items[i]);
}

bool GraphicsElementCollection::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != PMATH_SYMBOL_LIST)
    return false;
  
  int oldlen = _items.length();
  int newlen = (int)expr.expr_length();
  
  for(int i = 0; i < newlen && i < oldlen; ++i) {
    Expr             elem_expr = expr[i + 1];
    GraphicsElement *elem      = _items[i];
    
    if(!elem->try_load_from_object(elem_expr, opts)) {
      elem->safe_destroy();
      elem = GraphicsElement::create(elem_expr, opts);
      _items.set(i, elem);
    }
  }
  
  for(int i = newlen; i < oldlen; ++i)
    _items[i]->safe_destroy();
    
  _items.length(newlen);
  
  for(int i = oldlen; i < newlen; ++i) {
    Expr             elem_expr = expr[i + 1];
    GraphicsElement *elem      = GraphicsElement::create(elem_expr, opts);
    
    _items.set(i, elem);
  }
  
  finish_load_from_object(std::move(expr));
  return true;
}

void GraphicsElementCollection::load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != PMATH_SYMBOL_LIST)
    expr = List(expr);
    
  try_load_from_object(std::move(expr), opts);
}

void GraphicsElementCollection::add(GraphicsElement *g) {
  assert(g != nullptr);
  
  _items.add(g);
}

void GraphicsElementCollection::insert(int i, GraphicsElement *g) {
  assert(g != nullptr);
  
  assert(0 <= i);
  assert(i <= count());
  _items.insert(i, 1, &g);
}

void GraphicsElementCollection::remove(int i) {
  assert(0 <= i);
  assert(i < count());
  
  _items[i]->safe_destroy();
  _items.remove(i, 1);
}

void GraphicsElementCollection::find_extends(GraphicsBounds &bounds) {
  for(int i = 0; i < count(); ++i)
    item(i)->find_extends(bounds);
}

void GraphicsElementCollection::paint(GraphicsBox *owner, Context &context) {
  context.canvas().save();
  Color old_color = context.canvas().get_color();
  
  for(int i = 0; i < count(); ++i)
    item(i)->paint(owner, context);
  
  context.canvas().set_color(old_color);
  context.canvas().restore();
}

Expr GraphicsElementCollection::to_pmath(BoxOutputFlags flags) {
  Expr result = MakeList((size_t)count());
  
  for(int i = 0; i < count(); ++i)
    result.set(i+1, item(i)->to_pmath(flags));
    
  return result;
}

//} ... class GraphicsElementCollection
