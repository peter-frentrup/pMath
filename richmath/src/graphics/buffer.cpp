#include <graphics/buffer.h>

#include <graphics/shapers.h>

#include <cmath>

#ifdef _WIN32
  #include <cairo-win32.h>
#endif


using namespace richmath;

//{ class Buffer ...

Buffer::Buffer(Canvas *dst, cairo_format_t format, float x, float y, float w, float h)
: Shareable()
{
  init(dst, format, x, y, w, h);
}

Buffer::Buffer(Canvas *dst, cairo_format_t format, const BoxSize &size)
: Shareable()
{
  float x0, y0;
  dst->current_pos(&x0, &y0);

  init(dst, format, x0, y0 - size.ascent, size.width, size.height());
}

void Buffer::init(Canvas *dst, cairo_format_t format, float x, float y, float w, float h){
  _width = 0;
  _height = 0;
  _canvas = 0;
  _cr = 0;
  _surface = 0;

  cairo_surface_t *target = dst->target();
  cairo_surface_type_t type = cairo_surface_get_type(target);

  switch(type){
    case CAIRO_SURFACE_TYPE_IMAGE:
    case CAIRO_SURFACE_TYPE_WIN32: {
      float x0, y0;
      dst->current_pos(&x0, &y0);

      dst->save();
      cairo_get_matrix(dst->cairo(), &u2d_matrix);

      float mx0 = u2d_matrix.x0;
      float my0 = u2d_matrix.y0;

      u2d_matrix.x0 = 0;
      u2d_matrix.y0 = 0;

      Canvas::transform_rect( u2d_matrix, &x, &y, &w, &h);

      memcpy(&d2u_matrix, &u2d_matrix, sizeof(u2d_matrix));
      cairo_matrix_invert(&d2u_matrix);

      x = floor(x - mx0) + mx0;
      y = floor(y - my0) + my0;

      Canvas::transform_point(d2u_matrix, &x, &y);
      cairo_matrix_translate(&u2d_matrix, x0 - x, y0 - y);

      memcpy(&d2u_matrix, &u2d_matrix, sizeof(u2d_matrix));
      cairo_matrix_invert(&d2u_matrix);

      _width  = (int)(ceil(w) + 1);
      _height = (int)(ceil(h) + 1);

      switch(type){
        case CAIRO_SURFACE_TYPE_IMAGE:
          _surface = cairo_image_surface_create(
            format, //CAIRO_FORMAT_ARGB32,
            _width, _height);
          break;

        #ifdef _WIN32
        case CAIRO_SURFACE_TYPE_WIN32:
          _surface = cairo_win32_surface_create_with_dib(
            format, //CAIRO_FORMAT_ARGB32,
            _width, _height);
          break;
        #endif

        default:
          assert(0 && "Unknown Surface Type");
      }

      _cr = cairo_create(_surface);
      _canvas = new Canvas(_cr);
      cairo_set_line_width(_cr, cairo_get_line_width(dst->cairo()));
      cairo_set_matrix(_cr, &u2d_matrix);
      dst->restore();

      _canvas->glass_background   = dst->glass_background;
//      _canvas->text_emboss_size   = dst->text_emboss_size;
//      _canvas->text_emboss_color  = dst->text_emboss_color;
      _canvas->native_show_glyphs = dst->native_show_glyphs;

      cairo_set_font_face(_cr, cairo_get_font_face(dst->cairo()));

      cairo_matrix_t fmat;
      cairo_get_font_matrix(dst->cairo(), &fmat);
      cairo_set_font_matrix(_cr, &fmat);

//      _canvas->set_color(0xFF0000);
//      _canvas->paint_with_alpha(0.1);

      _canvas->set_color(dst->get_color());
    } break;

    default:
      cairo_matrix_init_identity(&u2d_matrix);
      cairo_matrix_init_identity(&d2u_matrix);
      break;
  }
}

Buffer::~Buffer(){
  if(_canvas)
    delete _canvas;

  if(_cr)
    cairo_destroy(_cr);

  if(_surface)
    cairo_surface_destroy(_surface);
}

