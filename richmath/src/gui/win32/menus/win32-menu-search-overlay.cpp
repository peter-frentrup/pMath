#include <gui/win32/menus/win32-menu-search-overlay.h>

#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-themes.h>
#include <gui/win32/ole/combase.h>
#include <gui/win32/menus/win32-menu.h>
#include <gui/win32/menus/win32-menubar.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/win32-document-window.h>
#include <gui/documents.h>
#include <gui/document.h>

#include <util/autovaluereset.h>

#define MN_OPENHIERARCHY    0x01E3
#define MN_SELECTITEM       0x01E5

using namespace richmath;

namespace {
  struct IndexAndMenu {
    int index;
    HMENU menu;
  };
  class MenuSearch {
    public:
      MenuSearch(String query);
      
      void collect_menu_matches(Array<MenuSearchResult> &results, HMENU menu, String prefix);
      static bool contains_search_menu_list(HMENU menu);
      static bool find_menu_item(Array<IndexAndMenu> &path, DWORD id, DWORD exclude_list_id);
    
    private:
      String query;
  };
}

namespace richmath {
  class Win32MenuSearchOverlay::Impl {
    public:
      static void init();
      
      static void update_query(String str, HMENU menu);
      
    private:
      static bool do_open_help_menu(Expr cmd);
      static bool open_menu_hierarchy(Expr item_cmd);
      static bool locate_menu_item_source(Expr submenu_cmd, Expr item_cmd);
  };
}

namespace richmath { namespace strings {
  extern String EmptyString;
  extern String MenuListSearchCommands;
  extern String SearchMenuItems;
}}

extern pmath_symbol_t richmath_System_Delimiter;
extern pmath_symbol_t richmath_System_MenuItem;

extern pmath_symbol_t richmath_FE_Private_SubStringMatch;

static const struct {} TimerIdBlinkSearchboxCursor; 

const uint16_t MenuLevelSeparatorChar = 0x25B8; // PMATH_CHAR_RULE

//{ class Win32MenuSearchOverlay ...

Win32MenuSearchOverlay::Win32MenuSearchOverlay(HMENU menu)
: menu{menu}, 
  hide_caret{false}
{
}

void Win32MenuSearchOverlay::init() {
  Impl::init();
}

void Win32MenuSearchOverlay::collect_menu_matches(Array<MenuSearchResult> &results, String query, HMENU menu, String prefix) {
  MenuSearch(std::move(query)).collect_menu_matches(results, menu, std::move(prefix));
}

bool Win32MenuSearchOverlay::calc_rect(RECT &rect, HWND hwnd, HMENU menu) {
//  int dpi = Win32HighDpi::get_dpi_for_window(hwnd);
//  int cx = Win32HighDpi::get_system_metrics_for_dpi(SM_CXMENUCHECK, dpi);
//  pmath_debug_print("[SM_CXMENUCHECK = %d @ %d dpi]\n", cx, dpi);
  
  if(!Win32MenuItemOverlay::calc_rect(rect, hwnd, menu, Win32MenuItemOverlay::All))
    return false;
  
  return true;
}

bool Win32MenuSearchOverlay::handle_char_message(WPARAM wParam, LPARAM lParam, HMENU menu) {
  String str = text();
  
  if(wParam == '\b') {
    if(str.length() > 0)
      str = str.part(0, str.length() - 1);
  }
  else if(wParam >= L' ') {
    str+= String::FromChar(wParam);
  }
  else {
    if(wParam == '\t') {
      HMENU sel_menu = menu;
      int old_sel = Win32Menu::find_hilite_menuitem(&sel_menu, false);
      if(old_sel < 0)
        old_sel = end_index;
      
      int count = GetMenuItemCount(menu);
      for(int i = old_sel + 1; i < count - 1; ++i) {
        DWORD state = GetMenuState(menu, (UINT)i, MF_BYPOSITION);
        if(state & MF_SEPARATOR) {
          SendMessageW(GetAncestor(control, GA_PARENT), MN_SELECTITEM, i + 1, 0);
          return true;
        }
      }
      
      SendMessageW(GetAncestor(control, GA_PARENT), MN_SELECTITEM, end_index + 1, 0);
    } 
    return true;
  }
  
  Impl::update_query(str, menu);
  if(str.length() > 0) {
    SendMessageW(GetAncestor(control, GA_PARENT), MN_SELECTITEM, end_index + 1, 0);
  }
  text(std::move(str));
  hide_caret = false;
  InvalidateRect(control, nullptr, false);
  return true;
}

