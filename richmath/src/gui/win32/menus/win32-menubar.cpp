#include <gui/win32/menus/win32-menubar.h>

#include <algorithm>
#include <cstdio>
#include <cctype>

#include <commctrl.h>
#include <cairo-win32.h>

#include <util/array.h>
#include <eval/application.h>
#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-themes.h>
#include <gui/win32/api/win32-touch.h>
#include <gui/win32/api/win32-version.h>
#include <gui/win32/menus/win32-automenuhook.h>
#include <gui/win32/menus/win32-menu.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/win32-document-window.h>
#include <resources.h>

#ifndef TPM_NOANIMATION
#  define TPM_NOANIMATION 0x4000
#endif

#ifndef TBCDRF_USECDCOLORS
#  define TBCDRF_USECDCOLORS  0x00800000
#endif

#define WM_MY_SHOWMENUITEM   (WM_USER + 1)

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif


using namespace richmath;

class Win32Menubar::Impl {
  public:
    Impl(Win32Menubar &self) : self(self) {}
    
    bool try_draw_pin_icon(HDC hdc, const RECT &rect, COLORREF color, ControlState state);
    
  private:
    Win32Menubar &self;
};

static Win32Menubar *current_menubar = nullptr;

static DWORD point_to_dword(const POINT &pt) {
  return ((short)pt.x) | (((short)pt.y) << 16);
}

namespace w { // win32 typedefs that mingw does not provide
  typedef struct tagNMKEY {
    NMHDR  hdr;
    UINT   nVKey;
    UINT   uFlags;
  } NMKEY, *LPNMKEY;
  
  typedef struct tagNMCHAR {
    NMHDR  hdr;
    UINT   ch;
    DWORD  dwItemPrev;
    DWORD  dwItemNext;
  } NMCHAR, *LPNMCHAR;
}

//{ class Win32Menubar ...

Win32Menubar::Win32Menubar(Win32DocumentWindow *window, HWND parent, SharedPtr<Win32Menu> menu)
  : Base(),
    _appearence(MenuAppearence::Show),
    _window(window),
    _hwnd(nullptr),
    _menu(menu),
    _font(nullptr),
    image_list(nullptr),
    current_popup(nullptr),
    current_item(0),
    next_item(0),
    hot_item(0),
    dpi(96),
    focused(false),
    menu_animation(false),
    _ignore_pressed_alt_key(false),
    _use_dark_mode(false)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  INITCOMMONCONTROLSEX icex;
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC  = 0;//ICC_BAR_CLASSES;
  InitCommonControlsEx(&icex);
  
  if(Win32Themes::BufferedPaintInit)
    Win32Themes::BufferedPaintInit();
  
  bool layered = Win32Version::is_windows_8_or_newer();
  _hwnd = CreateWindowExW(
            layered ? WS_EX_LAYERED : 0, // only supported for child windows on Windows 8 or later
            TOOLBARCLASSNAMEW,
            L"",
            WS_CHILD | CCS_NOMOVEY | CCS_NORESIZE | CCS_NODIVIDER | TBSTYLE_LIST | TBSTYLE_FLAT,
            0, 0, 0, 0,
            parent,
            0/* id */,
            GetModuleHandleW(nullptr),
            nullptr);
  
  if(layered)
    SetLayeredWindowAttributes(_hwnd, CLR_NONE, 0xFF, LWA_ALPHA);
  
  SendMessageW(_hwnd, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
  
  dpi = Win32HighDpi::get_dpi_for_window(_hwnd);
  
  _num_items = _menu.is_valid() ? WIN32report_errval(GetMenuItemCount(_menu->hmenu()), -1) : 0;
  if(_num_items < 0) _num_items = 0;
  _visible_items   = _num_items;
  
  Array<TBBUTTON>  buttons(_num_items + 3);
  Array<wchar_t[100]> texts(buttons.length());
  
  for(int i = 0; i < _num_items; ++i) {
    MENUITEMINFOW info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_SUBMENU;
    info.dwTypeData = texts[i] + 1;                      // prepend ' '
    info.cch = sizeof(texts[i]) / sizeof(texts[i][0]) - 3; // append ' ' + '\0'
    
    if(!WIN32report(GetMenuItemInfoW(_menu->hmenu(), i, TRUE, &info)))
      continue;
    
    texts[i][0] = ' ';
    texts[i][info.cch + 1] = ' ';
    texts[i][info.cch + 2] = '\0';
    
    buttons[i].iBitmap = I_IMAGENONE;
    buttons[i].idCommand = i + 1;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = BTNS_AUTOSIZE | BTNS_DROPDOWN;
    buttons[i].dwData = 0;
    buttons[i].iString = (INT_PTR)texts[i];
  }
  
  buttons[overflow_index()].iBitmap = I_IMAGENONE;
  buttons[overflow_index()].idCommand = overflow_index() + 1;
  buttons[overflow_index()].fsState = TBSTATE_ENABLED | TBSTATE_HIDDEN;
  buttons[overflow_index()].fsStyle = BTNS_AUTOSIZE | BTNS_DROPDOWN;
  buttons[overflow_index()].dwData = 0;
  buttons[overflow_index()].iString = (INT_PTR)L"\xbb";
  
  buttons[separator_index()].iBitmap = I_IMAGENONE;
  buttons[separator_index()].idCommand = separator_index() + 1;
  buttons[separator_index()].fsState = TBSTATE_ENABLED;
  buttons[separator_index()].fsStyle = BTNS_BUTTON;
  buttons[separator_index()].dwData = 0;
  buttons[separator_index()].iString = (INT_PTR)L"";
  
  buttons[pin_index()].iBitmap = 0;
  buttons[pin_index()].idCommand = pin_index() + 1;
  buttons[pin_index()].fsState = TBSTATE_ENABLED;
  buttons[pin_index()].fsStyle = BTNS_BUTTON | BTNS_CHECK;
  buttons[pin_index()].dwData = 0;
  buttons[pin_index()].iString = (INT_PTR)L"";
  
  SendMessageW(_hwnd, TB_ADDBUTTONSW,
               (WPARAM)buttons.length(),
               (LPARAM)buttons.items());
  
  reload_image_list();
  theme_changed();
}