bool Buffer::is_compatible(Canvas *dst){
  if(!_cr)
    return false;

  cairo_surface_t *target = dst->target();

  switch(cairo_surface_get_type(target)){
    case CAIRO_SURFACE_TYPE_IMAGE:
    case CAIRO_SURFACE_TYPE_WIN32: {
      cairo_matrix_t mat;
      cairo_get_matrix(dst->cairo(), &mat);

      if(mat.xx == u2d_matrix.xx
      && mat.xy == u2d_matrix.xy
      && mat.yx == u2d_matrix.yx
      && mat.yy == u2d_matrix.yy){
        return true;
      }
    } break;

    default: break;
  }

  return false;
}

bool Buffer::paint(Canvas *dst){
  return paint_with_alpha(dst, 1);
}

bool Buffer::paint_with_alpha(Canvas *dst, float alpha){
  if(!is_compatible(dst))
    return false;

  float x, y;
  dst->current_pos(&x, &y);

  cairo_surface_flush(_surface);

  dst->save();

  dst->user_to_device(&x, &y);
  x-= u2d_matrix.x0;
  y-= u2d_matrix.y0;
  x = floor(x + 0.5);
  y = floor(y + 0.5);

  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  cairo_set_matrix(dst->cairo(), &mat);
  dst->translate(x, y);

  //dst->translate(x0, y0);
  cairo_pattern_t *pattern = cairo_pattern_create_for_surface(_surface);

  cairo_set_source(dst->cairo(), pattern);
  cairo_pattern_destroy(pattern);

  if(alpha >= 1)
    dst->paint();
  else
    dst->paint_with_alpha(alpha);

  dst->restore();
  return true;
}

bool Buffer::mask(Canvas *dst){
  if(!is_compatible(dst))
    return false;

  float x, y;
  dst->current_pos(&x, &y);

  cairo_surface_flush(_surface);


//  dst->save();

  dst->user_to_device(&x, &y);
  x-= u2d_matrix.x0;
  y-= u2d_matrix.y0;
  x = floor(x + 0.5);
  y = floor(y + 0.5);

  cairo_matrix_t mat;
  cairo_matrix_t oldmat;
  cairo_matrix_init_identity(&mat);
  cairo_get_matrix(dst->cairo(), &oldmat);
  cairo_set_matrix(dst->cairo(), &mat);
  dst->translate(x, y);

  //dst->translate(x0, y0);
  cairo_pattern_t *pattern = cairo_pattern_create_for_surface(_surface);

  cairo_mask(dst->cairo(), pattern);
  cairo_pattern_destroy(pattern);

//  dst->restore();
  cairo_set_matrix(dst->cairo(), &oldmat);
  return true;
}

bool Buffer::clear(){
  if(!_canvas)
    return false;

  _canvas->save();
  cairo_set_operator(_cr, CAIRO_OPERATOR_CLEAR);
  cairo_reset_clip(_cr);
  _canvas->paint();
  _canvas->restore();
  return true;
}