bool Win32MenuSearchOverlay::handle_keydown_message(WPARAM wParam, LPARAM lParam, HMENU menu) {
  switch(wParam) {
    case VK_ESCAPE:
      if(Menus::current_menu_search_text().length() == 0)
        return false;
      
      Impl::update_query(String(), menu);
      text(String());
      hide_caret = false;
      InvalidateRect(control, nullptr, false);
      return true;
  }
  return false;
}

bool Win32MenuSearchOverlay::handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu) {
  return true;
}

bool Win32MenuSearchOverlay::on_create(CREATESTRUCTW *args) {
  text(Menus::current_menu_search_text());
  return true;
}

void Win32MenuSearchOverlay::on_paint(HDC hdc) {
  RECT rect;
  GetClientRect(control, &rect);
  
  SetBkMode(hdc, TRANSPARENT);
  
  HBRUSH brush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
  (void)SelectObject(hdc, brush);
  
  FillRect(hdc, &rect, brush);
  //Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
  
  HFONT oldfont = nullptr;
  
  RECT icon_rect = rect;
  icon_rect.right = icon_rect.left + rect.bottom - rect.top;
  {
    LOGFONTW lf = {};
    wcscpy_s(lf.lfFaceName, sizeof(lf.lfFaceName)/sizeof(wchar_t), Win32Themes::symbol_font_name());
    
    lf.lfHeight = (icon_rect.bottom - icon_rect.top) * 2 / 3;
    if(HFONT font = CreateFontIndirectW(&lf)) {
      oldfont = (HFONT)SelectObject(hdc, font);
      
      // U+E11A: "Search" in Segoe UI Symbol, Segoe Mdl2 Assets, and Segoe Fluent Icons
      DrawTextW(hdc, L"\xE11A", -1, &icon_rect, DT_CENTER | DT_VCENTER | DT_HIDEPREFIX | DT_SINGLELINE);
    }
  }
  
  if(oldfont) {
    DeleteObject(SelectObject(hdc, oldfont));
    oldfont = nullptr;
  }
  
  rect.left = icon_rect.right;
  
  class MenuControlContext : public ControlContext {
    public:
      virtual bool is_foreground_window() override { return true; }
      virtual bool is_focused_widget() override { return true; }
      virtual bool is_using_dark_mode() override { return Win32Menu::use_dark_mode; }
      virtual int dpi() override { return _dpi; }
      
      MenuControlContext(int _dpi) : _dpi{_dpi} {}
      
    private:
      int _dpi;
  } cc(Win32HighDpi::get_dpi_for_window(control));
  
  int theme_part;
  int theme_state;
  HTHEME theme = Win32ControlPainter::win32_painter.get_control_theme(cc, ContainerType::InputField, ControlState::Pressed, &theme_part, &theme_state);
  
  InflateRect(&rect, -1, -1);
  
  COLORREF text_color = 0;
  if(theme) {
    HRreport(Win32Themes::DrawThemeBackground(theme, hdc, theme_part, theme_state, &rect, nullptr));
    //HRreport(Win32Themes::GetThemeBackgroundContentRect(theme, hdc, theme_part, theme_state, &rect, &rect));
    InflateRect(&rect, -2, -2);
    text_color = Win32ControlPainter::win32_painter.control_font_color(
                   cc, ContainerType::InputField, ControlState::Pressed).to_bgr24();
  }
  else {
    DrawEdge(hdc, &rect, BDR_SUNKEN, BF_RECT | BF_ADJUST);
    FillRect(hdc, &rect, (HBRUSH)(1 + COLOR_WINDOW));
    text_color = GetSysColor(COLOR_WINDOWTEXT);
  }
  SetTextColor(hdc, text_color);
  
  rect.left+= 1;
  String str = text();
  if(str.length() > 0) {
    NONCLIENTMETRICSW ncm = {sizeof(ncm)};
    if(Win32HighDpi::get_nonclient_metrics_for_dpi(&ncm, cc.dpi())) {
      if(HFONT font = CreateFontIndirectW(&ncm.lfMenuFont))
        oldfont = (HFONT)SelectObject(hdc, font);
    }
    
    UINT flags = DT_LEFT | DT_VCENTER | DT_HIDEPREFIX | DT_SINGLELINE;
    
    DrawTextW(hdc, str.buffer_wchar(), str.length(), &rect, flags);
    
    DrawTextW(hdc, str.buffer_wchar(), str.length(), &rect, flags | DT_CALCRECT);
    
    rect.left = rect.right;
  }
  else {
    NONCLIENTMETRICSW ncm = {sizeof(ncm)};
    if(Win32HighDpi::get_nonclient_metrics_for_dpi(&ncm, cc.dpi())) {
      ncm.lfMenuFont.lfItalic = TRUE;
      if(HFONT font = CreateFontIndirectW(&ncm.lfMenuFont))
        oldfont = (HFONT)SelectObject(hdc, font);
    }
    
    SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
    wchar_t hint[200];
    MENUITEMINFOW mii = {sizeof(mii)};
    mii.fMask = MIIM_STRING;
    mii.cch = sizeof(hint)/sizeof(hint[0]);
    mii.dwTypeData = hint;
    if(GetMenuItemInfoW(menu, start_index, TRUE, &mii)) {
      unsigned tabpos = 0;
      while(tabpos < mii.cch && hint[tabpos] != '\t')
        ++tabpos;
      
      DrawTextW(hdc, hint, tabpos, &rect, DT_LEFT | DT_VCENTER | DT_HIDEPREFIX | DT_SINGLELINE);
      
      rect.right-= 1;
      DrawTextW(hdc, hint + tabpos, mii.cch - tabpos, &rect, DT_RIGHT | DT_VCENTER | DT_HIDEPREFIX | DT_SINGLELINE);
    }
  }
  
  if(!hide_caret) {
    rect.right = rect.left + 1;
    InflateRect(&rect, 0, -1);
    SetDCBrushColor(hdc, text_color);
    FillRect(hdc, &rect, (HBRUSH)GetStockObject(DC_BRUSH));
  }
  UINT blink_time = GetCaretBlinkTime();
  if(blink_time != INFINITE)
    SetTimer(control, (UINT_PTR)&TimerIdBlinkSearchboxCursor, blink_time, nullptr);
  
  if(oldfont)
    DeleteObject(SelectObject(hdc, oldfont));
}

