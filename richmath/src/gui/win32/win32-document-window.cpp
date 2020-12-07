#include <gui/win32/win32-document-window.h>

#include <cmath>
#include <cstdio>
#include <cairo-win32.h>

#include <boxes/section.h>
#include <eval/application.h>
#include <eval/binding.h>
#include <eval/dynamic.h>
#include <gui/control-painter.h>
#include <gui/documents.h>
#include <gui/messagebox.h>
#include <gui/win32/win32-automenuhook.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/win32-highdpi.h>
#include <gui/win32/win32-menu.h>
#include <gui/win32/win32-menubar.h>
#include <gui/win32/win32-recent-documents.h>
#include <gui/win32/win32-scrollbar-overlay.h>
#include <gui/win32/win32-themes.h>
#include <resources.h>

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL  0x020E
#endif

using namespace richmath;

namespace richmath { namespace strings {
  extern String Docked;
}}

class Win32DocumentChildWidget: public Win32Widget {
    using base = Win32Widget;
    friend class Win32DocumentWindow;
  public:
    Win32DocumentChildWidget(
      Document *doc,
      DWORD style_ex,
      DWORD style,
      int x,
      int y,
      int width,
      int height,
      Win32DocumentWindow *parent)
      : base(doc, style_ex, style, x, y, width, height, &parent->hwnd()),
        _parent(parent)
    {
    }
    
    Win32DocumentWindow *parent() { return _parent; }
    
    virtual void running_state_changed() override {
      if(_parent)
        _parent->reset_title();
    }
    
    virtual void bring_to_front() override {
      ShowWindow(_parent->hwnd(), SW_SHOWNORMAL);
      SetFocus(_hwnd);
    }
    
    virtual void close() override {
      SendMessageW(_parent->hwnd(), WM_CLOSE, 0, 0);
    }
    
    virtual void invalidate_options() override {
      if(_parent)
        _parent->invalidate_options();
    }
    
    virtual String directory() override { return _parent->directory(); }
    virtual void directory(String new_directory) override { _parent->directory(new_directory); }
    
    virtual String filename() override { return _parent->filename(); }
    virtual void filename(String new_filename) override { _parent->filename(new_filename); }
    
    virtual String full_filename() override { return _parent->full_filename(); }
    virtual void full_filename(String new_full_filename) override { _parent->full_filename(new_full_filename); }
    
    virtual bool can_toggle_menubar() override {          return _parent->can_toggle_menubar(); }
    virtual bool has_menubar() override {                 return _parent->has_menubar(); }
    virtual bool try_set_menubar(bool visible) override { return _parent->try_set_menubar(visible); }
    
    virtual String window_title() override { return _parent->title(); }
    
    virtual void on_idle_after_edit() override { 
      base::on_idle_after_edit();
      _parent->on_idle_after_edit(this);
    }
    virtual void on_saved() override { _parent->on_saved(); }
    
    virtual bool is_foreground_window() override { return _parent->is_foreground_window(); }
    virtual bool is_focused_widget() override { return _parent->is_focused_widget() && base::is_focused_widget(); }
    virtual bool is_using_dark_mode() override { return _parent->is_using_dark_mode(); }
  
  private:
    Win32DocumentWindow *_parent;
  
  protected:
    virtual void do_set_current_document() override {
      Documents::current(_parent->document());
    }
    
    virtual void paint_background(Canvas &canvas) override {
      parent()->paint_background_at(canvas, _hwnd);
    }
    
};

class richmath::Win32WorkingArea: public Win32DocumentChildWidget {
    using base = Win32DocumentChildWidget;
    friend class Win32DocumentWindow;
  public:
    Win32WorkingArea(
      Document *doc,
      DWORD style_ex,
      DWORD style,
      int x,
      int y,
      int width,
      int height,
      Win32DocumentWindow *parent)
      : base(doc, style_ex, style, x, y, width, height, parent),
        _overlay(&parent->hwnd(), &hwnd()),
        auto_size(false),
        best_width(1),
        best_height(1)
    {
    }
    
    virtual Vector2F page_size() override {
      Vector2F size = base::page_size();
      if(auto_size)
        size.x = HUGE_VAL;
      return size;
    }
    
  private:
    Win32ScrollBarOverlay _overlay;
    
  public:
    bool auto_size;
    
    int best_width;
    int best_height;
    
  protected:
    virtual void after_construction() override {
      base::after_construction();
      
      fprintf(stderr, "[Win32WorkingArea::after_construction, hwnd = %p]\n", hwnd());
      _overlay.init();
      SetWindowTextW(_overlay.hwnd(), L"Scrollbar overlay");
      _overlay.update();
    }
    
    virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override {
      _overlay.handle_scrollbar_owner_callback(message, wParam, lParam);
      return base::callback(message, wParam, lParam);
    }
    
    void rearrange() {
      if(auto_size) {
        RECT rect;
        GetClientRect(_hwnd, &rect);
        if( best_width  != rect.right - rect.left ||
            best_height != rect.bottom - rect.top)
          parent()->rearrange();
      }
    }
    
    virtual void set_cursor(CursorType type) override {
      if(auto_size && document()->count() == 0)
        return;
        
      base::set_cursor(type);
    }
    
