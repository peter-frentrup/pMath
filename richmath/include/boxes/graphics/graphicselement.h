#ifndef RICHMATH__BOXES__GRAPHICS__GRAPHICSELEMENT_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__GRAPHICSELEMENT_H__INCLUDED

#include <util/array.h>
#include <util/interval.h>
#include <util/pmath-extra.h>
#include <util/double-point.h>
#include <util/styled-object.h>

#include <cairo.h>


namespace richmath {
  class GraphicsDrawingContext;
  enum class BoxOutputFlags;
  enum class BoxInputFlags;
  
  class GraphicsBounds {
    public:
      GraphicsBounds();
      
      bool is_finite() const;
      bool contains(const DoublePoint &p) const { return x_range.contains(p.x) && y_range.contains(p.y); }
      
      void add_point(double elem_x, double elem_y);
      void add_point(const DoublePoint &p) { add_point(p.x, p.y); }
      
    public:
      cairo_matrix_t elem_to_container;
      
      Interval<double> x_range;
      Interval<double> y_range;
  };
  
  class GraphicsElement: public StyledObject {
      friend class GraphicsElementCollection;
    public:
      virtual StyledObject *style_parent() final override { return _style_parent_or_limbo_next.as_normal(); }
      
      static GraphicsElement *create(Expr expr, BoxInputFlags opts);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) = 0;
      
      virtual void find_extends(GraphicsBounds &bounds) = 0;
      virtual void paint(GraphicsDrawingContext &gc) = 0;
      Expr to_pmath(BoxOutputFlags flags);
      
      void request_repaint_all();
      virtual void dynamic_updated() override { request_repaint_all(); }
      virtual Expr prepare_dynamic(Expr expr) override;
      
    protected:
      GraphicsElement();
      virtual ~GraphicsElement();
      void delete_owned(GraphicsElement *child) { 
        RICHMATH_ASSERT(child != nullptr);
        child->safe_destroy();
      }

      void finish_load_from_object(Expr expr) {}
      
      virtual Expr to_pmath_impl(BoxOutputFlags flags) = 0;
      
      void style_parent(StyledObject *sp) { if(_style_parent_or_limbo_next.is_normal()) _style_parent_or_limbo_next.set_to_normal(sp); }
      virtual ObjectWithLimbo *next_in_limbo() final override { return _style_parent_or_limbo_next.as_tinted(); }
      virtual void next_in_limbo(ObjectWithLimbo *next) final override;
      
    private:
      TintedPtr<StyledObject, ObjectWithLimbo> _style_parent_or_limbo_next;
  };
  
  class GraphicsDirectiveBase: public GraphicsElement {};
  
  class GraphicsElementCollection: public GraphicsElement {
      using base = GraphicsElement;
      friend class GraphicsBox;
    protected:
      virtual ~GraphicsElementCollection();
    public:
      GraphicsElementCollection(StyledObject *owner);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      void load_from_object(Expr expr, BoxInputFlags opts);
      
      int              count() {     return _items.length(); }
      GraphicsElement *item(int i) { return _items[i]; }
      
      void add(GraphicsElement *g);
      void insert(int i, GraphicsElement *g);
      void remove(int i);
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      virtual void paint(GraphicsDrawingContext &gc) override;
    
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      
    private:
      Array<GraphicsElement *> _items;
  };
}


#endif // RICHMATH__BOXES__GRAPHICS__GRAPHICSELEMENT_H__INCLUDED