bool Win32MenuSearchOverlay::on_timer(UINT_PTR id, TIMERPROC proc) {
  if(id == (UINT_PTR)&TimerIdBlinkSearchboxCursor) {
    KillTimer(control, id);
    
    hide_caret = !hide_caret;
    InvalidateRect(control, nullptr, false);
    
    return true;
  }
  
  return false;
}

//} ... class Win32MenuSearchOverlay

//{ class Win32MenuSearchOverlay::Impl ...

void Win32MenuSearchOverlay::Impl::init() {
  Menus::register_command(strings::SearchMenuItems, do_open_help_menu);
  
  Menus::register_submenu_item_locator(strings::MenuListSearchCommands, locate_menu_item_source);
}

void Win32MenuSearchOverlay::Impl::update_query(String str, HMENU menu) {
  if(Menus::current_menu_search_text() == str)
    return;
  
  Menus::current_menu_search_text(str);
  Win32Menu::init_popupmenu(menu);
}

bool Win32MenuSearchOverlay::Impl::do_open_help_menu(Expr cmd) {
  Document *doc = Menus::current_document();
  if(!doc)
    return false;
  
  auto wid = dynamic_cast<Win32Widget*>(doc->native());
  if(!wid)
    return false;
  
  auto win = wid->find_parent<Win32DocumentWindow>();
  if(!win || !win->menubar())
    return false;
  
  Menus::current_menu_search_text(strings::EmptyString);
  HMENU menu = win->menubar()->menu()->hmenu();
  int count = GetMenuItemCount(menu);
  for(int i = count-1; i >= 0; --i) {
    MENUITEMINFOW mii = {sizeof(mii)};
    mii.fMask = MIIM_SUBMENU;
    if(GetMenuItemInfoW(menu, i, TRUE, &mii)) {
      if(mii.hSubMenu && MenuSearch::contains_search_menu_list(mii.hSubMenu)) {
        win->menubar()->set_focus(i);
        win->menubar()->show_menu(i + 1);
        return true;
      }
    }
  }
  
  return false;
}

