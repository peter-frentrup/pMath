#include <boxes/graphics/graphicselement.h>

#include <boxes/graphics/colorbox.h>
#include <boxes/graphics/linebox.h>

#include <graphics/context.h>

#include <cmath>


using namespace richmath;

namespace {
  class DummyGraphicsElement: public GraphicsElement {
    public:
      DummyGraphicsElement(Expr expr)
        : GraphicsElement(),
        _expr(expr)
      {
      }
      
      virtual void find_extends(GraphicsBounds &bounds) {
      }
      
      virtual void paint(Context *context) {
      }
      
      virtual Expr to_pmath(int flags) { // BoxFlagXXX
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

void GraphicsBounds::add_point(double elem_x, double elem_y) {
  cairo_matrix_transform_point(&elem_to_container, &elem_x, &elem_y);
  
  if(elem_x < xmin) xmin = elem_x;
  if(elem_x > xmax) xmax = elem_x;
  
  if(elem_y < ymin) ymin = elem_y;
  if(elem_y > ymax) ymax = elem_y;
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
  
  return new DummyGraphicsElement(expr);
}

//} ...class GraphicsElement

//{ class GraphicsElementCollection ...

GraphicsElementCollection::GraphicsElementCollection()
  : GraphicsElement()
{
}

GraphicsElementCollection::~GraphicsElementCollection()
{
  for(int i = 0; i < _items.length(); ++i)
    delete _items[i];
}

void GraphicsElementCollection::load_from_object(Expr expr, int opts) {
  if(!expr.is_null() && expr[0] != PMATH_SYMBOL_LIST)
    expr = List(expr);
    
  for(int i = 0; i < _items.length(); ++i)
    delete _items[i];
    
  _items.length((int)expr.expr_length());
  
  for(int i = 0; i < _items.length(); ++i) {
    _items.set(i, GraphicsElement::create(expr[i + 1], opts));
  }
}

void GraphicsElementCollection::add(GraphicsElement *g) {
  assert(g != NULL);
  
  _items.add(g);
}

void GraphicsElementCollection::insert(int i, GraphicsElement *g) {
  assert(g != NULL);
  
  assert(0 <= i);
  assert(i <= count());
  _items.insert(i, 1, &g);
}

void GraphicsElementCollection::remove(int i) {
  assert(0 <= i);
  assert(i < count());
  
  delete _items[i];
  _items.remove(i, 1);
}

void GraphicsElementCollection::find_extends(GraphicsBounds &bounds) {
  for(int i = 0; i < count(); ++i)
    item(i)->find_extends(bounds);
}

void GraphicsElementCollection::paint(Context *context) {
  context->canvas->save();
  int c = context->canvas->get_color();
  
  for(int i = 0; i < count(); ++i)
    item(i)->paint(context);
    
  context->canvas->set_color(c);
  context->canvas->restore();
}

Expr GraphicsElementCollection::to_pmath(int flags) { // BoxFlagXXX
  Gather g;
  
  for(int i = 0; i < count(); ++i)
    Gather::emit(item(i)->to_pmath(flags));
    
  return g.end();
}

//} ... class GraphicsElementCollection
