#ifndef RICHMATH__GUI__WIN32__BASIC_WIN32_WINDOW_H__INCLUDED
#define RICHMATH__GUI__WIN32__BASIC_WIN32_WINDOW_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <graphics/canvas.h>
#include <gui/common-document-windows.h>
#include <gui/win32/ole/virtual-desktops.h>
#include <gui/win32/basic-win32-widget.h>
#include <gui/win32/win32-themes.h>
#include <gui/control-painter.h>
#include <util/hashtable.h>


namespace richmath {
  class Win32BlurBehindWindow;
  
  // Must call init() immediately after the construction of a derived object!
  class BasicWin32Window: public CommonDocumentWindow, public BasicWin32Widget, public ControlContext, public IVirtualDesktopNotification {
      class Impl;
    public:
      struct SnapPosition {
        HWND src;
        HWND dst;
        Point src_rel_touch;
        Point dst_rel_touch;
        
        SnapPosition() : src(nullptr), dst(nullptr) {}
        
        SnapPosition(HWND _src, const Point &_src_rel_touch, HWND _dst, const Point &_dst_rel_touch) 
          : src(_src),
            dst(_dst),
            src_rel_touch(_src_rel_touch),
            dst_rel_touch(_dst_rel_touch)
        {
        }
      };

    protected:
      virtual void after_construction() override;
      
    public:
      BasicWin32Window(
        DWORD style_ex,
        DWORD style,
        int x,
        int y,
        int width,
        int height);
      virtual ~BasicWin32Window();
      
      void get_client_rect(RECT *rect);
      void get_client_size(int *width, int *height);
      void get_glassfree_rect(RECT *rect);
      void get_nc_margins(Win32Themes::MARGINS *margins);
      
      void get_snap_alignment(bool *right, bool *bottom);
      
      void extend_glass(const Win32Themes::MARGINS &margins);
      bool glass_enabled() { return _glass_enabled; }
      
      virtual bool is_closed();
      
      void paint_background_at(Canvas &canvas, HWND child, bool wallpaper_only = false);
      void paint_background_at(Canvas &canvas, POINT pos, bool wallpaper_only = false);
      
      virtual void paint_background(Canvas &canvas);
      bool has_themed_frame() { return _themed_frame; }
      
      // All windows with zorder_level = i are always in front of all
      // windows with zorder_level < i.
      // Snap affinity is also guided by zorder_level: a window carries any
      // bordering window with a higher zorder_level when moved.
      int zorder_level() { return _zorder_level; }
      
      virtual bool is_foreground_window() override { return _active; }
      virtual bool is_focused_widget() override { return _active; }
      virtual int dpi() override;
      
      virtual bool is_using_dark_mode() override { return _use_dark_mode; }
      
    protected:
      int min_client_height;
      int max_client_height;
      int min_client_width;
      int max_client_width;
      
      int _zorder_level;
      
      AutoCairoSurface background_image;
      
    protected:
      virtual void use_dark_mode(bool dark_mode);
      
      virtual void on_sizing(WPARAM wParam, RECT *lParam);
      virtual void on_moving(RECT *lParam);
      virtual void on_move(LPARAM Param);
      
      virtual void on_theme_changed();
      void invalidate_non_child();
      void invalidate_caption();
      
      virtual void finish_apply_title(String displayed_title) override;
      
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override;
      
    protected:
      Array<SnapPosition> all_snapper_positions;
      Hashset<HWND> all_snappers;
      
    private:
      Win32BlurBehindWindow *_blur_behind_window;
      ObservableValue<bool> _active;
      int8_t _hit_test_mouse_over;
      int8_t _hit_test_mouse_down;
      bool _glass_enabled : 1;
      bool _themed_frame : 1;
      bool _use_dark_mode : 1;
      Win32Themes::MARGINS _extra_glass;
      DWORD _virtual_desktop_notification_cookie;
      
      int snap_correction_x;
      int snap_correction_y;
      int last_moving_cx;
      int last_moving_cy;
      
    public:
      static HANDLE composition_window_theme(int dpi);
      static COLORREF title_font_color(bool glass_enabled, int dpi, bool active, bool dark_mode);
      
      void lost_blur_behind_window(Win32BlurBehindWindow *bb);
      
    public:
      STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override {  return BasicWin32Widget::AddRef(); }
      STDMETHODIMP_(ULONG) Release(void) override { return BasicWin32Widget::Release(); }
       
      //
      // IVirtualDesktopNotification members
      //
      STDMETHODIMP VirtualDesktopCreated(IVirtualDesktop *pDesktop) override;
      STDMETHODIMP VirtualDesktopDestroyBegin(IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) override;
      STDMETHODIMP VirtualDesktopDestroyFailed(IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) override;
      STDMETHODIMP VirtualDesktopDestroyed(IVirtualDesktop *pDesktopDestroyed, IVirtualDesktop *pDesktopFallback) override;
      STDMETHODIMP ViewVirtualDesktopChanged(IApplicationView *pView) override;
      STDMETHODIMP CurrentVirtualDesktopChanged(IVirtualDesktop *pDesktopOld, IVirtualDesktop *pDesktopNew) override;
  };
}

#endif // RICHMATH__GUI__WIN32__BASIC_WIN32_WINDOW_H__INCLUDED
