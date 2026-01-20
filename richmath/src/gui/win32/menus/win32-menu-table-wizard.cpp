#include <gui/win32/menus/win32-menu-table-wizard.h>

#include <gui/win32/menus/win32-menu.h>
#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-version.h>
#include <gui/win32/win32-control-painter.h>

#include <gui/document.h>
#include <gui/edit-helper.h>


#define MN_SELECTITEM       0x01E5

namespace {
  struct TableWizardDims {
    RECT rect;
    int dpi;
    POINT table_pos;
    SIZE cell_size;
  };
}

namespace richmath {
  class Win32MenuTableWizard::Impl {
    public:
      Impl(Win32MenuTableWizard &self);
      
      static void init();
      static void invoke() { do_insert_new_table(Expr()); }
      
      bool selected();
      String description();
      
      void layout(TableWizardDims *dims);
      
    public:
      static int num_rows;
      static int num_columns;
    
    private:
      static bool do_insert_new_table(Expr cmd);
      
    private:
      Win32MenuTableWizard &self;
  };
}

namespace richmath { namespace strings {
  extern String InsertTable;
}}

using namespace richmath;

namespace {
  static const int MaxRows    = 10;
  static const int MaxColumns = 10;
}

//{ class Win32MenuTableWizard ...

Win32MenuTableWizard::Win32MenuTableWizard(HMENU menu)
: menu{menu},
  gutter_width(0),
  shown_selected{false}
{
}

void Win32MenuTableWizard::init() {
  Impl::init();
}

int Win32MenuTableWizard::preferred_height() {
  int cy = GetSystemMetrics(SM_CYMENUSIZE); // for primary monitor DPI
  
  return cy + MaxRows * (cy * 2 / 3);
}

bool Win32MenuTableWizard::calc_rect(RECT &rect, HWND hwnd, HMENU menu) {
  Win32MenuItemOverlay::Layout layout;
  if(Win32MenuItemOverlay::calc_layout(layout, hwnd, menu)) {
    rect = layout.rect_for(Win32MenuItemOverlay::All);
    gutter_width = layout.content_left - rect.left;
    return true;
  }
  return false;
}

bool Win32MenuTableWizard::consumes_navigation_key(DWORD keycode, HMENU menu, int sel_item) {
  if(menu != this->menu || sel_item != start_index)
    return false;
  
  if(keycode == VK_LEFT)
    return Impl::num_columns > 1;
  
  if(keycode == VK_RIGHT)
    return Impl::num_columns < MaxColumns;
  
  return false;
}

bool Win32MenuTableWizard::handle_char_message(WPARAM wParam, LPARAM lParam, HMENU menu) {
  HMENU sel_menu = menu;
  int old_sel = Win32Menu::find_hilite_menuitem(&sel_menu, false);
  
  if(old_sel != start_index)
    return false;

  return true;
}

bool Win32MenuTableWizard::handle_keydown_message(WPARAM wParam, LPARAM lParam, HMENU menu) {
  HMENU sel_menu = menu;
  int old_sel = Win32Menu::find_hilite_menuitem(&sel_menu, false);
  
  //if(shown_selected != Impl(*this).selected())
    InvalidateRect(control, nullptr, false);
  
  switch(wParam) {
    case VK_LEFT: {
      if(old_sel != start_index)
        return false;
      
      if(Impl::num_columns > 1) {
        Impl::num_columns -= 1;
        InvalidateRect(control, nullptr, false);
        return true;
      }
    } break;
    case VK_RIGHT: {
      if(old_sel != start_index)
        return false;
      
      if(Impl::num_columns < MaxColumns) {
        Impl::num_columns += 1;
        InvalidateRect(control, nullptr, false);
        return true;
      }
    } break;
    case VK_UP: {
      if(old_sel != start_index)
        return false;
      
      if(Impl::num_rows > 1) {
        Impl::num_rows -= 1;
        InvalidateRect(control, nullptr, false);
        return true;
      }
    } break;
    case VK_DOWN: {
      if(old_sel != start_index)
        return false;
      
      if(Impl::num_rows < MaxRows) {
        Impl::num_rows += 1;
        InvalidateRect(control, nullptr, false);
        return true;
      }
    } break;
  }
  return false;
}

