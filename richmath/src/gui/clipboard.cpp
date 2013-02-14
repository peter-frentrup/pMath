#include <gui/clipboard.h>
#include <graphics/cairostream.h>
#include <cairo-svg.h>

using namespace richmath;

class DummyClipboard: public Clipboard {
  public:
    virtual bool has_format(String mimetype) {
      return false;
    }
    
    virtual ReadableBinaryFile read_as_binary_file(String mimetype) {
      return ReadableBinaryFile();
    }
    
    virtual String read_as_text(String mimetype) {
      return String();
    }
    
    virtual SharedPtr<OpenedClipboard> open_write() {
      return 0;
    }
};

static DummyClipboard dummy_cb;

Clipboard *Clipboard::dummy = &dummy_cb;
Clipboard *Clipboard::std   = &dummy_cb;

const char *const Clipboard::PlainText           = "text/plain";
const char *const Clipboard::BoxesText           = "text/x-pmath-boxes";
const char *const Clipboard::BoxesBinary         = "application/x-pmath-boxes";
const char *const Clipboard::PlatformBitmapImage = "Bitmap";
const char *const Clipboard::SvgImage            = "image/svg+xml";

struct abf_info_t {
  OpenedClipboard *self;
  String           mimetype;
  bool             success;
};

static void abf_flush(
  uint8_t        *readable,
  uint8_t       **writable,
  const uint8_t  *end,
  void           *closure
) {
  struct abf_info_t *info = (struct abf_info_t *)closure;
  
  info->success = info->self->add_binary(
                    info->mimetype,
                    readable,
                    (size_t) * writable - (size_t)readable);
}

bool OpenedClipboard::add_image(String suggested_mimetype, cairo_surface_t *image) {
  CairoStream *stream = CairoStream::from_surface(image);
  if(stream) {
    cairo_surface_type_t type = cairo_surface_get_type(image);
    
    cairo_status_t status = cairo_surface_status(image);
    if(status != CAIRO_STATUS_SUCCESS) {
      pmath_debug_print("[cairo error: %s]\n", cairo_status_to_string(status));
    }
    
    cairo_surface_finish(image);
    
    switch(type) {
      case CAIRO_SURFACE_TYPE_SVG: {
        String as_text = stream->copy_to_string_utf8();
        add_text(Clipboard::PlainText, as_text);
        return add_binary_buffer(Clipboard::SvgImage, stream->file);
      }
        
      default:
        return false;
    }
  }
  
  return false;
}

bool OpenedClipboard::add_binary_buffer(String mimetype, Expr binbuffer) {
  struct abf_info_t info;
  info.self     = this;
  info.mimetype = mimetype;
  info.success  = false;
  
  pmath_file_binary_buffer_manipulate(binbuffer.get(), abf_flush, &info);
  
  return info.success;
}

cairo_surface_t *Clipboard::create_image(String mimetype, double width, double height) {
#if CAIRO_HAS_SVG_SURFACE
  if(mimetype.equals(SvgImage)) {
    cairo_surface_t *image = CairoStream::create_svg_surface(
                               WriteableBinaryFile(pmath_file_create_binary_buffer(0)),
                               width,
                               height);
    
    if(cairo_surface_status(image) == CAIRO_STATUS_SUCCESS)
      return image;
    return 0;
  }
#endif
  
  return 0;
}
