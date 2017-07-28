#ifndef __BOXES__GRAPHICS__GRAPHICSELEMENT_H__
#define __BOXES__GRAPHICS__GRAPHICSELEMENT_H__

#include <util/base.h>
#include <util/array.h>
#include <util/pmath-extra.h>

#include <cairo.h>


namespace richmath {
  class Context;
  class GraphicsBox;
  enum class BoxFlags;
  enum class BoxOptions;
  
  class GraphicsBoxContext: public Base {
    public:
      GraphicsBoxContext(GraphicsBox *_box, Context *_ctx)
        : Base(),
          box(_box),
          ctx(_ctx)
      {
        SET_BASE_DEBUG_TAG(typeid(*this).name());
      }
      
    public:
      GraphicsBox *box;
      Context     *ctx;
  };
  
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
    public:
      static GraphicsElement *create(Expr expr, BoxOptions opts);
      virtual ~GraphicsElement();
      
      void safe_destroy() { delete this; }
      
      virtual bool try_load_from_object(Expr expr, BoxOptions opts) = 0;
      
      virtual void find_extends(GraphicsBounds &bounds) = 0;
      virtual void paint(GraphicsBoxContext *context) = 0;
      virtual Expr to_pmath(BoxFlags flags) = 0;
      
    protected:
      GraphicsElement();
  };
  
  class GraphicsDirective: public GraphicsElement {
    public:
      GraphicsDirective();
      virtual ~GraphicsDirective();
      
      virtual bool try_load_from_object(Expr expr, BoxOptions opts) override;
      
      int              count() {     return _items.length(); }
      GraphicsElement *item(int i) { return _items[i]; }
      
      void add(GraphicsElement *g);
      void insert(int i, GraphicsElement *g);
      void remove(int i);
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      virtual void paint(GraphicsBoxContext *context) override;
      virtual Expr to_pmath(BoxFlags flags) override; // BoxFlagXXX
      
    private:
      Array<GraphicsElement *> _items;
  };
  
  class GraphicsElementCollection: public GraphicsDirective {
    public:
      GraphicsElementCollection();
      virtual ~GraphicsElementCollection();
      
      virtual bool try_load_from_object(Expr expr, BoxOptions opts) override;
      void load_from_object(Expr expr, BoxOptions opts);
      
      virtual void paint(GraphicsBoxContext *context) override;
      virtual Expr to_pmath(BoxFlags flags) override; // BoxFlagXXX
  };
}


#endif // __BOXES__GRAPHICS__GRAPHICSELEMENT_H__
