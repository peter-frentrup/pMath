#ifndef RICHMATH__GRAPHICS__BUFFER_H__INCLUDED
#define RICHMATH__GRAPHICS__BUFFER_H__INCLUDED

#include <graphics/canvas.h>
#include <util/sharedptr.h>


namespace richmath {
  class BoxSize;
  
  class Buffer final : public Shareable {
    public:
      Buffer(Canvas &dst, cairo_format_t format, const RectangleF &rect);
      Buffer(Canvas &dst, cairo_format_t format, const BoxSize &size);
      virtual ~Buffer();
      
      int width() {  return _width; }
      int height() { return _height; }
      
      // 0 on error, owned by buffer:
      Canvas          *canvas() {  return _canvas; }
      cairo_t         *cairo() {   return _cr; }
      cairo_surface_t *surface() { return _surface; }
      
      const cairo_matrix_t &user_to_device() { return u2d_matrix; }
      const cairo_matrix_t &device_to_user() { return d2u_matrix; }
      
      bool is_compatible(Canvas &dst);
      bool is_compatible(Canvas &dst, float w, float h);
      bool is_compatible(Canvas &dst, const BoxSize &size);
      bool paint(Canvas &dst);
      bool paint_with_alpha(Canvas &dst, float alpha);
      bool mask(Canvas &dst);
      
      bool clear();
      bool blend(SharedPtr<Buffer> buf1, SharedPtr<Buffer> buf2, double t);
      
      // currently only for CAIRO_FORMAT_A8 buffers:
      bool blur(float radius);
      
    private:
      void init(Canvas &dst, cairo_format_t format, RectangleF rect);
      
    private:
      int _width;
      int _height;
      
      Canvas          *_canvas;
      cairo_t         *_cr;
      cairo_surface_t *_surface;
      cairo_matrix_t   u2d_matrix;
      cairo_matrix_t   d2u_matrix;
  };
}

#endif // RICHMATH__GRAPHICS__BUFFER_H__INCLUDED
