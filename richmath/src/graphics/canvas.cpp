#include <graphics/canvas.h>

#include <graphics/rectangle.h>

#include <cmath>

#ifdef _WIN32
#  include <Windows.h>
#  include <cairo-win32.h>
#endif


using namespace richmath;

//{ class Canvas ...

Canvas::Canvas(cairo_t *cr)
  : Base(),
    pixel_device(true),
    glass_background(false),
    native_show_glyphs(true),
    show_only_text(false),
    _cr(cr),
    _font_size(10),
    _color(Color::Black)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

Canvas::~Canvas() {
}

void Canvas::transform_point(
  const cairo_matrix_t &m,
  float *x, float *y
) {
  double dx = *x;
  double dy = *y;
  
  cairo_matrix_transform_point(&m, &dx, &dy);
  
  *x = dx;
  *y = dy;
}

void Canvas::transform_rect(
  const cairo_matrix_t &m,
  float *x, float *y, float *w, float *h
) {
  double x1 = *x;
  double y1 = *y;
  double x2 = *x + *w;
  double y2 = *y;
  double x3 = *x + *w;
  double y3 = *y + *h;
  double x4 = *x;
  double y4 = *y + *h;
  
  cairo_matrix_transform_point(&m, &x1, &y1);
  cairo_matrix_transform_point(&m, &x2, &y2);
  cairo_matrix_transform_point(&m, &x3, &y3);
  cairo_matrix_transform_point(&m, &x4, &y4);
  
  *x = x1;
  if(*x > x2) *x = x2;
  if(*x > x3) *x = x3;
  if(*x > x4) *x = x4;
  
  if(x1 < x2) x1 = x2;
  if(x1 < x3) x1 = x3;
  if(x1 < x4) x1 = x4;
  
  *w = x1 - *x;
  
  *y = y1;
  if(*y > y2) *y = y2;
  if(*y > y3) *y = y3;
  if(*y > y4) *y = y4;
  
  if(y1 < y2) y1 = y2;
  if(y1 < y3) y1 = y3;
  if(y1 < y4) y1 = y4;
  
  *h = y1 - *y;
}

void Canvas::save() {
  cairo_save(_cr);
}

void Canvas::restore() {
  cairo_restore(_cr);
}

bool Canvas::has_current_pos() {
  return cairo_has_current_point(_cr);
}

void Canvas::current_pos(float *x, float *y) {
  double dx, dy;
  cairo_get_current_point(_cr, &dx, &dy);
  *x = dx;
  *y = dy;
}

void Canvas::current_pos(double *x, double *y) {
  cairo_get_current_point(_cr, x, y);
}

void Canvas::user_to_device(float *x, float *y) {
  double dx = *x;
  double dy = *y;
  cairo_user_to_device(_cr, &dx, &dy);
  *x = dx;
  *y = dy;
}

void Canvas::user_to_device(double *x, double *y) {
  cairo_user_to_device(_cr, x, y);
}

void Canvas::user_to_device_dist(float *dx, float *dy) {
  double ddx = *dx;
  double ddy = *dy;
  cairo_user_to_device_distance(_cr, &ddx, &ddy);
  *dx = ddx;
  *dy = ddy;
}

void Canvas::user_to_device_dist(double *dx, double *dy) {
  cairo_user_to_device_distance(_cr, dx, dy);
}

void Canvas::device_to_user(float *x, float *y) {
  double dx = *x;
  double dy = *y;
  cairo_device_to_user(_cr, &dx, &dy);
  *x = dx;
  *y = dy;
}

void Canvas::device_to_user(double *x, double *y) {
  cairo_device_to_user(_cr, x, y);
}

void Canvas::device_to_user_dist(float *dx, float *dy) {
  double ddx = *dx;
  double ddy = *dy;
  cairo_device_to_user_distance(_cr, &ddx, &ddy);
  *dx = ddx;
  *dy = ddy;
}

void Canvas::device_to_user_dist(double *dx, double *dy) {
  cairo_device_to_user_distance(_cr, dx, dy);
}

cairo_matrix_t Canvas::get_matrix() {
  cairo_matrix_t mat;
  cairo_get_matrix(_cr, &mat);
  return mat;
}

