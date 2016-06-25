#ifndef __GUI__WIN32__BASIC_WIN32_WINDOW_H__
#define __GUI__WIN32__BASIC_WIN32_WINDOW_H__

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <graphics/canvas.h>
#include <gui/win32/basic-win32-widget.h>
#include <gui/win32/win32-themes.h>
#include <gui/control-painter.h>
#include <util/hashtable.h>


namespace richmath {
  // Must call init() immediately after the construction of a derived object!
  class BasicWin32Window: public BasicWin32Widget {
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
      
      // all windows are arranged in a ring buffer:
      static int basic_window_count();
      static BasicWin32Window *first_window();
      BasicWin32Window *prev_window() { return _prev_window; }
      BasicWin32Window *next_window() { return _next_window; }
      
      // All windows with zorder_level = i are always in front of all
      // windows with zorder_level < i.
      // Snap affinity is also guided by zorder_level: a window carries any
      // bordering window whit a higher zorder_level when moved.
      int zorder_level() { return _zorder_level; }
      
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
      
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override;
      
    protected:
      Hashtable<HWND, Void, cast_hash> all_snappers;
      
    private:
      bool _active;
      bool _glass_enabled;
      bool _themed_frame;
      bool _mouse_over_caption_buttons;
      Win32Themes::MARGINS _extra_glass;
      
      int snap_correction_x;
      int snap_correction_y;
      int last_moving_x;
      int last_moving_y;
      
      BasicWin32Window *_prev_window;
      BasicWin32Window *_next_window;
      
    private:
      static BOOL CALLBACK find_snap_hwnd(HWND hwnd, LPARAM lParam);
      
      void snap_rect_or_pt(RECT *windowrect, POINT *pt); // pt may be 0, rect must not
      void find_all_snappers();
      HDWP move_all_snappers(HDWP hdwp, int dx, int dy);
    
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
        
  };
}

#endif // __GUI__WIN32__BASIC_WIN32_WINDOW_H__