bool Buffer::blend(SharedPtr<Buffer> buf1, SharedPtr<Buffer> buf2, double t){
  if(!buf1 || !buf1->_canvas
  || !buf2 || !buf2->_canvas
  || _width != buf1->_width
  || _width != buf2->_width
  || _height != buf1->_height
  || _height != buf2->_height)
    return false;

  // CLEAR:           xR <- 0
  // ADD (mask: 1-t): xR <- (1-t) * xA + xR = (1-t) * xA
  // ADD (mask: t):   xR <-     t * xB + xR = t * xB + (1-t) * xA
  _canvas->save();
  buf1->_canvas->save();
  buf2->_canvas->save();
  {
    cairo_matrix_t mat;
    cairo_matrix_init_identity(&mat);
    cairo_set_matrix(_cr, &mat);
    cairo_set_matrix(buf1->_cr, &mat);
    cairo_set_matrix(buf2->_cr, &mat);

    cairo_reset_clip(_cr);

    cairo_set_operator(_cr, CAIRO_OPERATOR_CLEAR);
    _canvas->paint();

    cairo_set_operator(_cr, CAIRO_OPERATOR_ADD);

    cairo_pattern_t *pattern;

    pattern = cairo_pattern_create_for_surface(buf1->_surface);
    cairo_set_source(_cr, pattern);
    cairo_pattern_destroy(pattern);
    _canvas->paint_with_alpha((1-t));

    pattern = cairo_pattern_create_for_surface(buf2->_surface);
    cairo_set_source(_cr, pattern);
    cairo_pattern_destroy(pattern);
    _canvas->paint_with_alpha(t);
  }
  buf2->_canvas->restore();
  buf1->_canvas->restore();
  _canvas->restore();
  return true;

// old algorithm: blending the image bytes directly
//  cairo_surface_t *s = 0;
//  cairo_surface_t *s1 = 0;
//  cairo_surface_t *s2 = 0;
//
//  switch(cairo_surface_get_type(_surface)){
//    case CAIRO_SURFACE_TYPE_IMAGE:
//      s = _surface;
//      break;
//
//    case CAIRO_SURFACE_TYPE_WIN32:
//      s = cairo_win32_surface_get_image(_surface);
//      break;
//
//    default: return false;
//  }
//
//  switch(cairo_surface_get_type(buf1->_surface)){
//    case CAIRO_SURFACE_TYPE_IMAGE:
//      s1 = buf1->_surface;
//      break;
//
//    case CAIRO_SURFACE_TYPE_WIN32:
//      s1 = cairo_win32_surface_get_image(buf1->_surface);
//      break;
//
//    default: return false;
//  }
//
//  switch(cairo_surface_get_type(buf2->_surface)){
//    case CAIRO_SURFACE_TYPE_IMAGE:
//      s2 = buf2->_surface;
//      break;
//
//    case CAIRO_SURFACE_TYPE_WIN32:
//      s2 = cairo_win32_surface_get_image(buf2->_surface);
//      break;
//
//    default: return false;
//  }
//
//  if(!s || !s1 || !s2)
//    return false;
//
//  if(cairo_image_surface_get_format(s)  != CAIRO_FORMAT_ARGB32
//  || cairo_image_surface_get_format(s1) != CAIRO_FORMAT_ARGB32
//  || cairo_image_surface_get_format(s2) != CAIRO_FORMAT_ARGB32)
//    return false;
//
//  int w = cairo_image_surface_get_width(s);
//  int h = cairo_image_surface_get_height(s);
//
//  if(w != cairo_image_surface_get_width(s1)
//  || w != cairo_image_surface_get_width(s2)
//  || h != cairo_image_surface_get_height(s1)
//  || h != cairo_image_surface_get_height(s2))
//    return false;
//
//  unsigned int t255 = (unsigned int)(t * 255);
//
//  unsigned char *data = cairo_image_surface_get_data(s);
//  unsigned char *d1 = cairo_image_surface_get_data(s1);
//  unsigned char *d2 = cairo_image_surface_get_data(s2);
//
//  if(!data || !d1 || !d2)
//    return false;
//
//  int stride = cairo_image_surface_get_stride(s);
//  int wb = w * 4;
//  for(int y = 0;y < h;++y){
//    for(int xb = 0;xb < wb;++xb){
//      data[xb] = (d2[xb] * t255 + d1[xb] * (255 - t255))/255;
//    }
//
//    data+= stride;
//    d1+= stride;
//    d2+= stride;
//  }
//
//  cairo_surface_mark_dirty(s);
//  cairo_surface_mark_dirty(_surface);
//
//  return true;
}

