#ifndef RICHMATH__GUI__WIN32__OLE__VIRTUAL_DESKTOPS_H__INCLUDED
#define RICHMATH__GUI__WIN32__OLE__VIRTUAL_DESKTOPS_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <ole2.h>
#include <objectarray.h>


/* https://stackoverflow.com/questions/32416843/altering-win10-virtual-desktop-behavior/32417530#32417530
   https://www.cyberforum.ru/blogs/105416/blog3671.html
   https://github.com/tplk/VirDesk
*/

// WinRT specific
class IApplicationView : public IUnknown {
  public:
};

extern const CLSID CLSID_ImmersiveShell;
extern const CLSID CLSID_VirtualDesktopManager;
extern const CLSID CLSID_VirtualDesktopManagerInternal;
extern const CLSID CLSID_VirtualDesktopNotificationService;
extern const CLSID CLSID_VirtualDesktopPinnedApps;


extern const IID IID_IVirtualDesktop;
class IVirtualDesktop : public IUnknown {
  public:
    virtual HRESULT STDMETHODCALLTYPE IsViewVisible(
      IApplicationView *pView,
      int *pfVisible) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE GetID(
      GUID *pGuid) = 0;
};

enum class AdjacentDesktop : int {
  LeftDirection = 3,
  RightDirection = 4
};

extern const IID IID_IVirtualDesktopManagerInternal_10130;
class IVirtualDesktopManagerInternal_10130 : public IUnknown {
  public:
    virtual HRESULT STDMETHODCALLTYPE GetCount(
      UINT *pCount) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE MoveViewToDesktop(
      IApplicationView *pView,
      IVirtualDesktop *pDesktop) = 0;
    
//    // inserted in 10240 
//    virtual HRESULT STDMETHODCALLTYPE CanViewMoveDesktops(
//      IApplicationView *pView,
//      int *pfCanViewMoveDesktops) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE GetCurrentDesktop(
      IVirtualDesktop** desktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE GetDesktops(
      IObjectArray **ppDesktops) = 0;
    
