#ifndef RICHMATH__GUI__WIN32__WIN32_WIDGET_H__INCLUDED
#define RICHMATH__GUI__WIN32__WIN32_WIDGET_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/document.h>
#include <gui/native-widget.h>
#include <gui/win32/api/stylusutil.h>
#include <gui/win32/api/win32-hd-trackpad.h>
#include <gui/win32/basic-win32-widget.h>


namespace richmath {
  // Must call init() immediately after the construction of a derived object!
  class Win32Widget: public NativeWidget, public BasicWin32Widget {
    protected:
      virtual ~Win32Widget();
      virtual void after_construction() override;
      
    public:
      Win32Widget(
        Document *doc,
        DWORD style_ex,
        DWORD style,
        int x,
        int y,
        int width,
        int height,
        HWND *parent);
    
    public:
      virtual Vector2F window_size() override;
      virtual Vector2F page_size() override { return window_size(); }
      virtual Vector2F monitor_size() override;
      
      virtual bool is_scrollable() override { return true; }
      virtual bool autohide_vertical_scrollbar() override { return _autohide_vertical_scrollbar; }
      virtual Point scroll_pos() override;
      virtual bool scroll_to(Point pos) override;
      
      void map_native_points_to_document_inline(ArrayView<Point> pts);
      void map_document_points_to_native_inline(ArrayView<Point> pts);
      Point map_native_point_to_document(POINT native_pt) { return map_native_point_to_document(Point(native_pt.x, native_pt.y)); }
      Point map_native_point_to_document(Point native_pt) { map_native_points_to_document_inline(ArrayView<Point>(1, &native_pt)); return native_pt; }
      Point map_document_point_to_native(Point doc_pt) {    map_document_points_to_native_inline(ArrayView<Point>(1, &doc_pt));    return doc_pt; }
      RectangleF map_native_rect_to_document(const RectangleF &native_rect);
      RectangleF map_document_rect_to_native(const RectangleF &doc_rect);
      
      virtual void show_tooltip(Box *source, Expr boxes) override;
      virtual void hide_tooltip() override;
      virtual Document *try_create_popup_window(const SelectionReference &anchor) override;
      void show_popup_menu(const VolatileSelection &src);
      
      virtual bool is_scaleable() override { return true; }
      
      virtual double message_time() override;
      virtual double double_click_time() override;
      virtual Vector2F double_click_dist() override;
      virtual void do_drag_drop(const VolatileSelection &src, MouseEvent &event) override;
      
      virtual void bring_to_front() override;
      virtual void close() override {}
      virtual void invalidate() override;
      virtual void invalidate_options() override;
      virtual void invalidate_rect(const RectangleF &rect) override;
      virtual void force_redraw() override;
      
      virtual void set_cursor(CursorType type) override;
      
      virtual void running_state_changed() override;
      
      virtual bool is_mouse_down() override;
      
      virtual void beep() override;
      
      virtual bool register_timed_event(SharedPtr<TimedEvent> event) override;
      
      virtual String directory() override { return String(); }
      virtual void directory(String new_directory) override {}
      
      virtual String filename() override { return String(); }
      virtual void filename(String new_filename) override {}
      
      virtual String full_filename() override { return String(); }
      virtual void full_filename(String new_full_filename) override {}
      
      virtual void on_saved() override {}
      virtual void on_selection_changed() override;
      
      virtual bool is_using_dark_mode() override { return has_dark_background(); }
      
    public:
      STDMETHODIMP DragEnter(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect) override;
      STDMETHODIMP DragLeave(void) override;
      
      STDMETHODIMP DataInterest(RealTimeStylusDataInterest* pEventInterest) override;
      STDMETHODIMP StylusDown(IRealTimeStylus*, const StylusInfo*, ULONG, LONG*, LONG**) override;
      STDMETHODIMP StylusUp(IRealTimeStylus*, const StylusInfo*, ULONG, LONG*, LONG**) override;
      
    public:
      cairo_format_t _image_format;
      bool _autohide_vertical_scrollbar : 1;
      bool _destination_has_alpha_channel: 1;
      
      bool has_dark_background() { return _has_dark_background; }
      
    private:
      cairo_surface_t *_old_pixels;
      cairo_surface_t *_old_pixels_with_alpha;
      CursorType cursor;
      bool mouse_moving : 1;
      bool scrolling : 1;
      bool already_scrolled : 1;
      bool _has_dark_background : 1;
      MouseEvent mouse_down_event; // coordinates in pixels, relative to widget top/left (no scrolling adjustment)
      
      ObservableValue<int> _width;
      int _height;
      
      float gesture_zoom_factor;
      
      Hashset<SharedPtr<TimedEvent>> animations;
      bool animation_running;
      bool is_dragging;
      bool is_drop_over;
      
      HiDefTrackpadHandler hd_trackpad_handler;
      ComBase<IRealTimeStylus> stylus;
      
    protected:
      virtual void paint_background(Canvas &canvas);
      virtual void paint_canvas(Canvas &canvas, bool resize_only);
      virtual void on_changed_dark_mode();
      
      virtual void on_paint(HDC dc, bool from_wmpaint);
      virtual void on_hscroll(WORD kind, WORD thumbPos);
      virtual void on_vscroll(WORD kind, WORD thumbPos);
      virtual void on_mousedown(MouseEvent &event);
      virtual void on_mouseup(MouseEvent &event);
      virtual void on_mousemove(MouseEvent &event);
      virtual void on_mousewheel(UINT message, WPARAM wParam, LPARAM lParam);
      virtual void on_keydown(DWORD virtkey, bool ctrl, bool alt, bool shift);
      virtual void on_popupmenu(VolatileSelection src, POINT screen_pt, const RECT *opt_exclude);
      virtual void do_set_selected_document() {}
      
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override;
      
      virtual DWORD preferred_drop_effect(IDataObject *data_object) override;
      virtual DWORD drop_effect(DWORD key_state, POINTL ptl, DWORD allowed_effects) override;
      virtual void apply_drop_description(DWORD effect, DWORD key_state, POINTL pt) override;
      virtual void ask_drop_data(IDataObject *data_object, POINTL pt, DWORD *effect, DWORD allowed_effects) override;
      virtual void position_drop_cursor(POINTL ptl) override;
    
    public: // allow Win32DragDropHandler to call do_drop_data:
      virtual void do_drop_data(IDataObject *data_object, DWORD effect) override;
  };
  
  SpecialKey win32_virtual_to_special_key(DWORD vkey);
}

#endif // RICHMATH__GUI__WIN32__WIN32_WIDGET_H__INCLUDED
