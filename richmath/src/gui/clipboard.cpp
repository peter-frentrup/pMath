#include <gui/clipboard.h>

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

const char *const Clipboard::PlainText   = "text/plain";
const char *const Clipboard::BoxesText   = "text/x-pmath-boxes";
const char *const Clipboard::BoxesBinary = "application/x-pmath-boxes";
const char *const Clipboard::BitmapImage = "image/bmp";

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
  struct abf_info_t *info = (struct abf_info_t*)closure;
  
  info->success = info->self->add_binary(
                    info->mimetype,
                    readable,
                    (size_t) * writable - (size_t)readable);
}

bool OpenedClipboard::add_image(cairo_surface_t *image) {
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
  return 0;
}
