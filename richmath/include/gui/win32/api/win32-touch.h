#ifndef RICHMATH__GUI__WIN32__API__WIN32_TOUCH_H__INCLUDED
#define RICHMATH__GUI__WIN32__API__WIN32_TOUCH_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif


#include <windows.h>
#include <boxes/box.h>


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
      
      typedef struct _GESTURECONFIG {
        DWORD dwID;
        DWORD dwWant;
        DWORD dwBlock;
      } GESTURECONFIG, *PGESTURECONFIG;
     
      typedef struct tagGESTURENOTIFYSTRUCT {
        UINT   cbSize;
        DWORD  dwFlags;
        HWND   hwndTarget;
        POINTS ptsLocation;
        DWORD  dwInstanceID;
      } GESTURENOTIFYSTRUCT, *PGESTURENOTIFYSTRUCT;

      typedef struct _TOUCHINPUT {
        LONG      x;
        LONG      y;
        HANDLE    hSource;
        DWORD     dwID;
        DWORD     dwFlags;
        DWORD     dwMask;
        DWORD     dwTime;
        ULONG_PTR dwExtraInfo;
        DWORD     cxContact;
        DWORD     cyContact;
      } TOUCHINPUT, *PTOUCHINPUT;
      
      enum INPUT_MESSAGE_DEVICE_TYPE  {
        IMDT_UNAVAILABLE        = 0x00000000,       // not specified
        IMDT_KEYBOARD           = 0x00000001,       // from keyboard
        IMDT_MOUSE              = 0x00000002,       // from mouse
        IMDT_TOUCH              = 0x00000004,       // from touch
        IMDT_PEN                = 0x00000008,       // from pen
        IMDT_TOUCHPAD           = 0x00000010,       // from touchpad, since Windows 8.1, does not seem to work during WM_MOUSEMOVE
     };
      
      enum INPUT_MESSAGE_ORIGIN_ID {
        IMO_UNAVAILABLE = 0x00000000,  // not specified
        IMO_HARDWARE    = 0x00000001,  // from a hardware device or injected by a UIAccess app
        IMO_INJECTED    = 0x00000002,  // injected via SendInput() by a non-UIAccess app
        IMO_SYSTEM      = 0x00000004,  // injected by the system
      };
      
      struct INPUT_MESSAGE_SOURCE {
        INPUT_MESSAGE_DEVICE_TYPE deviceType;
        INPUT_MESSAGE_ORIGIN_ID   originId;
      };
      
      enum POINTER_INPUT_TYPE {
        PT_POINTER  = 1,   // Generic pointer
        PT_TOUCH    = 2,   // Touch
        PT_PEN      = 3,   // Pen
        PT_MOUSE    = 4,   // Mouse
        PT_TOUCHPAD = 5,   // Touchpad
      };

    public:
      static BOOL (WINAPI *GetGestureInfo)(HANDLE, PGESTUREINFO);
      static BOOL (WINAPI *CloseGestureInfoHandle)(HANDLE);
      static BOOL (WINAPI *SetGestureConfig)(HWND, DWORD, UINT, PGESTURECONFIG, UINT);
      static BOOL (WINAPI *GetGestureConfig)(HWND, DWORD, DWORD, PUINT, PGESTURECONFIG, UINT);
      
      static BOOL (WINAPI *RegisterTouchWindow)(HWND, ULONG);
      static BOOL (WINAPI *UnregisterTouchWindow)(HWND);
      static BOOL (WINAPI *GetTouchInputInfo)(HANDLE, UINT, PTOUCHINPUT, int);
      static BOOL (WINAPI *CloseTouchInputHandle)(HANDLE);
      
      static BOOL (WINAPI *GetCurrentInputMessageSource)(INPUT_MESSAGE_SOURCE*);
      static BOOL (WINAPI *EnableMouseInPointer)(BOOL fEnable);
      
      static BOOL (WINAPI *GetPointerType)(UINT32, POINTER_INPUT_TYPE*);
      
      static DeviceKind get_mouse_message_source(int *id) {
        return get_mouse_message_source(id, ::GetMessageExtraInfo());
      }
      static DeviceKind get_mouse_message_source(int *id, LPARAM messageExtraInfo);
    
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

#ifndef GC_ALLGESTURES
#  define GC_ALLGESTURES                         0x00000001

#  define GC_ZOOM                                0x00000001

#  define GC_PAN                                 0x00000001
#  define GC_PAN_WITH_SINGLE_FINGER_VERTICALLY   0x00000002
#  define GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY 0x00000004
#  define GC_PAN_WITH_GUTTER                     0x00000008
#  define GC_PAN_WITH_INERTIA                    0x00000010

#  define GC_ROTATE                              0x00000001

#  define GC_TWOFINGERTAP                        0x00000001

#  define GC_PRESSANDTAP                         0x00000001
#  define GC_ROLLOVER                            GC_PRESSANDTAP

#  define GESTURECONFIGMAXCOUNT                  256

#  define GCF_INCLUDE_ANCESTORS                  0x00000001
#endif

#ifndef WM_TOUCH
#  define WM_TOUCH   0x0240
#endif

#ifndef TOUCHEVENTF_MOVE
#  define TOUCHEVENTF_MOVE        0x0001
#  define TOUCHEVENTF_DOWN        0x0002
#  define TOUCHEVENTF_UP          0x0004
#  define TOUCHEVENTF_INRANGE     0x0008
#  define TOUCHEVENTF_PRIMARY     0x0010
#  define TOUCHEVENTF_NOCOALESCE  0x0020
#  define TOUCHEVENTF_PEN         0x0040
#  define TOUCHEVENTF_PALM        0x0080

#  define TOUCHINPUTMASKF_TIMEFROMSYSTEM  0x0001
#  define TOUCHINPUTMASKF_EXTRAINFO       0x0002
#  define TOUCHINPUTMASKF_CONTACTAREA     0x0004

#  define TWF_FINETOUCH (0x00000001)
#  define TWF_WANTPALM  (0x00000002)
#endif

}


#endif // RICHMATH__GUI__WIN32__API__WIN32_TOUCH_H__INCLUDED