bool Win32MenuTableWizard::handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu) {
  TableWizardDims dims;
  Impl(*this).layout(&dims);
  
  int col = 1;
  int row = 1;
  
  if(dims.table_pos.x <= pt.x && dims.cell_size.cx > 0) {
    col = 1 + (pt.x - dims.table_pos.x) / dims.cell_size.cx;
  }
  
  if(dims.table_pos.y <= pt.y && dims.cell_size.cy > 0) {
    row = 1 + (pt.y - dims.table_pos.y) / dims.cell_size.cy;
  }
  
  if(row > MaxRows)    row = MaxRows;
  if(col > MaxColumns) col = MaxColumns;
  
  if(!PtInRect(&dims.rect, pt))
    return false;
  
  switch(msg) {
    case WM_MOUSEMOVE: 
      if(row != Impl::num_rows || col != Impl::num_columns) {
        Impl::num_rows    = row;
        Impl::num_columns = col;
        InvalidateRect(control, nullptr, false);
      }
      if(HWND menu_wnd = GetAncestor(control, GA_PARENT)) {
        PostMessageW(menu_wnd, MN_SELECTITEM, start_index, 0);
        
        if(!shown_selected) {
          InvalidateRect(control, nullptr, false);
        }
      }
      break;
    
    case WM_LBUTTONUP:
      Impl::num_rows    = row;
      Impl::num_columns = col;
      // TODO: better invoke the menu item normally and let the standard infrastructure find the command binding
      // Or use the current Win32AutoMenuHook (if any) to return the cmd with exit_reason = ReturnCmd
      Impl::invoke();
      EndMenu();
      return true;
  }
  
  return false;
}

void Win32MenuTableWizard::on_mouse_leave() {
  if(shown_selected) {
    shown_selected = false;
    InvalidateRect(control, nullptr, false);
  }
}

void Win32MenuTableWizard::on_paint(HDC hdc) {
  TableWizardDims dims;
  Impl(*this).layout(&dims);
  
  SetBkMode(hdc, TRANSPARENT);
  
  Win32ControlPainter::win32_painter.draw_menu_popup(hdc, &dims.rect, Win32Menu::use_dark_mode);
  
  //SetBkMode(hdc, OPAQUE);
  
  HFONT oldfont = nullptr;
  
  COLORREF sel_color;
  COLORREF text_color;
  COLORREF window_color;
  
  // TODO: respect Win32Menu::use_dark_mode
  text_color     = GetSysColor(COLOR_WINDOWTEXT);
  window_color   = GetSysColor(COLOR_WINDOW);
  sel_color      = GetSysColor(COLOR_HOTLIGHT);
  
  if(Win32Menu::use_dark_mode) {
    text_color   = 0xFFFFFF ^ text_color;
    window_color = 0xFFFFFF ^ window_color;
  }
  
  shown_selected = Impl(*this).selected();
  //Color blend_sel = Color::blend(Color::from_bgr24(sel_color), Color::from_bgr24(window_color), 0.5);
  Color blend_sel = Color::blend(Color::from_bgr24(sel_color), Color::White, 0.5);
  
  if(!shown_selected) {
    auto gray = blend_sel.gray();
    blend_sel = Color::from_rgb(gray, gray, gray);
  }
  
  sel_color = blend_sel.to_bgr24();
  
//  sel_color      = shown_selected ? SelectionColor.to_bgr24() : InactiveSelectionColor.to_bgr24();
//  sel_color      = shown_selected ? GetSysColor(COLOR_HOTLIGHT) : /*GetSysColor(COLOR_BTNFACE)*/ 0x808080;
    
  NONCLIENTMETRICSW ncm = {sizeof(ncm)};
  if(Win32HighDpi::get_nonclient_metrics_for_dpi(&ncm, dims.dpi)) {
    if(HFONT font = CreateFontIndirectW(&ncm.lfMenuFont))
      oldfont = (HFONT)SelectObject(hdc, font);
  }
  
  String str = Impl(*this).description();
  str+= String::FromChar(0);
  
  RECT title_rect   = dims.rect;
  title_rect.left   = dims.table_pos.x;
  title_rect.bottom = dims.table_pos.y;
  
  DrawTextW(hdc, str.buffer_wchar(), str.length(), &title_rect, DT_LEFT | DT_VCENTER | DT_HIDEPREFIX | DT_SINGLELINE);
  
  HBRUSH dc_brush = (HBRUSH)GetStockObject(DC_BRUSH);
  
  RECT cell_rect;
  
  SetDCBrushColor(hdc, sel_color);
  cell_rect = {dims.table_pos.x, dims.table_pos.y, dims.table_pos.x + Impl::num_columns * dims.cell_size.cx, dims.table_pos.y + Impl::num_rows * dims.cell_size.cy };
  FillRect(hdc, &cell_rect, dc_brush);
  
  SetDCBrushColor(hdc, window_color);
  cell_rect.left  = cell_rect.right;
  cell_rect.right = dims.table_pos.x + MaxColumns * dims.cell_size.cx;
  FillRect(hdc, &cell_rect, dc_brush);
  
  cell_rect.top    = cell_rect.bottom;
  cell_rect.left   = dims.table_pos.x;
  cell_rect.bottom = dims.table_pos.y + MaxRows * dims.cell_size.cy;
  FillRect(hdc, &cell_rect, dc_brush);
  
  cell_rect = {dims.table_pos.x, dims.table_pos.y, dims.table_pos.x + MaxColumns * dims.cell_size.cx + 1, dims.table_pos.y + dims.cell_size.cy + 1 };
  SetDCBrushColor(hdc, text_color);
  for(int i = 0; i < MaxRows; ++i) {
    FrameRect(hdc, &cell_rect, dc_brush);
    cell_rect.top    += dims.cell_size.cy;
    cell_rect.bottom += dims.cell_size.cy;
  }
  
  cell_rect = {dims.table_pos.x, dims.table_pos.y, dims.table_pos.x + dims.cell_size.cx + 1, dims.table_pos.y + MaxRows * dims.cell_size.cy + 1 };
  for(int i = 0; i < MaxRows; ++i) {
    FrameRect(hdc, &cell_rect, dc_brush);
    cell_rect.left  += dims.cell_size.cx;
    cell_rect.right += dims.cell_size.cx;
  }
  
  if(oldfont)
    DeleteObject(SelectObject(hdc, oldfont));
}

