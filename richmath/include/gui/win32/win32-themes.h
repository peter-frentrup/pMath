#ifndef __GUI__WIN32__WIN32_THEMES_H__
#define __GUI__WIN32__WIN32_THEMES_H__

#ifndef RICHMATH_USE_WIN32_GUI
#error this header is win32 specific
#endif

#include <windows.h>

#include <util/base.h>


namespace richmath {
  /* (optional) XP Theming API and Vista DWM API */
  class Win32Themes: public Base {
    public:
      typedef struct _MARGINS {
        int cxLeftWidth;
        int cxRightWidth;
        int cyTopHeight;
        int cyBottomHeight;
      } MARGINS, *PMARGINS;
      
      typedef ULONGLONG DWM_FRAME_COUNT;
      typedef ULONGLONG QPC_TIME;
      
      typedef struct _UNSIGNED_RATIO {
        UINT32 uiNumerator;
        UINT32 uiDenominator;
      } UNSIGNED_RATIO;
      
      typedef enum _DWMWINDOWATTRIBUTE {
        DWMWA_NCRENDERING_ENABLED = 1,
        DWMWA_NCRENDERING_POLICY,
        DWMWA_TRANSITIONS_FORCEDISABLED,
        DWMWA_ALLOW_NCPAINT,
        DWMWA_CAPTION_BUTTON_BOUNDS,
        DWMWA_NONCLIENT_RTL_LAYOUT,
        DWMWA_FORCE_ICONIC_REPRESENTATION,
        DWMWA_FLIP3D_POLICY,
        DWMWA_EXTENDED_FRAME_BOUNDS,
        DWMWA_LAST
      } DWMWINDOWATTRIBUTE;
      
      typedef struct _DWM_TIMING_INFO {
        UINT32 cbSize;
        UNSIGNED_RATIO rateRefresh;
        QPC_TIME qpcRefreshPeriod;
        UNSIGNED_RATIO rateCompose;
        QPC_TIME qpcVBlank;
        DWM_FRAME_COUNT cRefresh;
        UINT cDXRefresh;
        QPC_TIME qpcCompose;
        DWM_FRAME_COUNT cFrame;
        UINT cDXPresent;
        DWM_FRAME_COUNT cRefreshFrame;
        DWM_FRAME_COUNT cFrameSubmitted;
        UINT cDXPresentSubmitted;
        DWM_FRAME_COUNT cFrameConfirmed;
        UINT cDXPresentConfirmed;
        DWM_FRAME_COUNT cRefreshConfirmed;
        UINT cDXRefreshConfirmed;
        DWM_FRAME_COUNT cFramesLate;
        UINT cFramesOutstanding;
        DWM_FRAME_COUNT cFrameDisplayed;
        QPC_TIME qpcFrameDisplayed;
        DWM_FRAME_COUNT cRefreshFrameDisplayed;
        DWM_FRAME_COUNT cFrameComplete;
        QPC_TIME qpcFrameComplete;
        DWM_FRAME_COUNT cFramePending;
        QPC_TIME qpcFramePending;
        DWM_FRAME_COUNT cFramesDisplayed;
        DWM_FRAME_COUNT cFramesComplete;
        DWM_FRAME_COUNT cFramesPending;
        DWM_FRAME_COUNT cFramesAvailable;
        DWM_FRAME_COUNT cFramesDropped;
        DWM_FRAME_COUNT cFramesMissed;
        DWM_FRAME_COUNT cRefreshNextDisplayed;
        DWM_FRAME_COUNT cRefreshNextPresented;
        DWM_FRAME_COUNT cRefreshesDisplayed;
        DWM_FRAME_COUNT cRefreshesPresented;
        DWM_FRAME_COUNT cRefreshStarted;
        ULONGLONG cPixelsReceived;
        ULONGLONG cPixelsDrawn;
        DWM_FRAME_COUNT cBuffersEmpty;
      } DWM_TIMING_INFO;
      
      typedef enum _BP_BUFFERFORMAT {
        BPBF_COMPATIBLEBITMAP,
        BPBF_DIB,
        BPBF_TOPDOWNDIB,
        BPBF_TOPDOWNMONODIB,
      } BP_BUFFERFORMAT;
      
      typedef struct _BP_PAINTPARAMS {
        DWORD                cbSize;
        DWORD                dwFlags;
        const RECT          *prcExclude;
        const BLENDFUNCTION *pBlendFunction;
      } BP_PAINTPARAMS, *PBP_PAINTPARAMS;
      
      static const int MaxIntListCount = 402;
      
      typedef struct _INTLIST {
        int iValueCount;
        int iValues[MaxIntListCount]; // MAX_INTLIST_COUNT
      } INTLIST, *PINTLIST;
      
      typedef enum {
        TS_MIN,
        TS_TRUE,
        TS_DRAW
      } THEME_SIZE;
      
      typedef int (WINAPI *DTT_CALLBACK_PROC)(HDC, LPWSTR, int, LPRECT, UINT, LPARAM);
      
