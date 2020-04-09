#include <gui/win32/win32-menu.h>

#include <eval/binding.h>
#include <eval/application.h>
#include <eval/observable.h>
#include <resources.h>

#include <util/array.h>
#include <util/hashtable.h>


using namespace richmath;

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC  0
#endif

#ifndef MAPVK_VK_TO_CHAR
#define MAPVK_VK_TO_CHAR  2
#endif

/* Windows reports Numpad Enter and Enter as VK_RETURN (0x0D). 
   0x0E is undefined, we use that ... 
 */
#define VK_MY_NUMPAD_ENTER   0x0E


namespace {
  class MenuItemBuilder {
    public:
      static void add_remove_menu(int delta);
      static void add_command(DWORD id, Expr cmd);
      static DWORD get_or_create_command_id(Expr cmd);
      
      static HMENU create_menu(Expr expr, bool is_popup);
      static bool init_info(MENUITEMINFOW *info, Expr item, String *buffer);
    
    private:
      static bool is_radiocheck_command(Expr cmd);
      static bool init_item_info(MENUITEMINFOW *info, Expr item, String *buffer);
      static bool init_delimiter_info(MENUITEMINFOW *info);
      static bool init_submenu_info(MENUITEMINFOW *info, Expr item, String *buffer);
      
    private:
      static DWORD next_id;
      
    public:
      static ObservableValue<DWORD> selected_menu_item_id;
  };
}


static Hashtable<Expr,  DWORD>  cmd_to_id;
static Hashtable<DWORD, Expr>   id_to_cmd;
static Hashtable<DWORD, String> id_to_shortcut_text;


extern pmath_symbol_t richmath_FE_Delimiter;
extern pmath_symbol_t richmath_FE_Menu;
extern pmath_symbol_t richmath_FE_MenuItem;


//{ class Win32Menu ...

SharedPtr<Win32Menu>  Win32Menu::main_menu;
SharedPtr<Win32Menu>  Win32Menu::popup_menu;

Win32Menu::Win32Menu(Expr expr, bool is_popup)
  : Shareable()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  MenuItemBuilder::add_remove_menu(1);
  _hmenu = MenuItemBuilder::create_menu(expr, is_popup);
}

Win32Menu::~Win32Menu() {
  MenuItemBuilder::add_remove_menu(-1);
  DestroyMenu(_hmenu);
}

Expr Win32Menu::id_to_command(DWORD  id) {
  return id_to_cmd[id];
}

DWORD Win32Menu::command_to_id(Expr cmd) {
  return cmd_to_id[cmd];
}

