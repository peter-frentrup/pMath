#define WINVER 0x501
#define _WIN32_IE 0x600

#include <gui/win32/win32-menubar.h>

#include <cstdio>
#include <cctype>

#include <commctrl.h>

#include <util/array.h>
#include <eval/client.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/win32-document-window.h>
#include <gui/win32/win32-themes.h>
#include <resources.h>

using namespace richmath;

static Win32Menubar *current_menubar = 0;

#define WM_MY_SHOWMENUITEM   (WM_USER + 1)

static POINT dword_to_point(DWORD dw){
  POINT pt;
  pt.x = (short)LOWORD(dw);
  pt.y = (short)HIWORD(dw);
  return pt;
}

static DWORD point_to_dword(const POINT &pt){
  return ((short)pt.x) | (((short)pt.y) << 16);
}

namespace w{ // win32 typedefs that mingw does not provide
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

Win32Menubar::Win32Menubar(Win32DocumentWindow *window, HWND parent, HMENU menu)
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
  
//  if(Win32Themes::DwmSetWindowAttribute){
//    BOOL value = TRUE;
//    
//    Win32Themes::DwmSetWindowAttribute(
//      _hwnd, 
//      Win32Themes::DWMWA_TRANSITIONS_FORCEDISABLED,
//      &value,
//      sizeof(value));
//  }
  
