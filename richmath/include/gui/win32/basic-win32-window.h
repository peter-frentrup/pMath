#ifndef __GUI__WIN32__BASIC_WIN32_WINDOW_H__
#define __GUI__WIN32__BASIC_WIN32_WINDOW_H__

#include <graphics/canvas.h>
#include <gui/win32/basic-win32-widget.h>
#include <gui/win32/win32-themes.h>
#include <gui/control-painter.h>
#include <util/hashtable.h>

namespace richmath{
  // Must call init() immediately after the construction of a derived object!
  class BasicWin32Window: public BasicWin32Widget{
    protected:
      virtual void after_construction();
      
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
      
      void snap_affinity(int value);
      bool snap_affinity(){ return _snap_affinity; }
      
      void get_snap_alignment(bool *right, bool *bottom);
      
      void extend_glass(Win32Themes::MARGINS *margins);
      bool glass_enabled(){ return _glass_enabled; }
      
      void paint_background(Canvas *canvas, HWND child, bool wallpaper_only = false);
      void paint_background(Canvas *canvas, int x, int y, bool wallpaper_only = false);
      
      virtual void on_paint_background(Canvas *canvas);
      bool has_themed_frame(){ return _themed_frame; }
      
    protected:
      int min_client_height;
      int max_client_height;
      int min_client_width;
      int max_client_width;
      
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
      
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam);
      
    protected:
      Hashtable<HWND,Void,cast_hash> all_snappers;
      
    private:
      bool _active;
      bool _glass_enabled;
      bool _snap_affinity;
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
      void move_all_snappers(int dx, int dy);
  };
}

#endif // __GUI__WIN32__BASIC_WIN32_WINDOW_H__