Win32Menubar::~Win32Menubar() {
  if(current_menubar == this)
    current_menubar = nullptr;
    
  WIN32report(DestroyWindow(_hwnd));
  if(_font)
    DeleteObject(_font);
  
  if(Win32Themes::BufferedPaintUnInit)
    Win32Themes::BufferedPaintUnInit();
    
  ImageList_Destroy(image_list);
}

void Win32Menubar::reload_image_list() {
  if(image_list)
    ImageList_Destroy(image_list);
  
  static const int sizes[] = {16, 24, 32};
  int best_size = MulDiv(16, dpi, 96);
  int index = 0;
  while(index < sizeof(sizes)/sizeof(sizes[0]) && sizes[index] <= best_size) {
    ++index;
  }
  --index;
  
  image_list = ImageList_Create(sizes[index], sizes[index], ILC_COLOR24 | ILC_MASK, 2, 0);
  
  if(HBITMAP hbmp = WIN32report(LoadBitmapW((HINSTANCE)GetModuleHandle(nullptr), MAKEINTRESOURCEW(BMP_PIN16 + index)))) {
    ImageList_AddMasked(image_list, hbmp, RGB(0xFF, 0, 0xFF));
    DeleteObject(hbmp);
  }
  
  TBBUTTONINFOW info = {0};
  info.cbSize = sizeof(info);
  info.dwMask = TBIF_BYINDEX | TBIF_SIZE;
  SendMessageW(_hwnd, TB_GETBUTTONINFOW, pin_index(), (LPARAM)&info);
  info.cx = MulDiv(16 + 4, dpi, 96);//best_size;
  SendMessageW(_hwnd, TB_SETBUTTONINFOW, pin_index(), (LPARAM)&info);
  
  SendMessageW(_hwnd, TB_SETIMAGELIST, 0, (LPARAM)image_list);
}

bool Win32Menubar::visible() {
  return (GetWindowLongW(_hwnd, GWL_STYLE) & WS_VISIBLE) != 0;
}

int Win32Menubar::best_height() {
  if(visible()) 
    return Win32HighDpi::get_system_metrics_for_dpi(SM_CYMENU, dpi);
  
  return 0;
}

void Win32Menubar::appearence(MenuAppearence value) {
  _appearence = value;
  
  switch(_appearence) {
    case MenuAppearence::Show:
      if(!visible()) {
        ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
        _window->rearrange();
      }
      break;
      
    case MenuAppearence::Hide:
      if(visible()) {
        kill_focus();
        ShowWindow(_hwnd, SW_HIDE);
        _window->rearrange();
      }
      break;
      
    case MenuAppearence::AutoShow:
      break;
  }
  
  for(int i = separator_index(); i <= pin_index(); ++i) {
    TBBUTTONINFOW info;
    info.cbSize = sizeof(info);
    info.dwMask = TBIF_BYINDEX | TBIF_STATE;
    
    SendMessageW(_hwnd, TB_GETBUTTONINFOW, i, (LPARAM)&info);
    
    if(_appearence == MenuAppearence::AutoShow)
      info.fsState &= ~TBSTATE_HIDDEN;
    else
      info.fsState |= TBSTATE_HIDDEN;
      
    SendMessageW(_hwnd, TB_SETBUTTONINFOW, i, (LPARAM)&info);
  }
//  if(_autohide){
//    if(visible() && !is_pinned())
//      kill_focus();
//  }
//  else if(!visible())
//    set_focus(0);
}

bool Win32Menubar::is_pinned() {
  if(_appearence != MenuAppearence::AutoShow)
    return false;
    
  TBBUTTONINFOW info;
  info.cbSize = sizeof(info);
  info.dwMask = TBIF_BYINDEX | TBIF_STATE;
  
  SendMessageW(_hwnd, TB_GETBUTTONINFOW, pin_index(), (LPARAM)&info);
  
  return (info.fsState & TBSTATE_CHECKED) != 0;
}

