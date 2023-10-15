#include <boxes/graphics/graphicsdrawingcontext.h>

#include <boxes/box.h>
#include <boxes/graphics/graphicsdirective.h>


using namespace richmath;

//{ class GraphicsDrawingContext ...

GraphicsDrawingContext::GraphicsDrawingContext(Box &owner, Context &context)
  : edge_color(Color::Black/*context.canvas().get_color()*/),
    edge_joinform(JoinFormMiter),
    edge_miter_limit(10.0f),
    edge_thickness(SymbolicSize::Automatic),
    point_size(SymbolicSize::Automatic),
    plot_range_width(100),
    draw_edges(false),
    _owner{owner},
    _context{context},
    _initial_matrix{context.canvas().get_matrix()}
{
}

void GraphicsDrawingContext::init_edgeform_from_style() {
  draw_edges     = _owner.get_own_style(DrawEdges,     false);
  edge_color     = _owner.get_own_style(EdgeColor,     Color::Black);
  edge_thickness = _owner.get_own_style(EdgeThickness, SymbolicSize::Automatic);
  
  enum JoinForm jf = JoinFormMiter; 
  float miter_limit = 10.0f;
  if(GraphicsDirective::decode_joinform(jf, miter_limit, _owner.get_own_style(JoinForm))) {
    edge_joinform    = jf;
    edge_miter_limit = miter_limit;
  }
}

void GraphicsDrawingContext::apply_thickness(Length thickness) {
  canvas().line_width(thickness.resolve(1.0f, LengthConversionFactors::ThicknessInPt, plot_range_width));
}

void GraphicsDrawingContext::fill_with_edgeform() {
  if(!draw_edges || !edge_color) {
    canvas().fill();
    return;
  }
  
  canvas().fill_preserve();
  canvas().save();
  Color c = canvas().get_color();
  canvas().set_color(edge_color);
  apply_thickness(edge_thickness);
  canvas().join_form(edge_joinform);
  if(edge_joinform == JoinFormMiter)
    canvas().miter_limit(edge_miter_limit);
  
  canvas().stroke();
  canvas().set_color(c);
  canvas().restore();
}

//} ... class GraphicsDrawingContext