void Canvas::set_matrix(const cairo_matrix_t &mat) {
  cairo_set_matrix(_cr, &mat);
}

void Canvas::reset_matrix() {
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  set_matrix(mat);
}

void Canvas::transform(const cairo_matrix_t &mat) {
  cairo_transform(_cr, &mat);
}

void Canvas::translate(double tx, double ty) {
  cairo_translate(_cr, tx, ty);
}

void Canvas::rotate(double angle) {
  cairo_rotate(_cr, angle);
}

void Canvas::scale(double sx, double sy) {
  cairo_scale(_cr, sx, sy);
}

void Canvas::set_color(Color color, float alpha) { 
  if(show_only_text)
    return;
    
  _color = color;
  cairo_set_source_rgba(_cr, color.red(), color.green(), color.blue(), alpha);
}

void Canvas::set_font_face(FontFace font) {
  cairo_set_font_face(_cr, cairo_font_face_reference(font.cairo()));
}

void Canvas::set_font_size(float size) {
  _font_size = size;
  cairo_set_font_size(_cr, size); //  * 4/3.
}

void Canvas::move_to(double x, double y) {
  cairo_move_to(_cr, x, y);
}

void Canvas::rel_move_to(double x, double y) {
  cairo_rel_move_to(_cr, x, y);
}

void Canvas::line_to(double x, double y) {
  cairo_line_to(_cr, x, y);
}

void Canvas::rel_line_to(double x, double y) {
  cairo_rel_line_to(_cr, x, y);
}

void Canvas::arc(
  double x, double y,
  double radius,
  double angle1,
  double angle2,
  bool negative
) {
  if(radius == 0) {
    if(!has_current_pos())
      move_to(x, y);
      
    line_to(x, y);
    return;
  }
  
  if(negative)
    cairo_arc_negative(_cr, x, y, radius, angle1, angle2);
  else
    cairo_arc(_cr, x, y, radius, angle1, angle2);
}

void Canvas::ellipse_arc(
  double x, double y,
  double radius_x,
  double radius_y,
  double angle1,
  double angle2,
  bool negative
) {
  if(radius_x == radius_y) {
    arc(x, y, radius_x, angle1, angle2, negative);
    return;
  }
  
  if(radius_x == 0 || radius_y == 0) {
    if(has_current_pos())
      line_to(x + radius_x * cos(angle1), y + radius_y * sin(angle1));
    else
      move_to(x + radius_x * cos(angle1), y + radius_y * sin(angle1));
      
    line_to(x + radius_x * cos(angle2), y + radius_y * sin(angle2));
    return;
  }
  
  save();
  {
    translate(x, y);
    scale(radius_x, radius_y);
    arc(0, 0, 1, angle1, angle2, negative);
  }
  restore();
}

void Canvas::close_path() {
  cairo_close_path(_cr);
}

void Canvas::align_point(float *x, float *y, bool tostroke) {
  if(pixel_device) {
    cairo_matrix_t ctm;
    cairo_get_matrix(_cr, &ctm);
    
    if( (ctm.xx == 0 && ctm.yy == 0) ||
        (ctm.xy == 0 && ctm.yx == 0))
    {
      user_to_device(x, y);
      if(tostroke) {
        *x = ceil(*x) - 0.5;
        *y = ceil(*y) - 0.5;
      }
      else {
        *x = floor(*x + 0.5);
        *y = floor(*y + 0.5);
      }
      device_to_user(x, y);
    }
  }
}

void Canvas::pixrect(float x1, float y1, float x2, float y2, bool tostroke) {
  Rectangle rect(x1, y1, x2 - x1, y2 - y1);
  
  rect.pixel_align(*this, tostroke, 0);
  rect.add_rect_path(*this);
}