    virtual void paint_canvas(Canvas &canvas, bool resize_only) override {
      _overlay.clear();
      base::paint_canvas(canvas, resize_only);
      
      _overlay.set_scale(scale_factor());
      if(auto sel = document()->selection_now()) {
        add_overlay(canvas, sel, Color::from_rgb24(0x000080), IndicatorLane::All);
      }
      for(auto ref : document()->current_word_references()) {
        if(auto sel = ref.get_all()) {
         add_overlay(canvas, sel, Color::from_rgb24(0xFF8000), IndicatorLane::Middle);
        }
      }
      _overlay.update();
      
      if(auto_size) {
        int old_bh = best_height;
        int old_bw = best_width;
        
        best_height = (int)ceilf(document()->extents().height() * scale_factor());
        best_width  = (int)ceilf(document()->unfilled_width     * scale_factor());
        
        if(best_height < 1)
          best_height = 1;
          
        if(best_width < 1)
          best_width = 1;
          
        RECT outer, inner;
        GetWindowRect(_hwnd, &outer);
        GetClientRect(_hwnd, &inner);
        
        best_width +=  outer.right  - outer.left - inner.right  + inner.left;
        best_height += outer.bottom - outer.top  - inner.bottom + inner.top;
        
        if(old_bw != best_width || old_bh != best_height)
          parent()->rearrange();
      }
      else
        best_width = best_height = 1;
    }
    
    virtual void on_changed_dark_mode() override {
      parent()->update_dark_mode();
      base::on_changed_dark_mode();
    }
    
    void add_overlay(Canvas &canvas, const VolatileSelection &sel, Color color, IndicatorLane lane) {
      sel.add_path(canvas);
      RectangleF doc_sel_rect = canvas.path_extents();
      canvas.new_path();
      
      float doc_y = doc_sel_rect.y + 0.5 * doc_sel_rect.height;
      _overlay.add(doc_y, color, lane);
    }
    
    virtual void on_paint(HDC dc, bool from_wmpaint) override {
      base::on_paint(dc, from_wmpaint);
      rearrange();
    }
};

class richmath::Win32Dock: public Win32DocumentChildWidget {
    using base = Win32DocumentChildWidget;
    friend class Win32DocumentWindow;
  protected:
    virtual void after_construction() override {
      base::after_construction();
      
      assert(document()->style.is_valid());
      
      document()->style->set(Editable,           false); // redirect Print() to console
      document()->style->set(Selectable,         AutoBoolFalse);
      document()->style->set(ShowSectionBracket, AutoBoolFalse);
      
      document()->select(0, 0, 0);
    }
    
  public:
    Win32Dock(Win32DocumentWindow *parent)
      : base(
        new Document(),
        0,
        WS_CHILD | WS_VISIBLE,
        0, 0, 10, 10,
        parent)
    {
      Style::reset(document()->style, strings::Docked);
    }
    
    void reload(Expr content, bool *change_flag) {
      if(content == _content)
        return;
        
      _content = content;
      int i = 0;
      
      document()->insert_pmath(&i, content, document()->count());
      document()->remove(i, document()->count());
      document()->invalidate_all();
      
      *change_flag = true;
      return;
    }
    
    virtual Vector2F page_size() override {
      Vector2F size = base::page_size();
      size.x = HUGE_VAL;
      return size;
    }
    
    virtual bool is_scrollable() override { return false; }
    virtual Point scroll_pos() override { return Point(0, 0); }
    virtual void scroll_to(Point pos) override {}
    
    int height() {
      return (int)(document()->extents().height() * scale_factor() + 0.5f);
    }
    
    int min_width() {
      return (int)(document()->unfilled_width * scale_factor() + 0.5f);
    }
    
    virtual Document *working_area_document() override { return parent()->working_area()->document(); }
    
    void resize() {
      HDC dc = GetDC(_hwnd);
      on_paint(dc, false);
      ReleaseDC(_hwnd, dc);
    }
    
    bool is_parent_bottom() {
      RECT self_rect;
      RECT parent_rect;
      GetClientRect(_hwnd, &self_rect);
      parent()->get_client_rect(&parent_rect);
      
      POINT pt;
      pt.x = 0;
      pt.y = self_rect.bottom;
      
      MapWindowPoints(_hwnd, parent()->hwnd(), &pt, 1);
      return pt.y == parent_rect.bottom;
    }
    
    bool have_size_grip() {
      if(!is_parent_bottom())
        return false;
        
      int last = document()->count() - 1;
      if(last < 0)
        return true;
        
      if(document()->section(last)->get_style(SectionMarginRight) < 12.0)
        return false;
        
      return true;
    }
    
  protected:
    void paint_size_grip(Canvas &canvas) {
      if(have_size_grip()) {
        canvas.save();
        canvas.scale(scale_factor(), scale_factor());
        
        Vector2F win_size = window_size();
        float sb = ControlPainter::std->scrollbar_width();
        
        canvas.glass_background = true;
        ControlPainter::std->paint_scrollbar_part(
          *this,
          canvas,
          ScrollbarSizeGrip,
          ScrollbarHorizontal,
          Normal,
          {win_size.x - sb, win_size.y - sb, sb, sb});
        canvas.restore();
      }
    }
    
    virtual Color get_textcolor() {
      //return Color::None;
      return parent()->is_using_dark_mode() ? Color::White : Color::Black;
    }
    