void Win32Menu::init_popupmenu(HMENU sub) {
  int count = GetMenuItemCount(sub);
  for(int i = 0; i < count; ++i) {
    MENUITEMINFOW mii = {0};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_DATA | MIIM_ID | MIIM_SUBMENU;
    if(GetMenuItemInfoW(sub, i, TRUE, &mii)) {
      if(mii.dwItemData) {
        DWORD list_id = mii.dwItemData;
        // dwItemData != 0 means that this item is dynamically generated from the menu item command of that id
        Expr new_items = Application::generate_dynamic_submenu(id_to_command(list_id));
        
        if(new_items.expr_length() == 0 || new_items[0] != PMATH_SYMBOL_LIST) {
          mii.fMask |= MIIM_STRING | MIIM_STATE;
          mii.fState |= MFS_DISABLED;
          mii.dwTypeData = L"(empty)";
          mii.cch = 7;
          
          if(mii.hSubMenu) {
            DestroyMenu(mii.hSubMenu);
            mii.hSubMenu = nullptr;
          }
          
          SetMenuItemInfoW(sub, i, TRUE, &mii);
          continue;
        }
        
        int first_menu_index = i;
        bool insert = false;
        size_t k;
        for(size_t k = 1; k <= new_items.expr_length(); ++k) {
          String buffer;
          mii.fState = 0;
          if(!MenuItemBuilder::init_info(&mii, new_items[k], &buffer))
            continue;
          
          mii.fMask |= MIIM_DATA | MIIM_STATE;
          mii.dwItemData = list_id;
          
          MenuCommandStatus status = Application::test_menucommand_status(id_to_command(mii.wID));
          if(status.enabled)
            mii.fState |= MFS_ENABLED;
          else
            mii.fState |= MFS_GRAYED;
          
          if(status.checked) 
            mii.fState |= MFS_CHECKED;
          else
            mii.fState |= MFS_UNCHECKED;
          
          if(insert) {
            if(InsertMenuItemW(sub, i, TRUE, &mii)) {
              ++i;
              ++count;
            }
            continue;
          }
          
          if(!SetMenuItemInfoW(sub, i, TRUE, &mii)) 
            continue;
          
          ++i;
          if(i < count) {
            mii.fMask = MIIM_DATA | MIIM_ID | MIIM_SUBMENU;
            if(GetMenuItemInfoW(sub, i, TRUE, &mii)) {
              if(mii.dwItemData == list_id) {
                if(mii.hSubMenu) {
                  DestroyMenu(mii.hSubMenu);
                  mii.hSubMenu = nullptr;
                  SetMenuItemInfoW(sub, i, TRUE, &mii);
                }
                continue;
              }
            }
          }
          
          insert = true;
        }
        
        if(i == first_menu_index) {
          mii.fMask = MIIM_DATA | MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_SUBMENU;
          mii.dwItemData = list_id;
          mii.wID = list_id;
          mii.fState |= MFS_DISABLED;
          mii.dwTypeData = L"(empty)";
          mii.cch = 7;
          
          if(mii.hSubMenu) {
            DestroyMenu(mii.hSubMenu);
            mii.hSubMenu = nullptr;
          }
          
          SetMenuItemInfoW(sub, i, TRUE, &mii);
          ++i;
        }
        
        while(i < count) {
          mii.fMask = MIIM_DATA | MIIM_ID;
          if(GetMenuItemInfoW(sub, i, TRUE, &mii)) {
            if(mii.dwItemData != list_id) 
              break;
              
            if(DeleteMenu(sub, i, MF_BYPOSITION))
              --count;
          }
        }
        
        --i;
        continue;
      }
    
      MenuCommandStatus status = Application::test_menucommand_status(id_to_command(mii.wID));
      mii.fMask = MIIM_STATE;
      
      if(status.enabled)
        mii.fState |= MFS_ENABLED;
      else
        mii.fState |= MFS_GRAYED;
      
      if(status.checked) 
        mii.fState |= MFS_CHECKED;
      else
        mii.fState |= MFS_UNCHECKED;
      
      SetMenuItemInfoW(sub, i, TRUE, &mii);
    }
  }
  
//  for(int i = count - 1; i >= 0; --i) {
//    MENUITEMINFOW mii;
//    
//    memset(&mii, 0, sizeof(mii));
//    mii.cbSize = sizeof(mii);
//    mii.fMask = MIIM_STATE;
//    
//    int id = GetMenuItemID(sub, i);
//    MenuCommandStatus status = Application::test_menucommand_status(id_to_command(id));
//    
//    if(status.enabled)
//      mii.fState |= MFS_ENABLED;
//    else
//      mii.fState |= MFS_GRAYED;
//      
//    if(status.checked) 
//      mii.fState |= MFS_CHECKED;
//    else
//      mii.fState |= MFS_UNCHECKED;
//    
//    SetMenuItemInfoW(sub, i, TRUE, &mii);
//  }
}

Expr Win32Menu::selected_item_command() {
  return Win32Menu::id_to_command(MenuItemBuilder::selected_menu_item_id);
}

