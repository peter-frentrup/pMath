#ifndef RICHMATH__BOXES__GRAPHICS__GRAPHICSDRAWINGCONTEXT_H_INCLUDED
#define RICHMATH__BOXES__GRAPHICS__GRAPHICSDRAWINGCONTEXT_H_INCLUDED

#include <graphics/context.h>
#include <graphics/symbolic-length.h>

namespace richmath {  
  class GraphicsDrawingContext {
    public:
      GraphicsDrawingContext(Box &owner, Context &context);
      
      void init_edgeform_from_style();
      
      Box                  &box() { return _owner; }
      Context              &context() { return _context; }
      Canvas               &canvas() { return _context.canvas(); }
      const cairo_matrix_t &initial_matrix() { return _initial_matrix; }
      
      void apply_thickness(Length thickness);
      void fill_with_edgeform();
      
    public:
      Array<double> edge_dash_array;
      double        edge_dash_offset;
      enum CapForm  edge_capform;
      Color         edge_color;
      enum JoinForm edge_joinform;
      float         edge_miter_limit;
      Length        edge_thickness;
      Length        point_size;
      float         plot_range_width;
      
      bool draw_edges: 1;
      
    private:
      Box            &_owner;
      Context        &_context;
      cairo_matrix_t  _initial_matrix;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__GRAPHICSDRAWINGCONTEXT_H_INCLUDED
