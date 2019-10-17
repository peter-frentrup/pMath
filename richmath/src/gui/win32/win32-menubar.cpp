#define WINVER 0x501
#define _WIN32_IE 0x600

#include <gui/win32/win32-menubar.h>

#include <cstdio>
#include <cctype>

#include <commctrl.h>
#include <cairo-win32.h>

#include <util/array.h>
#include <eval/application.h>
#include <gui/win32/win32-automenuhook.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/win32-document-window.h>
#include <gui/win32/win32-menu.h>
#include <gui/win32/win32-highdpi.h>
#include <gui/win32/win32-themes.h>
#include <resources.h>

#ifndef TPM_NOANIMATION
#  define TPM_NOANIMATION 0x4000
#endif

#ifndef TBCDRF_USECDCOLORS
#  define TBCDRF_USECDCOLORS  0x00800000
#endif

#define WM_MY_SHOWMENUITEM   (WM_USER + 1)

using namespace richmath;


//extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;


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
    _appearence(MaAllwaysShow),
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
    focused(false)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  INITCOMMONCONTROLSEX icex;
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC  = 0;//ICC_BAR_CLASSES;
  InitCommonControlsEx(&icex);
  
  if(Win32Themes::BufferedPaintInit)
    Win32Themes::BufferedPaintInit();
    
  _hwnd = CreateWindowExW(
            0,
            TOOLBARCLASSNAMEW,
            L"",
            WS_CHILD | CCS_NOMOVEY | CCS_NORESIZE | CCS_NODIVIDER | TBSTYLE_LIST | TBSTYLE_FLAT,
            0, 0, 0, 0,
            parent,
            0/* id */,
            GetModuleHandle(0),
            nullptr);
            
  SendMessageW(_hwnd, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
  
  dpi = Win32HighDpi::get_dpi_for_window(_hwnd);
  reload_image_list();
  
  
  Array<TBBUTTON>  buttons(_menu.is_valid() ? GetMenuItemCount(_menu->hmenu()) : 0);
  Array<wchar_t[100]> texts(buttons.length());
  
  for(int i = 0; i < buttons.length(); ++i) {
    MENUITEMINFOW info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_SUBMENU;
    info.dwTypeData = texts[i] + 1;                      // prepend ' '
    info.cch = sizeof(texts[i]) / sizeof(texts[i][0]) - 3; // append ' ' + '\0'
    
    GetMenuItemInfoW(_menu->hmenu(), i, TRUE, &info);
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
  
  separator_index = buttons.length();
  buttons.length(separator_index + 2);
  buttons[separator_index].iBitmap = I_IMAGENONE;
  buttons[separator_index].idCommand = separator_index + 1;
  buttons[separator_index].fsState = TBSTATE_ENABLED;
  buttons[separator_index].fsStyle = BTNS_BUTTON;
  buttons[separator_index].dwData = 0;
  buttons[separator_index].iString = (INT_PTR)L"";
  
  pin_index = separator_index + 1;
  buttons[pin_index].iBitmap = 0;
  buttons[pin_index].idCommand = pin_index + 1;
  buttons[pin_index].fsState = TBSTATE_ENABLED;
  buttons[pin_index].fsStyle = BTNS_AUTOSIZE | BTNS_BUTTON | BTNS_CHECK;
  buttons[pin_index].dwData = 0;
  buttons[pin_index].iString = (INT_PTR)L"";
  
  SendMessageW(_hwnd, TB_ADDBUTTONSW,
               (WPARAM)buttons.length(),
               (LPARAM)buttons.items());
  
  theme_changed();
}

Win32Menubar::~Win32Menubar() {
  if(current_menubar == this)
    current_menubar = 0;
    
  DestroyWindow(_hwnd);
  if(_font)
    DeleteObject(_font);
  
  if(Win32Themes::BufferedPaintUnInit)
    Win32Themes::BufferedPaintUnInit();
    
  ImageList_Destroy(image_list);
}

void Win32Menubar::reload_image_list() {
  if(image_list)
    ImageList_Destroy(image_list);
  
  // TODO: use resource compatible with current dpi
  
  int size = 16;//MulDiv(16, dpi, 96);
  image_list = ImageList_Create(size, size, ILC_COLOR24 | ILC_MASK, 2, 0);
  
  HBITMAP hbmp = LoadBitmapW((HINSTANCE)GetModuleHandle(nullptr), MAKEINTRESOURCEW(BMP_PIN));
  ImageList_AddMasked(image_list, hbmp, RGB(0xFF, 0, 0xFF));
  DeleteObject(hbmp);
  
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
    case MaAllwaysShow:
      if(!visible()) {
        ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
        _window->rearrange();
      }
      break;
      
    case MaNeverShow:
      if(visible()) {
        kill_focus();
        ShowWindow(_hwnd, SW_HIDE);
        _window->rearrange();
      }
      break;
      
    case MaAutoShow:
      break;
  }
  
  for(int i = separator_index; i <= pin_index; ++i) {
    TBBUTTONINFOW info;
    info.cbSize = sizeof(info);
    info.dwMask = TBIF_BYINDEX | TBIF_STATE;
    
    SendMessageW(_hwnd, TB_GETBUTTONINFOW, i, (LPARAM)&info);
    
    if(_appearence == MaAutoShow)
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
  if(_appearence != MaAutoShow)
    return false;
    
  TBBUTTONINFOW info;
  info.cbSize = sizeof(info);
  info.dwMask = TBIF_BYINDEX | TBIF_STATE;
  
  SendMessageW(_hwnd, TB_GETBUTTONINFOW, pin_index, (LPARAM)&info);
  
  return (info.fsState & TBSTATE_CHECKED) != 0;
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
  
  POINT pt = {0, 0};
  ClientToScreen(_hwnd, &pt);
  tpm.rcExclude.left  += pt.x;
  tpm.rcExclude.right += pt.x;
  tpm.rcExclude.top   += pt.y;
  tpm.rcExclude.bottom += pt.y;
  
  SetFocus(_hwnd);
  
  int cmd = 0;
  if(!current_menubar || current_menubar == this) {
    current_popup = GetSubMenu(_menu->hmenu(), item - 1);
    current_item = item;
    current_menubar = this;
    
    Win32AutoMenuHook menu_hook(current_popup, _hwnd, true, true);
    
    pt.y = tpm.rcExclude.bottom;
    UINT align;
    if(GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0) {
      align = TPM_LEFTALIGN;
      pt.x = tpm.rcExclude.left;
    }
    else {
      align = TPM_RIGHTALIGN;
      pt.x = tpm.rcExclude.right;
    }
    
    UINT flags = TPM_RETURNCMD | align;
    if(!menu_animation)
      flags |= TPM_NOANIMATION;
      
    Application::delay_dynamic_updates(true);
    
    cmd = TrackPopupMenuEx(
            current_popup,
            flags,
            pt.x,
            pt.y,
            parent,
            &tpm);
    
    switch(menu_hook.exit_reason) {
      case MenuExitReason::LeftKey:
        next_item = item - 1;
        if(next_item <= 0) {
          next_item = GetMenuItemCount(_menu->hmenu());
        }
        break;
        
      case MenuExitReason::RightKey:
        next_item = item + 1;
        if(next_item > GetMenuItemCount(_menu->hmenu())) {
          next_item = 1;
        }
        break;
        
      default:
        Application::delay_dynamic_updates(false);
        break;
    }
    
    current_item = 0;
    current_popup = nullptr;
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
  GetWindowRect(parent, &tpm.rcExclude);
  
  tpm.rcExclude.left += Win32HighDpi::get_system_metrics_for_dpi(SM_CXSIZEFRAME, dpi);
  tpm.rcExclude.top +=  Win32HighDpi::get_system_metrics_for_dpi(SM_CYSIZEFRAME, dpi);
  tpm.rcExclude.right  = tpm.rcExclude.left + Win32HighDpi::get_system_metrics_for_dpi(SM_CXSMICON, dpi);
  tpm.rcExclude.bottom = tpm.rcExclude.top  + Win32HighDpi::get_system_metrics_for_dpi(SM_CYCAPTION, dpi);
  
  int cmd = 0;
  {
    HMENU menu = GetSystemMenu(parent, FALSE);
    Win32AutoMenuHook menu_hook(menu, nullptr, false, false);
    
    int x;
    UINT align;
    if(GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0) {
      align = TPM_LEFTALIGN;
      x = tpm.rcExclude.left;
    }
    else {
      align = TPM_RIGHTALIGN;
      x = tpm.rcExclude.right;
    }
    
    UINT flags = TPM_RETURNCMD | align;
    if(!menu_animation)
      flags |= TPM_NOANIMATION;
      
    Application::delay_dynamic_updates(true);
    
    cmd = TrackPopupMenuEx(
            menu,
            flags,
            x,
            tpm.rcExclude.bottom,
            parent,
            &tpm);
    
    Application::delay_dynamic_updates(false);
  }
  
  if(cmd) {
    SendMessageW(parent, WM_COMMAND, cmd, 0);
    kill_focus();
  }
}

void Win32Menubar::set_focus(int item) {
  if(_appearence == MaNeverShow)
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
  Application::delay_dynamic_updates(true);
  
//  }
}

void Win32Menubar::kill_focus() {
  //EndMenu();
  focused = false;
  SendMessageW(_hwnd, TB_SETHOTITEM, -1, 0);
  SetFocus((HWND)GetWindowLongPtr(_hwnd, GWLP_HWNDPARENT));
  
  if(current_menubar == this)
    current_menubar = nullptr;
  
  hot_item = 0;
  if(_appearence == MaAutoShow && !is_pinned()) {
    ShowWindow(_hwnd, SW_HIDE);
    _window->rearrange();
  }
  
  Application::delay_dynamic_updates(false);
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
  for(int i = 0; i < separator_index; ++i) {
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
  
  SIZE size;
  SendMessageW(_hwnd, TB_GETMAXSIZE, 0, (LPARAM)&size);
  
  TBBUTTONINFOW info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  info.dwMask = TBIF_BYINDEX | TBIF_SIZE;
  
  SendMessageW(_hwnd, TB_GETBUTTONINFOW, separator_index, (LPARAM)&info);
  
  info.cx += rect.right - size.cx;
  if(info.cx < 1)
    info.cx = 1;
  SendMessageW(_hwnd, TB_SETBUTTONINFOW, separator_index, (LPARAM)&info);
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
                  if(hi->idNew <= separator_index) {
                    if(current_item)
                      next_item = hi->idNew;
                    
                    pmath_debug_print("[TBN_HOTITEMCHANGE -> EndMenu]\n");
                    EndMenu();
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
                  
                  if(index == pin_index) {
                    TBBUTTONINFOW info;
                    info.cbSize = sizeof(info);
                    info.dwMask = TBIF_BYINDEX | TBIF_STATE;
                    SendMessageW(_hwnd, TB_GETBUTTONINFOW, pin_index, (LPARAM)&info);
                    
                    info.dwMask = TBIF_BYINDEX | TBIF_IMAGE;
                    if(info.fsState & TBSTATE_CHECKED) {
                      info.iImage = 1;
                      kill_focus();
                    }
                    else {
                      info.iImage = 0;
                      set_focus(0);
                    }
                    
                    SendMessageW(_hwnd, TB_SETBUTTONINFOW, pin_index, (LPARAM)&info);
                  }
                  else if(index < 0 || index == separator_index) 
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
//                Win32ControlPainter::win32_painter.draw_menubar(
//                  draw->nmcd.hdc,
//                  &rect/*&draw->nmcd.rc*/);

                      cairo_surface_t *surface = cairo_win32_surface_create_with_dib(
                                                   CAIRO_FORMAT_RGB24,
                                                   rect.right  - rect.left,
                                                   rect.bottom - rect.top);
                      if(cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
                        HDC bmp_dc = cairo_win32_surface_get_dc(surface);
                        Win32ControlPainter::win32_painter.draw_menubar(bmp_dc, &rect);
                        
                        cairo_surface_mark_dirty(surface);
                        cairo_t *cr = cairo_create(surface);
                        {
                          Canvas canvas(cr);
                          
                          _window->paint_background(&canvas, _hwnd, true);
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
                        Win32ControlPainter::win32_painter.draw_menubar(draw->nmcd.hdc, &rect);
                        
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
                      ControlState state = Normal;
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
                        
                      if((int)draw->nmcd.dwItemSpec == separator_index + 1) {
                        *result = CDRF_SKIPDEFAULT;
                      }
                      else if((int)draw->nmcd.dwItemSpec > separator_index) {
                      
                        TBBUTTONINFOW info;
                        info.cbSize = sizeof(info);
                        info.dwMask = TBIF_STATE;
                        
                        SendMessageW(_hwnd, TB_GETBUTTONINFOW, (int)draw->nmcd.dwItemSpec, (LPARAM)&info);
                        
                        if(info.fsState & TBSTATE_CHECKED) 
                          state = Pressed;
                        else if(draw->nmcd.uItemState & CDIS_CHECKED)
                          state = Pressed;
                        else if(hot_tracking && draw->nmcd.dwItemSpec == hot_item)
                          state = Hovered; // Hot
                      }
                      else {
                        if( current_item == (int)draw->nmcd.dwItemSpec ||
                            next_item == (int)draw->nmcd.dwItemSpec) 
                        {
                          state = Pressed;
                        }
                        else if(hot_tracking && hot_item == (int)draw->nmcd.dwItemSpec) 
                          state = Hovered;
                      }
                      
                      Win32ControlPainter::win32_painter.draw_menubar_itembg(
                        draw->nmcd.hdc,
                        &draw->nmcd.rc,
                        state);
                    } return true;
                }
              } break;
          }
        }
      } break;
      
    case WM_MY_SHOWMENUITEM: {
        RECT rect;
        SendMessageW(_hwnd, TB_GETRECT, (int)wParam, (LPARAM)&rect);
        
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
    
//    case WM_MENUSELECT: {
//        HMENU menu = (HMENU)lParam;
//        UINT item_or_index = LOWORD(wParam);
//        UINT flags = HIWORD(wParam);
//        
//        static HWND previous_aero_peak = nullptr;
//        HWND aero_peak = nullptr;
//        
//        //pmath_debug_print("[WM_MENUSELECT %p %d (%x)]\n", menu, item_or_index, flags);
//        if(!(flags & MF_POPUP)) {
//          Expr cmd = Win32Menu::id_to_command(item_or_index);
//          pmath_debug_print_object("[WM_MENUSELECT ", cmd.get(), "]\n");
//          
//          if(cmd[0] == richmath_FrontEnd_SetSelectedDocument) {
//            auto id = FrontEndReference::from_pmath(cmd[1]);
//            auto doc = FrontEndObject::find_cast<Document>(id);
//            if(doc) {
//              Win32Widget *wid = dynamic_cast<Win32Widget*>(doc->native());
//              if(wid) {
//                aero_peak = wid->hwnd();
//                while(auto parent = GetParent(aero_peak))
//                  aero_peak = parent;
//              }
//            }
//          }
//        }
//        else {
//          pmath_debug_print("[WM_MENUSELECT %p %d (%x)]\n", menu, item_or_index, flags);
//        }
//        
////        if(Win32Themes::has_areo_peak()) {
////          if(aero_peak) {
////            struct callback_data {
////              HWND owner_window;
////              DWORD process_id;
////              DWORD thread_id;
////            } data;
////            data.owner_window = _window->hwnd();
////            data.process_id = GetCurrentProcessId();
////            data.thread_id = GetCurrentThreadId();
////            
////            EnumWindows(
////              [](HWND wnd, LPARAM _data) {
////                auto data = (callback_data*)_data;
////                
////                DWORD pid;
////                DWORD tid = GetWindowThreadProcessId(wnd, &pid);
////                
////                if(pid != data->process_id || tid != data->thread_id)
////                  return TRUE;
////                
////                char class_name[20];
////                const char MenuWindowClass[] = "#32768";
////                if(GetClassNameA(wnd, class_name, sizeof(class_name)) > 0 && strcmp(MenuWindowClass, class_name) == 0) {
////                  BOOL disallow_peak = FALSE;
////                  BOOL excluded_from_peak = FALSE;
////                  Win32Themes::DwmGetWindowAttribute(wnd, Win32Themes::DWMWA_DISALLOW_PEEK,      &disallow_peak,      sizeof(disallow_peak));
////                  Win32Themes::DwmGetWindowAttribute(wnd, Win32Themes::DWMWA_EXCLUDED_FROM_PEEK, &excluded_from_peak, sizeof(excluded_from_peak));
////                  
////                  pmath_debug_print("[menu window %p %s %s]", wnd, disallow_peak ? "disallow peak" : "", excluded_from_peak ? "exclude from peak" : "");
////                  
////                  excluded_from_peak = TRUE;
////                  Win32Themes::DwmSetWindowAttribute(wnd, Win32Themes::DWMWA_EXCLUDED_FROM_PEEK, &excluded_from_peak, sizeof(excluded_from_peak));
////                };
////                return TRUE;
////              },
////              (LPARAM)&data
////            );
////            
////            BOOL excluded_from_peak = TRUE;
////            Win32Themes::DwmSetWindowAttribute(data.owner_window, Win32Themes::DWMWA_EXCLUDED_FROM_PEEK, &excluded_from_peak, sizeof(excluded_from_peak));
////            
////            Win32Themes::activate_aero_peak(true, aero_peak, data.owner_window, LivePreviewTrigger::TaskbarThumbnail);
////            previous_aero_peak = aero_peak;
////          }
////          else if(previous_aero_peak /*&& (flags != 0xFFFF || menu != nullptr)*/) {
////            previous_aero_peak = nullptr;
////            Win32Themes::activate_aero_peak(false, nullptr, nullptr, LivePreviewTrigger::TaskbarThumbnail);
////          
////            BOOL excluded_from_peak = FALSE;
////            Win32Themes::DwmSetWindowAttribute(_window->hwnd(), Win32Themes::DWMWA_EXCLUDED_FROM_PEEK, &excluded_from_peak, sizeof(excluded_from_peak));
////          }
////        }
//      } break;
      
    case WM_SYSKEYDOWN: {
        if(_appearence != MaNeverShow) {
          if(wParam == VK_MENU)
            return true;
      
          if(!(GetKeyState(VK_SHIFT) & ~1) && !(GetKeyState(VK_CONTROL) & ~1)) {
            int id = 0;
            if(SendMessageW(_hwnd, TB_MAPACCELERATOR, wParam, (LPARAM)&id)) {
              set_focus(0);
              show_menu(id);
              return true;
            }
          }
        }
      } break;
      
    case WM_SYSKEYUP: {
        if(_appearence != MaNeverShow) {
          bool was_visible = visible();
          
          switch(wParam) {
            case VK_MENU:
              set_focus(0);
              if(was_visible)
                show_menu(1);
              return true;
              
            case VK_F10:
              if(!(GetKeyState(VK_SHIFT) & ~1)) {
                set_focus(0);
                if(was_visible)
                  show_menu(1);
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
