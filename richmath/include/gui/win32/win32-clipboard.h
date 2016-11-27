#ifndef __GUI__WIN32__WIN32_CLIPBOARD_H__
#define __GUI__WIN32__WIN32_CLIPBOARD_H__

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/clipboard.h>
#include <util/hashtable.h>


namespace richmath {
  class Win32Clipboard: public Clipboard {
    public:
      static Win32Clipboard obj;
      
    public:
      virtual ~Win32Clipboard();
      
      virtual bool has_format(String mimetype) override;
      
      virtual ReadableBinaryFile read_as_binary_file(String mimetype) override;
      virtual String             read_as_text(String mimetype) override;
      
      virtual SharedPtr<OpenedClipboard> open_write() override;
      virtual cairo_surface_t *create_image(String mimetype, double width, double height) override;
      
    public:
      static Hashtable<String, unsigned int> mime_to_win32cbformat;
      
      static void init();
      static void done();
      
    private:
      Win32Clipboard();
  };
}

#endif // __GUI__WIN32__WIN32_CLIPBOARD_H__