bool Win32MenuSearchOverlay::Impl::open_menu_hierarchy(Expr item_cmd) {
  Document *doc = Menus::current_document();
  if(!doc)
    return false;
  
  auto wid = dynamic_cast<Win32Widget*>(doc->native());
  if(!wid)
    return false;
  
  auto win = wid->find_parent<Win32DocumentWindow>();
  if(!win || !win->menubar())
    return false;
  
  DWORD id = Win32Menu::command_to_id(item_cmd);
  if(!id)
    return false;
  
  struct OpenSubmenu final : public Win32MenuSelector {
    Array<IndexAndMenu> path;
    int current_level;
    bool found_final_item;
    
    OpenSubmenu() : current_level{0}, found_final_item{false} {}
    
    virtual void init_popupmenu(HWND menu_wnd, HMENU menu) override {
      if(current_level + 1 >= path.length())
        return;
      
      if(menu != path[current_level].menu)
        return;
      
      ++current_level;
      
      int index = path[current_level].index;
      PostMessageW(menu_wnd, MN_SELECTITEM, index, 0);
      
      if(path[current_level].menu && path[current_level].menu == GetSubMenu(menu, index)) {
        PostMessageW(menu_wnd, MN_OPENHIERARCHY, 0, 0);
      }
    }
    
    virtual void on_menuselect(HMENU menu, unsigned item_or_index, unsigned flags) override {
      if(found_final_item)
        return;
      
      if(current_level + 1 == path.length()) {
        if(menu != path[current_level - 1].menu)
          return;
        
        int index = path[current_level].index;
        
        found_final_item = true;
        
        BOOL snap_mouse = FALSE;
        if(SystemParametersInfoW(SPI_GETSNAPTODEFBUTTON, 0, &snap_mouse, FALSE) && snap_mouse) {
          RECT rect;
          if(GetMenuItemRect(nullptr, menu, index, &rect)) {
            SetCursorPos(
              rect.left + (rect.right - rect.left) / 2,
              rect.top  + (rect.bottom - rect.top) / 2);
          }
        }
      }
    }
    
  } opener;
  
  HMENU menu = win->menubar()->menu()->hmenu();
  opener.path.add({-1, menu});
  
  if(!MenuSearch::find_menu_item(opener.path, id, Win32Menu::command_to_id(strings::MenuListSearchCommands)))
    return false;
  
  if(opener.path.length() < 1)
    return false;
  
  AutoValueReset<Win32MenuSelector*> avr(Win32Menu::menu_selector);
  
  opener.current_level = 1;
  if(GetSubMenu(menu, opener.path[1].index) != opener.path[1].menu)
    return false;
  
  Win32Menu::menu_selector = &opener;
  
  win->menubar()->set_focus(opener.path[1].index);
  win->menubar()->show_menu(opener.path[1].index + 1);
  
  return true;
}

bool Win32MenuSearchOverlay::Impl::locate_menu_item_source(Expr submenu_cmd, Expr item_cmd) {
  if(open_menu_hierarchy(item_cmd))
    return true;
  
  if(Documents::locate_document_from_command(item_cmd))
    return true;
  
  return false;
}

//} ... class Win32MenuSearchOverlay::Impl

//{ class MenuSearch ...

