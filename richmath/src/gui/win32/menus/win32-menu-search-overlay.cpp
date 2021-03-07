#include <gui/win32/menus/win32-menu-search-overlay.h>

#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-themes.h>
#include <gui/win32/ole/combase.h>
#include <gui/win32/menus/win32-menu.h>
#include <gui/win32/menus/win32-menubar.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/win32-document-window.h>
#include <gui/menus.h>
#include <gui/documents.h>
#include <gui/document.h>

#include <algorithm>


#define MN_SELECTITEM   0x01E5

using namespace richmath;

namespace {
  struct SearchResult {
    int quality = 0;
    int index;
    Expr item;
    
    SearchResult() : quality(0) {}
    SearchResult(int quality, int index, Expr item) : quality{quality}, index{index}, item{std::move(item)} {}
    
    friend bool operator<(const SearchResult &left, const SearchResult &right) {
      if(left.quality > right.quality)
        return true;
      if(left.quality < right.quality)
        return false;
      
      if(left.index < right.index)
        return true;
      if(left.index > right.index)
        return false;
      
      return left.item < right.item;
    }
  };
  
  class MenuSearch {
    public:
      MenuSearch(String query);
      
      void collect_menu_matches(Array<SearchResult> &results, HMENU menu, String prefix);
      static bool contains_search_menu_list(HMENU menu);
    
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
      static Expr get_search_commands(Expr name);
    
    public:
      static String current_query;
  };
}

namespace richmath { namespace strings {
  extern String SearchMenuNoMatch_label;
  extern String MenuListSearchCommands;
  extern String SearchMenuItems;
}}

extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_Delimiter;
extern pmath_symbol_t richmath_System_MenuItem;
extern pmath_symbol_t richmath_FE_Private_SubStringMatch;

//{ class Win32MenuSearchOverlay ...

Win32MenuSearchOverlay::Win32MenuSearchOverlay(HMENU menu)
: menu{menu}
{
}

Win32MenuSearchOverlay::~Win32MenuSearchOverlay() {
  Impl::current_query = String();
}

void Win32MenuSearchOverlay::init() {
  Impl::init();
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
  
  if(wParam == 8) {
    if(str.length() > 0)
      str = str.part(0, str.length() - 1);
  }
  else if(wParam >= L' ') {
    str+= String::FromChar(wParam);
  }
  
  Impl::update_query(str, menu);
  if(str.length() > 0) {
    SendMessageW(GetAncestor(control, GA_PARENT), MN_SELECTITEM, end_index + 1, 0);
  }
  text(std::move(str));
  InvalidateRect(control, nullptr, false);
  return true;
}

bool Win32MenuSearchOverlay::handle_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu) {
  return true;
}

bool Win32MenuSearchOverlay::on_create(CREATESTRUCTW *args) {
  //text("Hello");
  text(Impl::current_query);
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
    const wchar_t name[] = L"Segoe UI Symbol";
    memcpy(lf.lfFaceName, name, sizeof(name));
    
    lf.lfHeight = icon_rect.bottom - icon_rect.top;
    if(HFONT font = CreateFontIndirectW(&lf)) {
      oldfont = (HFONT)SelectObject(hdc, font);
      
      // U+1F50D LEFT-POINTING MAGNIFYING GLASS
      DrawTextW(hdc, L"\xD83D\xDD0D", -1, &icon_rect, DT_CENTER | DT_VCENTER | DT_HIDEPREFIX | DT_SINGLELINE);
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
    if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, FALSE)) {
      if(HFONT font = CreateFontIndirectW(&ncm.lfMenuFont))
        oldfont = (HFONT)SelectObject(hdc, font);
    }
    
    UINT flags = DT_LEFT | DT_VCENTER | DT_HIDEPREFIX | DT_SINGLELINE;
    
    DrawTextW(hdc, (const wchar_t*)str.buffer(), str.length(), &rect, flags);
    
    DrawTextW(hdc, (const wchar_t*)str.buffer(), str.length(), &rect, flags | DT_CALCRECT);
    
    rect.left = rect.right;
  }
  else {
    NONCLIENTMETRICSW ncm = {sizeof(ncm)};
    if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, FALSE)) {
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
      int tabpos = 0;
      while(tabpos < mii.cch && hint[tabpos] != '\t')
        ++tabpos;
      
      DrawTextW(hdc, hint, tabpos, &rect, DT_LEFT | DT_VCENTER | DT_HIDEPREFIX | DT_SINGLELINE);
      
      rect.right-= 1;
      DrawTextW(hdc, hint + tabpos, mii.cch - tabpos, &rect, DT_RIGHT | DT_VCENTER | DT_HIDEPREFIX | DT_SINGLELINE);
    }
    
    
  }
  
  rect.right = rect.left + 1;
  InflateRect(&rect, 0, -1);
  SetDCBrushColor(hdc, text_color);
  FillRect(hdc, &rect, (HBRUSH)GetStockObject(DC_BRUSH));
  //FillRect(hdc, &rect, (HBRUSH)(1 + COLOR_WINDOWTEXT));
  
  if(oldfont)
    DeleteObject(SelectObject(hdc, oldfont));
}