void Win32Menu::on_menuselect(WPARAM wParam, LPARAM lParam) {
  HMENU menu = (HMENU)lParam;
  UINT item_or_index = LOWORD(wParam);
  UINT flags = HIWORD(wParam);
  
  static HWND previous_aero_peak = nullptr;
  HWND aero_peak = nullptr;
  
  //pmath_debug_print("[WM_MENUSELECT %p %d (%x)]\n", menu, item_or_index, flags);
  
  if(flags == 0xFFFF) {
    MenuItemBuilder::selected_menu_item_id = 0;
  }
  else if(!(flags & MF_POPUP)) {
    MenuItemBuilder::selected_menu_item_id = item_or_index;
    
//    Expr cmd = Win32Menu::id_to_command(item_or_index);
//    pmath_debug_print_object("[WM_MENUSELECT ", cmd.get(), "]\n");
//    
//    if(cmd[0] == richmath_FrontEnd_SetSelectedDocument) {
//      auto id = FrontEndReference::from_pmath(cmd[1]);
//      auto doc = FrontEndObject::find_cast<Document>(id);
//      if(doc) {
//        Win32Widget *wid = dynamic_cast<Win32Widget*>(doc->native());
//        if(wid) {
//          aero_peak = wid->hwnd();
//          while(auto parent = GetParent(aero_peak))
//            aero_peak = parent;
//        }
//      }
//    }
  }
  else {
    // popup item selected
    MenuItemBuilder::selected_menu_item_id = 0;
  }
  
  if(flags & MF_MOUSESELECT) {
    Expr cmd = Win32Menu::id_to_command(MenuItemBuilder::selected_menu_item_id);
    Expr subitems_cmd;
    
    MENUITEMINFOW info = { sizeof(info) };
    info.fMask = MIIM_DATA | MIIM_ID;
    if(GetMenuItemInfoW(menu, item_or_index, 0 != (flags & MF_POPUP), &info)) {
      subitems_cmd = Win32Menu::id_to_command((DWORD)info.dwItemData);      
//      pmath_debug_print_object("[on_menuselect ", subitems_cmd.get(), ", ");
//      pmath_debug_print_object("", cmd.get(), "]\n");
    }
    
    if(subitems_cmd.is_null()) {
      SetCursor(LoadCursor(0, IDC_ARROW));
    }
    else {
      SetCursor(LoadCursor(0, IDC_HELP)); // TODO: reset when mouse leaves menu
    }
  }
//  if(Win32Themes::has_areo_peak()) {
//    if(aero_peak) {
//      struct callback_data {
//        HWND owner_window;
//        DWORD process_id;
//        DWORD thread_id;
//      } data;
//      data.owner_window = _window->hwnd();
//      data.process_id = GetCurrentProcessId();
//      data.thread_id = GetCurrentThreadId();
//      
//      EnumWindows(
//        [](HWND wnd, LPARAM _data) {
//          auto data = (callback_data*)_data;
//          
//          DWORD pid;
//          DWORD tid = GetWindowThreadProcessId(wnd, &pid);
//          
//          if(pid != data->process_id || tid != data->thread_id)
//            return TRUE;
//          
//          char class_name[20];
//          const char MenuWindowClass[] = "#32768";
//          if(GetClassNameA(wnd, class_name, sizeof(class_name)) > 0 && strcmp(MenuWindowClass, class_name) == 0) {
//            BOOL disallow_peak = FALSE;
//            BOOL excluded_from_peak = FALSE;
//            Win32Themes::DwmGetWindowAttribute(wnd, Win32Themes::DWMWA_DISALLOW_PEEK,      &disallow_peak,      sizeof(disallow_peak));
//            Win32Themes::DwmGetWindowAttribute(wnd, Win32Themes::DWMWA_EXCLUDED_FROM_PEEK, &excluded_from_peak, sizeof(excluded_from_peak));
//            
//            pmath_debug_print("[menu window %p %s %s]", wnd, disallow_peak ? "disallow peak" : "", excluded_from_peak ? "exclude from peak" : "");
//            
//            excluded_from_peak = TRUE;
//            Win32Themes::DwmSetWindowAttribute(wnd, Win32Themes::DWMWA_EXCLUDED_FROM_PEEK, &excluded_from_peak, sizeof(excluded_from_peak));
//          };
//          return TRUE;
//        },
//        (LPARAM)&data
//      );
//      
//      BOOL excluded_from_peak = TRUE;
//      Win32Themes::DwmSetWindowAttribute(data.owner_window, Win32Themes::DWMWA_EXCLUDED_FROM_PEEK, &excluded_from_peak, sizeof(excluded_from_peak));
//      
//      Win32Themes::activate_aero_peak(true, aero_peak, data.owner_window, LivePreviewTrigger::TaskbarThumbnail);
//      previous_aero_peak = aero_peak;
//    }
//    else if(previous_aero_peak /*&& (flags != 0xFFFF || menu != nullptr)*/) {
//      previous_aero_peak = nullptr;
//      Win32Themes::activate_aero_peak(false, nullptr, nullptr, LivePreviewTrigger::TaskbarThumbnail);
//    
//      BOOL excluded_from_peak = FALSE;
//      Win32Themes::DwmSetWindowAttribute(_window->hwnd(), Win32Themes::DWMWA_EXCLUDED_FROM_PEEK, &excluded_from_peak, sizeof(excluded_from_peak));
//    }
//  }
}

