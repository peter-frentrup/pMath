#include <gui/win32/menus/win32-menu.h>

#include <eval/binding.h>
#include <eval/application.h>
#include <eval/observable.h>

#include <gui/menus.h>
#include <gui/win32/api/win32-themes.h>
#include <gui/win32/api/win32-version.h>
#include <gui/win32/menus/win32-automenuhook.h>
#include <gui/win32/menus/win32-menu-gutter-slider.h>
#include <gui/win32/menus/win32-menu-search-overlay.h>
#include <gui/win32/menus/win32-menu-table-wizard.h>
#include <gui/win32/win32-clipboard.h>
#include <gui/win32/win32-control-painter.h>
#include <gui/win32/api/win32-themes.h>
#include <gui/win32/ole/dataobject.h>
#include <gui/win32/ole/dropsource.h>
#include <gui/win32/ole/droptarget.h>

#include <util/array.h>
#include <util/hashtable.h>

#include <resources.h>


using namespace richmath;

#ifndef MAPVK_VK_TO_VSC
#  define MAPVK_VK_TO_VSC  0
#endif

#ifndef MAPVK_VK_TO_CHAR
#  define MAPVK_VK_TO_CHAR  2
#endif

/* Windows reports Numpad Enter and Enter as VK_RETURN (0x0D). 
   0x0E is undefined, we use that ... 
 */
#define VK_MY_NUMPAD_ENTER   0x0E

// For the names, see http://blog.airesoft.co.uk/2009/11/wm_messages/
#define WM_UAHDESTROYWINDOW    0x0090
#define WM_UAHDRAWMENU         0x0091
#define WM_UAHDRAWMENUITEM     0x0092
#define WM_UAHINITMENU         0x0093
#define WM_UAHMEASUREMENUITEM  0x0094
#define WM_UAHNCPAINTMENUPOPUP 0x0095

#define MN_SETHMENU                 0x01E0
//#define MN_GETHMENU                 0x01E1
#define MN_SIZEWINDOW               0x01E2
#define MN_OPENHIERARCHY            0x01E3
#define MN_CLOSEHIERARCHY           0x01E4
#define MN_SELECTITEM               0x01E5
#define MN_CANCELMENUS              0x01E6
#define MN_SELECTFIRSTVALIDITEM     0x01E7

#define MN_GETPPOPUPMENU            0x01EA
#define MN_FINDMENUWINDOWFROMPOINT  0x01EB
#define MN_SHOWPOPUPWINDOW          0x01EC
#define MN_BUTTONDOWN               0x01ED
#define MN_MOUSEMOVE                0x01EE
#define MN_BUTTONUP                 0x01EF
#define MN_SETTIMERTOOPENHIERARCHY  0x01F0
#define MN_DBLCLK                   0x01F1
#define MN_ENDMENU                  0x01F2
#define MN_DODRAGDROP               0x01F3
// ???                              0x01F4  ??? seems to initiate WM_MENUDRAG notification

namespace {
  class MenuItemBuilder {
    public:
      static void add_remove_menu(int delta);
      static void add_command(DWORD id, Expr cmd);
      static DWORD get_or_create_command_id(Expr cmd);
      
      static HMENU create_menu(Expr expr, bool is_popup);
      
      static void update_items(HMENU sub);
    
    private:
      static bool init_info(MENUITEMINFOW *info, Expr item, String *buffer);
      static bool init_item_info(MENUITEMINFOW *info, Expr item, String *buffer);
      static bool init_delimiter_info(MENUITEMINFOW *info);
      static bool init_submenu_info(MENUITEMINFOW *info, Expr item, String *buffer);
      
    private:
      static DWORD   next_id;
    public:
      static ObservableValue<DWORD> selected_menu_item_id;
  };
  
  class MenuBitmaps {
    public:
      void stretch_item(MENUITEMINFOW *info);
      HBITMAP get_unchecked_bitmap(void);
      HBITMAP get_checkmark_bitmap(bool radio, bool enabled);
      
      void clear();
      
    private:
      MenuBitmaps() = default;
      ~MenuBitmaps() = default;
      
    public:
      static MenuBitmaps singleton;
      
    private:
      HBITMAP stretch_bmp {};
      HBITMAP checkmark_bmps[5] {}; // deselected, normal, disabled, buttet normal, bullet disabled. If >0, index = state (MC_*) for MENU_POPUPCHECK
      bool stretch_bmp_dark_mode = false;
      bool checkmark_bmps_dark_mode[5] {};
  };
  
  class MenuDropTarget: public DropTarget {
    public:
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
    
    public:
      MenuDropTarget(HWND hwnd_menu, HMENU menu, UINT pos, DWORD _flags);
    
    public:
      HWND  _hwnd_menu;
      HMENU _menu;
      UINT  _pos;
      DWORD _flags;
    
    protected:
      virtual HWND &hwnd() override { return _hwnd_menu; }
      
    private:
      LONG refcount;
  };
  
  class StaticMenuOverride {
    public:
      static void ensure_init();
    
    public:
      static StaticMenuOverride singleton;
      
      static void init_popupmenu(HMENU sub);
      static bool consumes_navigation_key(DWORD keycode, HMENU menu, int sel_item);
      static bool handle_child_window_mouse_message(HWND hwnd_menu, HWND hwnd_child, UINT msg, WPARAM wParam, const POINT &screen_pt);
    
    private:
      StaticMenuOverride();
      ~StaticMenuOverride();
    
    private:
      LRESULT (CALLBACK * default_wnd_proc)(HWND, UINT, WPARAM, LPARAM);
      void on_menu_connected(HWND hwnd, HMENU menu);
      void on_init_popupmenu(HWND hwnd, HMENU menu);
      void on_create(HWND hwnd);
      void on_ncdestroy(HWND hwnd);
      LRESULT on_char(   HWND hwnd, HMENU menu, WPARAM wParam, LPARAM lParam);
      LRESULT on_keydown(HWND hwnd, HMENU menu, WPARAM wParam, LPARAM lParam);
      LRESULT on_ctlcolorstatic(HWND hwnd, HDC hdc, HWND control);
      LRESULT on_find_menuwindow_from_point(HWND hwnd, WPARAM wParam, LPARAM lParam);
      LRESULT on_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
      
    private:
      static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    private:
      Hashtable<HWND, HMENU> popup_window_to_menu; // essentially same as SendMessage(hwnd, MN_GETHMENU, 0, 0)
      Hashtable<HMENU, HWND> menu_to_popup_window;
      Hashtable<HWND, Win32MenuItemOverlay*> popup_window_overlays;
      HWND prev_menu_under_mouse;
      HWND prev_control_under_mouse;
      HMENU initializing_popup_menu;
      HBRUSH light_background_brush;
      HBRUSH dark_background_brush;
  };
}

static Hashtable<Expr,  DWORD>  cmd_to_id;
static Hashtable<DWORD, Expr>   id_to_cmd;
static Hashtable<DWORD, String> id_to_shortcut_text;

