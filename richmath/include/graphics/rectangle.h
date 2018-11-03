#ifndef RICHMATH__GRAPHICS__RECTANGLE_H__INCLUDED
#define RICHMATH__GRAPHICS__RECTANGLE_H__INCLUDED


namespace pmath {
  class Expr;
}

namespace richmath {
  class Canvas;
  class BoxRadius;
  
  class Point {
    public:
      Point(): x(0), y(0) {}
      Point(float _x, float _y): x(_x), y(_y) {}
      
      Point &operator+= (const Point &delta) {
        x += delta.x;
        y += delta.y;
        return *this;
      }
      
      Point &operator-= (const Point &delta) {
        x -= delta.x;
        y -= delta.y;
        return *this;
      }
      
      Point &operator*= (float m) {
        x *= m;
        y *= m;
        return *this;
      }
      
      Point &operator/= (float m) {
        x /= m;
        y /= m;
        return *this;
      }
      
      void pixel_align_point(Canvas &canvas, bool tostroke);
      void pixel_align_distance(Canvas &canvas);
      
    public:
      float x;
      float y;
  };
  
  class Rectangle {
    public:
      Rectangle()
        : x(0), y(0), width(0), height(0)
      {}
      
      Rectangle(float _x, float _y, float _w, float _h)
        : x(_x), y(_y), width(_w), height(_h)
      {}
      
      Rectangle(const Point &p1, const Point &p2)
        : x(p1.x), y(p1.y), width(p2.x - p1.x), height(p2.y - p1.y)
      {}
      
      void normalize();
      void normalize_to_zero();
      void pixel_align(Canvas &canvas, bool tostroke, int direction = 0); // -1 = inside, 0 = nearest, +1 = outside
      
      bool is_positive() const { return width > 0 && height > 0; }
      bool is_empty() const { return width == 0 || height == 0; }
      
      bool contains(const Rectangle &other) const {
        return x <= other.x &&
               y <= other.y &&
               other.x + other.width  <= x + width &&
               other.y + other.height <= y + height;
      }
      
      bool contains(const Point &pt) const {
        return contains(pt.x, pt.y);
      }
      
      bool contains(float px, float py) const {
        return x <= px && y <= py && px <= x + width && py <= y + height;
      }
      
      void grow(float delta) {         grow(delta, delta); }
      void grow(const Point &delta) { grow(delta.x, delta.y); }
      void grow(float dx, float dy);
      
      // radii: assuming Y axis goes "down", X axis goes "right", all is normalized
      void add_round_rect_path( Canvas &canvas, const BoxRadius &radii, bool negative = false) const;
      
      void add_rect_path(Canvas &canvas, bool negative = false) const;
      
    public:
      float x;
      float y;
      float width;
      float height;
  };
  
  /// A CSS3 Border Radius
  class BoxRadius {
    public:
      BoxRadius(float all = 0);
      BoxRadius(float all_x, float all_y);
      BoxRadius(float tl, float tr, float br, float bl);
      BoxRadius(float tl_x, float tl_y, float tr_x, float tr_y, float br_x, float br_y, float bl_x, float bl_y);
      BoxRadius(const pmath::Expr &expr);
      
      BoxRadius &operator+=(const BoxRadius &other);
      
      void normalize(float max_width, float max_height);
      
    public:
      float top_left_x;
      float top_left_y;
      float top_right_x;
      float top_right_y;
      float bottom_right_x;
      float bottom_right_y;
      float bottom_left_x;
      float bottom_left_y;
  };
  
}


#endif // RICHMATH__GRAPHICS__RECTANGLES_H__INCLUDED
