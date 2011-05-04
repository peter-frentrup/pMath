#include <graphics/canvas.h>

#include <cmath>

#ifdef _WIN32
  #include <Windows.h>
  #include <cairo-win32.h>
#endif

using namespace richmath;

//{ class Canvas ...

Canvas::Canvas(cairo_t *cr)
: pixel_device(true),
  glass_background(false),
  native_show_glyphs(true),
  show_only_text(false),
  _cr(cr),
  _font_size(10),
  _color(0)
{
}

Canvas::~Canvas(){
}

void Canvas::transform_point(
  const cairo_matrix_t &m,
  float *x, float *y
){
  double dx = *x;
  double dy = *y;

  cairo_matrix_transform_point(&m, &dx, &dy);

  *x = dx;
  *y = dy;
}

void Canvas::transform_rect(
  const cairo_matrix_t &m,
  float *x, float *y, float *w, float *h
){
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

void Canvas::save(){
  cairo_save(_cr);
}

void Canvas::restore(){
  cairo_restore(_cr);
}

void Canvas::current_pos(float *x, float *y){
  double dx, dy;
  cairo_get_current_point(_cr, &dx, &dy);
  *x = dx;
  *y = dy;
}

void Canvas::user_to_device(float *x, float *y){
  double dx = *x;
  double dy = *y;
  cairo_user_to_device(_cr, &dx, &dy);
  *x = dx;
  *y = dy;
}

void Canvas::user_to_device_dist(float *dx, float *dy){
  double ddx = *dx;
  double ddy = *dy;
  cairo_user_to_device_distance(_cr, &ddx, &ddy);
  *dx = ddx;
  *dy = ddy;
}

void Canvas::device_to_user(float *x, float *y){
  double dx = *x;
  double dy = *y;
  cairo_device_to_user(_cr, &dx, &dy);
  *x = dx;
  *y = dy;
}

void Canvas::device_to_user_dist(float *dx, float *dy){
  double ddx = *dx;
  double ddy = *dy;
  cairo_device_to_user_distance(_cr, &ddx, &ddy);
  *dx = ddx;
  *dy = ddy;
}

void Canvas::transform(const cairo_matrix_t &mat){
  cairo_transform(_cr, &mat);
}

void Canvas::translate(float tx, float ty){
  cairo_translate(_cr, (double)tx, (double)ty);
}

void Canvas::rotate(float angle){
  cairo_rotate(_cr, (double)angle);
}

void Canvas::scale(float sx, float sy){
  cairo_scale(_cr, (double)sx, (double)sy);
}

void Canvas::set_color(int color, float alpha){ // 0xRRGGBB
  if(show_only_text)
    return;

  _color = color;
  cairo_set_source_rgba(_cr,
    ((color & 0xFF0000) >> 16) / 255.0,
    ((color & 0x00FF00) >>  8) / 255.0,
     (color & 0x0000FF)        / 255.0,
    alpha);
}

int Canvas::get_color(){ // 0xRRGGBB
  return _color;
}

void Canvas::set_font_face(FontFace font){
  cairo_set_font_face(_cr, cairo_font_face_reference(font.cairo()));
}

void Canvas::set_font_size(float size){
  _font_size = size;
  cairo_set_font_size(_cr, size); //  * 4/3.
}

void Canvas::move_to(float x, float y){
  cairo_move_to(_cr, (double)x, (double)y);
}

void Canvas::rel_move_to(float x, float y){
  cairo_rel_move_to(_cr, (double)x, (double)y);
}

void Canvas::line_to(float x, float y){
  cairo_line_to(_cr, (double)x, (double)y);
}

void Canvas::rel_line_to(float x, float y){
  cairo_rel_line_to(_cr, (double)x, (double)y);
}

void Canvas::arc(
  float x, float y,
  float radius,
  float angle1,
  float angle2,
  bool negative
){
  if(negative)
    cairo_arc_negative(_cr, x, y, radius, angle1, angle2);
  else
    cairo_arc(_cr, x, y, radius, angle1, angle2);
}

void Canvas::close_path(){
  cairo_close_path(_cr);
}

void Canvas::align_point(float *x, float *y, bool tostroke){
  if(pixel_device){
    cairo_matrix_t ctm;
    cairo_get_matrix(_cr, &ctm);

    if((ctm.xx == 0 && ctm.yy == 0)
    || (ctm.xy == 0 && ctm.yx == 0)){
      user_to_device(x, y);
      if(tostroke){
        *x = ceil(*x) - 0.5;
        *y = ceil(*y) - 0.5;
      }
      else{
        *x = floor(*x + 0.5);
        *y = floor(*y + 0.5);
      }
      device_to_user(x, y);
    }
  }
}

void Canvas::pixrect(float x1, float y1, float x2, float y2, bool tostroke){
  if(pixel_device){
    float x3 = x1;
    float y3 = y2;
    user_to_device(&x1, &y1);
    user_to_device(&x2, &y2);
    user_to_device(&x3, &y3);
    if(x3 == x1 || x3 == x2){
    /* only align to pixel boundaries,
       if the rectangle is rotated by 0°, 90°, 180° or 270° */
      if(tostroke){
        x1 = ceil(x1) - 0.5;
        y1 = ceil(y1) - 0.5;
        x2 = ceil(x2) - 0.5;
        y2 = ceil(y2) - 0.5;
      }
      else{
        x1 = floor(x1 + 0.5);
        y1 = floor(y1 + 0.5);
        x2 = floor(x2 + 0.5);
        y2 = floor(y2 + 0.5);
      }

//      if(x1 == x2)
//        x2+= 1;
//      if(y1 == y2)
//        y2+= 1;
    }
    device_to_user(&x1, &y1);
    device_to_user(&x2, &y2);
  }

  move_to(x1, y1);
  line_to(x2, y1);
  line_to(x2, y2);
  line_to(x1, y2);
  close_path();
}

void Canvas::pixframe(float x1, float y1, float x2, float y2, float thickness){
  float tx = thickness;
  float ty = thickness;

  if(pixel_device){
    float x3 = x1;
    float y3 = y2;
    user_to_device(&x1, &y1);
    user_to_device(&x2, &y2);
    user_to_device(&x3, &y3);
    user_to_device_dist(&tx, &ty);
    if(x3 == x1 || x3 == x2){
    /* only align to pixel boundaries,
       if the rectangle is rotated by 0°, 90°, 180° or 270° */
      x1 = floor(x1 + 0.5);
      y1 = floor(y1 + 0.5);
      x2 = floor(x2 + 0.5);
      y2 = floor(y2 + 0.5);
      tx = floor(tx);
      ty = floor(ty);
      if(tx < 0 && tx > -1)
        tx = -1;
      else if(tx >= 0 && tx < 1)
        tx = 1;

      if(ty < 0 && ty > -1)
        ty = -1;
      else if(ty >= 0 && ty < 1)
        ty = 1;
    }
    device_to_user(&x1, &y1);
    device_to_user(&x2, &y2);
    device_to_user_dist(&tx, &ty);
  }

  move_to(x1, y1);
  line_to(x1, y2);
  line_to(x2, y2);
  line_to(x2, y1);
  close_path();

  move_to(x1 + tx, y1 + ty);
  line_to(x2 - tx, y1 + ty);
  line_to(x2 - tx, y2 - ty);
  line_to(x1 + tx, y2 - ty);
  close_path();
}

void Canvas::show_blur_rect(
  float x1, float y1,
  float x2, float y2,
  float radius
){
  if(show_only_text)
    return;

  cairo_pattern_t *pat;
  float r = ((_color & 0xFF0000) >> 16) / 255.0;
  float g = ((_color & 0x00FF00) >>  8) / 255.0;
  float b =  (_color & 0x0000FF)        / 255.0;

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
  arc(x1, y1, radius, M_PI, 3*M_PI/2, false);
  close_path();
  pat = cairo_pattern_create_radial(x1, y1, 0, x1, y1, radius);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();

  move_to(x2, y1);
  arc(x2, y1, radius, 3*M_PI/2, 0, false);
  close_path();
  pat = cairo_pattern_create_radial(x2, y1, 0, x2, y1, radius);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();

  move_to(x2, y2);
  arc(x2, y2, radius, 0, M_PI/2, false);
  close_path();
  pat = cairo_pattern_create_radial(x2, y2, 0, x2, y2, radius);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();

  move_to(x1, y2);
  arc(x1, y2, radius, M_PI/2, M_PI, false);
  close_path();
  pat = cairo_pattern_create_radial(x1, y2, 0, x1, y2, radius);
  cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 1, r, g, b, 0);
  cairo_set_source(_cr, pat);
  cairo_pattern_destroy(pat);
  fill();

  set_color(_color);
}

