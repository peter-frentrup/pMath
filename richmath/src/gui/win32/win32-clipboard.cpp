#define WINVER 0x0500

#include <gui/win32/win32-clipboard.h>
#include <gui/win32/basic-win32-widget.h>

using namespace richmath;

static HWND hwnd_message = HWND_MESSAGE;

class Win32ClipboardInfo: public BasicWin32Widget {
  public:
    Win32ClipboardInfo()
      : BasicWin32Widget(0, 0, 0, 0, 0, 0, &hwnd_message)
    {
      init(); // total exception!!! normally not callable in constructor
    }
};

static Win32ClipboardInfo w32cbinfo;

class OpenedWin32Clipboard: public OpenedClipboard {
  public:
    OpenedWin32Clipboard()
      : OpenedClipboard()
    {
      if(OpenClipboard(w32cbinfo.hwnd()))
        EmptyClipboard();
    }
    
    virtual ~OpenedWin32Clipboard() {
      CloseClipboard();
    }
    
    virtual bool add_binary(String mimetype, void *data, size_t size) {
      unsigned int id = Win32Clipboard::mime_to_win32cbformat[mimetype];
      if(!id)
        return false;
        
      HANDLE hglb = GlobalAlloc(GMEM_MOVEABLE, size);
      if(!hglb)
        return false;
        
      void *dst = GlobalLock(hglb);
      memcpy(dst, data, size);
      GlobalUnlock(hglb);
      
      return NULL != SetClipboardData(id, hglb);
    }
    
    virtual bool add_text(String mimetype, String data) {
      unsigned int id = Win32Clipboard::mime_to_win32cbformat[mimetype];
      if(!id)
        return false;
        
      int len = data.length();
      HANDLE hglb = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(uint16_t));
      if(!hglb)
        return false;
        
      uint16_t *dst = (uint16_t*)GlobalLock(hglb);
      memcpy(dst, data.buffer(), len * sizeof(uint16_t));
      for(int i = 0; i < len; ++i) {
        if(dst[i] == '\0')
          dst[i] = '?';
      }
      dst[len] = '\0';
      GlobalUnlock(hglb);
      
      return NULL != SetClipboardData(id, hglb);
    }
};

//{ class Win32Clipboard ...

Win32Clipboard Win32Clipboard::obj;
Hashtable<String, unsigned int> Win32Clipboard::mime_to_win32cbformat;

Win32Clipboard::Win32Clipboard()
  : Clipboard()
{
  Clipboard::std = this;
}

Win32Clipboard::~Win32Clipboard() {
  if(Clipboard::std == this)
    Clipboard::std = Clipboard::dummy;
}

bool Win32Clipboard::has_format(String mimetype) {
  unsigned int id = mime_to_win32cbformat[mimetype];
  
  if(id)
    return 0 != IsClipboardFormatAvailable(id);
    
  return false;
}

ReadableBinaryFile Win32Clipboard::read_as_binary_file(String mimetype) {
  unsigned int id = mime_to_win32cbformat[mimetype];
  
  if(!id || !IsClipboardFormatAvailable(id))
    return ReadableBinaryFile();
    
  if(!OpenClipboard(w32cbinfo.hwnd()))
    return ReadableBinaryFile();
    
  ReadableBinaryFile result;
  HANDLE hglb = GetClipboardData(id);
  if(hglb) {
    void *data = GlobalLock(hglb);
    if(data) {
      size_t size = GlobalSize(hglb);
      
      result = ReadableBinaryFile(pmath_file_create_binary_buffer(size));
      pmath_file_write(result.get(), data, size);
      
      GlobalUnlock(hglb);
    }
  }
  
  CloseClipboard();
  return result;
}

String Win32Clipboard::read_as_text(String mimetype) {
  unsigned int id = mime_to_win32cbformat[mimetype];
  
  if(!id || !IsClipboardFormatAvailable(id))
    return Expr();
    
  if(!OpenClipboard(w32cbinfo.hwnd()))
    return Expr();
    
  String result;
  HANDLE hglb = GetClipboardData(id);
  if(hglb) {
    uint16_t *data = (uint16_t*)GlobalLock(hglb);
    if(data) {
      size_t size = GlobalSize(hglb);
      
      int len = 0;
      while(len < (int)size && data[len])
        ++len;
        
      result = String::FromUcs2(data, len);
      
      GlobalUnlock(hglb);
    }
  }
  
  CloseClipboard();
  return result;
}

SharedPtr<OpenedClipboard> Win32Clipboard::open_write() {
  return new OpenedWin32Clipboard();
}

void Win32Clipboard::init() {
  mime_to_win32cbformat.set(Clipboard::PlainText, CF_UNICODETEXT);
  
  mime_to_win32cbformat.set(Clipboard::BoxesText,
                            RegisterClipboardFormatA(Clipboard::BoxesText));
  mime_to_win32cbformat.set(Clipboard::BoxesBinary,
                            RegisterClipboardFormatA(Clipboard::BoxesBinary));
}

void Win32Clipboard::done() {
  mime_to_win32cbformat.clear();
}

//} ... class Win32Clipboard