MenuSearch::MenuSearch(String query)
: query{query}
{
}

void MenuSearch::collect_menu_matches(Array<MenuSearchResult> &results, HMENU menu, String prefix) {
  if(contains_search_menu_list(menu))
    return;
  
  Win32Menu::init_popupmenu(menu);
  
  int count = GetMenuItemCount(menu);
  
  for(int i = 0; i < count; ++i) {
    MENUITEMINFOW mii = {sizeof(mii)};
    
    mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_ID | MIIM_SUBMENU;
    if(!GetMenuItemInfoW(menu, (UINT)i, TRUE, &mii))
      continue;
    
    int prefix_len = prefix.length();
    pmath_string_t str = pmath_string_new_raw(prefix_len + (int)mii.cch + 1);
    uint16_t *buf;
    int len = 0;
    if(pmath_string_begin_write(&str, &buf, &len)) {
      if(len == prefix_len + (int)mii.cch + 1) {
        if(prefix_len)
          memcpy(buf, prefix.buffer(), prefix_len * sizeof(uint16_t));
        
        mii.dwTypeData = (wchar_t*)&buf[prefix_len];
        mii.cch = len - prefix_len;
        GetMenuItemInfoW(menu, (UINT)i, TRUE, &mii);
        len = prefix_len + (int)mii.cch;
        
        int j = prefix_len;
        for(int i = prefix_len; i < len; ++i) {
          wchar_t ch = buf[i];
          if(ch == '\t')
            break;
          
          if(ch != L'&')
            buf[j++] = ch;
        }
        len = j;
      }
      else
        len = 0;
      
      pmath_string_end_write(&str, &buf);
    }
    String label{pmath_string_part(str, 0, len)};
    str = PMATH_NULL;
    
    if(mii.hSubMenu) {
      collect_menu_matches(results, mii.hSubMenu, label + String::FromChar(MenuLevelSeparatorChar));
      continue;
    }
    
    if(!mii.wID)
      continue;
    
    Expr matches = Evaluate(Call(Symbol(richmath_FE_Private_SubStringMatch), label, query));
    if(matches.is_int32()) {
      int num_matches = PMATH_AS_INT32(matches.get());
      if(num_matches > 0) {
        results.add(
          MenuSearchResult{
            num_matches, 
            results.length(), 
            Call(Symbol(richmath_System_MenuItem), label, Win32Menu::id_to_command(mii.wID))});
      }
    }
  }
}

bool MenuSearch::contains_search_menu_list(HMENU menu) {
  int count = GetMenuItemCount(menu);
  
  for(int i = 0; i < count; ++i) {
    MENUITEMINFOW mii = {sizeof(mii)};
    mii.fMask = MIIM_DATA;
    if(GetMenuItemInfoW(menu, (UINT)i, TRUE, &mii)) {
      DWORD list_id = mii.dwItemData;
      Expr list_name = Win32Menu::id_to_command(list_id);
      if(list_name == strings::MenuListSearchCommands)
        return true;
    }
  }
  
  return false;
}

bool MenuSearch::find_menu_item(Array<IndexAndMenu> &path, DWORD id, DWORD exclude_list_id) {
  int level = path.length();
  if(level <= 0 || level > 4)
    return false;
  
  HMENU menu = path[level-1].menu;
  int count = GetMenuItemCount(menu);
  path.add({0, nullptr});
  
  for(int i = 0; i < count; ++i) {
    {
      MENUITEMINFOW mii = {sizeof(mii)};
      mii.fMask = MIIM_ID | MIIM_SUBMENU | MIIM_DATA;
      if(!GetMenuItemInfoW(menu, (UINT)i, TRUE, &mii))
        continue;
      
      if(mii.dwItemData == exclude_list_id)
        continue;
      
      path[level].index = i;
      path[level].menu = mii.hSubMenu;
      if(id == mii.wID)
        return true;
    }
    
    if(path[level].menu) {
      if(find_menu_item(path, id, exclude_list_id))
        return true;
    }
  }
  
  path.length(level);
  return false;
}

//} ... class MenuSearch