namespace richmath { namespace strings {
  extern String Close;
  extern String InsertTable;
  extern String SearchMenuItems;
}}

extern pmath_symbol_t richmath_System_Delimiter;
extern pmath_symbol_t richmath_System_Inherited;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Menu;
extern pmath_symbol_t richmath_System_MenuItem;

extern pmath_symbol_t richmath_FE_ScopedCommand;
extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;

//{ class Win32Menu ...

SharedPtr<Win32Menu>  Win32Menu::main_menu;
bool                  Win32Menu::use_dark_mode = false;
bool                  Win32Menu::use_large_items = false;
Win32MenuSelector    *Win32Menu::menu_selector = nullptr;

Win32Menu::Win32Menu(Expr expr, bool is_popup)
  : Shareable()
{
  StaticMenuOverride::ensure_init();
  
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  MenuItemBuilder::add_remove_menu(1);
  _hmenu = MenuItemBuilder::create_menu(expr, is_popup);
}

Win32Menu::~Win32Menu() {
  MenuItemBuilder::add_remove_menu(-1);
  WIN32report(DestroyMenu(_hmenu));
}

Expr Win32Menu::id_to_command(DWORD  id) {
  return id_to_cmd[id];
}

DWORD Win32Menu::command_to_id(Expr cmd) {
  return cmd_to_id[cmd];
}

void Win32Menu::init_popupmenu(HMENU sub) {
  MenuItemBuilder::update_items(sub);
  StaticMenuOverride::init_popupmenu(sub);
}

Expr Win32Menu::selected_item_command() {
  return Win32Menu::id_to_command(MenuItemBuilder::selected_menu_item_id);
}

int Win32Menu::find_hilite_menuitem(HMENU *menu, bool recursive) {
  if(!*menu)
    return -1;
  
  for(int i = 0; i < WIN32report_errval(GetMenuItemCount(*menu), -1); ++i) {
    UINT state = WIN32report_errval(GetMenuState(*menu, i, MF_BYPOSITION), (UINT)(-1));
    
    if(state & MF_HILITE) {
      if(recursive && (state & MF_POPUP)) {
        HMENU old = *menu;
        *menu = GetSubMenu(*menu, i);
        int result = find_hilite_menuitem(menu, recursive);
        if(result >= 0)
          return result;
          
        *menu = old;
      }
    
      return i;
    }
  }
  
  return -1;
}

void Win32Menu::on_menuselect(WPARAM wParam, LPARAM lParam) {
  HMENU menu = (HMENU)lParam;
  UINT item_or_index = LOWORD(wParam);
  UINT flags = HIWORD(wParam);
  
  if(menu) { // NULL when a menu is closed
//    static HWND previous_aero_peak = nullptr;
//    HWND aero_peak = nullptr;
    
    //pmath_debug_print("[WM_MENUSELECT %p %d (%x)]\n", menu, item_or_index, flags);
    
    if(flags == 0xFFFF) {
      MenuItemBuilder::selected_menu_item_id = 0;
    }
    else if(!(flags & MF_POPUP)) {
      MenuItemBuilder::selected_menu_item_id = item_or_index;
      
//      Expr cmd = Win32Menu::id_to_command(item_or_index);
//      pmath_debug_print_object("[WM_MENUSELECT ", cmd.get(), "]\n");
//      
//      if(cmd.item_equals(0, richmath_FrontEnd_SetSelectedDocument)) {
//        auto id = FrontEndReference::from_pmath(cmd[1]);
//        auto doc = FrontEndObject::find_cast<Document>(id);
//        if(doc) {
//          Win32Widget *wid = dynamic_cast<Win32Widget*>(doc->native());
//          if(wid) {
//            aero_peak = wid->hwnd();
//            while(auto parent = GetParent(aero_peak))
//              aero_peak = parent;
//          }
//        }
//      }
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
      if(WIN32report(GetMenuItemInfoW(menu, item_or_index, 0 != (flags & MF_POPUP), &info))) {
        subitems_cmd = Win32Menu::id_to_command((DWORD)info.dwItemData);      
//        pmath_debug_print_object("[on_menuselect ", subitems_cmd.get(), ", ");
//         pmath_debug_print_object("", cmd.get(), "]\n");
      }
      
      if(subitems_cmd.is_null()) {
        SetCursor(LoadCursor(0, IDC_ARROW));
      }
      else {
        SetCursor(LoadCursor(0, IDC_HELP)); // TODO: reset when mouse leaves menu
      }
    }
//    if(Win32Themes::has_areo_peak()) {
//      if(aero_peak) {
//        struct callback_data {
//          HWND owner_window;
//          DWORD process_id;
//          DWORD thread_id;
//        } data;
//        data.owner_window = _window->hwnd();
//        data.process_id = GetCurrentProcessId();
//        data.thread_id = GetCurrentThreadId();
//        
//        EnumWindows(
//          [](HWND wnd, LPARAM _data) {
//            auto data = (callback_data*)_data;
//            
//            DWORD pid;
//            DWORD tid = GetWindowThreadProcessId(wnd, &pid);
//            
//            if(pid != data->process_id || tid != data->thread_id)
//              return TRUE;
//            
//            char class_name[20];
//            if(GetClassNameA(wnd, class_name, sizeof(class_name)) > 0 && strcmp(MenuWindowClass, class_name) == 0) {
//              BOOL disallow_peak = FALSE;
//              BOOL excluded_from_peak = FALSE;
//              Win32Themes::DwmGetWindowAttribute(wnd, Win32Themes::DWMWA_DISALLOW_PEEK,      &disallow_peak,      sizeof(disallow_peak));
//              Win32Themes::DwmGetWindowAttribute(wnd, Win32Themes::DWMWA_EXCLUDED_FROM_PEEK, &excluded_from_peak, sizeof(excluded_from_peak));
//              
//              pmath_debug_print("[menu window %p %s %s]", wnd, disallow_peak ? "disallow peak" : "", excluded_from_peak ? "exclude from peak" : "");
//              
//              excluded_from_peak = TRUE;
//              Win32Themes::DwmSetWindowAttribute(wnd, Win32Themes::DWMWA_EXCLUDED_FROM_PEEK, &excluded_from_peak, sizeof(excluded_from_peak));
//            };
//            return TRUE;
//          },
//          (LPARAM)&data
//        );
//        
//        BOOL excluded_from_peak = TRUE;
//        Win32Themes::DwmSetWindowAttribute(data.owner_window, Win32Themes::DWMWA_EXCLUDED_FROM_PEEK, &excluded_from_peak, sizeof(excluded_from_peak));
//        
//        Win32Themes::activate_aero_peak(true, aero_peak, data.owner_window, LivePreviewTrigger::TaskbarThumbnail);
//        previous_aero_peak = aero_peak;
//      }
//      else if(previous_aero_peak /*&& (flags != 0xFFFF || menu != nullptr)*/) {
//        previous_aero_peak = nullptr;
//        Win32Themes::activate_aero_peak(false, nullptr, nullptr, LivePreviewTrigger::TaskbarThumbnail);
//      
//        BOOL excluded_from_peak = FALSE;
//        Win32Themes::DwmSetWindowAttribute(_window->hwnd(), Win32Themes::DWMWA_EXCLUDED_FROM_PEEK, &excluded_from_peak, sizeof(excluded_from_peak));
//      }
//    }
  }

  if(Win32Menu::menu_selector)
    Win32Menu::menu_selector->on_menuselect(menu, item_or_index, flags);
}