//} ... class Win32Menu

//{ class MenuItemBuilder ...

DWORD                  MenuItemBuilder::next_id = 1000;
ObservableValue<DWORD> MenuItemBuilder::selected_menu_item_id { 0 };

void MenuItemBuilder::add_remove_menu(int delta) {
  static int num_menus = 0;
  
  if(num_menus == 0) {
    assert(delta == 1);
    
    cmd_to_id.default_value = 0;
    
    add_command(SC_CLOSE, String("Close"));
  }
  
  num_menus += delta;
  if(num_menus == 0) {
    cmd_to_id.clear();
    id_to_cmd.clear();
    id_to_shortcut_text.clear();
  }
}

void MenuItemBuilder::add_command(DWORD id, Expr cmd) {
  cmd_to_id.set(cmd, id);
  id_to_cmd.set(id,  cmd);
}

DWORD MenuItemBuilder::get_or_create_command_id(Expr cmd) {
  DWORD id = cmd_to_id[cmd];
  if(!id) {
    id = next_id++;
    add_command(id, cmd);
  }
  return id;
}

HMENU MenuItemBuilder::create_menu(Expr expr, bool is_popup) {
  if(expr[0] != richmath_FE_Menu || expr.expr_length() != 2)
    return nullptr;
    
  String name(expr[1]);
  if(name.is_null())
    return nullptr;
    
  expr = expr[2];
  if(expr[0] != PMATH_SYMBOL_LIST)
    return nullptr;
  
  HMENU menu = is_popup ? CreatePopupMenu() : CreateMenu();
  if(menu) {
    for(size_t i = 1; i <= expr.expr_length(); ++i) {
      Expr item = expr[i];
      
      MENUITEMINFOW item_info = {0};
      item_info.cbSize = sizeof(item_info);
      String buffer;
      if(init_info(&item_info, expr[i], &buffer)) {
        InsertMenuItemW(menu, GetMenuItemCount(menu), TRUE, &item_info);
      }
    }
  }
  
  return menu;
}

bool MenuItemBuilder::init_info(MENUITEMINFOW *info, Expr item, String *buffer) {
  assert(info);
  assert(info->cbSize >= sizeof(MENUITEMINFOW));
  assert(buffer);
  
  if(item == richmath_FE_Delimiter)
    return init_delimiter_info(info);
  
  if(item[0] == richmath_FE_MenuItem && item.expr_length() == 2)
    return init_item_info(info, std::move(item), buffer);
  
  if(item[0] == richmath_FE_Menu && item.expr_length() == 2)
    return init_submenu_info(info, std::move(item), buffer);
  
  return false;
}

bool MenuItemBuilder::is_radiocheck_command(Expr cmd) {
  if(cmd[0] == richmath_FE_ScopedCommand)
    cmd = cmd[1];
  
  // style->value  is a simple setter (does not toggle)
  return cmd.is_rule();
}

bool MenuItemBuilder::init_item_info(MENUITEMINFOW *info, Expr item, String *buffer) {
  *buffer = String(item[1]);
  Expr cmd = item[2];
  
  info->fMask |= MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING;
  info->wID = get_or_create_command_id(cmd);
  info->fType = MFT_STRING;
  info->fState = MFS_ENABLED;
  
  if(is_radiocheck_command(cmd))
    info->fType |= MFT_RADIOCHECK;
  
  String shortcut = id_to_shortcut_text[info->wID];
  if(shortcut.length() > 0)
    *buffer += String::FromChar('\t') + shortcut;
    
  *buffer += String::FromChar(0);
  info->dwTypeData = (wchar_t *)buffer->buffer();
  info->cch = buffer->length() - 1;
  return buffer->length() >= 0;
}

bool MenuItemBuilder::init_delimiter_info(MENUITEMINFOW *info) {
  info->fMask = MIIM_FTYPE;
  info->fType = MFT_SEPARATOR;
  return true;
}

