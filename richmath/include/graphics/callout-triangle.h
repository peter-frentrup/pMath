#ifndef RICHMATH__GRAPHICS__CALLOUT_TRIANGLE_H__INCLUDED
#define RICHMATH__GRAPHICS__CALLOUT_TRIANGLE_H__INCLUDED


#include <util/interval.h>
#include <graphics/rectangle.h>

namespace richmath {
  struct CalloutTriangle {
    float x_base_from;
    float x_base_to;
    float x_tip_projection;
    float y_tip_height;
    
    static CalloutTriangle ForBasePointingToTarget(const Interval<float> &base, const Interval<float> &target, float tip_size);
    static CalloutTriangle ForSideOfBasePointingToTarget(const RectangleF &base, Side side, const RectangleF &target, float tip_size, bool relative);
    
    void offset_x(float dx);
    void swap_x_base();
    
    void get_triangle_points(Point out_points[3], const RectangleF &base, Side side) const;
  };
}

#endif // RICHMATH__GRAPHICS__CALLOUT_TRIANGLE_H__INCLUDED
