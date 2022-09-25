#ifndef RICHMATH__GUI__WIN32__OLE__VIRTUAL_DESKTOPS_H__INCLUDED
#define RICHMATH__GUI__WIN32__OLE__VIRTUAL_DESKTOPS_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <ole2.h>
#include <objectarray.h>
#include <hstring.h>


/* https://stackoverflow.com/questions/32416843/altering-win10-virtual-desktop-behavior/32417530#32417530
   https://www.cyberforum.ru/blogs/105416/blog3671.html
   https://github.com/tplk/VirDesk
   https://github.com/Grabacr07/VirtualDesktop
*/

// WinRT specific
class IApplicationView : public IUnknown {
  public:
};

extern const CLSID CLSID_ImmersiveShell;
extern const CLSID CLSID_VirtualDesktopManager;
extern const CLSID CLSID_VirtualDesktopManagerInternal;
extern const CLSID CLSID_VirtualDesktopNotificationService;
//extern const CLSID CLSID_VirtualDesktopPinnedApps;


class IVirtualDesktop : public IUnknown {};

extern const IID IID_IVirtualDesktop_10240;
class IVirtualDesktop_10240 : public IVirtualDesktop {
  public:
    virtual HRESULT STDMETHODCALLTYPE IsViewVisible(
      /* [in]  */ IApplicationView *pView,
      /* [out] */ BOOL *pfVisible) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE GetID(
      /* [out] */ GUID *pGuid) = 0;
};

extern const IID IID_IVirtualDesktop_22000;
class IVirtualDesktop_22000: public IVirtualDesktop {
  public:
    virtual HRESULT STDMETHODCALLTYPE IsViewVisible(
      /* [in]  */ IApplicationView *pView,
      /* [out] */ BOOL *pfVisible) = 0;
      
    virtual HRESULT STDMETHODCALLTYPE GetID(
      /* [out] */ GUID *pGuid) = 0;
    
    virtual HRESULT STDMETHODCALLTYPE UnknownProc5(
      void *pv) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetName(
      /* [out] */ HSTRING *pName) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetWallpaperPath(
      /* [out] */ HSTRING *pPath) = 0;
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
//        const GUID *desktopId,
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
        const GUID *desktopId,
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
        const GUID *desktopId,
        IVirtualDesktop **ppDesktop) = 0;
};

extern const IID IID_IVirtualDesktopManagerInternal_22000;
class IVirtualDesktopManagerInternal_22000 : public IUnknown {
  public:
    virtual HRESULT STDMETHODCALLTYPE GetCount(
      HANDLE  hWndOrMon,
      UINT   *pCount) = 0;
      
    virtual HRESULT STDMETHODCALLTYPE MoveViewToDesktop(
      IApplicationView *pView,
      IVirtualDesktop  *pDesktop) = 0;
    