bool MenuItemBuilder::init_submenu_info(MENUITEMINFOW *info, Expr item, String *buffer) {
  *buffer = String(item[1]);
  *buffer += String::FromChar(0);
  if(buffer->length() == 0)
    return false;
  
  info->fMask |= MIIM_FTYPE | MIIM_STATE | MIIM_STRING;
  
  info->dwTypeData = (wchar_t *)buffer->buffer();
  info->cch = buffer->length() - 1;
  
  info->fType = MFT_STRING;
  info->fState = MFS_ENABLED;
  
  Expr sub_items = item[2];
  if(sub_items.is_string()) {
    info->fMask |= MIIM_DATA | MIIM_ID;
    info->wID = get_or_create_command_id(sub_items);
    info->dwItemData = info->wID; // dwItemData != 0 means that this item is dynamically generated from the menu item command of that id
    info->fState |= MFS_DISABLED;
    info->dwTypeData = L"(empty)";
    info->cch = 7;
  }
  else if(sub_items[0] == PMATH_SYMBOL_LIST) {
    info->fMask |= MIIM_SUBMENU;
    info->hSubMenu = create_menu(std::move(item), true);
  }
  
  return true;
}

//} ... class MenuItemBuilder

//{ class Win32AcceleratorTable ...

static bool set_accel_key(Expr expr, ACCEL *accel) {
  if(expr[0] != richmath_FE_KeyEvent || expr.expr_length() != 2)
    return false;
    
  Expr modifiers = expr[2];
  if(modifiers[0] != PMATH_SYMBOL_LIST)
    return false;
    
  accel->fVirt = 0;
  for(size_t i = modifiers.expr_length(); i > 0; --i) {
    Expr item = modifiers[i];
    
    if(item == richmath_FE_KeyAlt)
      accel->fVirt |= FALT;
    else if(item == richmath_FE_KeyControl)
      accel->fVirt |= FCONTROL;
    else if(item == richmath_FE_KeyShift)
      accel->fVirt |= FSHIFT;
    else
      return false;
  }
  
  String key(expr[1]);
  if(key.length() == 0)
    return false;
    
  if(key.length() == 1) {
    uint16_t ch = key.buffer()[0];
    
    if(ch >= 'A' && ch <= 'Z')
      ch -= 'A' - 'a';
      
    accel->fVirt |= FVIRTKEY;
    unsigned short vk = VkKeyScanW(ch);
    if(vk == 0xFFFF)
      return false;
      
    accel->key = vk & 0xFF;
    if(vk & 0x100)
      accel->fVirt |= FSHIFT;
    if(vk & 0x200)
      accel->fVirt |= FCONTROL;
    if(vk & 0x400)
      accel->fVirt |= FALT;
      
    return true;
  }
  
  accel->fVirt |= FVIRTKEY;
  
  if(     key.equals("F1"))                 accel->key = VK_F1;
  else if(key.equals("F2"))                 accel->key = VK_F2;
  else if(key.equals("F3"))                 accel->key = VK_F3;
  else if(key.equals("F4"))                 accel->key = VK_F4;
  else if(key.equals("F5"))                 accel->key = VK_F5;
  else if(key.equals("F6"))                 accel->key = VK_F6;
  else if(key.equals("F7"))                 accel->key = VK_F7;
  else if(key.equals("F8"))                 accel->key = VK_F8;
  else if(key.equals("F9"))                 accel->key = VK_F9;
  else if(key.equals("F10"))                accel->key = VK_F10;
  else if(key.equals("F11"))                accel->key = VK_F11;
  else if(key.equals("F12"))                accel->key = VK_F12;
  else if(key.equals("F13"))                accel->key = VK_F13;
  else if(key.equals("F14"))                accel->key = VK_F14;
  else if(key.equals("F15"))                accel->key = VK_F15;
  else if(key.equals("F16"))                accel->key = VK_F16;
  else if(key.equals("F17"))                accel->key = VK_F17;
  else if(key.equals("F18"))                accel->key = VK_F18;
  else if(key.equals("F19"))                accel->key = VK_F19;
  else if(key.equals("F20"))                accel->key = VK_F20;
  else if(key.equals("F21"))                accel->key = VK_F21;
  else if(key.equals("F22"))                accel->key = VK_F22;
  else if(key.equals("F23"))                accel->key = VK_F23;
  else if(key.equals("F24"))                accel->key = VK_F24;
  else if(key.equals("Enter"))              accel->key = VK_RETURN;
  else if(key.equals("Tab"))                accel->key = VK_TAB;
  else if(key.equals("Esc"))                accel->key = VK_ESCAPE;
  else if(key.equals("PageUp"))             accel->key = VK_PRIOR;
  else if(key.equals("PageDown"))           accel->key = VK_NEXT;
  else if(key.equals("End"))                accel->key = VK_END;
  else if(key.equals("Home"))               accel->key = VK_HOME;
  else if(key.equals("Left"))               accel->key = VK_LEFT;
  else if(key.equals("Up"))                 accel->key = VK_UP;
  else if(key.equals("Right"))              accel->key = VK_RIGHT;
  else if(key.equals("Down"))               accel->key = VK_DOWN;
  else if(key.equals("Insert"))             accel->key = VK_INSERT;
  else if(key.equals("Delete"))             accel->key = VK_DELETE;
  else if(key.equals("Numpad0"))            accel->key = VK_NUMPAD0;
  else if(key.equals("Numpad1"))            accel->key = VK_NUMPAD1;
  else if(key.equals("Numpad2"))            accel->key = VK_NUMPAD2;
  else if(key.equals("Numpad3"))            accel->key = VK_NUMPAD3;
  else if(key.equals("Numpad4"))            accel->key = VK_NUMPAD4;
  else if(key.equals("Numpad5"))            accel->key = VK_NUMPAD5;
  else if(key.equals("Numpad6"))            accel->key = VK_NUMPAD6;
  else if(key.equals("Numpad7"))            accel->key = VK_NUMPAD7;
  else if(key.equals("Numpad8"))            accel->key = VK_NUMPAD8;
  else if(key.equals("Numpad9"))            accel->key = VK_NUMPAD9;
  else if(key.equals("Numpad+"))            accel->key = VK_ADD;
  else if(key.equals("Numpad-"))            accel->key = VK_SUBTRACT;
  else if(key.equals("Numpad*"))            accel->key = VK_MULTIPLY;
  else if(key.equals("Numpad/"))            accel->key = VK_DIVIDE;
  else if(key.equals("NumpadDecimal"))      accel->key = VK_DECIMAL;
  else if(key.equals("NumpadEnter"))        accel->key = VK_MY_NUMPAD_ENTER;
  else if(key.equals("Play"))               accel->key = VK_PLAY;
  else if(key.equals("Zoom"))               accel->key = VK_ZOOM;
  else                                      return false;
  
  return true;
}