    virtual void paint_background(Canvas &canvas) override {
      if(Color color = get_textcolor()) {
        if(!document()->style)
          document()->style = new Style();
        
        document()->style->set(FontColor, color);
      }
      
      base::paint_background(canvas);
    }
    
    virtual void paint_canvas(Canvas &canvas, bool resize_only) override {
      base::paint_canvas(canvas, resize_only);
      paint_size_grip(canvas);
    }
    
    void rearrange() {
      RECT rect;
      GetClientRect(_hwnd, &rect);
      if(height() != rect.bottom)
        parent()->rearrange();
    }
    
    virtual void on_paint(HDC dc, bool from_wmpaint) override {
      base::on_paint(dc, from_wmpaint);
      rearrange();
    }
    
    virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override {
      if(!initializing()) {
        switch(message) {
          case WM_MOUSEMOVE: {
              base::callback(message, wParam, lParam);
              
              if(have_size_grip()) {
                int x = (int16_t) (lParam & 0xFFFF)            + GetScrollPos(_hwnd, SB_HORZ);
                int y = (int16_t)((lParam & 0xFFFF0000) >> 16) + GetScrollPos(_hwnd, SB_VERT);
                
                Vector2F win_size = window_size();
                float sb = ControlPainter::std->scrollbar_width();
                
                if( x / scale_factor() >= win_size.x - sb &&
                    y / scale_factor() >= win_size.y - sb)
                {
                  SetCursor(LoadCursor(0, IDC_SIZENWSE));
                }
              }
            } return 0;
            
          case WM_LBUTTONDOWN: {
              base::callback(message, wParam, lParam);
              
              if(have_size_grip()) {
                int x = (int16_t) (lParam & 0xFFFF)            + GetScrollPos(_hwnd, SB_HORZ);
                int y = (int16_t)((lParam & 0xFFFF0000) >> 16) + GetScrollPos(_hwnd, SB_VERT);
                
                Vector2F win_size = window_size();
                float sb = ControlPainter::std->scrollbar_width();
                
                if( x / scale_factor() >= win_size.x - sb &&
                    y / scale_factor() >= win_size.y - sb)
                {
                  SendMessageW(_hwnd, WM_LBUTTONUP, wParam, lParam);
                  SendMessageW(parent()->hwnd(), WM_NCLBUTTONDOWN, HTBOTTOMRIGHT, 0);
                  return 1;
                }
              }
            } return 0;
        }
      }
      
      return base::callback(message, wParam, lParam);
    }
    
  protected:
    Expr  _content;
};

class richmath::Win32GlassDock: public Win32Dock {
    using base = Win32Dock;
  protected:
    virtual void after_construction() override {
      base::after_construction();
      
      document()->style->set(Background, Color::None);
      
      reload_shadows(Win32HighDpi::get_dpi_for_window(_hwnd));
      set_textshadows();
    }
    
  public:
    Win32GlassDock(Win32DocumentWindow *parent)
      : base(parent)
    {
    }
    
  protected:
    Expr shadows;
    
  protected:
    void set_textshadows() {
      if(!document()->style)
        document()->style = new Style();
        
      document()->style->set(TextShadow, shadows);
    }
    
    virtual Color get_textcolor() override {
      AutoResetCurrentObserver auto_observer;
      Dynamic::current_observer_id = document()->id();
      
      COLORREF color = BasicWin32Window::title_font_color(
        parent()->glass_enabled(),
        Win32HighDpi::get_dpi_for_window(_hwnd), 
        parent()->is_foreground_window(),
        parent()->is_using_dark_mode());
      
      return Color::from_bgr24(color);
    }
    
    void remove_textshadows() {
      if(document()->style)
        document()->style->remove(TextShadow);
    }
    
    virtual void paint_background(Canvas &canvas) override {
      if( Win32Themes::IsCompositionActive &&
          Win32Themes::IsCompositionActive())
      {
        set_textshadows();
        canvas.glass_background = true;
        
//        /* Workaround a Cairo (1.8.8) Bug:
//            Platform: Windows, Cleartype on, ARGB32 image or HDC
//
//            The last (cleartype-blured) pixel column of the last glyph and the zero-th
//            column (also cleartype-blured) of the first pixel in a glyph-string wont
//            be drawn. That looks ugly.
//
//            To see the difference, comment out the following lines.
//         */
//        BOOL haveFontSmoothing = FALSE;
//        UINT fontSmoothingType = FE_FONTSMOOTHINGSTANDARD;
//
//        SystemParametersInfo(SPI_GETFONTSMOOTHING,     0, &haveFontSmoothing, 0);
//        SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &fontSmoothingType, 0);
//
//        if(haveFontSmoothing && fontSmoothingType == FE_FONTSMOOTHINGCLEARTYPE){
//          //canvas.native_show_glyphs = false;
//
//          cairo_font_options_t *opt = cairo_font_options_create();
//          cairo_font_options_set_antialias(opt, CAIRO_ANTIALIAS_GRAY);
//          cairo_set_font_options(canvas.cairo(), opt);
//          cairo_font_options_destroy(opt);
//        }
      }
      else {
        remove_textshadows();
        
        //canvas.native_show_glyphs = true;
        
        if(parent()->glass_enabled())
          canvas.glass_background = true;
      }
      
      base::paint_background(canvas);
    }
    