//} ... class Win32MenuSearchOverlay

//{ class Win32MenuSearchOverlay::Impl ...

String Win32MenuSearchOverlay::Impl::current_query;

void Win32MenuSearchOverlay::Impl::init() {
  Menus::register_command(strings::SearchMenuItems, do_open_help_menu);
  Menus::register_dynamic_submenu(strings::MenuListSearchCommands, get_search_commands);
}

bool Win32MenuSearchOverlay::Impl::do_open_help_menu(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
  
  auto wid = dynamic_cast<Win32Widget*>(doc->native());
  if(!wid)
    return false;
  
  auto win = wid->find_parent<Win32DocumentWindow>();
  if(!win || !win->menubar())
    return false;
  
  HMENU menu = win->menubar()->menu()->hmenu();
  int count = GetMenuItemCount(menu);
  for(int i = count-1; i >= 0; --i) {
    MENUITEMINFOW mii = {sizeof(mii)};
    mii.fMask = MIIM_SUBMENU;
    if(GetMenuItemInfoW(menu, i, TRUE, &mii)) {
      if(mii.hSubMenu && MenuSearch::contains_search_menu_list(mii.hSubMenu)) {
        win->menubar()->set_focus(i + 1);
        win->menubar()->show_menu(i + 1);
        win->menubar()->kill_focus();
        return true;
      }
    }
  }
  
  return false;
}

Expr Win32MenuSearchOverlay::Impl::get_search_commands(Expr name) {
  Gather g;
  
  Gather::emit(Call(Symbol(richmath_System_MenuItem), String("Search"), strings::SearchMenuItems));
  if(current_query.length() > 0) {
    //Gather::emit(Call(Symbol(richmath_System_MenuItem), current_query, Expr()));
    
    Array<SearchResult> results;
    MenuSearch(current_query).collect_menu_matches(results, Win32Menu::main_menu->hmenu(), String());
    
    std::sort(results.items(), results.items() + results.length());
    
    int max_results = 15;
    for(auto &res : results) {
      Gather::emit(std::move(res.item));
      if(max_results-- == 0)
        break;
    }
    
    if(results.length() == 0) {
      Gather::emit(Call(Symbol(richmath_System_MenuItem), strings::SearchMenuNoMatch_label, Symbol(richmath_System_DollarFailed)));
    }
    
    Gather::emit(Symbol(richmath_System_Delimiter));
  }
  
  return g.end();
}

void Win32MenuSearchOverlay::Impl::update_query(String str, HMENU menu) {
  if(current_query == str)
    return;
  
  current_query = str;
  Win32Menu::init_popupmenu(menu);
}

//} ... class Win32MenuSearchOverlay::Impl

//{ class MenuSearch ...

MenuSearch::MenuSearch(String query)
: query{query}
{
}

void MenuSearch::collect_menu_matches(Array<SearchResult> &results, HMENU menu, String prefix) {
  if(contains_search_menu_list(menu))
    return;
  
  Win32Menu::init_popupmenu(menu);
  
  int count = GetMenuItemCount(menu);
  
  for(int i = 0; i < count; ++i) {
    MENUITEMINFOW mii = {sizeof(mii)};
    mii.fMask = MIIM_FTYPE;
    if(!GetMenuItemInfoW(menu, (UINT)i, TRUE, &mii))
      continue;
    
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
      collect_menu_matches(results, mii.hSubMenu, label + String::FromChar(PMATH_CHAR_RULE));
      continue;
    }
    
    if(!mii.wID)
      continue;
    
    Expr matches = Evaluate(Call(Symbol(richmath_FE_Private_SubStringMatch), label, query));
    if(matches.is_int32()) {
      int num_matches = PMATH_AS_INT32(matches.get());
      if(num_matches > 0) {
        results.add(
          SearchResult{
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

//} ... class MenuSearch