    virtual HRESULT STDMETHODCALLTYPE GetAdjacentDesktop(
      IVirtualDesktop *pDesktopReference,
      AdjacentDesktop uDirection,
      IVirtualDesktop **ppAdjacentDesktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE SwitchDesktop(
      IVirtualDesktop *pDesktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE CreateDesktopW(
      IVirtualDesktop **ppNewDesktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE RemoveDesktop(
        IVirtualDesktop *pRemove,
        IVirtualDesktop *pFallbackDesktop) = 0;
        
//    // appended in 10240
//    virtual HRESULT STDMETHODCALLTYPE FindDesktop(
//        GUID *desktopId,
//        IVirtualDesktop **ppDesktop) = 0;
};

extern const IID IID_IVirtualDesktopManagerInternal_10240;
class IVirtualDesktopManagerInternal_10240 : public IUnknown {
  public:
    virtual HRESULT STDMETHODCALLTYPE GetCount(
      UINT *pCount) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE MoveViewToDesktop(
      IApplicationView *pView,
      IVirtualDesktop *pDesktop) = 0;
    
    // 10240
    virtual HRESULT STDMETHODCALLTYPE CanViewMoveDesktops(
      IApplicationView *pView,
      int *pfCanViewMoveDesktops) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE GetCurrentDesktop(
      IVirtualDesktop** desktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE GetDesktops(
      IObjectArray **ppDesktops) = 0;
    
    virtual HRESULT STDMETHODCALLTYPE GetAdjacentDesktop(
      IVirtualDesktop *pDesktopReference,
      AdjacentDesktop uDirection,
      IVirtualDesktop **ppAdjacentDesktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE SwitchDesktop(
      IVirtualDesktop *pDesktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE CreateDesktopW(
      IVirtualDesktop **ppNewDesktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE RemoveDesktop(
        IVirtualDesktop *pRemove,
        IVirtualDesktop *pFallbackDesktop) = 0;
 
    // 10240
    virtual HRESULT STDMETHODCALLTYPE FindDesktop(
        GUID *desktopId,
        IVirtualDesktop **ppDesktop) = 0;
};

extern const IID IID_IVirtualDesktopManagerInternal_14328;
class IVirtualDesktopManagerInternal_14328 : public IUnknown {
  public:
    virtual HRESULT STDMETHODCALLTYPE GetCount(
      UINT *pCount) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE MoveViewToDesktop(
      IApplicationView *pView,
      IVirtualDesktop *pDesktop) = 0;
    
    virtual HRESULT STDMETHODCALLTYPE CanViewMoveDesktops(
      IApplicationView *pView,
      int *pfCanViewMoveDesktops) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE GetCurrentDesktop(
      IVirtualDesktop** desktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE GetDesktops(
      IObjectArray **ppDesktops) = 0;
    
    virtual HRESULT STDMETHODCALLTYPE GetAdjacentDesktop(
      IVirtualDesktop *pDesktopReference,
      AdjacentDesktop uDirection,
      IVirtualDesktop **ppAdjacentDesktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE SwitchDesktop(
      IVirtualDesktop *pDesktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE CreateDesktopW(
      IVirtualDesktop **ppNewDesktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE RemoveDesktop(
        IVirtualDesktop *pRemove,
        IVirtualDesktop *pFallbackDesktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE FindDesktop(
        GUID *desktopId,
        IVirtualDesktop **ppDesktop) = 0;
};

extern const IID IID_IVirtualDesktopManager;
class IVirtualDesktopManager : public IUnknown {
  public:
    virtual HRESULT STDMETHODCALLTYPE IsWindowOnCurrentVirtualDesktop(
      /* [in] */ HWND topLevelWindow,
      /* [out] */ BOOL *onCurrentDesktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE GetWindowDesktopId(
      /* [in] */ HWND topLevelWindow,
      /* [out] */ GUID *desktopId) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE MoveWindowToDesktop(
      /* [in] */ HWND topLevelWindow,
      /* [in] */ REFGUID desktopId) = 0;
};

extern const IID IID_IVirtualDesktopNotification;
class IVirtualDesktopNotification : public IUnknown {
  public:
    virtual HRESULT STDMETHODCALLTYPE VirtualDesktopCreated(
      IVirtualDesktop *pDesktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE VirtualDesktopDestroyBegin(
      IVirtualDesktop *pDesktopDestroyed,
      IVirtualDesktop *pDesktopFallback) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE VirtualDesktopDestroyFailed(
      IVirtualDesktop *pDesktopDestroyed,
      IVirtualDesktop *pDesktopFallback) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE VirtualDesktopDestroyed(
      IVirtualDesktop *pDesktopDestroyed,
      IVirtualDesktop *pDesktopFallback) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE ViewVirtualDesktopChanged(
      IApplicationView *pView) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE CurrentVirtualDesktopChanged(
      IVirtualDesktop *pDesktopOld,
      IVirtualDesktop *pDesktopNew) = 0;
};

extern const IID IID_IVirtualDesktopNotificationService;
class IVirtualDesktopNotificationService : public IUnknown {
  public:
    virtual HRESULT STDMETHODCALLTYPE Register(
      IVirtualDesktopNotification *pNotification,
      DWORD *pdwCookie) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE Unregister(
      DWORD dwCookie) = 0;
};

#ifdef _MSC_VER
  class __declspec(uuid("FF72FFDD-BE7E-43FC-9C03-AD81681E88E4")) IVirtualDesktop;
  class __declspec(uuid("EF9F1A6C-D3CC-4358-B712-F84B635BEBE7")) IVirtualDesktopManagerInternal_10130;
  class __declspec(uuid("AF8DA486-95BB-4460-B3B7-6E7A6B2962B5")) IVirtualDesktopManagerInternal_10240;
  class __declspec(uuid("f31574d6-b682-4cdc-bd56-1827860abec6")) IVirtualDesktopManagerInternal_14328;
  class __declspec(uuid("a5cd92ff-29be-454c-8d04-d82879fb3f1b")) IVirtualDesktopManager;
  class __declspec(uuid("C179334C-4295-40D3-BEA1-C654D965605A")) IVirtualDesktopNotification;
  class __declspec(uuid("0CD45E71-D927-4F15-8B0A-8FEF525337BF")) IVirtualDesktopNotificationService;
#elif defined(__GNUC__)
#  define RICHMATH_MAKE_MINGW_UUID(cls) \
     template<> inline const GUID &__mingw_uuidof<cls>()  { return IID_ ## cls; } \
     template<> inline const GUID &__mingw_uuidof<cls*>() { return IID_ ## cls; }
  
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktop)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopManagerInternal_10130)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopManagerInternal_10240)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopManagerInternal_14328)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopManager)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopNotification)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopNotificationService)
#endif


#endif // RICHMATH__GUI__WIN32__OLE__VIRTUAL_DESKTOPS_H__INCLUDED
