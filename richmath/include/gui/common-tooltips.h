#ifndef RICHMATH__GUI__TOOLTIPS_H__INCLUDED
#define RICHMATH__GUI__TOOLTIPS_H__INCLUDED

#include <util/interval.h>
#include <util/style.h>
#include <graphics/rectangle.h>

namespace pmath {
  class Expr;
}

namespace richmath {
  class Document;
  class Stylesheet;
  
  namespace CommonTooltips {
    void load_content(Document *doc, const pmath::Expr &boxes, SharedPtr<Stylesheet> stylesheet);
    
    Interval<float> popup_placement_before(Interval<float> target, float size, Interval<float> outer);
    Interval<float> popup_placement_after( Interval<float> target, float size, Interval<float> outer);
    Interval<float> popup_placement_at(    Interval<float> target, float size, Interval<float> outer);
    
    RectangleF popup_placement(const RectangleF &target_rect, Vector2F size, ControlPlacementKind cpk, const RectangleF &monitor);
    RectangleF popup_placement(const RectangleF &target_rect, Vector2F size, ControlPlacementKind cpk);
  }
}

#endif // RICHMATH__GUI__TOOLTIPS_H__INCLUDED