      typedef struct _DTTOPTS {
        DWORD dwSize;
        DWORD dwFlags;
        COLORREF crText;
        COLORREF crBorder;
        COLORREF crShadow;
        int iTextShadowType;
        POINT ptShadowOffset;
        int iBorderSize;
        int iFontPropId;
        int iColorPropId;
        int iStateId;
        BOOL fApplyOverlay;
        int iGlowSize;
        DTT_CALLBACK_PROC pfnDrawTextCallback;
        LPARAM lParam;
      } DTTOPTS, *PDTTOPTS;
      
#ifndef DTT_VALIDBITS
#define DTT_TEXTCOLOR       (1UL << 0)
#define DTT_BORDERCOLOR     (1UL << 1)
#define DTT_SHADOWCOLOR     (1UL << 2)
#define DTT_SHADOWTYPE      (1UL << 3)
#define DTT_SHADOWOFFSET    (1UL << 4)
#define DTT_BORDERSIZE      (1UL << 5)
#define DTT_FONTPROP        (1UL << 6)
#define DTT_COLORPROP       (1UL << 7)
#define DTT_STATEID         (1UL << 8)
#define DTT_CALCRECT        (1UL << 9)
#define DTT_APPLYOVERLAY    (1UL << 10)
#define DTT_GLOWSIZE        (1UL << 11)
#define DTT_CALLBACK        (1UL << 12)
#define DTT_COMPOSITED      (1UL << 13)
#define DTT_VALIDBITS       (DTT_TEXTCOLOR | \
                             DTT_BORDERCOLOR | \
                             DTT_SHADOWCOLOR | \
                             DTT_SHADOWTYPE | \
                             DTT_SHADOWOFFSET | \
                             DTT_BORDERSIZE | \
                             DTT_FONTPROP | \
                             DTT_COLORPROP | \
                             DTT_STATEID | \
                             DTT_CALCRECT | \
                             DTT_APPLYOVERLAY | \
                             DTT_GLOWSIZE | \
                             DTT_COMPOSITED)
#endif
      
    public:
      static HRESULT(WINAPI *DwmEnableComposition)(UINT);
      static HRESULT(WINAPI *DwmExtendFrameIntoClientArea)(HWND, const MARGINS*);
      static HRESULT(WINAPI *DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
      static HRESULT(WINAPI *DwmGetCompositionTimingInfo)(HWND, DWM_TIMING_INFO*);
      static HRESULT(WINAPI *DwmDefWindowProc)(HWND, UINT, WPARAM, LPARAM, LRESULT*);
      
      static HANDLE(WINAPI *OpenThemeData)(HWND, LPCWSTR);
      static HRESULT(WINAPI *CloseThemeData)(HANDLE);
      static HRESULT(WINAPI *DrawThemeBackground)(HANDLE, HDC, int, int, const RECT*, const RECT*);
      static HRESULT(WINAPI *DrawThemeEdge)(HANDLE, HDC, int, int, LPCRECT, UINT, UINT, LPRECT);
      static HRESULT(WINAPI *DrawThemeTextEx)(HANDLE, HDC, int, int, LPCWSTR, int, DWORD, LPRECT, const DTTOPTS*);
      static HRESULT(WINAPI *GetThemeSysFont)(HANDLE, int, LOGFONTW*);
      static COLORREF(WINAPI *GetThemeSysColor)(HANDLE, int);
      static HRESULT(WINAPI *GetThemeBackgroundExtent)(HANDLE, HDC, int, int, LPCRECT, LPRECT);
      static HRESULT(WINAPI *GetThemeBackgroundContentRect)(HANDLE, HDC hdc, int, int, LPCRECT, LPRECT);
      static HRESULT(WINAPI *GetThemeBool)(HANDLE, int, int, int, BOOL*);
      static HRESULT(WINAPI *GetThemeColor)(HANDLE, int, int, int, COLORREF*);
      static HRESULT(WINAPI *GetThemeMargins)(HANDLE, HDC, int, int, int, LPRECT, MARGINS*);
      static HRESULT(WINAPI *GetThemeMetric)(HANDLE, HDC, int, int, int, int*);
      static HRESULT(WINAPI *GetThemeInt)(HANDLE, int, int, int, int*);
      static HRESULT(WINAPI *GetThemeIntList)(HANDLE, int, int, int, INTLIST*);
      static HRESULT(WINAPI *GetThemePartSize)(HANDLE hTheme, HDC, int, int, LPCRECT, THEME_SIZE, SIZE*);
      static HRESULT(WINAPI *GetThemePosition)(HANDLE, int, int, int, POINT*);
      static HRESULT(WINAPI *GetThemeTransitionDuration)(HANDLE, int, int, int, int, DWORD*);
      static HRESULT(WINAPI *GetCurrentThemeName)(LPWSTR, int, LPWSTR, int, LPWSTR, int);
      static BOOL (WINAPI *IsThemePartDefined)(HANDLE, int, int);
      static int (WINAPI *GetThemeSysSize)(HANDLE, int);
      static HRESULT(WINAPI *SetWindowTheme)(HWND, LPCWSTR, LPCWSTR);
      
      static HRESULT(WINAPI *BufferedPaintInit)(void);
      static HRESULT(WINAPI *BufferedPaintUnInit)(void);
      static HRESULT(WINAPI *BufferedPaintStopAllAnimations)(HWND);
      static HANDLE(WINAPI *BeginBufferedPaint)(HDC, const RECT*, BP_BUFFERFORMAT, BP_PAINTPARAMS*, HDC*);
      static HANDLE(WINAPI *EndBufferedPaint)(HANDLE, BOOL);
      static BOOL (WINAPI *IsCompositionActive)(void);
      static BOOL (WINAPI *IsThemeActive)(void);
      
      static void init();
      static bool current_theme_is_aero();
      static bool check_osversion(int min_major, int min_minor);
      
    private:
      static HMODULE dwmapi;
      static HMODULE uxtheme;
      
    protected:
      Win32Themes();
      ~Win32Themes();
  };
}

#endif // __GUI__WIN32__WIN32_THEMES_H__
