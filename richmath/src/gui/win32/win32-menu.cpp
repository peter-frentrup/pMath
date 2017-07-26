#include <gui/win32/win32-menu.h>

#include <eval/binding.h>
#include <eval/application.h>
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


static DWORD next_id = 1000;
static Hashtable<Expr,  DWORD>             cmd_to_id;
static Hashtable<DWORD, Expr,   cast_hash> id_to_cmd;
static Hashtable<DWORD, String, cast_hash> id_to_shortcut_text;

static void add_command(DWORD id, Expr cmd) {
  cmd_to_id.set(cmd, id);
  id_to_cmd.set(id,  cmd);
}

static void add_remove_menu(int delta) {
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


//{ class Win32Menu ...

static HMENU create_menu(Expr expr, bool is_popup) {
  if(expr[0] != GetSymbol(FESymbolIndex::Menu) || expr.expr_length() != 2)
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
      
      if( item[0] == GetSymbol(FESymbolIndex::Item) &&
          item.expr_length() == 2)
      {
        String name(item[1]);
        Expr   cmd( item[2]);
        
        DWORD id = cmd_to_id[cmd];
        
        if(!id) {
          id = next_id++;
          add_command(id, cmd);
        }
        
        String shortcut = id_to_shortcut_text[id];
        if(shortcut.length() > 0)
          name += String::FromChar('\t') + shortcut;
          
        name += String::FromChar(0);
        AppendMenuW(
          menu,
          MF_STRING | MF_ENABLED,
          id,
          (const wchar_t *)name.buffer());
          
        continue;
      }
      
      if(item == GetSymbol(FESymbolIndex::Delimiter)) {
        AppendMenuW(
          menu,
          MF_SEPARATOR,
          0,
          L"");
        continue;
      }
      
      if( item[0] == GetSymbol(FESymbolIndex::Menu) &&
          item.expr_length() == 2)
      {
        String name(item[1]);
        
        if(name.length() > 0) {
          if(HMENU submenu = create_menu(item, true)) {
            name += String::FromChar(0);
            AppendMenuW(
              menu,
              MF_STRING | MF_ENABLED | MF_POPUP,
              (UINT_PTR)submenu,
              (const wchar_t *)name.buffer());
          }
        }
        continue;
      }
    }
  }
  
  return menu;
}

SharedPtr<Win32Menu>  Win32Menu::main_menu;
SharedPtr<Win32Menu>  Win32Menu::popup_menu;

Win32Menu::Win32Menu(Expr expr, bool is_popup)
  : Shareable()
{
  add_remove_menu(1);
  _hmenu = create_menu(expr, is_popup);
}

Win32Menu::~Win32Menu() {
  add_remove_menu(-1);
  DestroyMenu(_hmenu);
}

Expr Win32Menu::id_to_command(DWORD  id) {
  return id_to_cmd[id];
}

DWORD  Win32Menu::command_to_id(Expr cmd) {
  return cmd_to_id[cmd];
}

void Win32Menu::init_popupmenu(HMENU sub) {
  for(int i = GetMenuItemCount(sub) - 1; i >= 0; --i) {
    MENUITEMINFOW mii;
    
    memset(&mii, 0, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STATE;
    
    int id = GetMenuItemID(sub, i);
    MenuCommandStatus status = Application::test_menucommand_status(id_to_command(id));
    
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

//} ... class Win32Menu


//{ class Win32AcceleratorTable ...

static bool set_accel_key(Expr expr, ACCEL *accel) {
  if(expr[0] != GetSymbol(FESymbolIndex::KeyEvent) || expr.expr_length() != 2)
    return false;
    
  Expr modifiers = expr[2];
  if(modifiers[0] != PMATH_SYMBOL_LIST)
    return false;
    
  accel->fVirt = 0;
  for(size_t i = modifiers.expr_length(); i > 0; --i) {
    Expr item = modifiers[i];
    
    if(item == GetSymbol(FESymbolIndex::KeyAlt))
      accel->fVirt |= FALT;
    else if(item == GetSymbol(FESymbolIndex::KeyControl))
      accel->fVirt |= FCONTROL;
    else if(item == GetSymbol(FESymbolIndex::KeyShift))
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
  }
  
  int len = GetKeyNameTextW(sc << 16, buf, 100);
  
  for(int i = 1; i < len; ++i) {
    if(buf[i] >= 'A' && buf[i] <= 'Z')
      buf[i] += 'a' - 'A';
  }
  
  return String::FromUcs2((const uint16_t *)buf, len);
}

static String accel_text(const ACCEL &accel) {
  String s("");
  
  if(accel.fVirt & FCONTROL)
    s += vk_name(VK_CONTROL) + "+";
    
  if(accel.fVirt & FALT)
    s += vk_name(VK_MENU) + "+";
    
  if(accel.fVirt & FSHIFT)
    s += vk_name(VK_SHIFT) + "+";
    
  if(accel.fVirt & FVIRTKEY)
    s += vk_name(accel.key);
  else // ASCII key code
    s += (char)accel.key;
    
  return s;
}

static HACCEL create_accel(Expr expr) {
  if(expr[0] != PMATH_SYMBOL_LIST)
    return nullptr;
    
  Array<ACCEL> accel(expr.expr_length());
  int j = 0;
  
  for(size_t i = 1; i <= expr.expr_length(); ++i) {
    Expr item(expr[i]);
    Expr cmd( item[2]);
    
    if( item[0] == GetSymbol(FESymbolIndex::Item) &&
        item.expr_length() == 2          &&
        set_accel_key(item[1], &accel[j]))
    {
      DWORD id = cmd_to_id[cmd];
      
      if(!id) {
        id = next_id++;
        add_command(id, cmd);
      }
      
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
  add_remove_menu(+1);
  _haccel = create_accel(expr);
}

Win32AcceleratorTable::~Win32AcceleratorTable() {
  DestroyAcceleratorTable(_haccel);
  add_remove_menu(-1);
}

//} ... class Win32AcceleratorTable