bool Win32Menubar::toggle_pin(bool new_pinned) {
  if(_appearence != MenuAppearence::AutoShow)
    return false;
  
  TBBUTTONINFOW info;
  info.cbSize = sizeof(info);
  info.dwMask = TBIF_BYINDEX | TBIF_STATE;
  
  SendMessageW(_hwnd, TB_GETBUTTONINFOW, pin_index(), (LPARAM)&info);
  
  bool was_pinned = (info.fsState & TBSTATE_CHECKED) != 0;
  if(new_pinned == was_pinned) 
    return true;
  
  if(new_pinned)
    info.fsState |= TBSTATE_CHECKED;
  else
    info.fsState &= ~TBSTATE_CHECKED;
  
  bool success = (0 != SendMessageW(_hwnd, TB_SETBUTTONINFOW, pin_index(), (LPARAM)&info));
  if(new_pinned) {
    if(!visible()) {
      ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
      _window->rearrange();
      InvalidateRect(_hwnd, nullptr, FALSE);
      UpdateWindow(_hwnd);
    }
  }
  else if(visible()) {
    ShowWindow(_hwnd, SW_HIDE);
    _window->rearrange();
  }
  return success;
}

void Win32Menubar::use_dark_mode(bool dark_mode) {
  if(_use_dark_mode == dark_mode)
    return;
  
  _use_dark_mode = dark_mode;
  InvalidateRect(_hwnd, nullptr, FALSE);
}

void Win32Menubar::show_menu(int item) {
  if(item <= 0 || !_menu.is_valid())
    return;
    
  next_item = 0;
  
  HWND parent = (HWND)GetWindowLongPtr(_hwnd, GWLP_HWNDPARENT);
  
  TPMPARAMS tpm;
  memset(&tpm, 0, sizeof(tpm));
  tpm.cbSize = sizeof(tpm);
  SendMessageW(_hwnd, TB_GETRECT, item, (LPARAM)&tpm.rcExclude);
  
  MapWindowPoints(_hwnd, nullptr, (POINT*)&tpm.rcExclude, 2);
  
  SetFocus(_hwnd);
  
  DWORD cmd = 0;
  if(!current_menubar || current_menubar == this) {
    HMENU tmp_menu = nullptr;
    if(item == overflow_index() + 1) {
      tmp_menu = WIN32report(CreatePopupMenu());
      if(_visible_items < _num_items) {
        for(int i = _visible_items; i < _num_items; ++i) {
          wchar_t text[100];
          GetMenuStringW(_menu->hmenu(), i, text, sizeof(text)/sizeof(text[0]), MF_BYPOSITION);
          text[sizeof(text)/sizeof(text[0]) - 1] = L'\0';
          WIN32report(AppendMenuW(tmp_menu, MF_POPUP, (UINT_PTR)GetSubMenu(_menu->hmenu(), i), text));
        }
      }
      else {
        WIN32report(AppendMenuW(tmp_menu, MF_DISABLED, 0, L"(empty)"));
      }
      
      current_popup = tmp_menu;
    }
    else
      current_popup = GetSubMenu(_menu->hmenu(), item - 1);
    
    current_item = item;
    current_menubar = this;
    
    MenuExitInfo exit_info;
    {
      Win32AutoMenuHook menu_hook(current_popup, parent, _hwnd, true, true);
      
      POINT pt;
      pt.y = tpm.rcExclude.bottom;
      UINT align;
      DWORD ex_style = GetWindowLongW(_hwnd, GWL_EXSTYLE);
      if(ex_style & WS_EX_LAYOUTRTL) {
        pt.x = tpm.rcExclude.right;
        align = TPM_RIGHTALIGN;
      }
      else {
        pt.x = tpm.rcExclude.left;
        align = TPM_LEFTALIGN;
      }

      UINT flags = TPM_RETURNCMD | align;
      if(!menu_animation)
        flags |= TPM_NOANIMATION;
      
      Win32Menu::use_dark_mode = _use_dark_mode;
      WIN32report(cmd = TrackPopupMenuEx(
              current_popup,
              flags,
              pt.x,
              pt.y,
              parent,
              &tpm));
      
      exit_info = menu_hook.exit_info;
    }
    
    switch(exit_info.reason) {
      case MenuExitReason::LeftKey:
        next_item = item - 1;
        if(next_item <= 0) {
          next_item = WIN32report_errval(GetMenuItemCount(_menu->hmenu()), -1);
        }
        break;
        
      case MenuExitReason::RightKey:
        next_item = item + 1;
        if(next_item > WIN32report_errval(GetMenuItemCount(_menu->hmenu()), -1)) {
          next_item = 1;
        }
        break;
      
      case MenuExitReason::ExplicitCmd: 
        cmd = exit_info.cmd;
        break;
      
      default:
        exit_info.handle_after_exit();
        break;
    }
    
    current_item = 0;
    current_popup = nullptr;
    if(tmp_menu) {
      for(int i = WIN32report_errval(GetMenuItemCount(tmp_menu), -1)-1; i >= 0; --i) {
        WIN32report(RemoveMenu(tmp_menu, i, MF_BYPOSITION));
      }
      WIN32report(DestroyMenu(tmp_menu));
    }
  }
  
  if(cmd) {
    SendMessageW(parent, WM_COMMAND, cmd, 0);
    kill_focus();
  }
  else if(/*_autohide && */visible())
    SetCapture(_hwnd);
  
  if(next_item > 0) {
    PostMessage(
      (HWND)GetWindowLongPtr(_hwnd, GWLP_HWNDPARENT),
      WM_MY_SHOWMENUITEM,
      (WPARAM)next_item,
      0);
  }
}