void Canvas::show_blur_line(float x1, float y1, float x2, float y2, float radius){
  if(show_only_text)
    return;

  float dx = x2 - x1;
  float dy = y2 - y1;

  float d2 = dx * dx + dy * dy;
  if(d2 <= 0)
    return;

  d2 = radius/sqrt(d2);
  dx*= d2;
  dy*= d2;

  cairo_pattern_t *pat;
  float r = ((_color & 0xFF0000) >> 16) / 255.0;
  float g = ((_color & 0x00FF00) >>  8) / 255.0;
  float b =  (_color & 0x0000FF)        / 255.0;


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
){
  if(show_only_text)
    return;

  cairo_pattern_t *pat;
  float r = ((_color & 0xFF0000) >> 16) / 255.0;
  float g = ((_color & 0x00FF00) >>  8) / 255.0;
  float b =  (_color & 0x0000FF)        / 255.0;

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

static float angle(float x, float y){
  if(x > 0)
    return atanf(y/x);

  if(x < 0){
    if(y >= 0)
      return atanf(y/x) + M_PI;
    return atanf(y/x) - M_PI;
  }

  if(y < 0)
    return - M_PI / 2;

  return M_PI / 2;
}

void Canvas::show_blur_stroke(float radius, bool preserve){
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
  if(abs_radius < 0){
    abs_radius = -abs_radius;
    right_angle = -right_angle;
  }

  for(int i = 0; i < path->num_data; i+= path->data[i].header.length){
    data = &path->data[i];
    switch(data->header.type){
      case CAIRO_PATH_MOVE_TO:
        xm0 = xm1 = x0 = x1 = data[1].point.x;
        ym0 = ym1 = y0 = y1 = data[1].point.y;
        first_line = true;
        break;

      case CAIRO_PATH_LINE_TO:
        if(data[1].point.x != x1 || data[1].point.y != y1){
          if(!first_line){
            float a1 = angle(x1 - x0,              y1 - y0);
            float a2 = angle(data[1].point.x - x1, data[1].point.y - y1);
            bool do_arc = (a1 >= 0 && a1 - M_PI < a2 && a2 < a1)
                       || (a1 <  0 && (a1 + M_PI < a2 || a2 < a1));
            if(radius < 0)
              do_arc = !do_arc;
            if(do_arc){
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
          if(first_line){
            xm1 = x1;
            ym1 = y1;
            first_line = false;
          }
        }
        break;

      case CAIRO_PATH_CURVE_TO: // flat path => no curves
        break;

      case CAIRO_PATH_CLOSE_PATH:
        if(xm0 != x1 || ym0 != y1){
          {
            float a1 = angle(x1  - x0, y1  - y0);
            float a2 = angle(xm0 - x1, ym0 - y1);
            bool do_arc = (a1 >= 0 && a1 - M_PI < a2 && a2 < a1)
                       || (a1 <  0 && (a1 + M_PI < a2 || a2 < a1));
            if(radius < 0)
              do_arc = !do_arc;
            if(do_arc){
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
        else{
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
          if(do_arc){
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

void Canvas::glyph_extents(const cairo_glyph_t *glyphs, int num_glyphs, cairo_text_extents_t *extents){
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

void Canvas::show_glyphs(const cairo_glyph_t *glyphs, int num_glyphs){
  bool sot = show_only_text;
  show_only_text = false;

  if(native_show_glyphs){
    cairo_show_glyphs(_cr, glyphs, num_glyphs);
  }
  else{
    cairo_glyph_path(_cr, glyphs, num_glyphs);
    fill();
  }

  show_only_text = sot;
}

void Canvas::new_path(){
  cairo_new_path(_cr);
}

void Canvas::clip(){
  cairo_clip(_cr);
}

void Canvas::clip_preserve(){
  cairo_clip_preserve(_cr);
}

void Canvas::fill(){
  if(show_only_text){
    new_path();
    return;
  }

  cairo_fill(_cr);
}

void Canvas::fill_preserve(){
  if(show_only_text)
    return;

  cairo_fill_preserve(_cr);
}

void Canvas::hair_stroke(){
  if(show_only_text){
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

void Canvas::stroke(){
  if(show_only_text){
    new_path();
    return;
  }

  cairo_stroke(_cr);
}

void Canvas::stroke_preserve(){
  if(show_only_text)
    return;

  cairo_stroke_preserve(_cr);
}

void Canvas::paint(){
  if(show_only_text)
    return;

  cairo_paint(_cr);
}

void Canvas::paint_with_alpha(float alpha){
  if(show_only_text)
    return;

  cairo_paint_with_alpha(_cr, alpha);
}

//} ... class Canvas
