#include <boxes/graphics/graphicsdrawingcontext.h>


using namespace richmath;

//{ class GraphicsDrawingContext ...

GraphicsDrawingContext::GraphicsDrawingContext(Box &owner, Context &context)
  : point_size(SymbolicSize::Automatic),
    plot_range_width(100),
    _owner{owner},
    _context{context},
    _initial_matrix{context.canvas().get_matrix()}
{
}

//} ... class GraphicsDrawingContext
