#include <gui/win32/api/win32-touch.h>


using namespace richmath;

// See Firefox issue "Implement ui.caretBlinkCount on Windows" -- https://bugzilla.mozilla.org/show_bug.cgi?id=1725454
#ifndef SPI_GETCARETTIMEOUT
#  define SPI_GETCARETTIMEOUT 0x2022
#endif


BOOL (WINAPI * Win32Touch::GetGestureInfo)(HANDLE, PGESTUREINFO) = nullptr;
BOOL (WINAPI * Win32Touch::CloseGestureInfoHandle)(HANDLE) = nullptr;
BOOL (WINAPI * Win32Touch::SetGestureConfig)(HWND, DWORD, UINT, PGESTURECONFIG, UINT) = nullptr;
BOOL (WINAPI * Win32Touch::GetGestureConfig)(HWND, DWORD, DWORD, PUINT, PGESTURECONFIG, UINT) = nullptr;
BOOL (WINAPI * Win32Touch::RegisterTouchWindow)(HWND, ULONG) = nullptr;
BOOL (WINAPI * Win32Touch::UnregisterTouchWindow)(HWND) = nullptr;
BOOL (WINAPI * Win32Touch::GetTouchInputInfo)(HANDLE, UINT, PTOUCHINPUT, int) = nullptr;
BOOL (WINAPI * Win32Touch::CloseTouchInputHandle)(HANDLE) = nullptr;
BOOL (WINAPI * Win32Touch::GetCurrentInputMessageSource)(INPUT_MESSAGE_SOURCE*) = nullptr;
BOOL (WINAPI * Win32Touch::EnableMouseInPointer)(BOOL) = nullptr;
BOOL (WINAPI * Win32Touch::GetPointerType)(UINT32, POINTER_INPUT_TYPE*) = nullptr;

HMODULE Win32Touch::user32 = nullptr;

DeviceKind Win32Touch::get_mouse_message_source(int *id, LPARAM messageExtraInfo) {
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms703320(v=vs.85).aspx
  // Distinguishing Pen Input from Mouse and Touch
  if(id)
    *id = (messageExtraInfo & 0x7F);
    
  bool isPenOrTouch = (messageExtraInfo & 0xFFFFFF00) == 0xFF515700;
  bool isTouch = isPenOrTouch && ((messageExtraInfo & 0x80) != 0);
  
  return isPenOrTouch ? (isTouch ? DeviceKind::Touch : DeviceKind::Pen) : DeviceKind::Mouse;
}

void Win32Touch::init() {
  static Win32Touch w;
}

Win32Touch::Win32Touch()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  fprintf(stderr, "Win32Touch init\n");
  if(user32)
    return;
  
  user32 = LoadLibrary("user32.dll");
  if(user32) {
    GetGestureInfo = (BOOL (WINAPI *)(HANDLE, PGESTUREINFO))
                     GetProcAddress(user32, "GetGestureInfo");
                     
    CloseGestureInfoHandle = (BOOL (WINAPI *)(HANDLE))
                             GetProcAddress(user32, "CloseGestureInfoHandle");
                             
    SetGestureConfig = (BOOL (WINAPI *)(HWND, DWORD, UINT, PGESTURECONFIG, UINT))
                       GetProcAddress(user32, "SetGestureConfig");
                       
    GetGestureConfig = (BOOL (WINAPI *)(HWND, DWORD, DWORD, PUINT, PGESTURECONFIG, UINT))
                       GetProcAddress(user32, "GetGestureConfig");
                       
    RegisterTouchWindow = (BOOL (WINAPI *)(HWND, ULONG))
                          GetProcAddress(user32, "RegisterTouchWindow");
                          
    UnregisterTouchWindow = (BOOL (WINAPI *)(HWND))
                            GetProcAddress(user32, "UnregisterTouchWindow");
                            
    GetTouchInputInfo = (BOOL (WINAPI *)(HANDLE, UINT, PTOUCHINPUT, int))
                        GetProcAddress(user32, "GetTouchInputInfo");
                        
    CloseTouchInputHandle = (BOOL (WINAPI *)(HANDLE))
                            GetProcAddress(user32, "CloseTouchInputHandle");
    
    GetCurrentInputMessageSource = (BOOL (WINAPI *)(INPUT_MESSAGE_SOURCE*))
                                   GetProcAddress(user32, "GetCurrentInputMessageSource");
    
    EnableMouseInPointer = (BOOL (WINAPI *)(BOOL))
                           GetProcAddress(user32, "EnableMouseInPointer");
    
    GetPointerType = (BOOL (WINAPI *)(UINT32, POINTER_INPUT_TYPE*))
                     GetProcAddress(user32, "GetPointerType");
  }
}

Win32Touch::~Win32Touch() {
  FreeLibrary(user32);   user32 = nullptr;
  
  GetGestureInfo = nullptr;
  CloseGestureInfoHandle = nullptr;
  SetGestureConfig = nullptr;
  GetGestureConfig = nullptr;
  RegisterTouchWindow = nullptr;
  UnregisterTouchWindow = nullptr;
  GetTouchInputInfo = nullptr;
  CloseTouchInputHandle = nullptr;
  GetCurrentInputMessageSource = nullptr;
  EnableMouseInPointer = nullptr;
}

DWORD Win32Touch::get_caret_timeout() {
  DWORD result = INFINITE;
  if(SystemParametersInfoW(SPI_GETCARETTIMEOUT, 0, &result, 0))
    return result;
  else
    return INFINITE;
//  DWORD result = INFINITE;
//  HKEY key = nullptr;
//  LONG status = RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_READ, &key);
//  if(status == ERROR_SUCCESS) {
//    DWORD size = sizeof(DWORD);
//    LONG status_et = RegGetValueW(key, nullptr, L"CaretTimeout", RRF_RT_REG_DWORD, nullptr, &result, &size);
//    if(status_et != ERROR_SUCCESS) {
//      result = INFINITE;
//    }
//  
//    RegCloseKey(key);
//  }
//  
//  return result;
}
