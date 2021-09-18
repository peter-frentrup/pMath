#ifndef RICHMATH__GRAPHICS__RECTANGLE_H__INCLUDED
#define RICHMATH__GRAPHICS__RECTANGLE_H__INCLUDED


namespace pmath {
  class Expr;
}

namespace richmath {
  class Canvas;
  class BoxRadius;
  class Point;
  
  class Vector2F final {
    public:
      Vector2F() : x(0), y(0) {}
      Vector2F(float x, float y) : x(x), y(y) {}
      
      explicit Vector2F(const Point &pt);
      
      Vector2F &operator+=(const Vector2F &other) { x+= other.x; y+= other.y; return *this; }
      Vector2F &operator-=(const Vector2F &other) { x-= other.x; y-= other.y; return *this; }
      Vector2F &operator*=(float factor) {        x*= factor;  y*= factor;  return *this; }
      Vector2F &operator/=(float divisor) {       x/= divisor; y/= divisor; return *this; }
      
      friend Vector2F operator+(Vector2F left, const Vector2F &right) { return left+= right; }
      friend Vector2F operator-(Vector2F left, const Vector2F &right) { return left-= right; }
      friend Vector2F operator-(Vector2F vec) { vec.x = -vec.x; vec.y = -vec.y; return vec; }
      friend Vector2F operator*(Vector2F vec, float factor) { return vec*= factor; }
      friend Vector2F operator*(float factor, Vector2F vec) { return vec*= factor; }
      friend Vector2F operator/(Vector2F vec, float divisor) { return vec/= divisor; }
      
      friend bool operator==(const Vector2F &left, const Vector2F &right) { return left.x == right.x && left.y == right.y; }
      friend bool operator!=(const Vector2F &left, const Vector2F &right) { return !(left == right); }
      
      float length();
      void pixel_align_distance(Canvas &canvas);
      
    public:
      float x;
      float y;
  };
  
  class Point final {
    public:
      Point(): x(0), y(0) {}
      Point(float x, float y): x(x), y(y) {}
      
      explicit Point(const Vector2F &vec) : x(vec.x), y(vec.y) {}
      
      Point &operator+=(const Vector2F &delta) { x += delta.x; y += delta.y; return *this; }
      Point &operator-=(const Vector2F &delta) { x -= delta.x; y -= delta.y; return *this; }
      
      friend Point operator+(Point pt, const Vector2F &vec) { return pt+= vec; }
      friend Point operator+(const Vector2F &vec, Point pt) { return pt+= vec; }
      friend Point operator-(Point pt, const Vector2F &vec) { return pt-= vec; }
      friend Vector2F operator-(const Point &left, const Point &right) { return Vector2F(left) -= Vector2F(right); }
      
      friend bool operator==(const Point &left, const Point &right) { return left.x == right.x && left.y == right.y; }
      friend bool operator!=(const Point &left, const Point &right) { return !(left == right); }
      
      void pixel_align_point(Canvas &canvas, bool tostroke);
      
    public:
      float x;
      float y;
  };
  
  inline Vector2F::Vector2F(const Point &pt) : x(pt.x), y(pt.y) {}
  
  class RectangleF final {
    public:
      RectangleF()
        : x(0), y(0), width(0), height(0)
      {}
      
      RectangleF(float x, float y, float w, float h)
        : x(x), y(y), width(w), height(h)
      {}
      
      RectangleF(const Point &p1, const Point &p2)
        : x(p1.x), y(p1.y), width(p2.x - p1.x), height(p2.y - p1.y)
      {}
      
      RectangleF(const Point &pos, const Vector2F &size)
        : x(pos.x), y(pos.y), width(size.x), height(size.y)
      {}
      
      RectangleF &operator+=(const Vector2F &delta) { x+= delta.x; y+= delta.y; return *this; }
      RectangleF &operator-=(const Vector2F &delta) { x-= delta.x; y-= delta.y; return *this; }
      
      friend RectangleF operator+(RectangleF rect, const Vector2F &vec) { return rect+= vec; }
      friend RectangleF operator-(RectangleF rect, const Vector2F &vec) { return rect-= vec; }
      
      void normalize();
      void normalize_to_zero();
      void pixel_align(Canvas &canvas, bool tostroke, int direction = 0); // -1 = inside, 0 = nearest, +1 = outside
      
      bool is_positive() const { return width > 0 && height > 0; }
      bool is_empty() const { return width == 0 || height == 0; }
      
      float left()   const { return x; }
      float right()  const { return x + width; }
      float top()    const { return y; }
      float bottom() const { return y + height; }
      
      Point top_left()     const { return Point(left(),  top()); }
      Point top_right()    const { return Point(right(), top()); }
      Point bottom_left()  const { return Point(left(),  bottom()); }
      Point bottom_right() const { return Point(right(), bottom()); }
      Point center() const { return top_left() + 0.5f * size(); }
      Vector2F size() const { return Vector2F(width, height); }
      
      bool overlaps(const RectangleF &other) const {
        return right() >= other.left() && left() <= other.right() &&
               bottom() >= other.top() && top() <= other.bottom();
      }
      
      bool overlaps(const RectangleF &other, float epsilon) const {
        return right() + epsilon >= other.left() && left() - epsilon <= other.right() &&
               bottom() + epsilon >= other.top() && top() - epsilon <= other.bottom();
      }
      
      bool contains(const RectangleF &other) const {
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
      
      void grow(float delta) {           grow(delta, delta); }
      void grow(const Vector2F &delta) { grow(delta.x, delta.y); }
      void grow(float dx, float dy);
      RectangleF enlarged_by(float dx, float dy) const { RectangleF rect = *this; rect.grow(dx, dy); return rect; }
      
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
  class BoxRadius final {
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