  SendMessageW(_hwnd, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
  
  Array<TBBUTTON>  buttons(GetMenuItemCount(menu));
  Array<wchar_t[100]> texts(buttons.length());
  
  for(int i = 0;i < buttons.length();++i){
    MENUITEMINFOW info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_SUBMENU;
    info.dwTypeData = texts[i] + 1;                      // prepend ' '
    info.cch = sizeof(texts[i])/sizeof(texts[i][0]) - 3; // append ' ' + '\0'
    
    GetMenuItemInfoW(_menu, i, TRUE, &info);
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
  
  SendMessageW(_hwnd, TB_ADDBUTTONSW, 
    (WPARAM)buttons.length(), 
    (LPARAM)buttons.items()); 
  
  SendMessageW(_hwnd, TB_AUTOSIZE, 0, 0); 
  SendMessageW(_hwnd, TB_SETPADDING, 0, (2 << 16) | 0);
  SendMessageW(_hwnd, TB_SETBUTTONSIZE, 0, 0x010001);
}

Win32Menubar::~Win32Menubar(){
  if(current_menubar == this)
    current_menubar = 0;
    
  DestroyWindow(_hwnd);
  DestroyMenu(_menu);
}

bool Win32Menubar::visible(){
  return (GetWindowLong(_hwnd, GWL_STYLE) & WS_VISIBLE) != 0;
}

int Win32Menubar::height(){
  if(visible()){
//    RECT rect;
//    GetClientRect(_hwnd, &rect);
//    return rect.bottom - rect.top;
    return GetSystemMetrics(SM_CYMENU);
  }
  return 0;
}

void Win32Menubar::appearence(MenuAppearence value){
  _appearence = value;
  
  switch(_appearence){
    case MaAllwaysShow:
      if(!visible()){
        ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
        _window->rearrange();
      }
      break;
      
    case MaNeverShow:
      if(visible()){
        ShowWindow(_hwnd, SW_HIDE);
        _window->rearrange();
      }
      break;
    
    case MaAutoShow:
      break;
  }
  
//  if(_autohide){
//    if(visible())
//      kill_focus();
//  }
//  else if(!visible())
//    set_focus(0);
}

void Win32Menubar::show_menu(int item){
  if(item <= 0)
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
  tpm.rcExclude.bottom+= pt.y;
  
  SetFocus(_hwnd);
  
  HHOOK hook = register_hook(item);
  if(hook){
    current_popup = GetSubMenu(_menu, item - 1);
    
    pt.y = tpm.rcExclude.bottom;
    UINT align;
    if(GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0){
      align = TPM_LEFTALIGN;
      pt.x = tpm.rcExclude.left;
    }
    else{
      align = TPM_RIGHTALIGN;
      pt.x = tpm.rcExclude.right;
    }
    
    int cmd = TrackPopupMenuEx(
      current_popup,
      GetSystemMetrics(SM_MENUDROPALIGNMENT) | TPM_RETURNCMD,
      pt.x,
      pt.y,
      parent,
      &tpm);
      
    current_popup = 0;
    
    unregister_hook(hook);
    
    if(cmd){
      SendMessageW(parent, WM_COMMAND, cmd, 0);
      kill_focus();
    }
    else if(/*_autohide && */visible())
      SetCapture(_hwnd);
  }
  
  if(next_item > 0){
    PostMessage(
      (HWND)GetWindowLongPtr(_hwnd, GWLP_HWNDPARENT),
      WM_MY_SHOWMENUITEM,
      (WPARAM)next_item,
      0);
  }
}

void Win32Menubar::show_sysmenu(){
  HWND parent = (HWND)GetWindowLongPtr(_hwnd, GWLP_HWNDPARENT);
  
  TPMPARAMS tpm;
  memset(&tpm, 0, sizeof(tpm));
  tpm.cbSize = sizeof(tpm);
  GetWindowRect(parent, &tpm.rcExclude);
  
  tpm.rcExclude.left+= GetSystemMetrics(SM_CXSIZEFRAME);
  tpm.rcExclude.top+=  GetSystemMetrics(SM_CYSIZEFRAME);
  tpm.rcExclude.right  = tpm.rcExclude.left + GetSystemMetrics(SM_CXICON);
  tpm.rcExclude.bottom = tpm.rcExclude.top  + GetSystemMetrics(SM_CYCAPTION);
  
  HHOOK hook = register_hook(-1);
  if(hook){
    current_popup = GetSystemMenu(parent, FALSE);
    
    int x;
    UINT align;
    if(GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0){
      align = TPM_LEFTALIGN;
      x = tpm.rcExclude.left;
    }
    else{
      align = TPM_RIGHTALIGN;
      x = tpm.rcExclude.right;
    }
    
    int cmd = TrackPopupMenuEx(
      current_popup,
      GetSystemMetrics(SM_MENUDROPALIGNMENT) | TPM_RETURNCMD,
      x,
      tpm.rcExclude.bottom,
      parent,
      &tpm);
    
    current_popup = 0;
    
    unregister_hook(hook);
    
    if(cmd){
      SendMessageW(parent, WM_COMMAND, cmd, 0);
      kill_focus();
    }
  }
}

void Win32Menubar::set_focus(int item){
  if(_appearence == MaNeverShow)
    return;
    
  if(!visible()){
    ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
    _window->rearrange();
  }
  
  focused = true;
  SetFocus(_hwnd);
  SendMessageW(_hwnd, TB_SETHOTITEM, item, 0);
  
//  if(_autohide){
    SetCursor(LoadCursor(0, IDC_ARROW));
    SetCapture(_hwnd);
//  }
}

void Win32Menubar::kill_focus(){
  //EndMenu();
  focused = false;
  SendMessageW(_hwnd, TB_SETHOTITEM, -1, 0);
  SetFocus((HWND)GetWindowLongPtr(_hwnd, GWLP_HWNDPARENT));
  
  if(current_menubar == this)
    current_menubar = 0;
  
  if(_appearence == MaAutoShow){
    ShowWindow(_hwnd, SW_HIDE);
    _window->rearrange();
  }
  
  if(GetCapture() == _hwnd){
    ReleaseCapture();
  }
}

HHOOK Win32Menubar::register_hook(int item){
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

void Win32Menubar::unregister_hook(HHOOK hook){
  UnhookWindowsHookEx(hook);
  current_item = 0;
}

int Win32Menubar::find_hilite_menuitem(HMENU *menu){
  for(int i = 0;i < GetMenuItemCount(*menu);++i){
    UINT state = GetMenuState(*menu, i, MF_BYPOSITION);
    
    if(state & MF_POPUP){
      HMENU old = *menu;
      *menu = GetSubMenu(*menu, i);
      int result = find_hilite_menuitem(menu);
      if(result >= 0)
        return result;
      
      *menu = old;
    }
    
    if(state & MF_HILITE){
      return i;
    }
  }
  
  return -1;
}

bool Win32Menubar::callback(LRESULT *result, UINT message, WPARAM wParam, LPARAM lParam){
  switch(message){
    case WM_NOTIFY: {
      NMHDR *header = (NMHDR*)lParam;
      
      if(header->hwndFrom == _hwnd){
        switch(header->code){
          case TBN_DROPDOWN: {
            NMTOOLBAR *tb = (NMTOOLBAR*)lParam;
            
            show_menu(tb->iItem);
            
            *result = TBDDRET_DEFAULT;//TBDDRET_TREATPRESSED;
          } return true;
          
          case TBN_HOTITEMCHANGE: {
            NMTBHOTITEM *hi = (NMTBHOTITEM*)lParam;
            
            if((hi->dwFlags & HICF_MOUSE)
            && current_menubar == this 
            && current_item != hi->idNew
            && hi->idNew){
              next_item = hi->idNew;
              
              EndMenu();
              return true;
            }
          } break;
          
          case NM_CLICK: {
            POINT pt = ((NMMOUSE*)lParam)->pt;
            
            if(current_popup == 0
            && SendMessageW(_hwnd, TB_HITTEST, 0, (LPARAM)&pt) < 0){
              kill_focus();
            }
          } break;
          
          case NM_KEYDOWN: {
            w::NMKEY *key = (w::NMKEY*)lParam;
            
            switch(key->nVKey){
              case VK_MENU: 
              case VK_ESCAPE: {
                kill_focus();
                *result = 1;
              } return true;
            }
          } break;
          
          case NM_CHAR: {
            w::NMCHAR *chr = (w::NMCHAR*)lParam;
            
            if(chr->ch == ' '){
              show_sysmenu();
              *result = TRUE;
              return true;
            }
          } break;
          
          case NM_CUSTOMDRAW: {
            NMTBCUSTOMDRAW *draw = (NMTBCUSTOMDRAW*)lParam;
            
            switch(draw->nmcd.dwDrawStage){
              case CDDS_PREPAINT: {
                RECT rect;
                GetClientRect(_hwnd, &rect);
                Win32ControlPainter::win32_painter.draw_menubar(
                  draw->nmcd.hdc, 
                  &rect/*&draw->nmcd.rc*/);
                *result = CDRF_NOTIFYITEMDRAW;//CDRF_DODEFAULT;
              } return true;
              
              case CDDS_ITEMPREPAINT: {
                ControlState state = Normal;
                if(current_item == (int)draw->nmcd.dwItemSpec
                ||    next_item == (int)draw->nmcd.dwItemSpec){
                  state = Pressed;
                }
                else if(current_item == 0 && next_item == 0
                && draw->nmcd.uItemState & CDIS_HOT)
                  state = Pressed;//Hot;
                
                if(!Win32ControlPainter::win32_painter.draw_menubar_itembg(
                    draw->nmcd.hdc, 
                    &draw->nmcd.rc,
                    state)
                && state != Normal){
                  draw->clrText = GetSysColor(COLOR_HIGHLIGHTTEXT);
                }
                
                draw->nmcd.uItemState = CDIS_DEFAULT;
                  
                *result = CDRF_DODEFAULT;
              } return true;
            }
          } break;
        }
      }
    } break;
    
    case WM_MY_SHOWMENUITEM: {
      RECT rect;
      SendMessageW(_hwnd, TB_GETRECT, (int)wParam, (LPARAM)&rect);
      
      POINT pt = {(rect.left + rect.right)/2, (rect.bottom + rect.top)/2};
      
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
    } break;
    
    case WM_INITMENUPOPUP: {
      HMENU sub = (HMENU)wParam;
      
      for(int i = GetMenuItemCount(sub)-1;i >= 0;--i){
        UINT flags = MF_BYPOSITION;
        
        int id = GetMenuItemID(sub, i);
        if(Client::is_menucommand_runnable(win32_command_id_to_command_string(id)))
          flags |= MF_ENABLED;
        else
          flags |= MF_GRAYED;
        
        EnableMenuItem(sub, i, flags);
      }
    } break;
    
    case WM_NEXTMENU: {
      printf("[next]");
    } break;
    
    case WM_SYSKEYDOWN: {
      if(_appearence != MaNeverShow){
        if(wParam == VK_MENU)
          return true;
      }
    } break;
    
    case WM_SYSKEYUP: {
      if(_appearence != MaNeverShow){
        switch(wParam){
          case VK_MENU: 
            set_focus(0);
            return true;
          case VK_F10: 
            if(!(GetKeyState(VK_SHIFT) & ~1)){
              set_focus(0);
              return true;
            }
            break;
        }
      }
    } break;
    
    case WM_SYSCOMMAND: {
      if((wParam & 0xFFF0) == SC_KEYMENU){
        if(!lParam){
          PostMessage(_hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
          kill_focus();
        }
      }
    } break;
    
    case WM_ACTIVATE: {
      if(wParam == WA_INACTIVE && _appearence == MaAutoShow && visible()){
        ShowWindow(_hwnd, SW_HIDE);
        _window->rearrange();
      }
    } break;
  }
  return false;
}

LRESULT CALLBACK Win32Menubar::menu_hook_proc(int code, WPARAM h_wParam, LPARAM h_lParam){
  if(code == MSGF_MENU && current_menubar){
    MSG *msg = (MSG*)h_lParam;
    
    switch(msg->message){
      case WM_LBUTTONDOWN: 
      {
        static int i = 0;
        i = i+1;
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
        switch(msg->wParam){
          case VK_LEFT: {
            HMENU menu = current_menubar->_menu;
            int item = find_hilite_menuitem(&menu);
            
            if(menu == current_menubar->current_popup
            || (item < 0 && current_menubar->current_item >= 0)){
              int count = GetMenuItemCount(current_menubar->_menu);
              
              if(count > 1){
                current_menubar->next_item = current_menubar->current_item - 1;
                if(current_menubar->next_item <= 0)
                  current_menubar->next_item = count;
                
                EndMenu();
                return 0;
              }
            }
          } break;
          
          case VK_RIGHT: {
            HMENU menu = current_menubar->_menu;
            int item = find_hilite_menuitem(&menu);
            
            if((item < 0 && current_menubar->current_item >= 0)
            || (GetMenuState(menu, item, MF_BYPOSITION) & MF_POPUP) == 0){
              int count = GetMenuItemCount(current_menubar->_menu);
              
              if(count > 1){
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
