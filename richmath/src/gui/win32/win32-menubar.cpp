#define WINVER 0x501
#define _WIN32_IE 0x600

#include <gui/win32/win32-menubar.h>

#include <cstdio>
#include <cctype>

#include <commctrl.h>
#include <cairo-win32.h>

#include <util/array.h>
#include <eval/application.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/win32-document-window.h>
#include <gui/win32/win32-menu.h>
#include <gui/win32/win32-themes.h>
#include <resources.h>

#ifndef TPM_NOANIMATION
#define TPM_NOANIMATION 0x4000
#endif

#ifndef TBCDRF_USECDCOLORS
#define TBCDRF_USECDCOLORS  0x00800000
#endif

using namespace richmath;

static Win32Menubar *current_menubar = 0;

#define WM_MY_SHOWMENUITEM   (WM_USER + 1)

static POINT dword_to_point(DWORD dw) {
  POINT pt;
  pt.x = (short)LOWORD(dw);
  pt.y = (short)HIWORD(dw);
  return pt;
}

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
  _hwnd(0),
  _menu(menu),
  focused(false),
  current_popup(0),
  current_item(0),
  next_item(0)
{
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
            NULL);
            
  SendMessageW(_hwnd, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
  
  
  init_image_list();
  
  
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
               
  SendMessageW(_hwnd, TB_AUTOSIZE, 0, 0);
  SendMessageW(_hwnd, TB_SETPADDING, 0, (4 << 16) | 0);
  SendMessageW(_hwnd, TB_SETBUTTONSIZE, 0, 0x010001);
}

Win32Menubar::~Win32Menubar() {
  if(current_menubar == this)
    current_menubar = 0;
    
  DestroyWindow(_hwnd);
  
  if(Win32Themes::BufferedPaintUnInit)
    Win32Themes::BufferedPaintUnInit();
    
  ImageList_Destroy(image_list);
}

void Win32Menubar::init_image_list() {
  image_list = ImageList_Create(16, 16, ILC_COLOR24 | ILC_MASK, 2, 0);
  
  HBITMAP hbmp = LoadBitmapW((HINSTANCE)GetModuleHandle(NULL), MAKEINTRESOURCEW(BMP_PIN));
  ImageList_AddMasked(image_list, hbmp, RGB(0xFF, 0, 0xFF));
  DeleteObject(hbmp);
  
  SendMessageW(_hwnd, TB_SETIMAGELIST, 0, (LPARAM)image_list);
}

bool Win32Menubar::visible() {
  return (GetWindowLongW(_hwnd, GWL_STYLE) & WS_VISIBLE) != 0;
}

int Win32Menubar::height() {
  if(visible()) {
//    RECT rect;
//    GetClientRect(_hwnd, &rect);
//    return rect.bottom - rect.top;
    return GetSystemMetrics(SM_CYMENU);
  }
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
  
  HHOOK hook = register_hook(item);
  if(hook) {
    current_popup = GetSubMenu(_menu->hmenu(), item - 1);
    
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
    
    UINT flags = TPM_RETURNCMD | GetSystemMetrics(SM_MENUDROPALIGNMENT);
    if(!menu_animation)
      flags |= TPM_NOANIMATION;
      
    Application::delay_dynamic_updates(true);
    
    int cmd = TrackPopupMenuEx(
                current_popup,
                flags,
                pt.x,
                pt.y,
                parent,
                &tpm);
                
    if(next_item <= 0)
      Application::delay_dynamic_updates(false);
      
    current_popup = 0;
    
    unregister_hook(hook);
    
    if(cmd) {
      SendMessageW(parent, WM_COMMAND, cmd, 0);
      kill_focus();
    }
    else if(/*_autohide && */visible())
      SetCapture(_hwnd);
  }
  
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
  
  tpm.rcExclude.left += GetSystemMetrics(SM_CXSIZEFRAME);
  tpm.rcExclude.top +=  GetSystemMetrics(SM_CYSIZEFRAME);
  tpm.rcExclude.right  = tpm.rcExclude.left + GetSystemMetrics(SM_CXSMICON);
  tpm.rcExclude.bottom = tpm.rcExclude.top  + GetSystemMetrics(SM_CYCAPTION);
  
  HHOOK hook = register_hook(-1);
  if(hook) {
    current_popup = GetSystemMenu(parent, FALSE);
    
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
    
    Application::delay_dynamic_updates(true);
    
    int cmd = TrackPopupMenuEx(
                current_popup,
                GetSystemMetrics(SM_MENUDROPALIGNMENT) | TPM_RETURNCMD,
                x,
                tpm.rcExclude.bottom,
                parent,
                &tpm);
                
    Application::delay_dynamic_updates(false);
    
    current_popup = 0;
    
    unregister_hook(hook);
    
    if(cmd) {
      SendMessageW(parent, WM_COMMAND, cmd, 0);
      kill_focus();
    }
  }
}