    static Expr text_shadows_from_theme(HANDLE theme, int part, int state) {
      if(!Win32Themes::GetThemeInt || !Win32Themes::GetThemeColor)
        return List();
      
      int glow_size = 0;
      // TMT_TEXTGLOWSIZE = 2425
      Win32Themes::GetThemeInt(theme, part, state, 2425, &glow_size);
      if(glow_size == 0)
        return List();
      
      COLORREF col_bgr = 0xFFFFFF;
      // TMT_GLOWINTENSITY = 3816
      Win32Themes::GetThemeColor(theme, part, state, 3816, &col_bgr);
      
      // is this an alpha value???
      //int glow_intensity = 0;
      //// TMT_GLOWINTENSITY = 2429
      //Win32Themes::GetThemeInt(theme, part, state, 2429, &glow_intensity);
      
      Expr color = Call(
                     Symbol(PMATH_SYMBOL_RGBCOLOR), 
                     ( col_bgr        & 0xFF) / 255.0, 
                     ((col_bgr >>  8) & 0xFF) / 255.0, 
                     ((col_bgr >> 16) & 0xFF) / 255.0);
      
      double radius = glow_size * 0.5 * 0.75;
      return List(
        List(-0.75, -0.75, color, radius),
        List( 0.75, -0.75, color, radius),
        List(-0.75,  0.75, color, radius),
        List( 0.75,  0.75, color, radius));
    }
    
    void reload_shadows(int dpi) {
      shadows = text_shadows_from_theme(
                  BasicWin32Window::composition_window_theme(dpi),
                  1, /* WP_CAPTION */
                  1 /* CS_ACTIVE */);
    }
    
    virtual void on_paint(HDC dc, bool from_wmpaint) override {
      if( Win32Themes::IsCompositionActive &&
          Win32Themes::IsCompositionActive())
      {
        _image_format = CAIRO_FORMAT_ARGB32;
      }
      else
        _image_format = CAIRO_FORMAT_RGB24;
        
      base::on_paint(dc, from_wmpaint);
    }
    
    virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override {
      if(!initializing()) {
        switch(message) {
          case WM_THEMECHANGED: {
            reload_shadows(Win32HighDpi::get_dpi_for_window(_hwnd));
          } break;
          
          case WM_LBUTTONDOWN: {
              if( base::callback(message, wParam, lParam) == 0 &&
                  document()->clicked_box_id() == document()->id() &&
                  parent()->glass_enabled())
              {
                SendMessageW(_hwnd, WM_LBUTTONUP, wParam, lParam);
                SendMessageW(parent()->hwnd(), WM_NCLBUTTONDOWN, HTCAPTION, lParam);
              }
            } return 0;
        }
      }
      
      return base::callback(message, wParam, lParam);
    }
};

//{ class Win32DocumentWindow ...

Win32DocumentWindow::Win32DocumentWindow(
  Document *doc,
  DWORD style_ex,
  DWORD style,
  int x,
  int y,
  int width,
  int height)
  : base(
    style_ex,
    style | WS_CLIPCHILDREN,
    x,
    y,
    width,
    height),
  _top_glass_area(nullptr),
  _top_area(nullptr),
  _working_area(nullptr),
  _bottom_area(nullptr),
  _bottom_glass_area(nullptr),
  menubar(nullptr),
  creation(true),
  _window_frame(WindowFrameNormal)
{
  _working_area = new Win32WorkingArea(
    doc,
    0, // | WS_EX_COMPOSITED,
    WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_VISIBLE | WS_CLIPSIBLINGS,
    0, 0, 0, 0,
    this);
  
  _content = _working_area->document();
    
  _top_glass_area    = new Win32GlassDock(this);
  _top_area          = new Win32Dock(this);
  _bottom_area       = new Win32Dock(this);
  _bottom_glass_area = new Win32GlassDock(this);
  
  if(Win32Themes::is_windows_10_or_newer()) {
    // Alpha channel only necessary for win10 custom blur behind ...
    // We still use CAIRO_FORMAT_RGB24 instead of CAIRO_FORMAT_ARGB32, since 
    // otherwise Cleartype subpixel rendering will be disabled.
    // TODO: It is not clear why, PangoCairo would still use Cleartype on the same surface.
    _working_area->_destination_has_alpha_channel = true;
    _top_area->_destination_has_alpha_channel     = true;
    _bottom_area->_destination_has_alpha_channel  = true;
  }
}

