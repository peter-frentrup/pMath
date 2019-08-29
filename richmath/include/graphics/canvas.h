#ifndef RICHMATH__GRAPHICS__CANVAS_H__INCLUDED
#define RICHMATH__GRAPHICS__CANVAS_H__INCLUDED

#include <graphics/color.h>
#include <graphics/fonts.h>

namespace richmath {
  class Base;
  class Rectangle;
  
  static const float SelectionAlpha               = 0.4f;
  static const Color SelectionFillColor           = Color::from_rgb24(0x6699FF);
  static const Color SelectionBorderColor         = Color::from_rgb24(0x0000FF);
  static const Color InactiveSelectionFillColor   = Color::from_rgb24(0xA0A0A0);
  static const Color InactiveSelectionBorderColor = Color::from_rgb24(0x000000);
  
  class Canvas: public Base {
    public:
      Canvas(cairo_t *cr);
      ~Canvas();
      
      static void transform_point(
        const cairo_matrix_t &m,
        float *x, float *y);
        
      static void transform_rect(
        const cairo_matrix_t &m,
        float *x, float *y, float *w, float *h);
        
      cairo_t *cairo() { return _cr; }
      cairo_surface_t *target() { return cairo_get_target(_cr); }
      
      void save();
      void restore();
      
      bool has_current_pos();
      void current_pos(float *x, float *y);
      void current_pos(double *x, double *y);
      void user_to_device(float *x, float *y);
      void user_to_device(double *x, double *y);
      void user_to_device_dist(float *dx, float *dy);
      void user_to_device_dist(double *dx, double *dy);
      void device_to_user(float *x, float *y);
      void device_to_user(double *x, double *y);
      void device_to_user_dist(float *dx, float *dy);
      void device_to_user_dist(double *dx, double *dy);
      cairo_matrix_t get_matrix();
      void set_matrix(const cairo_matrix_t &mat);
      void reset_matrix();
      void transform(const cairo_matrix_t &mat);
      void translate(double tx, double ty);
      void rotate(double angle);
      void scale(double sx, double sy);
      
      void set_color(Color color, float alpha = 1.0f);
      Color get_color() { return _color; }
      
      void set_font_face(FontFace font);
      void set_font_size(float size);
      float get_font_size() { return _font_size; }
      
      void move_to(double x, double y);
      void rel_move_to(double x, double y);
      void line_to(double x, double y);
      void rel_line_to(double x, double y);
      
      void arc(
        double x, double y,
        double radius,
        double angle1,
        double angle2,
        bool negative);
        
      void ellipse_arc(
        double x, double y,
        double radius_x,
        double radius_y,
        double angle1,
        double angle2,
        bool negative);
        
      void close_path();
      
      void align_point(float *x, float *y, bool tostroke);
      
      void pixrect(float x1, float y1, float x2, float y2, bool tostroke);
      
      void show_blur_rect(
        float x1, float y1,
        float x2, float y2,
        float radius);
        
      void show_blur_line(float x1, float y1, float x2, float y2, float radius);
      
      void show_blur_arc(
        float x, float y,
        float radius,
        float angle1,
        float angle2,
        bool negative);
        
      void show_blur_stroke(float radius, bool preserve);
      
      void glyph_extents(const cairo_glyph_t *glyphs, int num_glyphs, cairo_text_extents_t *extents);
      void show_glyphs(const cairo_glyph_t *glyphs, int num_glyphs);
      
      void new_path();
      void new_sub_path();
      void clip();
      void clip_preserve();
      void fill();
      void fill_preserve();
      void hair_stroke();
      void stroke();
      void stroke_preserve();
      void clip_extents(Rectangle *rect);
      void clip_extents(float *x1, float *y1, float *x2, float *y2);
      void clip_extents(double *x1, double *y1, double *x2, double *y2);
      void path_extents(Rectangle *rect);
      void path_extents(float *x1, float *y1, float *x2, float *y2);
      void path_extents(double *x1, double *y1, double *x2, double *y2);
      void stroke_extents(float *x1, float *y1, float *x2, float *y2);
      void stroke_extents(double *x1, double *y1, double *x2, double *y2);
      
      void paint();
      void paint_with_alpha(float alpha);
      
    public:
      bool pixel_device;
      bool glass_background;
      
      bool  native_show_glyphs;
      bool  show_only_text;
      
    private:
      cairo_t           *_cr;
      float              _font_size;
      Color              _color;
  };
  
  class CanvasAutoSave {
    public:
      CanvasAutoSave(Canvas *_canvas)
      : canvas(_canvas) 
      {
        canvas->save();
      }
      
      ~CanvasAutoSave() {
        canvas->restore();
      }
    
    private:
      Canvas *canvas;
  };
  
  typedef AutoRefBase < cairo_surface_t, cairo_surface_reference, cairo_surface_destroy > AutoCairoSurface;


#if CAIRO_HAS_WIN32_SURFACE
  HDC safe_cairo_win32_surface_get_dc(cairo_surface_t *surface);
#endif

}

#endif // RICHMATH__GRAPHICS__CANVAS_H__INCLUDED