    virtual HRESULT STDMETHODCALLTYPE CanViewMoveDesktops(
      IApplicationView *pView,
      int              *pfCanViewMoveDesktops) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE GetCurrentDesktop(
      /* [in]  */ HANDLE            hWndOrMon,
      /* [out] */ IVirtualDesktop **desktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE GetDesktops(
      /* [in]  */ HANDLE         hWndOrMon,
      /* [out] */ IObjectArray **ppDesktops) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetAdjacentDesktop(
      /* [in]  */ IVirtualDesktop  *pDesktopReference,
      /* [in]  */ AdjacentDesktop   uDirection,
      /* [out] */ IVirtualDesktop **ppAdjacentDesktop) = 0;

    virtual HRESULT STDMETHODCALLTYPE SwitchDesktop(
      HANDLE           hWndOrMon,
      IVirtualDesktop *pDesktop) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE CreateDesktop(
      /* [in]  */ HANDLE hWndOrMon,
      /* [out] */ IVirtualDesktop **ppNewDesktop) = 0;

    virtual HRESULT STDMETHODCALLTYPE MoveDesktop(
      /* [in]  */ IVirtualDesktop *pDesktop, 
      /* [in]  */ HANDLE           hWndOrMon, 
      /* [in]  */ int              nIndex) = 0;

    virtual HRESULT STDMETHODCALLTYPE RemoveDesktop(
      /* [in]  */ IVirtualDesktop *pRemove,
      /* [in]  */ IVirtualDesktop *pFallbackDesktop) = 0;

    virtual HRESULT STDMETHODCALLTYPE FindDesktop(
      /* [in]  */ const GUID *desktopId,
      /* [out] */ IVirtualDesktop **ppDesktop) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetDesktopSwitchIncludeExcludeViews(
      /* [in]  */ IVirtualDesktop  *pDesktop, 
      /* [out] */ IObjectArray    **o1, 
      /* [out] */ IObjectArray    **o2) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetDesktopName(
      /* [in]  */ IVirtualDesktop *pDesktop, 
      /* [in]  */ HSTRING          name) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetDesktopWallpaper(
      /* [in]  */ IVirtualDesktop *pDesktop, 
      /* [in]  */ HSTRING          name) = 0;

    virtual HRESULT STDMETHODCALLTYPE UpdateWallpaperPathForAllDesktops(
      /* [in]  */ HSTRING path) = 0;

    virtual HRESULT STDMETHODCALLTYPE CopyDesktopState(
      /* [in]  */ IApplicationView *pView0, 
      /* [in]  */ IApplicationView *pView1) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetDesktopIsPerMonitor(
      /* [out] */ BOOL *pbState) = 0;
    
    virtual HRESULT STDMETHODCALLTYPE SetDesktopIsPerMonitor(
      /* [in]  */ BOOL bState) = 0;
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

class IVirtualDesktopNotification : public IUnknown {};

// Probably since build 10240. Comment on https://www.cyberforum.ru/blogs/105416/blog3671.html mentions 2015-07-13
extern const IID IID_IVirtualDesktopNotification_10240;
class IVirtualDesktopNotification_10240 : public IVirtualDesktopNotification {
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

// See https://github.com/Grabacr07/VirtualDesktop/blob/master/src/VirtualDesktop/Interop/Build22000/.interfaces/IVirtualDesktopNotification.cs
extern const IID IID_IVirtualDesktopNotification_22000;
class IVirtualDesktopNotification_22000 : public IVirtualDesktopNotification {
  virtual HRESULT STDMETHODCALLTYPE VirtualDesktopCreated(
    IObjectArray    *p0, 
    IVirtualDesktop *pDesktop) = 0;

  virtual HRESULT STDMETHODCALLTYPE VirtualDesktopDestroyBegin(
    IObjectArray    *p0, 
    IVirtualDesktop *pDesktopDestroyed, 
    IVirtualDesktop *pDesktopFallback) = 0;

  virtual HRESULT STDMETHODCALLTYPE VirtualDesktopDestroyFailed(
    IObjectArray    *p0, 
    IVirtualDesktop *pDesktopDestroyed, 
    IVirtualDesktop *pDesktopFallback) = 0;

  virtual HRESULT STDMETHODCALLTYPE VirtualDesktopDestroyed(
    IObjectArray    *p0, 
    IVirtualDesktop *pDesktopDestroyed, 
    IVirtualDesktop *pDesktopFallback) = 0;

  virtual HRESULT STDMETHODCALLTYPE UnknownProc7( // TODO: is this signature correct on Win64 ?
    int p0) = 0;

  virtual HRESULT STDMETHODCALLTYPE VirtualDesktopMoved(
    IObjectArray    *p0, 
    IVirtualDesktop *pDesktop, 
    int              nIndexFrom, 
    int              nIndexTo) = 0;

  virtual HRESULT STDMETHODCALLTYPE VirtualDesktopRenamed(
    IVirtualDesktop *pDesktop, 
    HSTRING          chName) = 0;

  virtual HRESULT STDMETHODCALLTYPE ViewVirtualDesktopChanged(
    IApplicationView *pView) = 0;

  virtual HRESULT STDMETHODCALLTYPE CurrentVirtualDesktopChanged(
    IObjectArray    *p0, 
    IVirtualDesktop *pDesktopOld, 
    IVirtualDesktop *pDesktopNew) = 0;

  virtual HRESULT STDMETHODCALLTYPE VirtualDesktopWallpaperChanged(
    IVirtualDesktop *pDesktop, 
    HSTRING         *chPath) = 0;
};

extern const IID IID_IVirtualDesktopNotificationService;
class IVirtualDesktopNotificationService : public IUnknown {
  public:
    virtual HRESULT STDMETHODCALLTYPE Register(
      IVirtualDesktopNotification *pNotification, // Luckily, Register seems to do a QueryInterface.
      DWORD *pdwCookie) = 0;
 
    virtual HRESULT STDMETHODCALLTYPE Unregister(
      DWORD dwCookie) = 0;
};

#ifdef _MSC_VER
  class __declspec(uuid("FF72FFDD-BE7E-43FC-9C03-AD81681E88E4")) IVirtualDesktop_10240;
  class __declspec(uuid("536D3495-B208-4CC9-AE26-DE8111275BF8")) IVirtualDesktop_22000;
  class __declspec(uuid("EF9F1A6C-D3CC-4358-B712-F84B635BEBE7")) IVirtualDesktopManagerInternal_10130;
  class __declspec(uuid("AF8DA486-95BB-4460-B3B7-6E7A6B2962B5")) IVirtualDesktopManagerInternal_10240;
  class __declspec(uuid("f31574d6-b682-4cdc-bd56-1827860abec6")) IVirtualDesktopManagerInternal_14328;
  class __declspec(uuid("b2f925b9-5a0f-4d2e-9f4d-2b1507593c10")) IVirtualDesktopManagerInternal_22000;
  class __declspec(uuid("a5cd92ff-29be-454c-8d04-d82879fb3f1b")) IVirtualDesktopManager;
  class __declspec(uuid("C179334C-4295-40D3-BEA1-C654D965605A")) IVirtualDesktopNotification_10240;
  class __declspec(uuid("CD403E52-DEED-4C13-B437-B98380F2B1E8")) IVirtualDesktopNotification_22000;
  class __declspec(uuid("0CD45E71-D927-4F15-8B0A-8FEF525337BF")) IVirtualDesktopNotificationService;
#elif defined(__GNUC__)
#  define RICHMATH_MAKE_MINGW_UUID(cls) \
     template<> inline const GUID &__mingw_uuidof<cls>()  { return IID_ ## cls; } \
     template<> inline const GUID &__mingw_uuidof<cls*>() { return IID_ ## cls; }
  
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktop_10240)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktop_22000)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopManagerInternal_10130)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopManagerInternal_10240)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopManagerInternal_14328)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopManagerInternal_22000)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopManager)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopNotification_10240)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopNotification_22000)
  RICHMATH_MAKE_MINGW_UUID(IVirtualDesktopNotificationService)
#endif


#endif // RICHMATH__GUI__WIN32__OLE__VIRTUAL_DESKTOPS_H__INCLUDED
