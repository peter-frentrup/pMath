#include <gui/win32/win32-touch.h>


using namespace richmath;


BOOL (WINAPI * Win32Touch::GetGestureInfo)(HANDLE, PGESTUREINFO) = 0;
BOOL (WINAPI * Win32Touch::CloseGestureInfoHandle)(HANDLE) = 0;

HMODULE Win32Touch::user32 = 0;

PointerEventSource Win32Touch::get_mouse_message_source(int *id, LPARAM messageExtraInfo) {
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms703320(v=vs.85).aspx
  // Distinguishing Pen Input from Mouse and Touch
  if(id)
    *id = (messageExtraInfo & 0x7F);
  
  bool isPenOrTouch = (messageExtraInfo & 0xFFFFFF00) == 0xFF515700;
  bool isTouch = isPenOrTouch && ((messageExtraInfo & 0x80) != 0);
  
  return isPenOrTouch ? (isTouch ? PointerEventSource::Touch : PointerEventSource::Pen) : PointerEventSource::Mouse; 
}

void Win32Touch::init() {
  static Win32Touch w;
}

Win32Touch::Win32Touch(): Base() {
  user32 = LoadLibrary("user32.dll");
  
  if(user32) {
    GetGestureInfo = (BOOL (WINAPI *)(HANDLE, PGESTUREINFO))
                     GetProcAddress(user32, "GetGestureInfo");
                     
    CloseGestureInfoHandle = (BOOL (WINAPI *)(HANDLE))
                             GetProcAddress(user32, "CloseGestureInfoHandle");
  }
}

Win32Touch::~Win32Touch() {
  FreeLibrary(user32);   user32 = 0;
  
  GetGestureInfo = 0;
}
