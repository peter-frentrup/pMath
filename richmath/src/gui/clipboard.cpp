#include <gui/clipboard.h>

using namespace richmath;

class DummyClipboard: public Clipboard{
  public:
    virtual bool has_format(String mimetype){ 
      return false; 
    }
    
    virtual Expr read_as_binary_file(String mimetype){
      return Expr(); 
    }
    
    virtual String read_as_text(String mimetype){
      return String();
    }
    
    virtual SharedPtr<OpenedClipboard> open_write(){
      return 0;
    }
};

static DummyClipboard dummy_cb;

Clipboard *Clipboard::dummy = &dummy_cb;
Clipboard *Clipboard::std   = &dummy_cb;

const char * const Clipboard::PlainText   = "text/plain";
const char * const Clipboard::BoxesText   = "text/x-pmath-boxes";
const char * const Clipboard::BoxesBinary = "application/x-pmath-boxes";

  struct abf_info_t{
    OpenedClipboard *self;
    String           mimetype;
    bool             success;
  };
  
  static void abf_flush(
    uint8_t        *readable, 
    uint8_t       **writable, 
    const uint8_t  *end, 
    void           *closure
  ){
    struct abf_info_t *info = (struct abf_info_t*)closure;
    
    info->success = info->self->add_binary(
      info->mimetype, 
      readable, 
      (size_t)*writable - (size_t)readable);
  }

bool OpenedClipboard::add_binary_file(String mimetype, Expr binfile){
  struct abf_info_t info;
  info.self     = this;
  info.mimetype = mimetype;
  info.success  = false;
  
  pmath_file_binary_buffer_manipulate(binfile.get(), abf_flush, &info);
  
  return info.success;
}
