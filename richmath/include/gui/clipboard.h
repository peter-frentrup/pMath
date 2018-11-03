#ifndef RICHMATH__GUI__CLIPBOARD_H__INCLUDED
#define RICHMATH__GUI__CLIPBOARD_H__INCLUDED

#include <util/sharedptr.h>
#include <util/pmath-extra.h>

typedef struct _cairo_surface cairo_surface_t;


namespace richmath {
  class OpenedClipboard: public Shareable {
    public:
      virtual bool add_binary(String mimetype, void *data, size_t size) = 0;
      virtual bool add_text(String mimetype, String data) = 0;
      
      virtual bool add_image(String suggested_mimetype, cairo_surface_t *image);
      virtual bool add_binary_buffer(String mimetype, Expr binbuffer);
  };
  
  class Clipboard: public Base {
    public:
      static Clipboard *dummy;
      static Clipboard *std;
      
      // mimetypes:
      static const char *const PlainText;
      static const char *const BoxesText;
      static const char *const BoxesBinary;
      static const char *const PlatformBitmapImage;
      static const char *const SvgImage;
      
    public:
      Clipboard()
        : Base()
      {
        SET_BASE_DEBUG_TAG(typeid(*this).name());
      }
      virtual ~Clipboard() {}
      
      virtual bool has_format(String mimetype) = 0;
      
      virtual ReadableBinaryFile read_as_binary_file(String mimetype) = 0;
      virtual String             read_as_text(       String mimetype) = 0;
      
      virtual SharedPtr<OpenedClipboard> open_write() = 0;
      virtual cairo_surface_t *create_image(String mimetype, double width, double height);
  };
}

#endif // RICHMATH__GUI__CLIPBOARD_H__INCLUDED
