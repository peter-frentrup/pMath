#ifndef RICHMATH__BOXES__GRAPHICS__GRAPHICSDRAWINGCONTEXT_H_INCLUDED
#define RICHMATH__BOXES__GRAPHICS__GRAPHICSDRAWINGCONTEXT_H_INCLUDED

#include <graphics/context.h>
#include <graphics/symbolic-length.h>

namespace richmath {  
  class GraphicsDrawingContext {
    public:
      GraphicsDrawingContext(Box &owner, Context &context);
      
      Box                  &box() { return _owner; }
      Context              &context() { return _context; }
      Canvas               &canvas() { return _context.canvas(); }
      const cairo_matrix_t &initial_matrix() { return _initial_matrix; }
      
    public:
      Length point_size;
      float  plot_range_width;
      
    private:
      Box            &_owner;
      Context        &_context;
      cairo_matrix_t  _initial_matrix;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__GRAPHICSDRAWINGCONTEXT_H_INCLUDED