void Win32Menubar::show_sysmenu() {
  HWND parent = (HWND)GetWindowLongPtr(_hwnd, GWLP_HWNDPARENT);
  
  TPMPARAMS tpm;
  memset(&tpm, 0, sizeof(tpm));
  tpm.cbSize = sizeof(tpm);
  WIN32report(GetWindowRect(parent, &tpm.rcExclude));
  
  tpm.rcExclude.left += Win32HighDpi::get_system_metrics_for_dpi(SM_CXSIZEFRAME, dpi) + Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
  tpm.rcExclude.top +=  Win32HighDpi::get_system_metrics_for_dpi(SM_CYSIZEFRAME, dpi) + Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi);
  tpm.rcExclude.right  = tpm.rcExclude.left + Win32HighDpi::get_system_metrics_for_dpi(SM_CXSMICON, dpi);
  tpm.rcExclude.bottom = tpm.rcExclude.top  + Win32HighDpi::get_system_metrics_for_dpi(SM_CYCAPTION, dpi);
  
  MenuExitInfo exit_info;
  DWORD cmd = 0;
  {
    HMENU menu = GetSystemMenu(parent, FALSE);
    Win32AutoMenuHook menu_hook(menu, parent, nullptr, false, false);
    
    int x;
    UINT align;
    DWORD ex_style = GetWindowLongW(_hwnd, GWL_EXSTYLE);
    if(ex_style & WS_EX_LAYOUTRTL) {
      align = TPM_RIGHTALIGN;
      x = tpm.rcExclude.right;
    }
    else {
      align = TPM_LEFTALIGN;
      x = tpm.rcExclude.left;
    }
    
    UINT flags = TPM_RETURNCMD | align;
    if(!menu_animation)
      flags |= TPM_NOANIMATION;
    
    Win32Menu::use_dark_mode = _use_dark_mode;
    WIN32report(cmd = TrackPopupMenuEx(
            menu,
            flags,
            x,
            tpm.rcExclude.bottom,
            parent,
            &tpm));
    
    exit_info = menu_hook.exit_info;
  }
  
  if(!cmd && !exit_info.handle_after_exit()) {
    if(exit_info.reason == MenuExitReason::ExplicitCmd)
      cmd = exit_info.cmd;
  }
  
  if(cmd) {
    SendMessageW(parent, WM_COMMAND, cmd, 0);
    kill_focus();
  }
}

void Win32Menubar::set_focus(int item) {
  if(_appearence == MenuAppearence::Hide)
    return;
    
  next_item = 0;
  
  if(!visible()) {
    ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
    _window->rearrange();
    InvalidateRect(_hwnd, nullptr, FALSE);
    UpdateWindow(_hwnd);
  }
  
  focused = true;
  SetFocus(_hwnd);
  SendMessageW(_hwnd, TB_SETHOTITEM, item, 0);
  
//  if(_autohide){
  SetCursor(LoadCursor(0, IDC_ARROW));
  SetCapture(_hwnd);
//  }
}

void Win32Menubar::kill_focus() {
  //EndMenu();
  focused = false;
  SendMessageW(_hwnd, TB_SETHOTITEM, -1, 0);
  
  if(GetFocus() == _hwnd) {
    SetFocus((HWND)GetWindowLongPtr(_hwnd, GWLP_HWNDPARENT));
  }
  
  if(current_menubar == this)
    current_menubar = nullptr;
  
  hot_item = 0;
  if(_appearence == MenuAppearence::AutoShow && !is_pinned()) {
    ShowWindow(_hwnd, SW_HIDE);
    _window->rearrange();
  }
  
  if(GetCapture() == _hwnd) {
    ReleaseCapture();
  }
}

void Win32Menubar::on_dpi_changed(int new_dpi) {
  if(new_dpi == dpi)
    return;
    
  dpi = new_dpi;
  reload_image_list();
  theme_changed();
  resized();
}

