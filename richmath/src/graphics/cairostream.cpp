#include <graphics/cairostream.h>

#include <cairo-svg.h>
#include <cairo-pdf.h>

#include <limits.h>


using namespace richmath;
using namespace pmath;


//{ class CairoStream ...

cairo_user_data_key_t CairoStreamUserDataKey;

CairoStream::CairoStream()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

CairoStream::CairoStream(WriteableBinaryFile _file)
  : Base(),
    file(_file)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

CairoStream::~CairoStream() {
}

cairo_surface_t *CairoStream::create_svg_surface(
  WriteableBinaryFile file,
  double              width_in_points,
  double              height_in_points
) {
  cairo_surface_t *surface;
  
#if CAIRO_HAS_SVG_SURFACE
  CairoStream *stream = new CairoStream(file);
  
  surface = cairo_svg_surface_create_for_stream(
              CairoStream::cairo_write_func,
              stream,
              width_in_points,
              height_in_points);
              
  stream->attach_to_surface(surface);
  return surface;
#else
  surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 0, 0);
  cairo_surface_finish(surface);
  return surface;
#endif
}

cairo_surface_t *CairoStream::create_pdf_surface(
  WriteableBinaryFile file,
  double              width_in_points,
  double              height_in_points
) {
  cairo_surface_t *surface;
  
#if CAIRO_HAS_PDF_SURFACE
  CairoStream *stream = new CairoStream(file);
  
  surface = cairo_pdf_surface_create_for_stream(
              CairoStream::cairo_write_func,
              stream,
              width_in_points,
              height_in_points);
              
  stream->attach_to_surface(surface);
  return surface;
#else
  surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 0, 0);
  cairo_surface_finish(surface);
  return surface;
#endif
}

CairoStream *CairoStream::from_surface(cairo_surface_t *surface) {
  return (CairoStream *)cairo_surface_get_user_data(
           surface,
           &CairoStreamUserDataKey);
}

static void copy_to_string_utf8_callback(
  uint8_t        *readable,
  uint8_t       **writable,
  const uint8_t  *end,
  void           *closure
) {
  pmath_string_t *result = (pmath_string_t *)closure;
  
  size_t len = (size_t) * writable - (size_t)readable;
  if(len < INT_MAX)
    *result = pmath_string_from_utf8((const char*)readable, (int)len);
}

String CairoStream::copy_to_string_utf8() {
  pmath_string_t result = PMATH_NULL;
  
  pmath_file_binary_buffer_manipulate(file.get(), copy_to_string_utf8_callback, &result);
  
  return String(result);
}

void CairoStream::cairo_destroy_func(void *self) {
  CairoStream *_this = (CairoStream *)self;
  
  delete _this;
}

cairo_status_t CairoStream::cairo_write_func(void *self, const unsigned char *data, unsigned int length) {
  CairoStream *_this = (CairoStream *)self;
  
  _this->file.write(data, length);
  
  if(_this->file.status() != PMATH_FILE_OK)
    return CAIRO_STATUS_WRITE_ERROR;
    
  return CAIRO_STATUS_SUCCESS;
}

// frees this
void CairoStream::attach_to_surface(cairo_surface_t *surface) {
  cairo_status_t success;
  
  success = cairo_surface_set_user_data(
              surface,
              &CairoStreamUserDataKey,
              this,
              cairo_destroy_func);
              
  if(success != CAIRO_STATUS_SUCCESS)
    delete this;
}

//} ... class CairoStream