//} ... class Win32MenuTableWizard

//{ class Win32MenuTableWizard::Impl ...

int Win32MenuTableWizard::Impl::num_rows = 1;
int Win32MenuTableWizard::Impl::num_columns = 1;

Win32MenuTableWizard::Impl::Impl(Win32MenuTableWizard &self)
: self{self}
{
}

void Win32MenuTableWizard::Impl::init() {
  Menus::register_command(strings::InsertTable, do_insert_new_table);
}

bool Win32MenuTableWizard::Impl::selected() {
  UINT state = WIN32report_errval(GetMenuState(self.menu, self.start_index, MF_BYPOSITION), (UINT)(-1));
  return (state & MF_HILITE) != 0;
}

String Win32MenuTableWizard::Impl::description() {
  String text;
  
  MENUITEMINFOW mii = {sizeof(mii)};
  mii.fMask = MIIM_STRING;
  if(WIN32report(GetMenuItemInfoW(self.menu, (UINT)self.start_index, TRUE, &mii))) {
    pmath_string_t str = pmath_string_new_raw((int)mii.cch + 1);
    
    uint16_t *buf;
    int len = 0;
    if(pmath_string_begin_write(&str, &buf, &len)) {
      mii.dwTypeData = (wchar_t*)buf;
      mii.cch        = len;
      if(WIN32report(GetMenuItemInfoW(self.menu, (UINT)self.start_index, TRUE, &mii)))
        len = mii.cch;
      else
        len = 0;
    
      pmath_string_end_write(&str, &buf);
    }
    
    text = String(pmath_string_part(str, 0, len));
    str = PMATH_NULL;
  }
  
  if(num_rows > 1 || num_columns > 1) {
    text += String(" (");
    text += Expr(num_rows).to_string();
    text += String::FromChar(PMATH_CHAR_TIMES);
    text += Expr(num_columns).to_string();
    text += String(")");
  }
  
  return text;
}

void Win32MenuTableWizard::Impl::layout(TableWizardDims *dims) {
  GetClientRect(self.control, &dims->rect);
  
  dims->dpi = Win32HighDpi::get_dpi_for_window(self.control);
  int cx    = Win32HighDpi::get_system_metrics_for_dpi(SM_CXMENUCHECK, dims->dpi);
  int cy    = Win32HighDpi::get_system_metrics_for_dpi(SM_CYMENUCHECK, dims->dpi);
  
  dims->table_pos.x = self.gutter_width >= 0 ? self.gutter_width : cx + cx/2;
  dims->table_pos.y = cy;
  dims->cell_size.cy = (dims->rect.bottom - dims->table_pos.y) / MaxRows;
  dims->cell_size.cx = (dims->rect.right  - dims->table_pos.x) / MaxColumns;
  
  if(dims->cell_size.cy > dims->cell_size.cx)
    dims->cell_size.cy = dims->cell_size.cx;
  else
    dims->cell_size.cx = dims->cell_size.cy;
}

bool Win32MenuTableWizard::Impl::do_insert_new_table(Expr cmd) {
  return EditHelper::insert_new_table_into_current_document(num_rows, num_columns);
}

//} ... class Win32MenuTableWizard::Impl