void Canvas::show_blur_rect(
  float x1, float y1,
  float x2, float y2,
  float radius
) {
  if(show_only_text)
    return;
    
  cairo_pattern_t *pat;
  double r = _color.red();
  double g = _color.green();
  double b = _color.blue();
  
  move_to(x1, y1);
  line_to(x2, y1);
  line_to(x2, y1 - radius);
  line_to(x1, y1 - radius);
  close_path();
  pat = cairo_pattern_create_linear(x1, y1, x1, y1 - radius);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();
  
  move_to(x2,          y1);
  line_to(x2 + radius, y1);
  line_to(x2 + radius, y2);
  line_to(x2,          y2);
  close_path();
  pat = cairo_pattern_create_linear(x2, y1, x2 + radius, y1);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();
  
  move_to(x1, y2);
  line_to(x2, y2);
  line_to(x2, y2 + radius);
  line_to(x1, y2 + radius);
  close_path();
  pat = cairo_pattern_create_linear(x1, y2, x1, y2 + radius);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();
  
  move_to(x1,          y1);
  line_to(x1 - radius, y1);
  line_to(x1 - radius, y2);
  line_to(x1,          y2);
  close_path();
  pat = cairo_pattern_create_linear(x1, y1, x1 - radius, y1);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();
  
  
  move_to(x1, y1);
  arc(x1, y1, radius, M_PI, 3 * M_PI / 2, false);
  close_path();
  pat = cairo_pattern_create_radial(x1, y1, 0, x1, y1, radius);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();
  
  move_to(x2, y1);
  arc(x2, y1, radius, 3 * M_PI / 2, 0, false);
  close_path();
  pat = cairo_pattern_create_radial(x2, y1, 0, x2, y1, radius);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();
  
  move_to(x2, y2);
  arc(x2, y2, radius, 0, M_PI / 2, false);
  close_path();
  pat = cairo_pattern_create_radial(x2, y2, 0, x2, y2, radius);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();
  
  move_to(x1, y2);
  arc(x1, y2, radius, M_PI / 2, M_PI, false);
  close_path();
  pat = cairo_pattern_create_radial(x1, y2, 0, x1, y2, radius);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();
  
  set_color(_color);
}

void Canvas::show_blur_line(float x1, float y1, float x2, float y2, float radius) {
  if(show_only_text)
    return;
    
  float dx = x2 - x1;
  float dy = y2 - y1;
  
  float d2 = hypot(dx, dy);
  if(!(d2 > 0))
    return;
    
  d2 = radius / d2;
  dx *= d2;
  dy *= d2;
  
  cairo_pattern_t *pat;
  double r = _color.red();
  double g = _color.green();
  double b = _color.blue();
  
  move_to(x1, y1);
  line_to(x2, y2);
  line_to(x2 - dy, y2 + dx);
  line_to(x1 - dy, y1 + dx);
  close_path();
  
  pat = cairo_pattern_create_linear(x1, y1, x1 - dy, y1 + dx);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();
}

void Canvas::show_blur_arc(
  float x, float y,
  float radius,
  float angle1,
  float angle2,
  bool negative
) {
  if(show_only_text)
    return;
    
  cairo_pattern_t *pat;
  double r = _color.red();
  double g = _color.green();
  double b = _color.blue();
  
  move_to(x, y);
  arc(x, y, radius, angle1, angle2, negative);
  close_path();
  pat = cairo_pattern_create_radial(x, y, 0, x, y, radius);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();
}

static float angle(float x, float y) {
  if(x > 0)
    return atanf(y / x);
    
  if(x < 0) {
    if(y >= 0)
      return atanf(y / x) + M_PI;
    return atanf(y / x) - M_PI;
  }
  
  if(y < 0)
    return - M_PI / 2;
    
  return M_PI / 2;
}

