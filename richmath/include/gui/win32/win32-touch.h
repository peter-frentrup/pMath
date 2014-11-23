#ifndef __GUI__WIN32__WIN32_GESTURES_H__
#define __GUI__WIN32__WIN32_GESTURES_H__

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif


#include <windows.h>
#include <util/base.h>


namespace richmath {
  /* (optional) Windows 7 Touch API */
  class Win32Touch: public Base {
    public:
      typedef struct _GESTUREINFO {
        UINT      cbSize;
        DWORD     dwFlags;
        DWORD     dwID;
        HWND      hwndTarget;
        POINTS    ptsLocation;
        DWORD     dwInstanceID;
        DWORD     dwSequenceID;
        ULONGLONG ullArguments;
        UINT      cbExtraArgs;
      } GESTUREINFO, *PGESTUREINFO;
    
    public:
      static BOOL (WINAPI *GetGestureInfo)(HANDLE, PGESTUREINFO);
      static BOOL (WINAPI *CloseGestureInfoHandle)(HANDLE);
      
      static void init();
      
    private:
      static HMODULE user32;
    
    protected:
      Win32Touch();
      ~Win32Touch();
  };

#ifndef WM_GESTURE
#  define WM_GESTURE         0x0119
#  define WM_GESTURENOTIFY   0x011A
#endif
  
#ifndef GF_BEGIN
#  define GF_BEGIN    0x00000001
#  define GF_INERTIA  0x00000002
#  define GF_END      0x00000004
#endif

#ifndef GID_BEGIN
#  define GID_BEGIN         1
#  define GID_END           2
#  define GID_ZOOM          3
#  define GID_PAN           4
#  define GID_ROTATE        5
#  define GID_TWOFINGERTAP  6
#  define GID_PRESSANDTAP   7
#  define GID_ROLLOVER      GID_PRESSANDTAP
#endif
}


#endif // __GUI__WIN32__WIN32_GESTURES_H__