LRESULT Win32Menu::on_menudrag(WPARAM wParam, LPARAM lParam) {
  pmath_debug_print("[on_menudrag]\n");
  
  HMENU menu = (HMENU)lParam;
  MENUITEMINFOW mii = { sizeof(mii) };
  mii.fMask = MIIM_DATA | MIIM_ID; 
  if(WIN32report(GetMenuItemInfoW(menu, wParam, TRUE, &mii))) {
    Expr cmd = id_to_command(mii.wID);
    
    if(cmd.item_equals(0, richmath_FrontEnd_SetSelectedDocument)) {
      DataObject *data_object = new DataObject;
      data_object->source_content = cmd[1];
      data_object->add_source_format(Win32Clipboard::AtomBoxesText);
      data_object->add_source_format(CF_UNICODETEXT);
      data_object->add_source_format(CF_TEXT);
      
      DropSource *drop_source = new DropSource();
      drop_source->description_data.copy(data_object);
      
      if(Win32Themes::is_app_themed())
        drop_source->set_flags(DSH_ALLOWDROPDESCRIPTIONTEXT);
      
      bool have_image = false;
      RECT rect;
      POINT pt = {0,0};
      GetCursorPos(&pt);
      if(WIN32report(GetMenuItemRect(nullptr, menu, wParam, &rect))) {
        have_image = HRbool(drop_source->set_drag_image_from_window_part(nullptr, &rect, &pt));
      }
      
      if(!have_image)
        drop_source->set_drag_image_from_window(nullptr);
  
      DWORD effect = DROPEFFECT_COPY;
      HRESULT res = data_object->do_drag_drop(drop_source, effect, &effect);
      
      data_object->Release();
      drop_source->Release();
      
      if(res == DRAGDROP_S_DROP)
        return MND_ENDMENU;
      else
        return MND_CONTINUE;
    }
  }
  
  return MND_CONTINUE;
}

LRESULT Win32Menu::on_menugetobject(WPARAM wParam, LPARAM lParam) {
  MENUGETOBJECTINFO *info = (MENUGETOBJECTINFO*)lParam;
  
  if(IDropTarget *dt = new MenuDropTarget(nullptr, info->hmenu, info->uPos, info->dwFlags)) {
    HRESULT hr = dt->QueryInterface(*(IID*)info->riid, &info->pvObj);
    dt->Release();
    return HRbool(hr) ? MNGO_NOERROR : MNGO_NOINTERFACE;
  }
  
  return MNGO_NOINTERFACE;
}

bool Win32Menu::consumes_navigation_key(DWORD keycode, HMENU menu, int sel_item) {
  return StaticMenuOverride::consumes_navigation_key(keycode, menu, sel_item);
}

bool Win32Menu::handle_child_window_mouse_message(HWND hwnd_menu, HWND hwnd_child, UINT msg, WPARAM wParam, const POINT &screen_pt) {
  return StaticMenuOverride::handle_child_window_mouse_message(hwnd_menu, hwnd_child, msg, wParam, screen_pt);
}

//} ... class Win32Menu

//{ class MenuItemBuilder ...

DWORD                  MenuItemBuilder::next_id = 1000;
ObservableValue<DWORD> MenuItemBuilder::selected_menu_item_id { 0 };