void Win32Menubar::theme_changed() {
  int row_height = Win32HighDpi::get_system_metrics_for_dpi(SM_CYMENU, dpi);
  
  SendMessageW(_hwnd, TB_SETPADDING, 0, (MulDiv(4, dpi, 96) << 16) | 0);
  SendMessageW(_hwnd, TB_SETBUTTONSIZE, 0, (row_height << 16) | 1);
  
  NONCLIENTMETRICSW ncm = {0};
  ncm.cbSize = sizeof(ncm);
  if(Win32HighDpi::get_nonclient_metrics_for_dpi(&ncm, dpi)) {
    if(_font)
      DeleteObject(_font);
    
    _font = CreateFontIndirectW(&ncm.lfMenuFont);
    SendMessageW(_hwnd, WM_SETFONT, (WPARAM)_font, (LPARAM)TRUE);
  }
  
  // reset the texts to force recalulating button sizes
  for(int i = 0; i < separator_index(); ++i) {
    wchar_t text[100];
    TBBUTTONINFOW info = {0};
    info.cbSize = sizeof(info);
    info.dwMask = TBIF_BYINDEX | TBIF_STYLE | TBIF_TEXT;
    info.pszText = text;
    info.cchText = sizeof(text)/sizeof(text[0]);
    int index = SendMessageW(_hwnd, TB_GETBUTTONINFOW, i, (LPARAM)&info);
    if(index >= 0) {
      SendMessageW(_hwnd, TB_SETBUTTONINFOW, i, (LPARAM)&info);
    }
  }
  
  SendMessageW(_hwnd, TB_AUTOSIZE, 0, 0);
  resized();
}

void Win32Menubar::resized() {
  RECT rect;
  GetClientRect(_hwnd, &rect);
  
  TBBUTTONINFOW info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  info.dwMask = TBIF_BYINDEX | TBIF_SIZE;
  SendMessageW(_hwnd, TB_GETBUTTONINFOW, pin_index(), (LPARAM)&info);
  int max_width = rect.right - info.cx;
  
  SendMessageW(_hwnd, TB_GETBUTTONINFOW, overflow_index(), (LPARAM)&info);
  max_width-= info.cx;
  
  int w = 0;
  _visible_items = 0;
  while(_visible_items < _num_items) {
    info.cx = 0;
    SendMessageW(_hwnd, TB_GETBUTTONINFOW, _visible_items, (LPARAM)&info);
    if(w + info.cx > max_width) 
      break;
    
    w+= info.cx;
    ++_visible_items;
  }
  
  info.dwMask = TBIF_BYINDEX | TBIF_STATE;
  for(int i = 0; i < _visible_items; ++i) {
    SendMessageW(_hwnd, TB_GETBUTTONINFOW, i, (LPARAM)&info);
    if(info.fsState & TBSTATE_HIDDEN) {
      info.fsState &= ~TBSTATE_HIDDEN;
      SendMessageW(_hwnd, TB_SETBUTTONINFOW, i, (LPARAM)&info);
    }
  }
  
  for(int i = _visible_items; i < _num_items; ++i) {
    SendMessageW(_hwnd, TB_GETBUTTONINFOW, i, (LPARAM)&info);
    if(!(info.fsState & TBSTATE_HIDDEN)) {
      info.fsState |= TBSTATE_HIDDEN;
      SendMessageW(_hwnd, TB_SETBUTTONINFOW, i, (LPARAM)&info);
    }
  }
  
  if(_visible_items < _num_items) {
    SendMessageW(_hwnd, TB_GETBUTTONINFOW, overflow_index(), (LPARAM)&info);
    if(info.fsState & TBSTATE_HIDDEN) {
      info.fsState &= ~TBSTATE_HIDDEN;
      SendMessageW(_hwnd, TB_SETBUTTONINFOW, overflow_index(), (LPARAM)&info);
    }
  }
  else {
    SendMessageW(_hwnd, TB_GETBUTTONINFOW, overflow_index(), (LPARAM)&info);
    if(!(info.fsState & TBSTATE_HIDDEN)) {
      info.fsState |= TBSTATE_HIDDEN;
      SendMessageW(_hwnd, TB_SETBUTTONINFOW, overflow_index(), (LPARAM)&info);
    }
  }
  
//  info.dwMask = TBIF_BYINDEX | TBIF_SIZE;
//  info.cx = std::max(0, max_width - w);
//  SendMessageW(_hwnd, TB_SETBUTTONINFOW, separator_index(), (LPARAM)&info);


  SIZE size;
  SendMessageW(_hwnd, TB_GETMAXSIZE, 0, (LPARAM)&size);
  
  //TBBUTTONINFOW info;
  //memset(&info, 0, sizeof(info));
  //info.cbSize = sizeof(info);
  info.dwMask = TBIF_BYINDEX | TBIF_SIZE;
  SendMessageW(_hwnd, TB_GETBUTTONINFOW, separator_index(), (LPARAM)&info);
  
  int new_sep_width = info.cx + rect.right - size.cx;
  if(new_sep_width < 1)
    new_sep_width = 1;
    
  info.cx = new_sep_width;
  SendMessageW(_hwnd, TB_SETBUTTONINFOW, separator_index(), (LPARAM)&info);
}