static String vk_name(UINT vk) {
  /*switch(vk){
    case VK_CONTROL: return "Ctrl";
    case VK_MENU:    return "Alt";
    case VK_SHIFT:   return "Shift";
  
    case VK_F1:      return "F1";
    case VK_F2:      return "F2";
    case VK_F3:      return "F3";
    case VK_F4:      return "F4";
    case VK_F5:      return "F5";
    case VK_F6:      return "F6";
    case VK_F7:      return "F7";
    case VK_F8:      return "F8";
    case VK_F9:      return "F9";
    case VK_F10:     return "F10";
    case VK_F11:     return "F11";
    case VK_F12:     return "F12";
    case VK_F13:     return "F13";
    case VK_F14:     return "F14";
    case VK_F15:     return "F15";
    case VK_F16:     return "F16";
    case VK_F17:     return "F17";
    case VK_F18:     return "F18";
    case VK_F19:     return "F19";
    case VK_F20:     return "F20";
    case VK_F21:     return "F21";
    case VK_F22:     return "F22";
    case VK_F23:     return "F23";
    case VK_F24:     return "F24";
  
    case VK_RETURN:  return "Enter";
    case VK_TAB:     return "Tab";
    case VK_ESCAPE:  return "Esc";
    case VK_PRIOR:   return "PageUp";
    case VK_NEXT:    return "PageDown";
    case VK_END:     return "End";
    case VK_HOME:    return "Home";
    case VK_LEFT:    return "Left";
    case VK_UP:      return "Up";
    case VK_RIGHT:   return "Right";
    case VK_DOWN:    return "Down";
    case VK_INSERT:  return "Insert";
    case VK_DELETE:  return "Delete";
  
    case VK_NUMPAD0: return "0";
    case VK_NUMPAD1: return "1";
    case VK_NUMPAD2: return "2";
    case VK_NUMPAD3: return "3";
    case VK_NUMPAD4: return "4";
    case VK_NUMPAD5: return "5";
    case VK_NUMPAD6: return "6";
    case VK_NUMPAD7: return "7";
    case VK_NUMPAD8: return "8";
    case VK_NUMPAD9: return "9";
  
    case VK_PLAY:    return "Play";
    case VK_ZOOM:    return "Zoom";
  }
  */
  
  UINT ch = MapVirtualKeyW(vk, MAPVK_VK_TO_CHAR) & 0x7FFFFFFF;
  if(ch > ' ')
    return String::FromChar(ch);
    
  // old code....
  wchar_t buf[100];
  
  UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
  
  switch(vk) {
    case VK_MY_NUMPAD_ENTER:
      sc = 0x100 | MapVirtualKeyW(VK_RETURN, MAPVK_VK_TO_VSC);
      break;
    
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_NEXT:
    case VK_PRIOR:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
      sc |= 0x100; // Add extended bit
      break;
  }
  
  int len = GetKeyNameTextW(sc << 16, buf, 100);
  
  for(int i = 1; i < len; ++i) {
    if(buf[i] >= 'A' && buf[i] <= 'Z')
      buf[i] += 'a' - 'A';
  }
  
  return String::FromUcs2((const uint16_t *)buf, len);
}

