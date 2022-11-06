#include <gui/common-tooltips.h>

#include <boxes/box-factory.h>
#include <gui/document.h>
#include <util/style.h>

#include <algorithm>
#include <math.h>

#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif

using namespace pmath;
using namespace richmath;

extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_ButtonBox;
extern pmath_symbol_t richmath_System_ButtonFrame;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_TemplateBox;

const Interval<float> InfiniteFloatInterval = {-HUGE_VALF, HUGE_VALF};
const RectangleF      InfiniteRectangleF = {InfiniteFloatInterval, InfiniteFloatInterval};

void CommonTooltips::load_content(
  Document              *doc, 
  const Expr            &boxes, 
  SharedPtr<Stylesheet>  stylesheet
) {
  if(stylesheet)
    doc->stylesheet(stylesheet);
  
  Section *section = nullptr;
  if(doc->length() > 0) {
    doc->remove(1, doc->length());
    section = doc->section(0);
  }
  
  Expr section_boxes = Call(
    Symbol(richmath_System_Section),
    Call(
      Symbol(richmath_System_BoxData),
      Call(
        Symbol(richmath_System_TemplateBox),
        List(boxes),
        String("TooltipContent"))),
    String("TooltipWindowSection"));
  
  if(!section) {
    section = BoxFactory::create_empty_section(section_boxes);
    doc->insert(0, section);
    if(!section->try_load_from_object(section_boxes, BoxInputFlags::Default))
      doc->swap(0, new ErrorSection(section_boxes))->safe_destroy();
  }
  else if(!section->try_load_from_object(section_boxes, BoxInputFlags::Default)) {
    section = BoxFactory::create_empty_section(section_boxes);
    doc->swap(0, section)->safe_destroy();
    if(!section->try_load_from_object(section_boxes, BoxInputFlags::Default))
      doc->swap(0, new ErrorSection(section_boxes))->safe_destroy();
  }
}

Interval<float> CommonTooltips::popup_placement_before(Interval<float> target, float size, Interval<float> outer) {
  return outer.intersect({target.from - size, target.from});
}

Interval<float> CommonTooltips::popup_placement_after(Interval<float> target, float size, Interval<float> outer) {
  return outer.intersect({target.to, target.to + size});
}

Interval<float> CommonTooltips::popup_placement_at(Interval<float> target, float size, Interval<float> outer) {
  return outer.snap({target.from, target.from + size});
}

RectangleF CommonTooltips::popup_placement(const RectangleF &target_rect, Vector2F size, ControlPlacementKind cpk, const RectangleF &monitor) {
  switch(cpk) {
    default:
    case ControlPlacementKindBottom:
      return {
        popup_placement_at(   target_rect.x_interval(), size.x, monitor.x_interval()),
        popup_placement_after(target_rect.y_interval(), size.y, monitor.y_interval()) };
        
    case ControlPlacementKindTop:
      return {
        popup_placement_at(    target_rect.x_interval(), size.x, monitor.x_interval()),
        popup_placement_before(target_rect.y_interval(), size.y, monitor.y_interval()) };
        
    case ControlPlacementKindLeft:
      return {
        popup_placement_before(target_rect.x_interval(), size.x, monitor.x_interval()),
        popup_placement_at(    target_rect.y_interval(), size.y, monitor.y_interval()) };
        
    case ControlPlacementKindRight:
      return {
        popup_placement_after(target_rect.x_interval(), size.x, monitor.x_interval()),
        popup_placement_at(   target_rect.y_interval(), size.y, monitor.y_interval()) };
  }
}

RectangleF CommonTooltips::popup_placement(const RectangleF &target_rect, Vector2F size, ControlPlacementKind cpk) {
  return popup_placement(target_rect, size, cpk, InfiniteRectangleF);
}

Side richmath::control_placement_side(ControlPlacementKind cpk) {
  switch(cpk) {
    case ControlPlacementKindLeft:   return Side::Left;
    case ControlPlacementKindRight:  return Side::Right;
    case ControlPlacementKindTop:    return Side::Top;
    default:
    case ControlPlacementKindBottom: return Side::Bottom;
  }
}