void Canvas::show_blur_stroke(float radius, bool preserve) {
  if(show_only_text)
    return;
    
  cairo_path_t *path = cairo_copy_path_flat(_cr);
  cairo_path_data_t *data;
  
  new_path();
  bool first_line = true;
  float x0 = 0;
  float x1 = 0;
  float xm0 = 0;
  float xm1 = 0;
  float y0 = 0;
  float y1 = 0;
  float ym0 = 0;
  float ym1 = 0;
  
  float right_angle = M_PI / 2;
  float abs_radius = radius;
  if(abs_radius < 0) {
    abs_radius = -abs_radius;
    right_angle = -right_angle;
  }
  
  for(int i = 0; i < path->num_data; i += path->data[i].header.length) {
    data = &path->data[i];
    switch(data->header.type) {
      case CAIRO_PATH_MOVE_TO:
        xm0 = xm1 = x0 = x1 = data[1].point.x;
        ym0 = ym1 = y0 = y1 = data[1].point.y;
        first_line = true;
        break;
        
      case CAIRO_PATH_LINE_TO:
        if(data[1].point.x != x1 || data[1].point.y != y1) {
          if(!first_line) {
            float a1 = angle(x1 - x0,              y1 - y0);
            float a2 = angle(data[1].point.x - x1, data[1].point.y - y1);
            bool do_arc = (a1 >= 0 && a1 - M_PI < a2 && a2 < a1)
                          || (a1 <  0 && (a1 + M_PI < a2 || a2 < a1));
            if(radius < 0)
              do_arc = !do_arc;
            if(do_arc) {
              show_blur_arc(
                x1, y1,
                abs_radius,
                a1 + right_angle,
                a2 + right_angle,
                radius > 0);
            }
          }
          
          show_blur_line(x1, y1, data[1].point.x, data[1].point.y, radius);
          x0 = x1;
          y0 = y1;
          x1 = data[1].point.x;
          y1 = data[1].point.y;
          if(first_line) {
            xm1 = x1;
            ym1 = y1;
            first_line = false;
          }
        }
        break;
        
      case CAIRO_PATH_CURVE_TO: // flat path => no curves
        break;
        
      case CAIRO_PATH_CLOSE_PATH:
        if(xm0 != x1 || ym0 != y1) {
          {
            float a1 = angle(x1  - x0, y1  - y0);
            float a2 = angle(xm0 - x1, ym0 - y1);
            bool do_arc = (a1 >= 0 && a1 - M_PI < a2 && a2 < a1)
                          || (a1 <  0 && (a1 + M_PI < a2 || a2 < a1));
            if(radius < 0)
              do_arc = !do_arc;
            if(do_arc) {
              show_blur_arc(
                x1, y1,
                abs_radius,
                a1 + right_angle,
                a2 + right_angle,
                radius > 0);
            }
          }
          
          show_blur_line(x1, y1, xm0, ym0, radius);
        }
        else {
          x1 = x0;
          y1 = y0;
        }
        
        {
          float a1 = angle(xm0 - x1,  ym0 - y1);
          float a2 = angle(xm1 - xm0, ym1 - ym0);
          bool do_arc = (a1 >= 0 && a1 - M_PI < a2 && a2 < a1)
                        || (a1 <  0 && (a1 + M_PI < a2 || a2 < a1));
          if(radius < 0)
            do_arc = !do_arc;
          if(do_arc) {
            show_blur_arc(
              xm0, ym0,
              abs_radius,
              a1 + right_angle,
              a2 + right_angle,
              radius > 0);
          }
        }
        break;
    }
  }
  
  if(preserve)
    cairo_append_path(_cr, path);
    
  cairo_path_destroy(path);
}

void Canvas::glyph_extents(const cairo_glyph_t *glyphs, int num_glyphs, cairo_text_extents_t *extents) {
  cairo_glyph_extents(_cr, glyphs, num_glyphs, extents);
  
//  save();
//  cairo_font_face_t *font_face = cairo_get_font_face(_cr);
//  scale(1/1024.0, 1/1024.0);
//  cairo_set_font_face(_cr, cairo_font_face_reference(font_face));
//
//  cairo_glyph_extents(_cr, glyphs, num_glyphs, extents);
//
//  restore();
//  cairo_set_font_face(_cr, font_face);
//
//  extents->x_bearing /= 1024;
//  extents->y_bearing /= 1024;
//  extents->width     /= 1024;
//  extents->height    /= 1024;
//  extents->x_advance /= 1024;
//  extents->y_advance /= 1024;
}

void Canvas::show_glyphs(const cairo_glyph_t *glyphs, int num_glyphs) {
  bool sot = show_only_text;
  show_only_text = false;
  
  if(native_show_glyphs) {
    cairo_show_glyphs(_cr, glyphs, num_glyphs);
  }
  else {
    cairo_glyph_path(_cr, glyphs, num_glyphs);
    fill();
  }
  
  show_only_text = sot;
}