static String accel_text(const ACCEL &accel) {
  return Win32AcceleratorTable::accel_text(accel.fVirt, accel.key);
}

static HACCEL create_accel(Expr expr) {
  if(expr[0] != PMATH_SYMBOL_LIST)
    return nullptr;
    
  Array<ACCEL> accel(expr.expr_length());
  int j = 0;
  
  for(size_t i = 1; i <= expr.expr_length(); ++i) {
    Expr item(expr[i]);
    Expr cmd( item[2]);
    
    if( item[0] == richmath_FE_MenuItem &&
        item.expr_length() == 2          &&
        set_accel_key(item[1], &accel[j]))
    {
      DWORD id = MenuItemBuilder::get_or_create_command_id(cmd);
      if(!id_to_shortcut_text.search(id))
        id_to_shortcut_text.set(id, accel_text(accel[j]));
      accel[j].cmd = id;
      ++j;
    }
    else
      pmath_debug_print_object("Cannot add shortcut ", item.get(), ".\n");
  }
  
  HACCEL haccel = CreateAcceleratorTableW(accel.items(), j);
  return haccel;
}


SharedPtr<Win32AcceleratorTable>  Win32AcceleratorTable::main_table;

Win32AcceleratorTable::Win32AcceleratorTable(Expr expr)
  : Shareable()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  MenuItemBuilder::add_remove_menu(+1);
  _haccel = create_accel(expr);
}

Win32AcceleratorTable::~Win32AcceleratorTable() {
  DestroyAcceleratorTable(_haccel);
  MenuItemBuilder::add_remove_menu(-1);
}

String Win32AcceleratorTable::accel_text(BYTE fVirt, WORD key) {
  String s("");
  
  if(fVirt & FCONTROL)
    s += vk_name(VK_CONTROL) + "+";
    
  if(fVirt & FALT)
    s += vk_name(VK_MENU) + "+";
    
  if(fVirt & FSHIFT)
    s += vk_name(VK_SHIFT) + "+";
    
  if(fVirt & FVIRTKEY)
    s += vk_name(key);
  else // ASCII key code
    s += (char)key;
    
  return s;
}

bool Win32AcceleratorTable::translate_accelerator(HWND hwnd, MSG *msg) {
  if(!msg)
    return false;
  
  if(msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) {
    if(msg->wParam == VK_RETURN && (msg->lParam & (1 << 24))) {
      msg->wParam = VK_MY_NUMPAD_ENTER;
      bool result = !!TranslateAcceleratorW(hwnd, _haccel, msg);
      if(!result) {
        msg->wParam = VK_RETURN;
        result = !!TranslateAcceleratorW(hwnd, _haccel, msg);
      }
      
      return result;
    }
  }
  
  return !!TranslateAcceleratorW(hwnd, _haccel, msg);
}

//} ... class Win32AcceleratorTable
