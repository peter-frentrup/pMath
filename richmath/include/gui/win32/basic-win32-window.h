#ifndef __GUI__WIN32__BASIC_WIN32_WINDOW_H__
#define __GUI__WIN32__BASIC_WIN32_WINDOW_H__

#include <graphics/canvas.h>
#include <gui/win32/basic-win32-widget.h>
#include <gui/win32/win32-themes.h>
#include <gui/control-painter.h>
#include <util/hashtable.h>

namespace richmath{
  // Must call immediately init after the construction of a derived object!
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
      int get_frame_color(HWND child);
      int get_frame_color(int x, int y);
      
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
      
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam);
      
    protected:
      Hashtable<HWND,Void,cast_hash> all_snappers;
      
    private:
      bool _active;
      bool _glass_enabled;
      bool _snap_affinity;
      bool _special_frame;
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