void Win32DocumentWindow::after_construction() {
  base::after_construction();
  
  Win32RecentDocuments::set_window_app_user_model_id(_hwnd);
  
  _working_area->init();
  _top_glass_area->init();
  _top_area->init();
  _bottom_area->init();
  _bottom_glass_area->init();
  
  top_glass()->main_document    = document();
  top()->main_document          = document();
  bottom()->main_document       = document();
  bottom_glass()->main_document = document();
  
  // for debugging purposes:
  SetWindowTextW(_working_area->hwnd(),      L"WorkingArea");
  SetWindowTextW(_top_glass_area->hwnd(),    L"TopGlassArea");
  SetWindowTextW(_top_area->hwnd(),          L"TopArea");
  SetWindowTextW(_bottom_area->hwnd(),       L"BottomArea");
  SetWindowTextW(_bottom_glass_area->hwnd(), L"BottomGlassArea");
  
  menubar = new Win32Menubar(
    this, _hwnd,
    Win32Menu::main_menu);
    
  HMENU sysmenu = GetSystemMenu(_hwnd, FALSE);
  AppendMenuW(sysmenu, MF_SEPARATOR, 0, L"");
  
  HMENU menu = Win32Menu::main_menu ? Win32Menu::main_menu->hmenu() : nullptr;
  for(int i = 0; i < GetMenuItemCount(menu); ++i) {
    wchar_t data[100];
    
    MENUITEMINFOW info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_SUBMENU;
    info.dwTypeData = data;
    info.cch = 99;
    
    GetMenuItemInfoW(menu, i, TRUE, &info);
    InsertMenuItemW(sysmenu, GetMenuItemCount(sysmenu), TRUE, &info);
  }
  
  if(!Documents::current()) 
    Documents::current(document());
  
  creation = false;
  
  on_theme_changed();
  title(String());
  
  working_area()->document()->style->set(Visible,                         true);
  working_area()->document()->style->set(InternalHasModifiedWindowOption, true);
}

Win32DocumentWindow::~Win32DocumentWindow() {
  menubar->destroy();            menubar = nullptr;
  _top_glass_area->destroy();    _top_glass_area = nullptr;
  _top_area->destroy();          _top_area = nullptr;
  _bottom_area->destroy();       _bottom_area = nullptr;
  _bottom_glass_area->destroy(); _bottom_glass_area = nullptr;
  _working_area->destroy();      _working_area = nullptr;
  
  // remove all menu items so that the submenus are not destroyed automatically
  HMENU sysmenu = GetSystemMenu(_hwnd, FALSE);
  int count = GetMenuItemCount(sysmenu);
  for(int i = 0; i < count; ++i) {
    RemoveMenu(sysmenu, 0, MF_BYPOSITION);
  }
  
  static bool deleting_all = false;
  
  if(!deleting_all) {
    bool have_only_palettes = true;
    for(auto _win : CommonDocumentWindow::All) {
      if(auto win = dynamic_cast<Win32DocumentWindow*>(_win)) {
        if(win != this && !win->is_palette() && win->document()->get_style(Visible, true)) {
          have_only_palettes = false;
          break;
        }
      }
    }
    
    if(have_only_palettes) {
      deleting_all = true;
      
      CommonDocumentWindow *other = next_window();
      while(other && other != this) {
        CommonDocumentWindow *next = other->next_window();
        
        delete other;
        
        other = next;
      }
      
      deleting_all = false;
      
      PostQuitMessage(0);
    }
  }
}

bool Win32DocumentWindow::is_all_glass() {
  return _top_area->document()->count()     == 0 &&
         _working_area->document()->count() == 0 &&
         _bottom_area->document()->count()  == 0;
}

void Win32DocumentWindow::update_dark_mode() {
  bool dark_mode = _working_area->has_dark_background() && !Win32Themes::use_high_contrast();
  use_dark_mode(dark_mode);
}

void Win32DocumentWindow::use_dark_mode(bool dark_mode) {
  base::use_dark_mode(dark_mode);
  
  menubar->use_dark_mode(dark_mode);
}

