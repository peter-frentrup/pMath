#ifndef __BOXES__GRAPHICS__GRAPHICSELEMENT_H__
#define __BOXES__GRAPHICS__GRAPHICSELEMENT_H__

#include <util/base.h>
#include <util/array.h>
#include <util/pmath-extra.h>


namespace richmath {
  class Context;
  
  class GraphicsElement: public Base {
    public:
      static GraphicsElement *create(Expr expr, int opts);
      
      virtual ~GraphicsElement();
      
      virtual void paint(Context *context) = 0;
      virtual Expr to_pmath(int flags) = 0; // BoxFlagXXX
    
    protected:
      GraphicsElement();
  };
  
  class GraphicsElementCollection: public GraphicsElement {
    public:
      GraphicsElementCollection();
      void load_from_object(Expr expr, int opts);
      
      virtual ~GraphicsElementCollection();
      
      int              count(){     return _items.length(); }
      GraphicsElement *item(int i){ return _items[i]; }
      
      void add(GraphicsElement *g);
      void insert(int i, GraphicsElement *g);
      void remove(int i);
      
      virtual void paint(Context *context);
      virtual Expr to_pmath(int flags); // BoxFlagXXX
      
    private:
      Array<GraphicsElement*> _items;
  };
}


#endif // __BOXES__GRAPHICS__GRAPHICSELEMENT_H__
