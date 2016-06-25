#define WINVER 0x0500

#include <gui/win32/win32-clipboard.h>
#include <gui/win32/basic-win32-widget.h>

#include <cairo-win32.h>
#include <cmath>


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
      
      return nullptr != SetClipboardData(id, hglb);
    }
    
    virtual bool add_text(String mimetype, String data) {
      unsigned int id = Win32Clipboard::mime_to_win32cbformat[mimetype];
      if(!id)
        return false;
        
      int len = data.length();
      HANDLE hglb = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(uint16_t));
      if(!hglb)
        return false;
        
      uint16_t *dst = (uint16_t *)GlobalLock(hglb);
      memcpy(dst, data.buffer(), len * sizeof(uint16_t));
      for(int i = 0; i < len; ++i) {
        if(dst[i] == '\0')
          dst[i] = '?';
      }
      dst[len] = '\0';
      GlobalUnlock(hglb);
      
      return nullptr != SetClipboardData(id, hglb);
    }
    
    virtual bool add_image(String suggested_mimetype, cairo_surface_t *image) {
      if(cairo_surface_get_type(image) == CAIRO_SURFACE_TYPE_WIN32) {
        cairo_surface_t *img = cairo_win32_surface_get_image(image);
        if(!img)
          return false;
          
//        HDC dc = cairo_win32_surface_get_dc(image);
//        if(dc) {
//          cairo_surface_flush(image);
//
//          int width  = cairo_image_surface_get_width( img);
//          int height = cairo_image_surface_get_height(img);
//
//          HDC memDC     = CreateCompatibleDC(dc);
//          HBITMAP memBM = CreateCompatibleBitmap(dc, width, height);
//          SelectObject(memDC, memBM);
//
//          BitBlt(memDC, 0, 0, width, height, dc, 0, 0, SRCCOPY);
//
//          bool success = nullptr != SetClipboardData(CF_BITMAP, memBM);
//
//          DeleteDC(memDC);
//
//          return success;
//
////          HBITMAP dummy_bmp = CreateCompatibleBitmap(dc, 1, 1);
////          if(!dummy_bmp)
////            return false;
////
////          HBITMAP bmp = (HBITMAP)SelectObject(dc, (HGDIOBJ)dummy_bmp);
////
////          return nullptr != SetClipboardData(CF_BITMAP, bmp);
//        }

        if(cairo_surface_get_type(img) == CAIRO_SURFACE_TYPE_IMAGE) {
          if(img) {
            int height = cairo_image_surface_get_height(img);
            int width  = cairo_image_surface_get_width(img);
            int stride = cairo_image_surface_get_stride(img);
            
            BITMAPINFOHEADER bmi;
            memset(&bmi, 0, sizeof(bmi));
            bmi.biSize = sizeof(bmi);
            bmi.biWidth  = width;
            bmi.biHeight = height; /* upside down */
            bmi.biPlanes = 1;
            bmi.biXPelsPerMeter = (LONG)((double)96 * 100 / 2.54 + 0.5);
            bmi.biYPelsPerMeter = (LONG)((double)96 * 100 / 2.54 + 0.5);
            
            cairo_format_t format = cairo_image_surface_get_format(img);
            switch(format) {
              case CAIRO_FORMAT_RGB24:
              case CAIRO_FORMAT_ARGB32:
                bmi.biBitCount     = 32;
                bmi.biCompression  = BI_RGB;
                bmi.biClrUsed      = 0;
                bmi.biClrImportant = 0;
                if(stride != 4 * width)
                  return false;
                break;
                
              default:
                return false;
            }
            
            cairo_surface_flush(img);
            const uint8_t *bits = cairo_image_surface_get_data(img);
            if(!bits)
              return false;
              
            HANDLE hglb = GlobalAlloc(GMEM_MOVEABLE, sizeof(bmi) + stride * height);
            if(!hglb)
              return false;
              
            uint8_t *dst = (uint8_t *)GlobalLock(hglb);
            memcpy(dst, &bmi, sizeof(bmi));
            //memcpy(dst + sizeof(bmi), bits, stride * height);
            
            bits = bits + stride * (height - 1);
            dst = dst + sizeof(bmi);
            for(int row = 0; row < height; ++row) {
              memcpy(dst, bits, stride);
              dst += stride;
              bits -= stride;
            }
            
            GlobalUnlock(hglb);
            
            return nullptr != SetClipboardData(CF_DIB, hglb);
          }
        }
      }
      
      
      return OpenedClipboard::add_image(suggested_mimetype, image);
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
    uint16_t *data = (uint16_t *)GlobalLock(hglb);
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
  mime_to_win32cbformat.set(Clipboard::PlainText,           CF_UNICODETEXT);
  mime_to_win32cbformat.set(Clipboard::PlatformBitmapImage, CF_DIB);
  
  mime_to_win32cbformat.set(Clipboard::BoxesText,
                            RegisterClipboardFormatA(Clipboard::BoxesText));
  mime_to_win32cbformat.set(Clipboard::BoxesBinary,
                            RegisterClipboardFormatA(Clipboard::BoxesBinary));
  mime_to_win32cbformat.set(Clipboard::SvgImage,
                            RegisterClipboardFormatA(Clipboard::SvgImage));
}

cairo_surface_t *Win32Clipboard::create_image(String mimetype, double width, double height) {
  if(mimetype.equals(Clipboard::PlatformBitmapImage)) {
    int w = (int)ceil(width);
    int h = (int)ceil(height);
    
    if(w < 1)
      w = 1;
    if(h < 1)
      h = 1;
      
    return cairo_win32_surface_create_with_dib(CAIRO_FORMAT_RGB24, w, h);
  }
  
  return Clipboard::create_image(mimetype, width, height);
}

void Win32Clipboard::done() {
  mime_to_win32cbformat.clear();
}

//} ... class Win32Clipboard
