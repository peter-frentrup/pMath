#include <gui/win32/win32-touch.h>


using namespace richmath;


BOOL (WINAPI * Win32Touch::GetGestureInfo)(HANDLE, PGESTUREINFO) = 0;
BOOL (WINAPI * Win32Touch::CloseGestureInfoHandle)(HANDLE) = 0;

HMODULE Win32Touch::user32 = 0;

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
