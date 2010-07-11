#ifndef __GRAPHICS__CANVAS_H__
#define __GRAPHICS__CANVAS_H__

#include <graphics/fonts.h>

namespace richmath{
  class Base;
  
  static const float SelectionAlpha = 0.4f;
  static const int SelectionColor = 0x6699FF;
  
  class Point{
    public:
      Point(){}
      Point(float _x, float _y): x(_x), y(_y){}
      
      float x;
      float y;
  };
  
  typedef enum{  // PD = 00 01 10 11
    BitOp0 = 0,  //       0  0  0  0
    BitOpDPon,   //       1  0  0  0
    BitOpDPna,   //       0  1  0  0
    BitOpPn,     //       1  1  0  0
    BitOpPDna,   //       0  0  1  0
    BitOpDn,     //       1  0  1  0
    BitOpDPx,    //       0  1  1  0
    BitOpDPan,   //       1  1  1  0
    BitOpDPa,    //       0  0  0  1
    BitOpDPxn,   //       1  0  0  1
    BitOpD,      //       0  1  0  1
    BitOpDPno,   //       1  1  0  1
    BitOpP,      //       0  0  1  1
    BitOpPDno,   //       1  0  1  1
    BitOpDPo,    //       0  1  1  1
    BitOp1       //       1  1  1  1
  }BitOperations;

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
      
      cairo_t *cairo(){ return _cr; }
      cairo_surface_t *target(){ return cairo_get_target(_cr); }
      
      void save();
      void restore();
      
      void current_pos(float *x, float *y);
      void user_to_device(float *x, float *y);
      void user_to_device_dist(float *dx, float *dy);
      void device_to_user(float *x, float *y);
      void device_to_user_dist(float *dx, float *dy);
      void transform(const cairo_matrix_t &mat);
      void translate(float tx, float ty);
      void rotate(float angle);
      void scale(float sx, float sy);
      
      void set_color(int color, float alpha = 1.0f); // 0xRRGGBB
      int get_color(); // 0xRRGGBB
      
      void set_font_face(FontFace font);
      void set_font_size(float size);
      float get_font_size(){ return _font_size; }
      void reset_font_cache(){ _font = 0; }
      
      void move_to(    float x, float y);
      void rel_move_to(float x, float y);
      void line_to(    float x, float y);
      void rel_line_to(float x, float y);
      
      void arc(
        float x, float y, 
        float radius, 
        float angle1, 
        float angle2,
        bool negative);
      
      void close_path();
      
      void align_point(float *x, float *y, bool tostroke);
      
      void pixrect(float x1, float y1, float x2, float y2, bool tostroke);
      void pixframe(float x1, float y1, float x2, float y2, float thickness);
      
//      void paint_selection(
//        float x1, float y1, float x2, float y2, 
//        bool rect = true);
      
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
      
      void show_glyphs(const cairo_glyph_t *glyphs, int num_glyphs);
      
      void new_path();
      void clip();
      void clip_preserve();
      void fill();
      void fill_preserve();
      void bitop_fill(BitOperations op, int pen_color);
      void bitop_fill_preserve(BitOperations op, int pen_color);
      void hair_stroke();
      void stroke();
      void stroke_preserve();
      
      void paint();
      void paint_with_alpha(float alpha);
    
    public:
      bool pixel_device;
      bool glass_background;
      
      bool  native_show_glyphs;
      bool  show_only_text;
      
    private:
      cairo_t           *_cr;
      cairo_font_face_t *_font;
      float              _font_size;
      int                _color;
  };
}

#endif // __GRAPHICS__CANVAS_H__