void Win32Menubar::set_focus(int item) {
  if(_appearence == MaNeverShow)
    return;
    
  next_item = 0;
  
  if(!visible()) {
    ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
    _window->rearrange();
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
    current_menubar = 0;
    
  if(_appearence == MaAutoShow && !is_pinned()) {
    ShowWindow(_hwnd, SW_HIDE);
    _window->rearrange();
  }
  
  Application::delay_dynamic_updates(false);
  if(GetCapture() == _hwnd) {
    ReleaseCapture();
  }
}

HHOOK Win32Menubar::register_hook(int item) {
  if(current_menubar && current_menubar != this)
    return 0;
    
  current_item = item;
  
  current_menubar = this;
  return SetWindowsHookEx(
           WH_MSGFILTER,
           &menu_hook_proc,
           NULL,
           GetCurrentThreadId());
}

void Win32Menubar::unregister_hook(HHOOK hook) {
  UnhookWindowsHookEx(hook);
  current_item = 0;
}

int Win32Menubar::find_hilite_menuitem(HMENU *menu) {
  for(int i = 0; i < GetMenuItemCount(*menu); ++i) {
    UINT state = GetMenuState(*menu, i, MF_BYPOSITION);
    
    if(state & MF_POPUP) {
      HMENU old = *menu;
      *menu = GetSubMenu(*menu, i);
      int result = find_hilite_menuitem(menu);
      if(result >= 0)
        return result;
        
      *menu = old;
    }
    
    if(state & MF_HILITE) {
      return i;
    }
  }
  
  return -1;
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
    case WM_NOTIFY: {
        NMHDR *header = (NMHDR*)lParam;
        
        if(header->hwndFrom == _hwnd) {
          switch(header->code) {
            case TBN_DROPDOWN: {
                NMTOOLBARW *tb = (NMTOOLBARW*)lParam;
                
                show_menu(tb->iItem);
                
                *result = TBDDRET_DEFAULT;//TBDDRET_TREATPRESSED;
              } return true;
              
            case TBN_HOTITEMCHANGE: {
                NMTBHOTITEM *hi = (NMTBHOTITEM*)lParam;
                
                if((hi->dwFlags & (HICF_MOUSE | ~HICF_LEAVING))
                    && current_menubar == this
                    && current_item != hi->idNew
                    && hi->idNew) {
                  if(hi->idNew <= separator_index) {
                    next_item = hi->idNew;
                    
                    EndMenu();
                    return true;
                  }
                  else {
                    *result = 0;
                    return true;
                  }
                }
              } break;
              
            case NM_CLICK: {
                POINT pt = ((NMMOUSE*)lParam)->pt;
                
                if(current_popup == 0) {
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
                w::NMKEY *key = (w::NMKEY*)lParam;
                
                switch(key->nVKey) {
                  case VK_MENU:
                  case VK_ESCAPE: {
                      kill_focus();
                      *result = 1;
                    } return true;
                }
              } break;
              
            case NM_CHAR: {
                w::NMCHAR *chr = (w::NMCHAR*)lParam;
                
                if(chr->ch == ' ') {
                  show_sysmenu();
                  *result = TRUE;
                  return true;
                }
              } break;
              
            case NM_CUSTOMDRAW: {
                NMTBCUSTOMDRAW *draw = (NMTBCUSTOMDRAW*)lParam;
                
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
                        
                      cairo_surface_destroy(surface);
                      
                      *result = CDRF_NOTIFYITEMDRAW;//CDRF_DODEFAULT;
                    } return true;
                    
                  case CDDS_ITEMPREPAINT: {
                      draw->nmcd.uItemState = CDIS_DEFAULT;
                      if(Win32Themes::DwmEnableComposition != 0) // >= Vista
                        *result = TBCDRF_USECDCOLORS;
                      else
                        *result = CDRF_DODEFAULT;
                        
                      ControlState state = Normal;
                      if(GetForegroundWindow() == _window->hwnd())
                        draw->clrText = GetSysColor(COLOR_MENUTEXT);
                      else
                        draw->clrText = GetSysColor(COLOR_GRAYTEXT);
                        
                      if((int)draw->nmcd.dwItemSpec == separator_index + 1) {
                        *result = CDRF_SKIPDEFAULT;
                      }
                      else if((int)draw->nmcd.dwItemSpec > separator_index) {
                      
                        TBBUTTONINFOW info;
                        info.cbSize = sizeof(info);
                        info.dwMask = TBIF_BYINDEX | TBIF_STATE;
                        
                        SendMessageW(_hwnd, TB_GETBUTTONINFOW, (int)draw->nmcd.dwItemSpec, (LPARAM)&info);
                        
                        if(info.fsState & TBSTATE_CHECKED) {
                          state = Pressed;
                        }
                        
                        if(draw->nmcd.uItemState & CDIS_CHECKED)
                          state = Pressed;
                        else if(draw->nmcd.uItemState & CDIS_HOT)
                          state = Hovered; // Hot
                      }
                      else {
                        if(current_item == (int)draw->nmcd.dwItemSpec
                            ||    next_item == (int)draw->nmcd.dwItemSpec) {
                          state = Pressed;
                        }
                        else if(current_item == 0 && next_item == 0
                                && draw->nmcd.uItemState & CDIS_HOT)
                          state = Pressed;//Hovered;
                      }
                      
                      if(!Win32ControlPainter::win32_painter.draw_menubar_itembg(
                            draw->nmcd.hdc,
                            &draw->nmcd.rc,
                            state)
                          && state != Normal) {
                        draw->clrText = GetSysColor(COLOR_HIGHLIGHTTEXT);
                      }
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
        
        for(int i = GetMenuItemCount(sub) - 1; i >= 0; --i) {
          UINT flags = MF_BYPOSITION;
          
          int id = GetMenuItemID(sub, i);
          if(Application::is_menucommand_runnable(Win32Menu::command_id_to_string(id)))
            flags |= MF_ENABLED;
          else
            flags |= MF_GRAYED;
            
          EnableMenuItem(sub, i, flags);
        }
      } break;
      
    case WM_SYSKEYDOWN: {
        if(_appearence != MaNeverShow) {
          if(wParam == VK_MENU)
            return true;
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
          if(!lParam) {
            PostMessage(_hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
            kill_focus();
          }
        }
      } break;
      
    case WM_ACTIVATE: {
        if(wParam == WA_INACTIVE && _appearence == MaAutoShow && visible() && !is_pinned()) {
          ShowWindow(_hwnd, SW_HIDE);
          _window->rearrange();
        }
        else {
          InvalidateRect(_hwnd, NULL, FALSE);
        }
      } break;
  }
  return false;
}

LRESULT CALLBACK Win32Menubar::menu_hook_proc(int code, WPARAM h_wParam, LPARAM h_lParam) {
  if(code == MSGF_MENU && current_menubar) {
    MSG *msg = (MSG*)h_lParam;
    
    switch(msg->message) {
      case WM_LBUTTONDOWN:
        {
          static int i = 0;
          i = i + 1;
        }
      case WM_LBUTTONUP:
      case WM_MOUSEMOVE: {
          POINT pt = dword_to_point(msg->lParam);
          
          ScreenToClient(current_menubar->_hwnd, &pt);
          
          return SendMessageW(
                   current_menubar->_hwnd,
                   msg->message,
                   msg->wParam,
                   point_to_dword(pt));
        } break;
        
      case WM_KEYDOWN: {
          switch(msg->wParam) {
            case VK_LEFT: if(current_menubar->_menu.is_valid()) {
                HMENU menu = current_menubar->_menu->hmenu();
                int item = find_hilite_menuitem(&menu);
                
                if(menu == current_menubar->current_popup
                    || (item < 0 && current_menubar->current_item >= 0)) {
                  int count = GetMenuItemCount(current_menubar->_menu->hmenu());
                  
                  if(count > 1) {
                    current_menubar->next_item = current_menubar->current_item - 1;
                    if(current_menubar->next_item <= 0)
                      current_menubar->next_item = count;
                      
                    EndMenu();
                    return 0;
                  }
                }
              } break;
              
            case VK_RIGHT: if(current_menubar->_menu.is_valid()) {
                HMENU menu = current_menubar->_menu->hmenu();
                int item = find_hilite_menuitem(&menu);
                
                if((item < 0 && current_menubar->current_item >= 0)
                    || (GetMenuState(menu, item, MF_BYPOSITION) & MF_POPUP) == 0) {
                  int count = GetMenuItemCount(current_menubar->_menu->hmenu());
                  
                  if(count > 1) {
                    current_menubar->next_item = current_menubar->current_item + 1;
                    if(current_menubar->next_item > count)
                      current_menubar->next_item = 1;
                      
                    EndMenu();
                    return 0;
                  }
                }
              } break;
          }
        } break;
    }
  }
  
  return CallNextHookEx(0, code, h_wParam, h_lParam);
}

//} ... class Win32Menubar