void Win32DocumentWindow::rearrange() {
  if(creation)
    return;
  
  RECT rect;
  get_client_rect(&rect);
  
#define PARTCOUNT  6
  HWND widgets[PARTCOUNT];
  widgets[0] = menubar->hwnd();
  widgets[1] = _top_glass_area->hwnd();
  widgets[2] = _top_area->hwnd();
  widgets[3] = _working_area->hwnd();
  widgets[4] = _bottom_area->hwnd();
  widgets[5] = _bottom_glass_area->hwnd();
  
  int new_ys[PARTCOUNT + 1] = {0};
  new_ys[0] = rect.top;
  new_ys[1] = new_ys[0] + menubar->best_height();
  new_ys[2] = new_ys[1] + _top_glass_area->height();
  new_ys[3] = new_ys[2] + _top_area->height();
  
  new_ys[6] = rect.bottom;
  new_ys[5] = new_ys[6] - _bottom_glass_area->height();
  new_ys[4] = new_ys[5] - _bottom_area->height();
  
  int work_height = new_ys[4] - new_ys[3];
  if(_working_area->auto_size) {
    if( work_height            != _working_area->best_height ||
        rect.right - rect.left != _working_area->best_width)
    {
      RECT outer;
      GetWindowRect(_hwnd, &outer);
      
      int h = _working_area->best_height;
      int w = _working_area->best_width;
      
      if(w < _top_glass_area->min_width())
        w = _top_glass_area->min_width();
        
      if(w < _top_area->min_width())
        w = _top_area->min_width();
        
      if(w < _bottom_area->min_width())
        w = _bottom_area->min_width();
        
      if(w < _bottom_glass_area->min_width())
        w = _bottom_glass_area->min_width();
        
      int oldh = outer.bottom - outer.top;
      int oldw = outer.right  - outer.left;
      int newh = oldh - work_height + h;
      int neww = oldw - (rect.right - rect.left)  + w;
      
      MONITORINFO monitor_info;
      memset(&monitor_info, 0, sizeof(monitor_info));
      monitor_info.cbSize = sizeof(monitor_info);
      HMONITOR hmon = MonitorFromWindow(_hwnd, MONITOR_DEFAULTTONEAREST);
      if(GetMonitorInfo(hmon, &monitor_info)) {
        if(newh > monitor_info.rcWork.bottom - monitor_info.rcWork.top)
          newh = monitor_info.rcWork.bottom - monitor_info.rcWork.top;
          
        if(neww > monitor_info.rcWork.right - monitor_info.rcWork.left)
          neww = monitor_info.rcWork.right - monitor_info.rcWork.left;
      }
      
      if(newh != oldh || neww != oldw) {
        bool align_right;
        bool align_bottom;
        get_snap_alignment(&align_right, &align_bottom);
        
        if(align_right)
          outer.left = outer.right - neww;
          
        if(align_bottom)
          outer.top = outer.bottom - newh;
          
        UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
        if(!align_right && !align_bottom)
          flags |= SWP_NOMOVE;
          
        SetWindowPos(
          _hwnd, nullptr,
          outer.left, outer.top,
          neww, newh,
          flags);
          
        //rect.right +=  neww - oldw;
        rect.bottom += newh - oldh;
        
        for(int i = 4; i <= PARTCOUNT; ++i)
          new_ys[i] += newh - oldh;
          
        work_height = new_ys[4] - new_ys[3];
      }
    }
  }
  
  if(glass_enabled()) {
    widgets[0] = _top_glass_area->hwnd();
    widgets[1] = menubar->hwnd();
    
    new_ys[1] = new_ys[0] + _top_glass_area->height();
    
    Win32Themes::MARGINS mar = {
      0, 0,
      _top_glass_area->height(),
      _bottom_glass_area->height()
    };
    
    if(_working_area->document()->count() == 0) {
      if( rect.bottom - rect.top <= mar.cyTopHeight + mar.cyBottomHeight + 1 ||
          _working_area->auto_size)
      {
        mar.cxLeftWidth = mar.cxRightWidth = mar.cyTopHeight = mar.cyBottomHeight = -1;
        
        SetWindowPos(_working_area->hwnd(), nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
      }
    }
    
    extend_glass(mar);
  }
  
  int order[PARTCOUNT] = {0, 1, 2, 5, 4, 3};
  
  /*
    When the bottom-glass-area moves up, we must move the (non-glass)
    bottom-area above it before to avoid artifacts.
    When the bottom-glass-area moves down, it must be moved before for the same
    reason.
  */
  POINT topleft = {0, 0};
  MapWindowPoints(widgets[5], _hwnd, &topleft, 1);
  if(topleft.y > new_ys[5]) {
    order[3] = 4;
    order[4] = 5;
  }
  
  for(int i = 0; i < PARTCOUNT; ++i) {
    int j = order[i];
    
    RECT oldrect;
    GetWindowRect(widgets[j], &oldrect);
    
    if( oldrect.left   != rect.left                ||
        oldrect.top    != new_ys[j]                ||
        oldrect.right  != rect.right - rect.left   ||
        oldrect.bottom != new_ys[j + 1] - new_ys[j])
    {
      int w = rect.right - rect.left;
      int h = new_ys[j + 1] - new_ys[j];
      
      SetWindowPos(
        widgets[j], nullptr,
        rect.left, new_ys[j], w, h,
        SWP_NOZORDER | SWP_NOACTIVATE);
    }
  }
  
  min_client_height = rect.bottom - rect.top - (new_ys[4] - new_ys[3]);
  
  if(_working_area->auto_size) {
    min_client_height += work_height;
    max_client_height = min_client_height;
    
    min_client_width = rect.right - rect.left;
    max_client_width = min_client_width;
  }
  else {
    min_client_height += GetSystemMetrics(SM_CYHSCROLL);
    max_client_height = -1;
    min_client_width = 0;
    max_client_width = -1;
  }
  
  if(has_themed_frame()) {
    _top_glass_area->invalidate();
    _top_area->invalidate();
    _bottom_area->invalidate();
    _bottom_glass_area->invalidate();
  }
  
  menubar->resized();
}

void Win32DocumentWindow::invalidate_options() {
  Document *doc = document();
  
  bool change = false;
  if(doc->load_stylesheet()) 
    change = true;
  
  String s = doc->get_style(WindowTitle, _default_title);
  if(!_title.unobserved_equals(s))
    title(s);
    
  _top_area->document()->stylesheet(doc->stylesheet());
  _top_glass_area->document()->stylesheet(doc->stylesheet());
  _bottom_area->document()->stylesheet(doc->stylesheet());
  _bottom_glass_area->document()->stylesheet(doc->stylesheet());
  
  _top_area->reload(         SectionList::group(doc->get_finished_flatlist_style(DockedSectionsTop)),         &change);
  _top_glass_area->reload(   SectionList::group(doc->get_finished_flatlist_style(DockedSectionsTopGlass)),    &change);
  _bottom_area->reload(      SectionList::group(doc->get_finished_flatlist_style(DockedSectionsBottom)),      &change);
  _bottom_glass_area->reload(SectionList::group(doc->get_finished_flatlist_style(DockedSectionsBottomGlass)), &change);
  
  if(change) {
    _top_area->resize();
    _top_glass_area->resize();
    _bottom_area->resize();
    _bottom_glass_area->resize();
    rearrange();
  }
  
  WindowFrameType f = (WindowFrameType)doc->get_style(WindowFrame, _window_frame);
  if(_window_frame != f)
    window_frame(f);
    
  if(doc->get_style(Visible, true)) {
    if(!IsWindowVisible(_hwnd)) {
      SetWindowPos(
        _hwnd, nullptr,
        0, 0, 1, 1,
        SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
  }
  else {
    if(IsWindowVisible(_hwnd))
      SetWindowPos(
        _hwnd, nullptr,
        0, 0, 1, 1,
        SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
  
  float scale = doc->get_style(Magnification, _working_area->custom_scale_factor());
  _working_area->set_custom_scale(scale);
}

void Win32DocumentWindow::reset_title() {
  title(document()->get_style(WindowTitle, _default_title));
}

bool Win32DocumentWindow::can_toggle_menubar() {
  return menubar->appearence() == MenuAppearence::AutoShow;
}

bool Win32DocumentWindow::has_menubar() {
  return menubar->is_pinned();
}

bool Win32DocumentWindow::try_set_menubar(bool visible) {
  return menubar->toggle_pin(visible);
}

void Win32DocumentWindow::window_frame(WindowFrameType type) {
  _window_frame = type;
  
  bool hide_temporary = IsWindowVisible(_hwnd);
  if(hide_temporary)
    ShowWindow(_hwnd, SW_HIDE);
    
  DWORD style    = GetWindowLongW(_hwnd, GWL_STYLE);
  DWORD style_ex = GetWindowLongW(_hwnd, GWL_EXSTYLE);
  
  switch(_window_frame) {
    case WindowFrameNormal:
      {
        _working_area->auto_size                    = false;
        _working_area->_autohide_vertical_scrollbar = false;
        
        // behind palettes:
        _zorder_level = 0;
        
        style_ex &= ~WS_EX_TOOLWINDOW;
        style    &= ~(WS_POPUP | WS_BORDER);
        style    |= WS_OVERLAPPEDWINDOW;
        
        if(glass_enabled())
          menubar->appearence(MenuAppearence::AutoShow);
        else
          menubar->appearence(MenuAppearence::Show);
      }
      break;
      
    case WindowFramePalette:
      {
        _working_area->auto_size                    = true;
        _working_area->_autohide_vertical_scrollbar = true;
        
        // in front of non-palettes:
        _zorder_level = 1;
        
        style_ex |= WS_EX_TOOLWINDOW;
        style    &= ~(WS_POPUP | WS_BORDER | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
        style    |= WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
        
        menubar->appearence(MenuAppearence::Hide);
      }
      break;
      
    case WindowFrameDialog:
      {
        _working_area->auto_size                    = true;
        _working_area->_autohide_vertical_scrollbar = true;
        
        // behind palettes:
        _zorder_level = 0;
        
        style_ex &= ~WS_EX_TOOLWINDOW;
        style    &= ~(WS_POPUP | WS_BORDER | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
        style    |= WS_CAPTION | WS_SYSMENU;
        
        menubar->appearence(MenuAppearence::Hide);
      }
      break;
      
    case WindowFrameNone: 
      {
        _working_area->auto_size                    = true;
        _working_area->_autohide_vertical_scrollbar = true;
         
        SetWindowLongW(_hwnd, GWL_EXSTYLE,
                       GetWindowLongW(_hwnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
                       
        style_ex |= WS_EX_TOOLWINDOW;
        style    &= ~(WS_OVERLAPPEDWINDOW | WS_BORDER);
        style    |= WS_POPUP;
        
        menubar->appearence(MenuAppearence::Hide);
      }
      break;
      
    case WindowFrameThin: 
      {
        _working_area->auto_size                    = true;
        _working_area->_autohide_vertical_scrollbar = true;
         
        SetWindowLongW(_hwnd, GWL_EXSTYLE,
                       GetWindowLongW(_hwnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
                       
        style_ex |= WS_EX_TOOLWINDOW;
        style    &= ~WS_OVERLAPPEDWINDOW;
        style    |= WS_POPUP | WS_BORDER;
        
        menubar->appearence(MenuAppearence::Hide);
      }
      break;
  }
  
  SetWindowLongW(_hwnd, GWL_EXSTYLE, style_ex);
  SetWindowLongW(_hwnd, GWL_STYLE, style);
  
  if(hide_temporary)
    ShowWindow(_hwnd, SW_SHOW);
    
  on_theme_changed();
}

bool Win32DocumentWindow::is_closed() {
  if(is_palette())
    return false;
    
  return base::is_closed();
}

void Win32DocumentWindow::on_setting_changed() {
  RECT rect;
  if(GetClientRect(menubar->hwnd(), &rect)) {
    if(rect.bottom - rect.top != menubar->best_height())
      rearrange();
  }
}

void Win32DocumentWindow::on_close() {
  if(_has_unsaved_changes && document()->get_style(Saveable)) {
    YesNoCancel answer = ask_save(document());
    
    switch(answer) {
      case YesNoCancel::Yes:
        if(Application::save(document()) == PMATH_SYMBOL_FAILED)
          return;
        break;
      
      case YesNoCancel::No:
        break;
        
      case YesNoCancel::Cancel:
        return;
    }
  }
  
  if(auto styles = _working_area->stylesheet_document())
    styles->native()->close();
  
  base::on_close();
}

void Win32DocumentWindow::on_theme_changed() {
  base::on_theme_changed();
  
  if(window_frame() != WindowFrameNormal)
    menubar->appearence(MenuAppearence::Hide);
  else if(glass_enabled())
    menubar->appearence(MenuAppearence::AutoShow);
  else
    menubar->appearence(MenuAppearence::Show);
    
  DWORD style_ex = GetWindowLongW(_working_area->hwnd(), GWL_EXSTYLE);
  if( (Win32Themes::IsCompositionActive &&
       Win32Themes::IsCompositionActive()) ||
      window_frame() != WindowFrameNormal)
  {
    style_ex = style_ex & ~WS_EX_STATICEDGE;
  }
  else {
    style_ex = style_ex | WS_EX_STATICEDGE;
  }
  SetWindowLongW(_working_area->hwnd(), GWL_EXSTYLE, style_ex);
  
  rearrange();
}

LRESULT Win32DocumentWindow::callback(UINT message, WPARAM wParam, LPARAM lParam) {
  LRESULT result = 0;
  
  if(!initializing()) {
    if(menubar->callback(&result, message, wParam, lParam))
      return result;
      
    switch(message) {
      case WM_DPICHANGED:
      case WM_SETTINGCHANGE: {
        on_setting_changed();
        } break;
      
      case WM_MOVE: {
          top_glass(   )->invalidate_popup_window_positions();
          top(         )->invalidate_popup_window_positions();
          document(    )->invalidate_popup_window_positions();
          bottom(      )->invalidate_popup_window_positions();
          bottom_glass()->invalidate_popup_window_positions();
        } break;
      
      case WM_SIZE: {
          _top_glass_area->resize();
          _top_area->resize();
          _bottom_area->resize();
          _bottom_glass_area->resize();
          rearrange();
        } break;
        
      case WM_MOUSEHWHEEL:
      case WM_MOUSEWHEEL: {
          POINT pt;
          if(GetCursorPos(&pt)) {
            RECT rect;
            GetClientRect(_working_area->hwnd(), &rect);
            ScreenToClient(_working_area->hwnd(), &pt);
            
            if(PtInRect(&rect, pt)) {
              return SendMessageW(_working_area->hwnd(), message, wParam, lParam);
            }
          }
        } break;
        
      case WM_SETFOCUS: {
          SetFocus(_working_area->hwnd());
          if(document()->selectable()) {
            Documents::current(document());
          }
        } break;
        
      case WM_MOUSEACTIVATE: {
          if(LOWORD(lParam) == HTCLIENT)
            return MA_NOACTIVATE;
        } break;
        
      case WM_ACTIVATE: {
          if(HIWORD(wParam)) { // minimizing
            bool have_only_palettes = true;
            
            for(auto _win : CommonDocumentWindow::All) {
              if(auto wnd = dynamic_cast<Win32DocumentWindow*>(_win)) {
                if( wnd != this && 
                    !wnd->is_palette() && 
                    IsWindowVisible(wnd->hwnd()) && 
                    (GetWindowLongW(wnd->hwnd(), GWL_STYLE) & WS_MINIMIZE) == 0) 
                {
                  have_only_palettes = false;
                  break;
                }
              }
            }
            
            if(have_only_palettes) {
              for(auto _win : CommonDocumentWindow::All) {
                if(auto tool = dynamic_cast<Win32DocumentWindow*>(_win)) {
                  if(tool->is_palette()) 
                    ShowWindow(tool->hwnd(), SW_HIDE);
                }
              }
            }
          }
        } break;
        
      case WM_ACTIVATEAPP: {
          Document *current_doc = Documents::current();
          
          if(wParam) { // activate
            if(current_doc)
              current_doc->focus_set();
          }
          else {
            if(current_doc)
              current_doc->focus_killed();
          }
        } break;
        
      case WM_SYSCOMMAND:
        if((wParam & 0xFFF0) == SC_MOUSEMENU || (wParam & 0xFFF0) == SC_KEYMENU) {
          LRESULT res;
          MenuExitInfo exit_info;
          DWORD explicit_cmd = 0;
          {
            Win32AutoMenuHook menu_hook(GetSystemMenu(hwnd(), FALSE), hwnd(), nullptr, false, false);
            Win32Menu::use_dark_mode = is_using_dark_mode();
            
            res = base::callback(message, wParam, lParam);
            
            exit_info = menu_hook.exit_info;
          }
          
          if(!explicit_cmd && !exit_info.handle_after_exit()) {
            if(exit_info.reason == MenuExitReason::ExplicitCmd)
              explicit_cmd = exit_info.cmd;
          }
  
          if(explicit_cmd)
            return SendMessageW(hwnd(), WM_COMMAND, explicit_cmd, 0);
          
          return res;
        }
        
        if(wParam >= 0xF000)
          break;
        /* no break */
      case WM_COMMAND: {
          Expr cmd = Win32Menu::id_to_command(LOWORD(wParam));
          Menus::run_command_now(std::move(cmd));
        } return 0;
        
      case WM_KEYDOWN:
      case WM_CHAR:
      case WM_KEYUP:
        return SendMessageW(_working_area->hwnd(), message, wParam, lParam);
    }
  }
  
  return base::callback(message, wParam, lParam);
}

//} ... class Win32DocumentWindow
