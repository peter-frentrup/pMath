#ifndef __GUI__CLIPBOARD_H__
#define __GUI__CLIPBOARD_H__

#include <util/sharedptr.h>
#include <util/pmath-extra.h>


namespace richmath {
  class OpenedClipboard: public Shareable {
    public:
      virtual bool add_binary(String mimetype, void *data, size_t size) = 0;
      virtual bool add_text(String mimetype, String data) = 0;
      
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
      
    public:
      virtual ~Clipboard() {}
      
      virtual bool has_format(String mimetype) = 0;
      
      virtual ReadableBinaryFile read_as_binary_file(String mimetype) = 0;
      virtual String             read_as_text(String mimetype) = 0;
      
      virtual SharedPtr<OpenedClipboard> open_write() = 0;
  };
}

#endif // __GUI__CLIPBOARD_H__
