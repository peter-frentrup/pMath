#ifndef RICHMATH__GUI__WIN32__BASIC_WIN32_WINDOW_H__INCLUDED
#define RICHMATH__GUI__WIN32__BASIC_WIN32_WINDOW_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <graphics/canvas.h>
#include <gui/common-document-windows.h>
#include <gui/win32/basic-win32-widget.h>
#include <gui/win32/win32-themes.h>
#include <gui/control-painter.h>
#include <util/hashtable.h>


namespace richmath {
  // Must call init() immediately after the construction of a derived object!
  class BasicWin32Window: public CommonDocumentWindow, public BasicWin32Widget, public ControlContext {
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
            dst_rel_touch(_dst_rel_touch),
            src_rel_touch(_src_rel_touch)
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
      
      void extend_glass(const Win32Themes::MARGINS *margins);
      bool glass_enabled() { return _glass_enabled; }
      
      virtual bool is_closed();
      
      void paint_background(Canvas *canvas, HWND child, bool wallpaper_only = false);
      void paint_background(Canvas *canvas, int x, int y, bool wallpaper_only = false);
      
      virtual void on_paint_background(Canvas *canvas);
      bool has_themed_frame() { return _themed_frame; }
      
      // All windows with zorder_level = i are always in front of all
      // windows with zorder_level < i.
      // Snap affinity is also guided by zorder_level: a window carries any
      // bordering window whit a higher zorder_level when moved.
      int zorder_level() { return _zorder_level; }
      
      virtual bool is_foreground_window() override { return _active; }
      virtual bool is_focused_widget() override { return _active; }
      virtual int dpi() override;
      
    protected:
      int min_client_height;
      int max_client_height;
      int min_client_width;
      int max_client_width;
      
      int _zorder_level;
      
      AutoCairoSurface background_image;
      
      static bool during_pos_changing;
      
    protected:
      virtual void on_sizing(WPARAM wParam, RECT *lParam);
      virtual void on_moving(RECT *lParam);
      virtual void on_move(LPARAM Param);
      
      virtual void on_theme_changed();
      virtual void paint_themed(HDC hdc);
      virtual void paint_themed_caption(HDC hdc);
      virtual LRESULT nc_hit_test(WPARAM wParam, LPARAM lParam);
      void invalidate_non_child();
      void invalidate_caption();
      
      virtual void finish_apply_title(String displayed_title) override;
      
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override;
      
    protected:
      Array<SnapPosition> all_snapper_positions;
      Hashset<HWND> all_snappers;
      
    private:
      ObservableValue<bool> _active;
      bool _glass_enabled;
      bool _themed_frame;
      bool _mouse_over_caption_buttons;
      Win32Themes::MARGINS _extra_glass;
      
      int snap_correction_x;
      int snap_correction_y;
      int last_moving_x;
      int last_moving_y;
      
    private:
      static BOOL CALLBACK find_snap_hwnd(HWND hwnd, LPARAM lParam);
      
      void snap_rect_or_pt(RECT *windowrect, POINT *pt); // pt may be 0, rect must not
      void find_all_snappers();
      HDWP move_all_snappers(HDWP hdwp, const RECT &new_bounds);
    
    public:
      static HDWP tryDeferWindowPos(
        HDWP hWinPosInfo,
        HWND hWnd,
        HWND hWndInsertAfter,
        int x,
        int y,
        int cx,
        int cy,
        UINT uFlags);
      
      static HANDLE composition_window_theme(int dpi);
      static COLORREF title_font_color(bool glass_enabled, int dpi, bool active);
  };
}

#endif // RICHMATH__GUI__WIN32__BASIC_WIN32_WINDOW_H__INCLUDED