bool Win32Menubar::callback(LRESULT *result, UINT message, WPARAM wParam, LPARAM lParam) {
  switch(message) {
    case WM_DPICHANGED: {
      on_dpi_changed((int)LOWORD(wParam));
    } break;
    
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED: {
      theme_changed();
    } break;
    
    case WM_NOTIFY: {
        NMHDR *header = (NMHDR *)lParam;
        
        if(header->hwndFrom == _hwnd) {
          switch(header->code) {
            case TBN_DROPDOWN: {
                NMTOOLBARW *tb = (NMTOOLBARW *)lParam;
                
                if(tb->iItem == current_item && current_popup != nullptr) {
                  *result = TBDDRET_DEFAULT;
                  return true;
                }
                Win32Menu::use_large_items = Win32Touch::get_mouse_message_source() == DeviceKind::Touch; // Does this work in TBN_DROPDOWN?
                show_menu(tb->iItem);
                
                *result = TBDDRET_DEFAULT;//TBDDRET_TREATPRESSED;
              } return true;
              
            case TBN_HOTITEMCHANGE: {
                NMTBHOTITEM *hi = (NMTBHOTITEM *)lParam;
                
                if(hi->dwFlags & HICF_LEAVING) 
                  hot_item = 0;
                else
                  hot_item = hi->idNew;
                
                if((hi->dwFlags & (HICF_MOUSE | ~HICF_LEAVING)) &&
                    current_menubar == this                     &&
                    current_item > 0                            &&
                    current_item != hi->idNew                   &&
                    hi->idNew)
                {
                  if(hi->idNew <= _num_items + 1) {
                    if(current_item)
                      next_item = hi->idNew;
                    
                    WIN32report(EndMenu());
                    *result = 0;
                    return true;
                  }
                  else {
                    *result = 0;
                    return true;
                  }
                }
              } break;
              
            case NM_CLICK: {
                POINT pt = ((NMMOUSE *)lParam)->pt;
                
                if(current_popup == nullptr) {
                  int index = SendMessageW(_hwnd, TB_HITTEST, 0, (LPARAM)&pt);
                  
                  if(index == pin_index()) {
                    TBBUTTONINFOW info;
                    info.cbSize = sizeof(info);
                    info.dwMask = TBIF_BYINDEX | TBIF_STATE;
                    SendMessageW(_hwnd, TB_GETBUTTONINFOW, index, (LPARAM)&info);
                    
                    info.dwMask = TBIF_BYINDEX | TBIF_IMAGE;
                    if(info.fsState & TBSTATE_CHECKED) {
                      info.iImage = 1;
                      kill_focus();
                    }
                    else {
                      info.iImage = 0;
                      set_focus(0);
                    }
                    
                    SendMessageW(_hwnd, TB_SETBUTTONINFOW, index, (LPARAM)&info);
                  }
                  else if(index < 0 || index == separator_index()) 
                    kill_focus();
                }
                
              } break;
              
            case NM_KEYDOWN: {
                w::NMKEY *key = (w::NMKEY *)lParam;
                
                switch(key->nVKey) {
                  case VK_MENU:
                  case VK_ESCAPE: {
                      kill_focus();
                      *result = 1;
                    } return true;
                }
              } break;
              
            case NM_CHAR: {
                w::NMCHAR *chr = (w::NMCHAR *)lParam;
                
                if(chr->ch == ' ') {
                  show_sysmenu();
                  *result = TRUE;
                  return true;
                }
              } break;
              
            case NM_CUSTOMDRAW: {
                NMTBCUSTOMDRAW *draw = (NMTBCUSTOMDRAW *)lParam;
                
                // The normal menu (Vista) also has no animation, but a toolbar has.
                // TODO: This has no effect before the first menu popped up
                //       (Press ALT,LEFT,RIGHT,DOWN,LEFT,RIGHT)
                if(Win32Themes::BufferedPaintStopAllAnimations)
                  Win32Themes::BufferedPaintStopAllAnimations(_hwnd);
                  
                switch(draw->nmcd.dwDrawStage) {
                  case CDDS_PREPAINT: {
                      RECT rect;
                      GetClientRect(_hwnd, &rect);

                      cairo_surface_t *surface = cairo_win32_surface_create_with_dib(
                                                   CAIRO_FORMAT_RGB24,
                                                   rect.right  - rect.left,
                                                   rect.bottom - rect.top);
                      if(cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
                        HDC bmp_dc = cairo_win32_surface_get_dc(surface);
                        Win32ControlPainter::win32_painter.draw_menubar(bmp_dc, &rect, _use_dark_mode);
                        
                        cairo_surface_mark_dirty(surface);
                        cairo_t *cr = cairo_create(surface);
                        {
                          Canvas canvas(cr);
                          
                          _window->paint_background_at(canvas, _hwnd, true);
                        }
                        cairo_destroy(cr);
                        
                        BitBlt(
                          draw->nmcd.hdc,
                          rect.left,
                          rect.top,
                          rect.right  - rect.left,
                          rect.bottom - rect.top,
                          bmp_dc,
                          0,
                          0,
                          SRCCOPY);
                      }
                      else
                        Win32ControlPainter::win32_painter.draw_menubar(draw->nmcd.hdc, &rect, _use_dark_mode);
                        
                      cairo_surface_destroy(surface);
                      
                      *result = CDRF_NOTIFYITEMDRAW;//CDRF_DODEFAULT;
                    } return true;
                    
                  case CDDS_ITEMPREPAINT: {
                      draw->nmcd.uItemState = CDIS_DEFAULT;
                      if(Win32Themes::DwmEnableComposition != 0) // >= Vista
                        *result = TBCDRF_USECDCOLORS;
                      else
                        *result = CDRF_DODEFAULT;
                      
                      bool hot_tracking = true;
                      ControlState state = ControlState::Normal;
                      if(GetForegroundWindow() == _window->hwnd()) {
                        BOOL ht;
                        if(SystemParametersInfoW(SPI_GETHOTTRACKING, 0, &ht, FALSE))
                          hot_tracking = ht;
                          
                        draw->clrText = GetSysColor(COLOR_MENUTEXT);
                      }
                      else {
                        if(Win32Themes::IsAppThemed && Win32Themes::IsAppThemed()) {
                          hot_tracking = false;
                        }
                        else {
                          BOOL ht;
                          if(SystemParametersInfoW(SPI_GETHOTTRACKING, 0, &ht, FALSE))
                            hot_tracking = ht;
                        }
                        
                        /* When the menu bar color is white, Windows uses gray for menu item text 
                           in background windows, no matter, what COLOR_GRAYTEXT is.
                           As soon as COLOR_MENUBAR is not 0xFFFFFF, Windows uses COLOR_GRAYTEXT for 
                           the background window menu bar item texts.
                           
                           Example: "High contrast white" theme, where COLOR_GRAYTEXT is actually green.
                         */
                        if(GetSysColor(COLOR_MENUBAR) == 0xFFFFFF)
                          draw->clrText = 0x808080;
                        else
                          draw->clrText = GetSysColor(COLOR_GRAYTEXT);
                      }
                        
                      if((int)draw->nmcd.dwItemSpec == separator_index() + 1) {
                        *result = CDRF_SKIPDEFAULT;
                      }
                      else if((int)draw->nmcd.dwItemSpec == pin_index() + 1) {
                      
                        TBBUTTONINFOW info;
                        info.cbSize = sizeof(info);
                        info.dwMask = TBIF_STATE;
                        
                        SendMessageW(_hwnd, TB_GETBUTTONINFOW, (int)draw->nmcd.dwItemSpec, (LPARAM)&info);
                        
                        if((info.fsState & TBSTATE_CHECKED) || (draw->nmcd.uItemState & CDIS_CHECKED)) {
                          if(hot_tracking && (int)draw->nmcd.dwItemSpec == hot_item)
                            state = ControlState::PressedHovered;
                          else
                            state = ControlState::Pressed;
                        }
                        else if(hot_tracking && (int)draw->nmcd.dwItemSpec == hot_item)
                          state = ControlState::Hovered; // ControlState::Hot
                      }
                      else {
                        if( current_item == (int)draw->nmcd.dwItemSpec ||
                            next_item == (int)draw->nmcd.dwItemSpec) 
                        {
                          state = ControlState::Pressed;
                        }
                        else if(hot_tracking && hot_item == (int)draw->nmcd.dwItemSpec) 
                          state = ControlState::Hovered;
                      }
                      
                      Win32ControlPainter::win32_painter.draw_menubar_itembg(
                        draw->nmcd.hdc,
                        &draw->nmcd.rc,
                        state,
                        _use_dark_mode);
                      
                      if(_use_dark_mode) {
                        draw->clrText = 0xFFFFFFu & (~draw->clrText);
                      }
                      
                      if((int)draw->nmcd.dwItemSpec == pin_index() + 1) {
                        if(Impl(*this).try_draw_pin_icon(draw->nmcd.hdc, draw->nmcd.rc, draw->clrText, state))
                          *result = CDRF_SKIPDEFAULT;
                      }
                    } return true;
                }
              } break;
          }
        }
      } break;
      
    case WM_MY_SHOWMENUITEM: {
        RECT rect;
        if(!SendMessageW(_hwnd, TB_GETRECT, (int)wParam, (LPARAM)&rect))
          break;
        
        POINT pt = {(rect.left + rect.right) / 2, (rect.bottom + rect.top) / 2};
        
        menu_animation = false;
        
        SendMessageW(
          _hwnd,
          WM_LBUTTONDOWN,
          MK_LBUTTON,
          point_to_dword(pt));
        SendMessageW(
          _hwnd,
          WM_LBUTTONUP,
          MK_LBUTTON,
          point_to_dword(pt));
          
        menu_animation = true;
      } break;
      
    case WM_INITMENUPOPUP: {
        HMENU sub = (HMENU)wParam;
        
        Win32Menu::init_popupmenu(sub);
      } break;
    
    case WM_MENUDRAG: {
        *result = Win32Menu::on_menudrag(wParam, lParam);
      } return true;
    
    case WM_MENUGETOBJECT: {
        *result = Win32Menu::on_menugetobject(wParam, lParam);
      } return true;
    
    case WM_MENUSELECT: {
        Win32Menu::on_menuselect(wParam, lParam);
      } break;
    
    case WM_EXITSIZEMOVE:
      _ignore_pressed_alt_key = (GetKeyState(VK_MENU) & ~1) != 0;
      break;
      
    case WM_SYSKEYDOWN: {
        if(_appearence != MenuAppearence::Hide) {
          if(wParam == VK_MENU)
            return true;
      
          if(!(GetKeyState(VK_SHIFT) & ~1) && !(GetKeyState(VK_CONTROL) & ~1)) {
            int id = 0;
            if(SendMessageW(_hwnd, TB_MAPACCELERATOR, wParam, (LPARAM)&id)) {
              set_focus(0);
              Win32Menu::use_large_items = false;
              show_menu(id);
              return true;
            }
          }
        }
      } break;
      
    case WM_SYSKEYUP: {
        if(wParam == VK_MENU && _ignore_pressed_alt_key) {
          _ignore_pressed_alt_key = false;
          break;
        }
        
        if(_appearence != MenuAppearence::Hide) {
          bool was_visible = visible();
          
          switch(wParam) {
            case VK_MENU:
              set_focus(0);
              if(was_visible) {
                Win32Menu::use_large_items = false;
                show_menu(1);
              }
              return true;
              
            case VK_F10:
              if(!(GetKeyState(VK_SHIFT) & ~1)) {
                set_focus(0);
                if(was_visible) {
                  Win32Menu::use_large_items = false;
                  show_menu(1);
                }
                return true;
              }
              break;
          }
        }
      } break;
      
    case WM_SYSCOMMAND: {
        if(wParam == SC_CLOSE) {
          if(focused) {
            *result = 0;
            return true;
          }
        }
        
        if((wParam & 0xFFF0) == SC_KEYMENU) {
          if(!lParam) { // TODO: what does this mean/when does this happen? This code predates version control history.
            PostMessage(_hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
            kill_focus();
          }
        }
      } break;
      
    case WM_ACTIVATE: {
        InvalidateRect(_hwnd, nullptr, FALSE);
        if(focused && wParam == WA_INACTIVE)
          kill_focus();
      } break;
  }
  return false;
}

//} ... class Win32Menubar

//{ class Win32Menubar::Impl ...

bool Win32Menubar::Impl::try_draw_pin_icon(HDC hdc, const RECT &rect, COLORREF color, ControlState state) {
  if(!Win32Version::is_windows_10_or_newer()) 
    return false; // Need at least Segoe MDL2 Assets font
  
  if( Win32Themes::OpenThemeData && 
      Win32Themes::CloseThemeData && 
      Win32Themes::DrawThemeTextEx) 
  {
    if(HANDLE theme = Win32Themes::OpenThemeData(self.hwnd(), L"MENU")) {
      Win32Themes::DTTOPTS dtt_opts;
      memset(&dtt_opts, 0, sizeof(dtt_opts));
      dtt_opts.dwSize    = sizeof(dtt_opts);
      dtt_opts.dwFlags   = DTT_COMPOSITED | DTT_TEXTCOLOR;
      dtt_opts.crText    = color;
      
      HFONT font = CreateFontW(
        std::min(
          MulDiv(12, self.dpi, 96),
          (int)std::min(rect.right  - rect.left, rect.bottom - rect.top)),
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        Win32Themes::symbol_font_name());
      HFONT old_font = (HFONT)SelectObject(hdc, font);
      
      static const wchar_t PinSymbol[] = L"\xE718";
      static const wchar_t PinnedSymbol[] = L"\xE840";
      static const wchar_t UnpinSymbol[] = L"\xE77A";
      
      const wchar_t *symbol;
      switch(state) {
        case ControlState::Pressed:        symbol = PinnedSymbol; break;
        case ControlState::PressedHovered: symbol = UnpinSymbol; break;
        default:             symbol = PinSymbol; break;
      }
      
      Win32Themes::DrawThemeTextEx(
        theme,
        hdc,
        0, 0,
        symbol,
        -1,
        DT_VCENTER | DT_CENTER | DT_SINGLELINE,
        (RECT*)&rect,
        &dtt_opts);
      
      SelectObject(hdc, old_font);
      DeleteObject(font);
      Win32Themes::CloseThemeData(theme);
      return true;
    }
  }
  
  return false;
}

//} ... class Win32Menubar::Impl
