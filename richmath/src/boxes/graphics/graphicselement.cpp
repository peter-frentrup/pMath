#include <boxes/graphics/graphicselement.h>

#include <boxes/graphics/colorbox.h>
#include <boxes/graphics/pointbox.h>
#include <boxes/graphics/linebox.h>

#include <graphics/context.h>

#include <cmath>


#ifdef _MSC_VER
#  define isfinite(x)  (_finite(x))
#endif


using namespace richmath;
using namespace std;

namespace {
  class DummyGraphicsElement: public GraphicsElement {
    public:
      DummyGraphicsElement(Expr expr)
        : GraphicsElement(),
          _expr(expr)
      {
      }
      
      virtual bool try_load_from_object(Expr expr, int opts) override {
        return false;
      }
      
      virtual void find_extends(GraphicsBounds &bounds) override {
      }
      
      virtual void paint(GraphicsBoxContext *context) override {
      }
      
      virtual Expr to_pmath(int flags) override { // BoxFlagXXX
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
}

GraphicsElement::~GraphicsElement() {
}

GraphicsElement *GraphicsElement::create(Expr expr, int opts) {
  Expr head = expr[0];
  
  if(head == PMATH_SYMBOL_POINTBOX) {
    GraphicsElement *ge = PointBox::create(expr, opts);
    
    if(ge)
      return ge;
  }
  
  if(head == PMATH_SYMBOL_LINEBOX) {
    GraphicsElement *ge = LineBox::create(expr, opts);
    
    if(ge)
      return ge;
  }
  
  if( head == PMATH_SYMBOL_RGBCOLOR  ||
      head == PMATH_SYMBOL_HUE       ||
      head == PMATH_SYMBOL_GRAYLEVEL)
  {
    GraphicsElement *ge = ColorBox::create(expr, opts);
    
    if(ge)
      return ge;
  }
  
  if(head == PMATH_SYMBOL_LIST) {
    GraphicsElementCollection *coll = new GraphicsElementCollection;
    coll->load_from_object(expr, opts);
    return coll;
  }
  
  if(head == PMATH_SYMBOL_DIRECTIVE) {
    GraphicsDirective *dir = new GraphicsDirective;
    
    if(dir->try_load_from_object(expr, opts))
      return dir;
      
    delete dir;
  }
  
  return new DummyGraphicsElement(expr);
}

//} ...class GraphicsElement

//{ class GraphicsDirective ...

GraphicsDirective::GraphicsDirective()
  : GraphicsElement()
{
}

GraphicsDirective::~GraphicsDirective()
{
  for(int i = 0; i < _items.length(); ++i)
    delete _items[i];
}

bool GraphicsDirective::try_load_from_object(Expr expr, int opts) {
  if(expr[0] != PMATH_SYMBOL_DIRECTIVE)
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
  
  return true;
}

void GraphicsDirective::add(GraphicsElement *g) {
  assert(g != nullptr);
  
  _items.add(g);
}

void GraphicsDirective::insert(int i, GraphicsElement *g) {
  assert(g != nullptr);
  
  assert(0 <= i);
  assert(i <= count());
  _items.insert(i, 1, &g);
}

void GraphicsDirective::remove(int i) {
  assert(0 <= i);
  assert(i < count());
  
  _items[i]->safe_destroy();
  _items.remove(i, 1);
}

void GraphicsDirective::find_extends(GraphicsBounds &bounds) {
  for(int i = 0; i < count(); ++i)
    item(i)->find_extends(bounds);
}

void GraphicsDirective::paint(GraphicsBoxContext *context) {
  for(int i = 0; i < count(); ++i)
    item(i)->paint(context);
}

Expr GraphicsDirective::to_pmath(int flags) { // BoxFlagXXX
  Gather g;
  
  for(int i = 0; i < count(); ++i)
    Gather::emit(item(i)->to_pmath(flags));
    
  Expr e = g.end();
  e.set(0, Symbol(PMATH_SYMBOL_DIRECTIVE));
  return e;
}

//} ... class GraphicsDirective

//{ class GraphicsElementCollection ...

GraphicsElementCollection::GraphicsElementCollection()
  : GraphicsDirective()
{
}

GraphicsElementCollection::~GraphicsElementCollection()
{
}

bool GraphicsElementCollection::try_load_from_object(Expr expr, int opts) {
  if(expr[0] != PMATH_SYMBOL_LIST)
    return false;
    
  expr.set(0, Symbol(PMATH_SYMBOL_DIRECTIVE));
  return GraphicsDirective::try_load_from_object(expr, opts);
}

void GraphicsElementCollection::load_from_object(Expr expr, int opts) {
  if(expr[0] == PMATH_SYMBOL_LIST)
    expr.set(0, Symbol(PMATH_SYMBOL_DIRECTIVE));
  else
    expr = Call(Symbol(PMATH_SYMBOL_DIRECTIVE), expr);
    
  GraphicsDirective::try_load_from_object(expr, opts);
}

void GraphicsElementCollection::paint(GraphicsBoxContext *context) {
  context->ctx->canvas->save();
  int old_color = context->ctx->canvas->get_color();
  
  GraphicsDirective::paint(context);
  
  context->ctx->canvas->set_color(old_color);
  context->ctx->canvas->restore();
}

Expr GraphicsElementCollection::to_pmath(int flags) { // BoxFlagXXX
  Expr e = GraphicsDirective::to_pmath(flags);
  e.set(0, Symbol(PMATH_SYMBOL_LIST));
  return e;
}

//} ... class GraphicsElementCollection