void Canvas::new_path() {
  cairo_new_path(_cr);
}

void Canvas::new_sub_path() {
  cairo_new_sub_path(_cr);
}

void Canvas::clip() {
  cairo_clip(_cr);
}

void Canvas::clip_preserve() {
  cairo_clip_preserve(_cr);
}

void Canvas::fill() {
  if(show_only_text) {
    new_path();
    return;
  }
  
  cairo_fill(_cr);
}

void Canvas::fill_preserve() {
  if(show_only_text)
    return;
    
  cairo_fill_preserve(_cr);
}

void Canvas::hair_stroke() {
  if(show_only_text) {
    new_path();
    return;
  }
  
  save();
  {
    cairo_matrix_t mat;
    cairo_matrix_init_identity(&mat);
    cairo_set_matrix(_cr, &mat);
    
    stroke();
  }
  restore();
  new_path();
}

void Canvas::stroke() {
  if(show_only_text) {
    new_path();
    return;
  }
  
  cairo_stroke(_cr);
}

void Canvas::stroke_preserve() {
  if(show_only_text)
    return;
    
  cairo_stroke_preserve(_cr);
}

void Canvas::clip_extents(richmath::Rectangle *rect) {
  double _x1, _y1, _x2, _y2;
  cairo_clip_extents(_cr, &_x1, &_y1, &_x2, &_y2);
  rect->x = _x1;
  rect->y = _y1;
  rect->width = _x2 - _x1;
  rect->height = _y2 - _y1;
}

void Canvas::clip_extents(float *x1, float *y1, float *x2, float *y2) {
  double _x1, _y1, _x2, _y2;
  cairo_clip_extents(_cr, &_x1, &_y1, &_x2, &_y2);
  *x1 = _x1;
  *y1 = _y1;
  *x2 = _x2;
  *y2 = _y2;
}

void Canvas::clip_extents(double *x1, double *y1, double *x2, double *y2) {
  cairo_clip_extents(_cr, x1, y1, x2, y2);
}

void Canvas::path_extents(richmath::Rectangle *rect) {
  double _x1, _y1, _x2, _y2;
  cairo_path_extents(_cr, &_x1, &_y1, &_x2, &_y2);
  rect->x = _x1;
  rect->y = _y1;
  rect->width = _x2 - _x1;
  rect->height = _y2 - _y1;
}

void Canvas::path_extents(float *x1, float *y1, float *x2, float *y2) {
  double _x1, _y1, _x2, _y2;
  cairo_path_extents(_cr, &_x1, &_y1, &_x2, &_y2);
  *x1 = _x1;
  *y1 = _y1;
  *x2 = _x2;
  *y2 = _y2;
}

void Canvas::path_extents(double *x1, double *y1, double *x2, double *y2) {
  cairo_path_extents(_cr, x1, y1, x2, y2);
}

void Canvas::stroke_extents(float *x1, float *y1, float *x2, float *y2) {
  double _x1, _y1, _x2, _y2;
  cairo_stroke_extents(_cr, &_x1, &_y1, &_x2, &_y2);
  *x1 = _x1;
  *y1 = _y1;
  *x2 = _x2;
  *y2 = _y2;
}

void Canvas::stroke_extents(double *x1, double *y1, double *x2, double *y2) {
  cairo_stroke_extents(_cr, x1, y1, x2, y2);
}

void Canvas::paint() {
  if(show_only_text)
    return;
    
  cairo_paint(_cr);
}

void Canvas::paint_with_alpha(float alpha) {
  if(show_only_text)
    return;
    
  cairo_paint_with_alpha(_cr, alpha);
}

//} ... class Canvas

#if CAIRO_HAS_WIN32_SURFACE
HDC richmath::safe_cairo_win32_surface_get_dc(cairo_surface_t *surface) {
  if(cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
    return nullptr;
  
  // cairo_win32_surface_get_dc unconditionally dereferences the backend pointer which is NULL 
  // for the nil surface that gets returned in out-of-memory situations.
  return cairo_win32_surface_get_dc(surface);
}
#endif