void MenuItemBuilder::add_remove_menu(int delta) {
  static int num_menus = 0;
  
  if(num_menus == 0) {
    RICHMATH_ASSERT(delta == 1);
    
    cmd_to_id.default_value = 0;
    
    add_command(SC_CLOSE, strings::Close);
    add_command((UINT)SpecialCommandID::None, {});
    add_command((UINT)SpecialCommandID::Select, {});
    add_command((UINT)SpecialCommandID::Remove, {});
    add_command((UINT)SpecialCommandID::GoToDefinition, {});
    
    Win32MenuSearchOverlay::init();
    Win32MenuTableWizard::init();
  }
  
  num_menus += delta;
  if(num_menus == 0) {
    cmd_to_id.clear();
    id_to_cmd.clear();
    id_to_shortcut_text.clear();
    
    MenuBitmaps::singleton.clear();
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
  if(!expr.item_equals(0, richmath_System_Menu) || expr.expr_length() != 2)
    return nullptr;
    
  String name(expr[1]);
  if(name.is_null())
    return nullptr;
    
  expr = expr[2];
  if(!expr.item_equals(0, richmath_System_List))
    return nullptr;
  
  HMENU menu = is_popup ? WIN32report(CreatePopupMenu()) : WIN32report(CreateMenu());
  if(menu) {
    MENUINFO mi = { sizeof(mi) };
    mi.fMask = MIM_STYLE;
    mi.dwStyle = MNS_DRAGDROP;
    WIN32report(SetMenuInfo(menu, &mi));
    
    for(size_t i = 1; i <= expr.expr_length(); ++i) {
      Expr item = expr[i];
      
      MENUITEMINFOW item_info = {0};
      item_info.cbSize = sizeof(item_info);
      String buffer;
      if(init_info(&item_info, expr[i], &buffer)) {
        WIN32report(InsertMenuItemW(menu, WIN32report_errval(GetMenuItemCount(menu), -1), TRUE, &item_info));
      }
    }
  }
  
  return menu;
}

void MenuItemBuilder::update_items(HMENU sub) {
  int count = WIN32report_errval(GetMenuItemCount(sub), -1);
  for(int i = 0; i < count; ++i) {
    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask = MIIM_DATA | MIIM_ID | MIIM_SUBMENU;
    if(WIN32report(GetMenuItemInfoW(sub, i, TRUE, &mii))) {
      if(mii.dwItemData) {
        DWORD list_id = mii.dwItemData;
        // dwItemData != 0 means that this item is dynamically generated from the menu item command of that id
        Expr new_items = Menus::generate_dynamic_submenu(Win32Menu::id_to_command(list_id));
        
        if(new_items.expr_length() == 0 || !new_items.item_equals(0, richmath_System_List)) {
          mii.fMask |= MIIM_STRING | MIIM_STATE;
          mii.fState |= MFS_DISABLED;
          mii.dwTypeData = const_cast<wchar_t*>(L"(empty)");
          mii.cch = 7;
          
          if(mii.hSubMenu) {
            WIN32report(DestroyMenu(mii.hSubMenu));
            mii.hSubMenu = nullptr;
          }
          
          WIN32report(SetMenuItemInfoW(sub, i, TRUE, &mii));
          continue;
        }
        
        int first_menu_index = i;
        bool insert = false;
        for(size_t k = 1; k <= new_items.expr_length(); ++k) {
          String buffer;
          mii.fState = 0;
          if(!MenuItemBuilder::init_info(&mii, new_items[k], &buffer))
            continue;
          
          mii.fMask |= MIIM_DATA;
          mii.dwItemData = list_id;
          
          if(Expr cmd = Win32Menu::id_to_command(mii.wID)) {
            MenuCommandStatus status = Menus::test_command_status(PMATH_CPP_MOVE(cmd));
            
            mii.fMask |= MIIM_STATE;
            if(status.enabled)
              mii.fState |= MFS_ENABLED;
            else
              mii.fState |= MFS_GRAYED;
            
            if(status.checked) 
              mii.fState |= MFS_CHECKED;
            else
              mii.fState |= MFS_UNCHECKED;
          }
          
          if(insert) {
            if(WIN32report(InsertMenuItemW(sub, i, TRUE, &mii))) {
              ++i;
              ++count;
            }
            continue;
          }
          
          if(!WIN32report(SetMenuItemInfoW(sub, i, TRUE, &mii))) 
            continue;
          
          ++i;
          if(i < count) {
            mii.fMask = MIIM_DATA | MIIM_ID | MIIM_SUBMENU;
            if(WIN32report(GetMenuItemInfoW(sub, i, TRUE, &mii))) {
              if(mii.dwItemData == list_id) {
                if(mii.hSubMenu) {
                  WIN32report(DestroyMenu(mii.hSubMenu));
                  mii.hSubMenu = nullptr;
                  WIN32report(SetMenuItemInfoW(sub, i, TRUE, &mii));
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
          mii.dwTypeData = const_cast<wchar_t*>(L"(empty)");
          mii.cch = 7;
          
          if(mii.hSubMenu) {
            WIN32report(DestroyMenu(mii.hSubMenu));
            mii.hSubMenu = nullptr;
          }
          
          WIN32report(SetMenuItemInfoW(sub, i, TRUE, &mii));
          ++i;
        }
        
        while(i < count) {
          mii.fMask = MIIM_DATA | MIIM_ID;
          if(WIN32report(GetMenuItemInfoW(sub, i, TRUE, &mii))) {
            if(mii.dwItemData != list_id) 
              break;
              
            if(WIN32report(DeleteMenu(sub, i, MF_BYPOSITION)))
              --count;
          }
        }
        
        --i;
        continue;
      }
    
      if(Expr cmd = Win32Menu::id_to_command(mii.wID)) {
        MenuCommandStatus status = Menus::test_command_status(PMATH_CPP_MOVE(cmd));
        mii.fMask = MIIM_STATE;
        
        if(status.enabled)
          mii.fState |= MFS_ENABLED;
        else
          mii.fState |= MFS_GRAYED;
        
        if(status.checked) 
          mii.fState |= MFS_CHECKED;
        else
          mii.fState |= MFS_UNCHECKED;
        
        WIN32report(SetMenuItemInfoW(sub, i, TRUE, &mii));
      }
    }
  }
}

bool MenuItemBuilder::init_info(MENUITEMINFOW *info, Expr item, String *buffer) {
  RICHMATH_ASSERT(info);
  RICHMATH_ASSERT(info->cbSize >= sizeof(MENUITEMINFOW));
  RICHMATH_ASSERT(buffer);
  
  if(item == richmath_System_Delimiter)
    return init_delimiter_info(info);
  
  if(item.item_equals(0, richmath_System_MenuItem) && item.expr_length() == 2)
    return init_item_info(info, PMATH_CPP_MOVE(item), buffer);
  
  if(item.item_equals(0, richmath_System_Menu) && item.expr_length() == 2)
    return init_submenu_info(info, PMATH_CPP_MOVE(item), buffer);
  
  return false;
}

bool MenuItemBuilder::init_item_info(MENUITEMINFOW *info, Expr item, String *buffer) {
  *buffer = String(item[1]);
  Expr cmd = item[2];
  
  info->fMask   |= MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_BITMAP;
  info->wID      = get_or_create_command_id(cmd);
  info->fType    = MFT_STRING;
  info->fState   = MFS_ENABLED;
  info->hbmpItem = nullptr;
  
  MenuItemType type = Menus::command_type(cmd);
  if(type == MenuItemType::RadioButton)
    info->fType |= MFT_RADIOCHECK;
  
  if(cmd.is_string() && cmd == strings::InsertTable) {
    static HBITMAP dummy_table_bmp = nullptr;
    if(!dummy_table_bmp) {
      int width = 1;
      int height = Win32MenuTableWizard::preferred_height();
      // Size is for primary monitor's DPI, on other monitors, windows scales the bitmap accordingly
      // e.g. Primary monitor DPI = 120 (125%), secondary monitor DPI = 96 (100%)  
      // =>  200 pixel tall bitmap will be 200 pixels tall on primary monitor, and 160 = 200 * 96/120 pixels on secondary monitor 
      dummy_table_bmp = CreateBitmap(width, height, 1, 1, nullptr);
      if(dummy_table_bmp) {
        // Init dummy_table_bmp contents to white. CreateBitmap(... nullptr) is documented to give undefined contents.
        HDC memDC          = CreateCompatibleDC(NULL);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(memDC, dummy_table_bmp);
        PatBlt(memDC, 0, 0, width, height, WHITENESS);
        SelectObject(memDC, hOldBitmap);
        DeleteDC(memDC);
      }
    }
    
    info->hbmpItem = dummy_table_bmp;
  }
  
  String shortcut = id_to_shortcut_text[info->wID];
  if(shortcut.length() > 0)
    *buffer += String::FromChar('\t') + shortcut;
    
  *buffer += String::FromChar(0);
  info->dwTypeData = (wchar_t *)buffer->buffer();
  
  if(buffer->length() == 0)
    return false;
    
  info->cch = buffer->length() - 1;
    
  info->fMask         &= ~MIIM_CHECKMARKS;
  info->hbmpChecked    = nullptr;
  info->hbmpUnchecked  = nullptr;
  
  MenuBitmaps::singleton.stretch_item(info);
  if((info->fMask & MIIM_BITMAP) != 0 && info->hbmpItem) {
    if(type == MenuItemType::CheckButton || type == MenuItemType::RadioButton) {
      // Use system default checkmarks even if hbmpItem is set (by stretch_item())
      // Setting to nullptr to choose system default does not work if hbmpItem is also provided :-/
      info->fMask         |= MIIM_CHECKMARKS;
      info->hbmpChecked    = MenuBitmaps::singleton.get_checkmark_bitmap((info->fType & MFT_RADIOCHECK) != 0, true);
      info->hbmpUnchecked  = (type == MenuItemType::CheckButton || type == MenuItemType::RadioButton) ? MenuBitmaps::singleton.get_unchecked_bitmap() : nullptr;
    }
  }
  
  return true;
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
  
  info->fMask     |= MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_BITMAP;
  
  info->dwTypeData = (wchar_t *)buffer->buffer();
  info->cch        = buffer->length() - 1;
  
  info->fType      = MFT_STRING;
  info->fState     = MFS_ENABLED;
  info->hbmpItem   = nullptr;
  
  Expr sub_items = item[2];
  if(sub_items.is_string()) {
    info->fMask     |= MIIM_DATA | MIIM_ID;
    info->wID        = get_or_create_command_id(sub_items);
    info->dwItemData = info->wID; // dwItemData != 0 means that this item is dynamically generated from the menu item command of that id
    info->fState    |= MFS_DISABLED;
    info->dwTypeData = const_cast<wchar_t*>(L"(empty)");
    info->cch        = 7;
  }
  else if(sub_items.item_equals(0, richmath_System_List)) {
    info->fMask |= MIIM_SUBMENU;
    info->hSubMenu = create_menu(PMATH_CPP_MOVE(item), true);
  }
  
  MenuBitmaps::singleton.stretch_item(info);
  return true;
}

//} ... class MenuItemBuilder

//{ class MenuBitmaps ...

MenuBitmaps MenuBitmaps::singleton {};

void MenuBitmaps::clear() {
  if(stretch_bmp) {
    DeleteObject(stretch_bmp); stretch_bmp = nullptr;
  }
  
  for(int i = 0; i < sizeof(checkmark_bmps) / sizeof(checkmark_bmps[0]); ++i) {
    if(checkmark_bmps[i]) {
      DeleteObject(checkmark_bmps[i]); 
      checkmark_bmps[i] = nullptr;
    }
  }
}

void MenuBitmaps::stretch_item(MENUITEMINFOW *info) {
  if(!Win32Menu::use_large_items) {
    if((info->fMask & MIIM_BITMAP) != 0 && info->hbmpItem == stretch_bmp) {
      info->hbmpItem = nullptr;
    }
    return;
  }
  
  if((info->fMask & MIIM_BITMAP) != 0 && info->hbmpItem && info->hbmpItem != stretch_bmp)
    return; // Already has a bitmap. Don't mess with that
  
  bool need_redraw = false;
  if(stretch_bmp_dark_mode != Win32Menu::use_dark_mode) {
    stretch_bmp_dark_mode = Win32Menu::use_dark_mode;
    need_redraw = true;
  }
  
  int cy_normal = GetSystemMetrics(SM_CYMENUCHECK); // For primary monitor!
//  int cx = GetSystemMetrics(SM_CXMENUCHECK); // For primary monitor!
  
  // Size is for primary monitor's DPI, on other monitors, windows scales the bitmap accordingly
  // e.g. Primary monitor DPI = 120 (125%), secondary monitor DPI = 96 (100%)  
  // =>  200 pixel tall bitmap will be 200 pixels tall on primary monitor, and 160 = 200 * 96/120 pixels on secondary monitor 
  int width = 1; //cx;
  int height = 2 * cy_normal;
  
  if(!stretch_bmp) {
//    pmath_debug_print("[GetSystemMetrics(SM_CXMENUCHECK): %d]\n", GetSystemMetrics(SM_CXMENUCHECK));
//    pmath_debug_print("[GetSystemMetrics(SM_CXMENUSIZE):  %d]\n", GetSystemMetrics(SM_CXMENUSIZE));
//    pmath_debug_print("[GetSystemMetrics(SM_CYMENUCHECK): %d]\n", GetSystemMetrics(SM_CYMENUCHECK));
//    pmath_debug_print("[GetSystemMetrics(SM_CYMENU):      %d]\n", GetSystemMetrics(SM_CYMENU));
//    pmath_debug_print("[GetSystemMetrics(SM_CYMENUSIZE):  %d]\n", GetSystemMetrics(SM_CYMENUSIZE));
    
    HDC     hdc  = GetDC(nullptr);
    stretch_bmp = CreateCompatibleBitmap(hdc, width, height); //CreateBitmap(width, height, 1, 1, nullptr);
    ReleaseDC(nullptr, hdc);
  
    need_redraw = true;
  }
  
  if(stretch_bmp && need_redraw) {
    // Init stretch_bmp contents to white. CreateBitmap(... nullptr) is documented to give undefined contents.
    HDC hdc        = CreateCompatibleDC(nullptr);
    HBITMAP hbmp_old = (HBITMAP)SelectObject(hdc, stretch_bmp);
//    PatBlt(hdc, 0, 0, width, height, WHITENESS);
    
    RECT rect = {0, 0, width, height};
    Color bg = Win32ControlPainter::win32_painter.win32_button_face_color(Win32Menu::use_dark_mode);
    SetBkColor(hdc, bg.to_bgr24());
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &rect, L"", 0, nullptr);
    
    SelectObject(hdc, hbmp_old);
    DeleteDC(hdc);
  }
  
  if(stretch_bmp) {
//      info->fMask &= ~MIIM_STRING;
    info->fMask    |= MIIM_BITMAP;
    info->hbmpItem = stretch_bmp;
//      info->dwTypeData = nullptr;
//      info->cch = 0;
//      return true;
  }
}

HBITMAP MenuBitmaps::get_unchecked_bitmap(void) {
  int index = 0;
  bool need_redraw = false;
  if(checkmark_bmps_dark_mode[index] != Win32Menu::use_dark_mode) {
    checkmark_bmps_dark_mode[index] = Win32Menu::use_dark_mode;
    need_redraw = true;
  }
  
  if(checkmark_bmps[index] && !need_redraw)
    return checkmark_bmps[index];
  
  int cx = GetSystemMetrics(SM_CXMENUCHECK) - 1; // For primary monitor!
  int cy = GetSystemMetrics(SM_CYMENUCHECK) - 1; // For primary monitor!
  
  if(!checkmark_bmps[index]) {
    HDC     hdc  = GetDC(nullptr);
    checkmark_bmps[index] = CreateCompatibleBitmap(hdc, cx, cy); //CreateBitmap(cx, cy, 1, 32, nullptr);  //= CreateCompatibleBitmap(hdc, cx, cy);
    ReleaseDC(nullptr, hdc);
  
    need_redraw = true;
  }
  
  if(checkmark_bmps[index] && need_redraw) {
    // Init contents to white. CreateBitmap(... nullptr) is documented to give undefined contents.
    HDC hdc        = CreateCompatibleDC(nullptr);
    HBITMAP hbmp_old = (HBITMAP)SelectObject(hdc, checkmark_bmps[index]);
//    PatBlt(hdc, 0, 0, cx, cy, WHITENESS);
    
    RECT rect = {0, 0, cx, cy};
    Color bg = Win32ControlPainter::win32_painter.win32_button_face_color(Win32Menu::use_dark_mode);
    SetBkColor(hdc, bg.to_bgr24());
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &rect, L"", 0, nullptr);
    
    SelectObject(hdc, hbmp_old);
    DeleteDC(hdc);
  }
  
  return checkmark_bmps[index];
}

HBITMAP MenuBitmaps::get_checkmark_bitmap(bool radio, bool enabled) {
  int index = 1 + (radio ? 2 : 0) + (enabled ? 0 : 1);
  
  bool need_redraw = false;
  if(checkmark_bmps_dark_mode[index] != Win32Menu::use_dark_mode) {
    checkmark_bmps_dark_mode[index] = Win32Menu::use_dark_mode;
    need_redraw = true;
  }
  
  if(checkmark_bmps[index] && !need_redraw)
    return checkmark_bmps[index];
  
  int cx = GetSystemMetrics(SM_CXMENUCHECK) - 1; // For primary monitor!
  int cy = GetSystemMetrics(SM_CYMENUCHECK) - 1; // For primary monitor!
  
  if(!checkmark_bmps[index]) {
    HDC     hdc  = GetDC(nullptr);
    checkmark_bmps[index] = CreateCompatibleBitmap(hdc, cx, cy); //CreateBitmap(cx, cy, 1, 32, nullptr);  //= CreateCompatibleBitmap(hdc, cx, cy);
    ReleaseDC(nullptr, hdc);
  
    need_redraw = true;
  }
  
  if(checkmark_bmps[index] && need_redraw) {
    bool fake_dark = Win32Menu::use_dark_mode;
    
    HANDLE theme = nullptr;
    if(Win32Themes::is_app_themed() && Win32Themes::OpenThemeData && Win32Themes::CloseThemeData && Win32Themes::DrawThemeBackground) {
      if(Win32Menu::use_dark_mode) {
        // Windows 10, 1903 (Build 18362): DarkMode::MENU exists but only gives dark colors for popup menu parts
        theme = Win32Themes::OpenThemeData(nullptr, L"DarkMode::MENU");
        if(theme)
          fake_dark = false;
      }
      
      if(!theme)
        theme = Win32Themes::OpenThemeData(nullptr, L"MENU");
    }
    
    RECT rect = {0, 0, cx, cy};
    
    HDC hdc = CreateCompatibleDC(NULL);
    HBITMAP hbmp_old = (HBITMAP)SelectObject(hdc, checkmark_bmps[index]);
    
    // see also StaticMenuOverride::on_ctlcolorstatic()
    Color bg = Win32ControlPainter::win32_painter.win32_button_face_color(Win32Menu::use_dark_mode);
    if(fake_dark)
      SetBkColor(hdc, bg.to_bgr24() ^ 0x00FFFFFF); // will be inverted later again
    else
      SetBkColor(hdc, bg.to_bgr24());
      
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &rect, L"", 0, nullptr);
    
    PatBlt(hdc, 0, 0, cx, cy, WHITENESS);  
  //  // Clear alpha channel
  //  { // does not help
  //    BITMAPINFO bi = {};
  //    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
  //    bi.bmiHeader.biWidth       = 1;
  //    bi.bmiHeader.biHeight      = 1;
  //    bi.bmiHeader.biPlanes      = 1;
  //    bi.bmiHeader.biBitCount    = 32;
  //    bi.bmiHeader.biCompression = BI_RGB;
  //
  //    RGBQUAD bitmapBits = { 0x00, 0x00, 0x00, 0x00 };
  //
  //    StretchDIBits(hdc, 0, 0, cx, cy,
  //                  0, 0, 1, 1, &bitmapBits, &bi,
  //                  DIB_RGB_COLORS, SRCAND);
  //  }
    
  //  {
  //    cairo_surface_t *surface = cairo_win32_surface_create_with_format(hdc, CAIRO_FORMAT_ARGB32);
  //    
  //    cairo_t *cr = cairo_create(surface);
  //    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
  //    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  //    cairo_paint(cr);
  //    cairo_destroy(cr);
  //    cairo_surface_flush(surface);
  //    cairo_surface_destroy(surface);
  //  }
    
    if(theme) {
      int part_bg  = 12; // MENU_POPUPCHECKBACKGROUND
      int state_bg = enabled ? 2 : 1; // 1=MCB_DISABLED  2=MCB_NORMAL  3=MCB_BITMAP
      Win32Themes::DrawThemeBackground(theme, hdc, part_bg, state_bg, &rect, nullptr);
      
      int part_check = 11; // MENU_POPUPCHECK
      int state_check = index; // 1=MC_CHECKMARKNORMAL, 2=MC_CHECKMARKDISABLED, 3=MC_BULLETNORMAL, 4=MC_BULLETDISABLED
      Win32Themes::DrawThemeBackground(theme, hdc, part_check, state_check, &rect, nullptr);
    }
    else {
      DrawFrameControl(hdc, &rect, DFC_MENU, radio ? DFCS_MENUBULLET : DFCS_MENUCHECK);
    }
    
    if(fake_dark) {
       BitBlt(hdc, 0, 0, cx, cy, nullptr, 0, 0, DSTINVERT);
    }
    
    SelectObject(hdc, hbmp_old);
    DeleteDC(hdc);
    
    if(theme)
      Win32Themes::CloseThemeData(theme);
  }
  
  return checkmark_bmps[index];
}

//} ... class MenuBitmaps

//{ class MenuDropTarget ...

MenuDropTarget::MenuDropTarget(HWND hwnd_menu, HMENU menu, UINT pos, DWORD flags)
: DropTarget(),
  _hwnd_menu(hwnd_menu),
  _menu(menu),
  _pos(pos),
  _flags(flags),
  refcount(1)
{
}

STDMETHODIMP_(ULONG) MenuDropTarget::AddRef(void) {
  return InterlockedIncrement(&refcount);
}

STDMETHODIMP_(ULONG) MenuDropTarget::Release(void) {
  LONG count = InterlockedDecrement(&refcount);
  if(count == 0) {
    delete this;
    return 0;
  }
  
  return count;
}

//} ... class MenuDropTarget

//{ class StaticMenuOverride ...

StaticMenuOverride StaticMenuOverride::singleton;

StaticMenuOverride::StaticMenuOverride() 
: default_wnd_proc{nullptr},
  prev_menu_under_mouse{nullptr},
  prev_control_under_mouse{nullptr},
  initializing_popup_menu{nullptr},
  light_background_brush{nullptr},
  dark_background_brush{nullptr}
{
}

StaticMenuOverride::~StaticMenuOverride() {
  if(light_background_brush) WIN32report(DeleteObject(light_background_brush));
  if(dark_background_brush)  WIN32report(DeleteObject(dark_background_brush));
}

void StaticMenuOverride::ensure_init() {
  if(singleton.default_wnd_proc)
    return;
  
  WNDCLASSW wc;
  
  if(GetClassInfoW(nullptr, L"#32768", &wc)) {
    singleton.default_wnd_proc = wc.lpfnWndProc;
    wc.lpfnWndProc = wnd_proc;
    WIN32report(RegisterClassW(&wc));
  }
}

void StaticMenuOverride::init_popupmenu(HMENU sub) {
  if(HWND hwnd = singleton.menu_to_popup_window[sub]) {
    singleton.on_init_popupmenu(hwnd, sub);
  }
  else {
    singleton.initializing_popup_menu = sub;
  }
}

bool StaticMenuOverride::consumes_navigation_key(DWORD keycode, HMENU menu, int sel_item) {
  if(HWND hwnd_menu = singleton.menu_to_popup_window[menu]) {
    for(auto overlay = singleton.popup_window_overlays[hwnd_menu]; overlay; overlay = overlay->next) {
      if(overlay->consumes_navigation_key(keycode, menu, sel_item))
        return true;
    }
  }
  
//  if(keycode == VK_RIGHT) {
//    if(GetMenuState(menu, item, MF_BYPOSITION) & MF_POPUP)
//      return true;
//  }
  
  return false;
}

bool StaticMenuOverride::handle_child_window_mouse_message(HWND hwnd_menu, HWND hwnd_child, UINT msg, WPARAM wParam, const POINT &screen_pt) {
  if(singleton.prev_menu_under_mouse != hwnd_menu || singleton.prev_control_under_mouse != hwnd_child) {
    if(singleton.prev_menu_under_mouse != singleton.prev_control_under_mouse) {
      for(auto overlay = singleton.popup_window_overlays[singleton.prev_menu_under_mouse]; overlay; overlay = overlay->next) {
        if(overlay->control == singleton.prev_control_under_mouse) {
          overlay->on_mouse_leave();
        }
      }
    }
    
    singleton.prev_menu_under_mouse    = hwnd_menu;
    singleton.prev_control_under_mouse = hwnd_child;
  }
  
  if(hwnd_menu != hwnd_child) {
    for(auto overlay = singleton.popup_window_overlays[hwnd_menu]; overlay; overlay = overlay->next) {
      if(overlay->control == hwnd_child) {
        POINT pt = screen_pt;
        ScreenToClient(overlay->control, &pt);
        if(HMENU menu = singleton.popup_window_to_menu[hwnd_menu]) {
          return overlay->handle_mouse_message(msg, wParam, pt, menu);
        }
        break;
      }
    }
  }
  return false;
}

void StaticMenuOverride::on_init_popupmenu(HWND hwnd, HMENU menu) {
  int count = WIN32report_errval(GetMenuItemCount(menu), -1);
  
  int region_start = -1;
  Expr region_lhs;
  Expr region_scope;
  
  Win32MenuItemOverlay *new_overlays = nullptr;
  Win32MenuItemOverlay **first_overlay_ptr = popup_window_overlays.search(hwnd);
  if(!first_overlay_ptr)
    first_overlay_ptr = &new_overlays;
  
  Win32MenuItemOverlay **next_overlay_ptr = first_overlay_ptr;
  
  for(int i = 0; i < count; ++i) {
    Expr scope;
    Expr lhs;
    
    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask = MIIM_ID;
    if(WIN32report(GetMenuItemInfoW(menu, i, TRUE, &mii))) {
      if(Expr cmd = Win32Menu::id_to_command(mii.wID)) {
        
        if(cmd.item_equals(0, richmath_FE_ScopedCommand)) {
          scope = cmd[2];
          cmd = cmd[1];
        }
        
        if(cmd.is_rule()) {
          lhs = cmd[1];
          
          Expr rhs = cmd[2];
          if(!rhs.is_number() && rhs != richmath_System_Inherited) {
            // Inherited is used in Magnification->Inherited for 100%
            lhs = Expr();
          }
        }
        else if(cmd == strings::SearchMenuItems) {
          auto search = dynamic_cast<Win32MenuSearchOverlay*>(*next_overlay_ptr);
          if(!search) {
            search = new Win32MenuSearchOverlay(menu);
            search->next = *next_overlay_ptr;
            *next_overlay_ptr = search;
          }
          
          search->start_index = i;
          search->end_index   = i;
          next_overlay_ptr = &(search->next);
        }
        else if(cmd == strings::InsertTable) {
          auto wizard = dynamic_cast<Win32MenuTableWizard*>(*next_overlay_ptr);
          if(!wizard) {
            wizard = new Win32MenuTableWizard(menu);
            wizard->next = *next_overlay_ptr;
            *next_overlay_ptr = wizard;
          }
          
          wizard->start_index = i;
          wizard->end_index   = i;
          next_overlay_ptr = &(wizard->next);
        }
      }
    }
    
    if(region_start >= 0) {
      if(region_lhs != lhs || region_scope != scope) {
        if(region_start + 1 < i) {
          auto slider = dynamic_cast<Win32MenuGutterSlider*>(*next_overlay_ptr);
          if(!slider) {
            slider = new Win32MenuGutterSlider();
            slider->next = *next_overlay_ptr;
            *next_overlay_ptr = slider;
          }
          
          slider->lhs         = region_lhs;
          slider->scope       = region_scope;
          slider->start_index = region_start;
          slider->end_index   = i - 1;
          next_overlay_ptr = &(slider->next);
        }
        
        region_start = -1;
      }
    }
    
    if(lhs && region_start < 0) {
      region_start = i;
      region_lhs = lhs;
      region_scope = scope;
    }
  }
  
  if(0 <= region_start && region_start + 1 < count) {
    auto slider = dynamic_cast<Win32MenuGutterSlider*>(*next_overlay_ptr);
    if(!slider) {
      slider = new Win32MenuGutterSlider();
      slider->next = *next_overlay_ptr;
      *next_overlay_ptr = slider;
    }
    
    slider->lhs         = region_lhs;
    slider->scope       = region_scope;
    slider->start_index = region_start;
    slider->end_index   = count-1;
    next_overlay_ptr = &(slider->next);
  }
  
  if(auto rest = *next_overlay_ptr) {
    *next_overlay_ptr = nullptr;
    rest->delete_all();
  }
  
  if(new_overlays) {
    Win32MenuGutterSlider::delete_all(popup_window_overlays[hwnd]);
    popup_window_overlays.set(hwnd, new_overlays);
  }
  
  if(*first_overlay_ptr) {
    for(auto overlay = *first_overlay_ptr; overlay; overlay = overlay->next) {
      overlay->update_rect(hwnd, menu);
      overlay->initialize(hwnd, menu);
    }
  }
  
  if(Win32Menu::menu_selector)
    Win32Menu::menu_selector->init_popupmenu(hwnd, menu);
}

void StaticMenuOverride::on_create(HWND hwnd) {
  if(Win32Themes::SetWindowTheme && Win32Menu::use_dark_mode)
    HRreport(Win32Themes::SetWindowTheme(hwnd, L"DarkMode", nullptr));
}

void StaticMenuOverride::on_ncdestroy(HWND hwnd) {
  HMENU menu = popup_window_to_menu[hwnd];
  popup_window_to_menu.remove(hwnd);
  menu_to_popup_window.remove(menu);
  
  if(auto overlays = popup_window_overlays[hwnd]) {
    popup_window_overlays.remove(hwnd);
    
    for(auto ov = overlays; ov; ov = ov->next)
      ov->control = nullptr; // already freed by parent window
    
    overlays->delete_all();
  }
}

void StaticMenuOverride::on_menu_connected(HWND hwnd, HMENU menu) {
  menu_to_popup_window.set(menu, hwnd);
  popup_window_to_menu.set(hwnd, menu);
//  pmath_debug_print("[menu %p uses hwnd=%p]\n", menu, hwnd);
  
  on_init_popupmenu(hwnd, menu);
}

LRESULT StaticMenuOverride::on_char(HWND hwnd, HMENU menu, WPARAM wParam, LPARAM lParam) {
  //TODO: ask current menu item first
  
  for(auto overlay = singleton.popup_window_overlays[hwnd]; overlay; overlay = overlay->next) {
    if(overlay->handle_char_message(wParam, lParam, menu)) 
      return 0;
  }
  return default_wnd_proc(hwnd, WM_CHAR, wParam, lParam);
}

LRESULT StaticMenuOverride::on_keydown(HWND hwnd, HMENU menu, WPARAM wParam, LPARAM lParam) {
  //TODO: ask current menu item first
  
  for(auto overlay = singleton.popup_window_overlays[hwnd]; overlay; overlay = overlay->next) {
    if(overlay->handle_keydown_message(wParam, lParam, menu)) 
      return 0;
  }
  return default_wnd_proc(hwnd, WM_KEYDOWN, wParam, lParam);
}

LRESULT StaticMenuOverride::on_ctlcolorstatic(HWND hwnd, HDC hdc, HWND control) {
  Color col = Win32ControlPainter::win32_painter.win32_button_face_color(Win32Menu::use_dark_mode);
  SetBkColor(hdc, col.to_bgr24());
  SetTextColor(hdc, Win32Menu::use_dark_mode ? RGB(255,255,255) : RGB(0,0,0));
  
  HBRUSH *brush_ptr = Win32Menu::use_dark_mode ? &dark_background_brush : &light_background_brush;
  if(!*brush_ptr) {
    *brush_ptr = CreateSolidBrush(col.to_bgr24());
  }
  return (LRESULT)*brush_ptr;
}

LRESULT StaticMenuOverride::on_find_menuwindow_from_point(HWND hwnd, WPARAM wParam, LPARAM lParam) {
  POINT pos { (short)LOWORD(lParam), (short)HIWORD(lParam) };
  POINT localpos = pos;
  ScreenToClient(hwnd, &localpos);
  
  if(auto overlays = popup_window_overlays[hwnd]) {
    for(auto ov = overlays; ov; ov = ov->next) {
      RECT overlay_rect;
      if(ov->control && GetWindowRect(ov->control, &overlay_rect)) {
        if(PtInRect(&overlay_rect, pos)) {
          if(wParam)
            *(DWORD*)wParam = (DWORD)(-1);
          return (LRESULT)hwnd;
        }
      }
    }
  }
  
  return default_wnd_proc(hwnd, MN_FINDMENUWINDOWFROMPOINT, wParam, lParam);
}

LRESULT StaticMenuOverride::on_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  static char delayed_resize = 0;
  
  HMENU menu = popup_window_to_menu[hwnd];
  if(!menu && initializing_popup_menu) {
    menu = initializing_popup_menu;
    on_menu_connected(hwnd, menu);
  }
  initializing_popup_menu = nullptr;
  
//  if(msg != WM_TIMER && msg != MN_SELECTITEM) {
//    pmath_debug_print("[menu hwnd=%p menu=%p msg=0x%x, w=%x, l=%x]\n", hwnd, menu, msg, wParam, lParam);
//  }
  
  switch(msg) {
    case WM_CREATE:    on_create(hwnd); break;
    case WM_NCDESTROY: on_ncdestroy(hwnd); break;
    
    case WM_WINDOWPOSCHANGED: 
      KillTimer(hwnd, (UINT_PTR)&delayed_resize);
      SetTimer(hwnd, (UINT_PTR)&delayed_resize, 0, nullptr);
      break;
    
    case WM_TIMER: 
      if(wParam == (WPARAM)&delayed_resize) {
        KillTimer(hwnd, wParam);
        {
          RECT r;
          if(!menu || !GetMenuItemRect(nullptr, menu, 0, &r)) {
            SetTimer(hwnd, (UINT_PTR)&delayed_resize, 50, nullptr);
            break;
          }
        }
        for(auto overlay = popup_window_overlays[hwnd]; overlay; overlay = overlay->next)
          overlay->update_rect(hwnd, menu);
      }
      break;
    
    case WM_CHAR:           return on_char(   hwnd, menu, wParam, lParam);
    case WM_KEYDOWN:        return on_keydown(hwnd, menu, wParam, lParam);
    case WM_CTLCOLORSTATIC: return on_ctlcolorstatic(hwnd, (HDC)wParam, (HWND)lParam);
    
    case WM_PRINT: lParam |= PRF_CHILDREN; break;
    
    case MN_FINDMENUWINDOWFROMPOINT: return on_find_menuwindow_from_point(hwnd, wParam, lParam);
  }
  return default_wnd_proc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK StaticMenuOverride::wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  return singleton.on_wndproc(hwnd, msg, wParam, lParam);
}

//} ... class StaticMenuOverride

//{ class Win32AcceleratorTable ...

static bool set_accel_key(Expr expr, ACCEL *accel) {
  if(!expr.item_equals(0, richmath_FE_KeyEvent) || expr.expr_length() != 2)
    return false;
    
  Expr modifiers = expr[2];
  if(!modifiers.item_equals(0, richmath_System_List))
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
  else if(key.equals("Pause"))              accel->key = (accel->fVirt & FCONTROL) ? VK_CANCEL : VK_PAUSE;
  else if(key.equals("PageUp"))             accel->key = VK_PRIOR;
  else if(key.equals("PageDown"))           accel->key = VK_NEXT;
  else if(key.equals("End"))                accel->key = VK_END;
  else if(key.equals("Home"))               accel->key = VK_HOME;
  else if(key.equals("LeftArrow"))          accel->key = VK_LEFT;
  else if(key.equals("UpArrow"))            accel->key = VK_UP;
  else if(key.equals("RightArrow"))         accel->key = VK_RIGHT;
  else if(key.equals("DownArrow"))          accel->key = VK_DOWN;
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
    
    case VK_CANCEL:
    case VK_PAUSE:
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
  if(!expr.item_equals(0, richmath_System_List))
    return nullptr;
    
  Array<ACCEL> accel(expr.expr_length());
  int j = 0;
  
  for(size_t i = 1; i <= expr.expr_length(); ++i) {
    Expr item(expr[i]);
    Expr cmd( item[2]);
    
    if( item.item_equals(0, richmath_System_MenuItem) &&
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
