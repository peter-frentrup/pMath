#ifndef RICHMATH__BOXES__GRAPHICS__GRAPHICSELEMENT_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__GRAPHICSELEMENT_H__INCLUDED

#include <util/base.h>
#include <util/array.h>
#include <util/pmath-extra.h>

#include <cairo.h>


namespace richmath {
  class Context;
  class GraphicsBox;
  enum class BoxOutputFlags;
  enum class BoxInputFlags;
  
  class GraphicsBounds {
    public:
      GraphicsBounds();
      
      bool is_finite();
      
      void add_point(double elem_x, double elem_y);
      
    public:
      cairo_matrix_t elem_to_container;
      
      double xmin;
      double xmax;
      double ymin;
      double ymax;
  };
  
  class GraphicsElement: public Base {
    protected:
      virtual ~GraphicsElement();
      static void delete_owned(GraphicsElement *child) { assert(child != nullptr); delete child; }

    public:
      static GraphicsElement *create(Expr expr, BoxInputFlags opts);
      
      void safe_destroy() { delete this; }
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) = 0;
      
      virtual void find_extends(GraphicsBounds &bounds) = 0;
      virtual void paint(GraphicsBox *owner, Context &context) = 0;
      virtual Expr to_pmath(BoxOutputFlags flags) = 0;
      
    protected:
      GraphicsElement();
      
      void finish_load_from_object(Expr expr) {}
  };
  
  class GraphicsDirective: public GraphicsElement {
    protected:
      virtual ~GraphicsDirective();
    public:
      GraphicsDirective();
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      int              count() {     return _items.length(); }
      GraphicsElement *item(int i) { return _items[i]; }
      
      void add(GraphicsElement *g);
      void insert(int i, GraphicsElement *g);
      void remove(int i);
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      virtual void paint(GraphicsBox *owner, Context &context) override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
    private:
      Array<GraphicsElement *> _items;
  };
  
  class GraphicsElementCollection: public GraphicsDirective {
      using base = GraphicsDirective;
      friend class GraphicsBox;
    protected:
      virtual ~GraphicsElementCollection();
    public:
      GraphicsElementCollection();
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      void load_from_object(Expr expr, BoxInputFlags opts);
      
      virtual void paint(GraphicsBox *owner, Context &context) override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
  };
}


#endif // RICHMATH__BOXES__GRAPHICS__GRAPHICSELEMENT_H__INCLUDED