// see firefox -> nsContextBoxBlur class (nsCSSRendering.cpp)
#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) > (b) ? (a) : (b))

  static void blur_horizontal_a8(
    uint8_t *in,
    uint8_t *out,
    int left,
    int right,
    int stride,
    int rows
  ){
    int boxsize = left + right + 1;

    for(int y = 0;y < rows;++y){
      int sum = 0;

      for(int i = 0;i < boxsize;++i){
        int pos = i - left;
        pos = MAX(pos, 0);
        pos = MIN(pos, stride - 1);
        sum+= in[y * stride + pos];
      }

      for(int x = 0;x < stride;++x) {
        int tmp = x - left;
        int last = MAX(tmp, 0);
        int next = MIN(tmp + boxsize, stride - 1);

        out[y * stride + x] = (uint8_t)(sum/boxsize);

        sum+= in[y * stride + next] - in[y * stride + last];
      }
    }
  }

  static void blur_vertical_a8(
    uint8_t *in,
    uint8_t *out,
    int top,
    int bottom,
    int stride,
    int rows
  ){
    int boxsize = top + bottom + 1;

    for(int x = 0; x < stride;++x){
      int sum = 0;

      for(int i = 0;i < boxsize;++i){
        int pos = i - top;
        pos = MAX(pos, 0);
        pos = MIN(pos, rows - 1); // stride-1 ??
        sum+= in[pos * stride + x];
      }

      for(int y = 0;y < rows;++y){
        int tmp = y - top;
        int last = MAX(tmp, 0);
        int next = MIN(tmp + boxsize, rows - 1);

        out[y * stride + x] = (uint8_t)(sum/boxsize);

        sum+= in[next * stride + x] - in[last * stride + x];
      }
    }
  }

bool Buffer::blur(float radius){
  cairo_surface_t *img_surface;
  switch(cairo_surface_get_type(_surface)){
    case CAIRO_SURFACE_TYPE_IMAGE:
      img_surface = _surface;
      break;

    #ifdef _WIN32
    case CAIRO_SURFACE_TYPE_WIN32:
      img_surface = cairo_win32_surface_get_image(_surface);
      break;
    #endif

    default:
      return false;
  }

  if(cairo_image_surface_get_format(img_surface) != CAIRO_FORMAT_A8)
    return false;

  int iradius;
  float rx, ry, rw, rh;
  rx = ry = 0;
  rw = rh = 1;

  Canvas::transform_rect(user_to_device(), &rx, &ry, &rw, &rh);
  float rd = (fabsf(rw) + fabsf(rh))/2;

  iradius = (int)floorf(radius * rd + 0.5f);
  if(iradius <= 0)
    return false;

  iradius = MAX(iradius, 2);

  int stride = cairo_image_surface_get_stride(img_surface);
  int rows   = cairo_image_surface_get_height(img_surface);

  Array<uint8_t> tmpdata_arr(stride * rows);
  uint8_t *tmpdata = tmpdata_arr.items();
  uint8_t *boxdata = cairo_image_surface_get_data(img_surface);

  if(iradius & 1){ // odd radius
    blur_horizontal_a8(boxdata, tmpdata, iradius/2, iradius/2, stride, rows);
    blur_horizontal_a8(tmpdata, boxdata, iradius/2, iradius/2, stride, rows);
    blur_horizontal_a8(boxdata, tmpdata, iradius/2, iradius/2, stride, rows);
    blur_vertical_a8(  tmpdata, boxdata, iradius/2, iradius/2, stride, rows);
    blur_vertical_a8(  boxdata, tmpdata, iradius/2, iradius/2, stride, rows);
    blur_vertical_a8(  tmpdata, boxdata, iradius/2, iradius/2, stride, rows);
  } else { // even radius
    blur_horizontal_a8(boxdata, tmpdata, iradius/2,     iradius/2 - 1, stride, rows);
    blur_horizontal_a8(tmpdata, boxdata, iradius/2 - 1, iradius/2,     stride, rows);
    blur_horizontal_a8(boxdata, tmpdata, iradius/2,     iradius/2,     stride, rows);
    blur_vertical_a8(  tmpdata, boxdata, iradius/2,     iradius/2 - 1, stride, rows);
    blur_vertical_a8(  boxdata, tmpdata, iradius/2 - 1, iradius/2,     stride, rows);
    blur_vertical_a8(  tmpdata, boxdata, iradius/2,     iradius/2,     stride, rows);
  }

  return true;
}

//} ... class Buffer
