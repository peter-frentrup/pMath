#include <gui/win32/win32-menu.h>

#include <eval/binding.h>
#include <eval/application.h>
#include <eval/observable.h>

#include <gui/documents.h>
#include <gui/document.h>
#include <gui/menus.h>
#include <gui/win32/win32-automenuhook.h>
#include <gui/win32/win32-clipboard.h>
#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-themes.h>
#include <gui/win32/ole/dataobject.h>
#include <gui/win32/ole/dropsource.h>
#include <gui/win32/ole/droptarget.h>

#include <util/array.h>
#include <util/hashtable.h>

#include <resources.h>

#include <algorithm>
#include <cmath>
#include <limits>

#ifdef _MSC_VER
namespace std {
  static bool isnan(double d) {return _isnan(d);}
}
#endif

#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif


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
      static DWORD next_id;
      
    public:
      static ObservableValue<DWORD> selected_menu_item_id;
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
  
  class NumericMenuItemRegion {
    public:
      NumericMenuItemRegion();
      ~NumericMenuItemRegion();
      NumericMenuItemRegion(const NumericMenuItemRegion&) = delete;
      NumericMenuItemRegion &operator=(const NumericMenuItemRegion&) = delete;
      
      void delete_all();
      static void delete_all(NumericMenuItemRegion *reg);
      
      void update_slider_rect(HWND hwnd, HMENU menu);
      static void update_all_slider_rects(NumericMenuItemRegion *reg, HWND hwnd, HMENU menu);
      
      void calc_rect(RECT *rect, HWND hwnd, HMENU menu);
      bool collect_float_values(Array<float> &values, HMENU menu);
      static float interpolation_index(const Array<float> &values, float val, bool clip);
      static float interpolation_value(const Array<float> &values, float index);
      
      bool handle_slider_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu);
      int slider_pos_from_point(const POINT &pt);
      void apply_slider_pos(HMENU menu, int pos);
      
    public:
      Expr lhs;
      Expr scope;
      int start_index;
      int end_index;
      
      HWND slider;
      NumericMenuItemRegion *next;
      
      static const int ScaleFactor = 100;
  };
  
  class StaticMenuOverride {
    public:
      static void ensure_init();
    
    public:
      static StaticMenuOverride singleton;
      
      static void init_popupmenu(HMENU sub);
      static bool handle_child_window_mouse_message(HWND hwnd_menu, HWND hwnd_child, UINT msg, WPARAM wParam, const POINT &screen_pt);
    
    private:
      StaticMenuOverride();
    
    private:
      LRESULT (CALLBACK * default_wnd_proc)(HWND, UINT, WPARAM, LPARAM);
      void on_menu_connected(HWND hwnd, HMENU menu);
      void on_init_popupmenu(HWND hwnd, HMENU menu);
      void on_create(HWND hwnd);
      void on_ncdestroy(HWND hwnd);
      LRESULT on_find_menuwindow_from_point(HWND hwnd, WPARAM wParam, LPARAM lParam);
      LRESULT on_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
      
    private:
      static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    private:
      Hashtable<HWND, HMENU> popup_window_to_menu; // essentially same as SendMessage(hwnd, MN_GETHMENU, 0, 0)
      Hashtable<HMENU, HWND> menu_to_popup_window;
      Hashtable<HWND, NumericMenuItemRegion*> popup_window_regions;
      HMENU initializing_popup_menu;
  };
}

static Hashtable<Expr,  DWORD>  cmd_to_id;
static Hashtable<DWORD, Expr>   id_to_cmd;
static Hashtable<DWORD, String> id_to_shortcut_text;


extern pmath_symbol_t richmath_System_Delimiter;
extern pmath_symbol_t richmath_System_Document;
extern pmath_symbol_t richmath_System_Inherited;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Menu;
extern pmath_symbol_t richmath_System_MenuItem;
extern pmath_symbol_t richmath_System_Section;

extern pmath_symbol_t richmath_FE_ScopedCommand;
extern pmath_symbol_t richmath_FrontEnd_SetSelectedDocument;

//{ class Win32Menu ...

SharedPtr<Win32Menu>  Win32Menu::main_menu;
bool                  Win32Menu::use_dark_mode = false;

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
  DestroyMenu(_hmenu);
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

void Win32Menu::on_menuselect(WPARAM wParam, LPARAM lParam) {
  HMENU menu = (HMENU)lParam;
  UINT item_or_index = LOWORD(wParam);
  UINT flags = HIWORD(wParam);
  
//  static HWND previous_aero_peak = nullptr;
//  HWND aero_peak = nullptr;
  
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

LRESULT Win32Menu::on_menudrag(WPARAM wParam, LPARAM lParam) {
  pmath_debug_print("[on_menudrag]\n");
  
  HMENU menu = (HMENU)lParam;
  MENUITEMINFOW mii = { sizeof(mii) };
  mii.fMask = MIIM_DATA | MIIM_ID; 
  if(GetMenuItemInfoW(menu, wParam, TRUE, &mii)) {
    Expr cmd = id_to_command(mii.wID);
    
    if(cmd[0] == richmath_FrontEnd_SetSelectedDocument) {
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
      if(GetMenuItemRect(nullptr, menu, wParam, &rect)) {
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
    assert(delta == 1);
    
    cmd_to_id.default_value = 0;
    
    add_command(SC_CLOSE, String("Close"));
    add_command((UINT)SpecialCommandID::None, {});
    add_command((UINT)SpecialCommandID::Select, {});
    add_command((UINT)SpecialCommandID::Remove, {});
    add_command((UINT)SpecialCommandID::GoToDefinition, {});
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
  if(expr[0] != richmath_System_Menu || expr.expr_length() != 2)
    return nullptr;
    
  String name(expr[1]);
  if(name.is_null())
    return nullptr;
    
  expr = expr[2];
  if(expr[0] != richmath_System_List)
    return nullptr;
  
  HMENU menu = is_popup ? CreatePopupMenu() : CreateMenu();
  if(menu) {
    MENUINFO mi = { sizeof(mi) };
    mi.fMask = MIM_STYLE;
    mi.dwStyle = MNS_DRAGDROP;
    SetMenuInfo(menu, &mi);
    
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

void MenuItemBuilder::update_items(HMENU sub) {
  int count = GetMenuItemCount(sub);
  for(int i = 0; i < count; ++i) {
    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask = MIIM_DATA | MIIM_ID | MIIM_SUBMENU;
    if(GetMenuItemInfoW(sub, i, TRUE, &mii)) {
      if(mii.dwItemData) {
        DWORD list_id = mii.dwItemData;
        // dwItemData != 0 means that this item is dynamically generated from the menu item command of that id
        Expr new_items = Menus::generate_dynamic_submenu(Win32Menu::id_to_command(list_id));
        
        if(new_items.expr_length() == 0 || new_items[0] != richmath_System_List) {
          mii.fMask |= MIIM_STRING | MIIM_STATE;
          mii.fState |= MFS_DISABLED;
          mii.dwTypeData = (wchar_t*)L"(empty)";
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
        for(size_t k = 1; k <= new_items.expr_length(); ++k) {
          String buffer;
          mii.fState = 0;
          if(!MenuItemBuilder::init_info(&mii, new_items[k], &buffer))
            continue;
          
          mii.fMask |= MIIM_DATA;
          mii.dwItemData = list_id;
          
          if(Expr cmd = Win32Menu::id_to_command(mii.wID)) {
            MenuCommandStatus status = Menus::test_command_status(std::move(cmd));
            
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
          mii.dwTypeData = (wchar_t*)L"(empty)";
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
    
      if(Expr cmd = Win32Menu::id_to_command(mii.wID)) {
        MenuCommandStatus status = Menus::test_command_status(std::move(cmd));
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
  }
}

bool MenuItemBuilder::init_info(MENUITEMINFOW *info, Expr item, String *buffer) {
  assert(info);
  assert(info->cbSize >= sizeof(MENUITEMINFOW));
  assert(buffer);
  
  if(item == richmath_System_Delimiter)
    return init_delimiter_info(info);
  
  if(item[0] == richmath_System_MenuItem && item.expr_length() == 2)
    return init_item_info(info, std::move(item), buffer);
  
  if(item[0] == richmath_System_Menu && item.expr_length() == 2)
    return init_submenu_info(info, std::move(item), buffer);
  
  return false;
}

bool MenuItemBuilder::init_item_info(MENUITEMINFOW *info, Expr item, String *buffer) {
  *buffer = String(item[1]);
  Expr cmd = item[2];
  
  info->fMask |= MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING;
  info->wID = get_or_create_command_id(cmd);
  info->fType = MFT_STRING;
  info->fState = MFS_ENABLED;
  
  if(Menus::command_type(cmd) == MenuItemType::RadioButton)
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
    info->dwTypeData = (wchar_t*)L"(empty)";
    info->cch = 7;
  }
  else if(sub_items[0] == richmath_System_List) {
    info->fMask |= MIIM_SUBMENU;
    info->hSubMenu = create_menu(std::move(item), true);
  }
  
  return true;
}

//} ... class MenuItemBuilder

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

//{ class NumericMenuItemRegion ...

NumericMenuItemRegion::NumericMenuItemRegion()
: start_index{-1},
  end_index{-1},
  slider{nullptr},
  next{nullptr}
{
}

NumericMenuItemRegion::~NumericMenuItemRegion() {
  if(slider)
    DestroyWindow(slider);
}

void NumericMenuItemRegion::delete_all() {
  delete_all(this);
}

void NumericMenuItemRegion::delete_all(NumericMenuItemRegion *reg) {
  while(auto tmp = reg) {
    reg = reg->next;
    delete tmp;
  }
}

void NumericMenuItemRegion::update_slider_rect(HWND hwnd, HMENU menu) {
  RECT rect;
  calc_rect(&rect, hwnd, menu);
  
  if(!slider) {
    INITCOMMONCONTROLSEX icc = {sizeof(icc)};
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);
    
    slider = CreateWindowExW(
               WS_EX_NOACTIVATE,
               TRACKBAR_CLASSW,
               L"menu item slider",
               WS_CHILD | WS_VISIBLE | TBS_VERT | TBS_BOTH | TBS_NOTICKS,
               rect.left,
               rect.top,
               rect.right - rect.left,
               rect.bottom - rect.top,
               hwnd,
               nullptr,
               nullptr,
               nullptr);
    
    SendMessageW(slider, TBM_SETRANGE, false, MAKELPARAM(0, (end_index - start_index) * ScaleFactor));
    
    DWORD hwnd_exstyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    DWORD hwnd_style   = GetWindowLongW(hwnd, GWL_STYLE);
    pmath_debug_print("[menu hwnd style 0x%x  exstyle 0x%x]", hwnd_style, hwnd_exstyle);
    hwnd_style |= WS_CLIPCHILDREN;
    
    SetWindowLongW(hwnd, GWL_STYLE, hwnd_style);
  }
  else {
    MoveWindow(
      slider,
      rect.left,
      rect.top,
      rect.right - rect.left,
      rect.bottom - rect.top,
      TRUE);
  }

  pmath_debug_print_object("[region for ", lhs.get(), "");
  pmath_debug_print(" from index %d to %d, slider=%p @ (%d,%d) %d x %d]\n", 
    start_index, 
    end_index,
    slider,
    rect.left, 
    rect.top,
    rect.right - rect.left,
    rect.bottom - rect.top);
}

void NumericMenuItemRegion::update_all_slider_rects(NumericMenuItemRegion *reg, HWND hwnd, HMENU menu) {
  for(; reg; reg = reg->next)
    reg->update_slider_rect(hwnd, menu);
}

void NumericMenuItemRegion::calc_rect(RECT *rect, HWND hwnd, HMENU menu) {
  *rect = {0,0,0,0};
  
  int menu_item_height = 16;
  bool first = true;
  for(int i = start_index; i <= end_index; ++i) {
    RECT item_rect;
    if(GetMenuItemRect(nullptr, menu, (UINT)i, &item_rect)) {
      if(first) {
        *rect = item_rect;
        menu_item_height = item_rect.bottom - item_rect.top;
        first = false;
      }
      else
        UnionRect(rect, rect, &item_rect);
    }
  }
  
  rect->right = rect->left + menu_item_height;
  
  MapWindowPoints(nullptr, hwnd, (POINT*)rect, 2);
}

bool NumericMenuItemRegion::collect_float_values(Array<float> &values, HMENU menu) {
  values.length(end_index - start_index + 1);
  for(int i = 0; i < values.length(); ++i) {
    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask = MIIM_ID;
    if(!GetMenuItemInfoW(menu, start_index + i, TRUE, &mii))
      return false;
    
    Expr cmd = Win32Menu::id_to_command(mii.wID);
    if(cmd[0] == richmath_FE_ScopedCommand)
      cmd = cmd[1];
    
    if(!cmd.is_rule())
      return false;
    
    Expr rhs = cmd[2];
    if(rhs.is_number()) {
      values[i] = rhs.to_double();
    }
    else if(rhs == richmath_System_Inherited) {
      // Magnification -> Inherited
      values[i] = 1;
    }
    else
      return false;
  }
  
  return true;
}

float NumericMenuItemRegion::interpolation_index(const Array<float> &values, float val, bool clip) {
  float prev = NAN;
  for(int i = 0; i < values.length(); ++i) {
    float cur = values[i];
    if(val == cur)
      return i;
    
    float rel = (cur - val) / (cur - prev);
    if(0 <= rel && rel <= 1) 
      return i - rel;
    
    prev = cur;
  }
  
  if(clip) {
    if(values.length() >= 2) {
      if(values[0] < values[1]) {
        if(val < values[0])
          return 0;
        else
          return values.length() - 1;
      }
      else {
        if(val > values[0])
          return values.length() - 1;
        else
          return 0;
      }
    }
  }
  return NAN;
}

bool NumericMenuItemRegion::handle_slider_mouse_message(UINT msg, WPARAM wParam, const POINT &pt, HMENU menu) {
 
  int pos = slider_pos_from_point(pt);
//  pmath_debug_print("[mouse msg %x over menu slider: w=%x pos %d]\n", msg, wParam, pos);
  
  if(!(wParam & MK_SHIFT)) {
    int nearest = ((pos + ScaleFactor/2)/ScaleFactor) * ScaleFactor;
    if(nearest - ScaleFactor/4 < pos && pos < nearest + ScaleFactor/4)
      pos = nearest;
  }
  
  if(pos == SendMessageW(slider, TBM_GETPOS, 0, 0))
    return false;
  
  switch(msg) {
    case WM_MOUSEMOVE: 
      if(wParam & MK_LBUTTON) {
        apply_slider_pos(menu, pos);
      }
      break;
    
    case WM_LBUTTONDOWN: apply_slider_pos(menu, pos); break;
    case WM_LBUTTONUP:   apply_slider_pos(menu, pos); break;
  }
  
  return false;
}

int NumericMenuItemRegion::slider_pos_from_point(const POINT &pt) {
  RECT channel_rect = {};
  RECT thumb_rect = {};
  
  SendMessageW(slider, TBM_GETCHANNELRECT, 0, (LPARAM)&channel_rect);
  // Work around windows bug (e.g. on Windows 10, 1909): TBM_GETCHANNELRECT acts as if TBS_HORZ was given
  if(channel_rect.right - channel_rect.left > channel_rect.bottom - channel_rect.top) {
    using std::swap;
    swap(channel_rect.left,  channel_rect.top);
    swap(channel_rect.right, channel_rect.bottom);
  }
  int channel_length = channel_rect.bottom - channel_rect.top;
  
  SendMessageW(slider, TBM_GETTHUMBRECT, 0, (LPARAM)&thumb_rect);
  int thumb_thickness = thumb_rect.bottom - thumb_rect.top;
  
  if(channel_length > thumb_thickness) {
    int thumb_pos_px = pt.y - (channel_rect.top + thumb_thickness/2);
    if(thumb_pos_px < 0)
      thumb_pos_px = 0;
    else if(thumb_pos_px > channel_length - thumb_thickness)
      thumb_pos_px = channel_length - thumb_thickness;
    
    int range_min = 0; //SendMessageW(slider, TBM_GETRANGEMIN, 0, 0);
    int range_max = (end_index - start_index) * ScaleFactor; // SendMessageW(slider, TBM_GETRANGEMAX, 0, 0);
    return range_min + MulDiv(thumb_pos_px, range_max - range_min, channel_length - thumb_thickness);
  }
  else
    return 0;
}

void NumericMenuItemRegion::apply_slider_pos(HMENU menu, int pos) {
  Array<float> values;
  if(!collect_float_values(values, menu))
    return;
  
  int i = pos / ScaleFactor;
  if(i < 0 || i >= values.length())
    return;
  
  float val;
  if(pos == i * ScaleFactor) {
    val = values[i];
  }
  else {
    if(i + 1 >= values.length())
      return;
    
    float v0 = values[i];
    float v1 = values[i+1];
    val = v0 + ((v1 - v0) * (pos - i * ScaleFactor)) / ScaleFactor;
  }
  
  Expr cmd = Rule(lhs, val);
  if(scope)
    cmd = Call(Symbol(richmath_FE_ScopedCommand), std::move(cmd), scope);
  
  if(!Menus::run_command_now(std::move(cmd)))
    return;
  
  // TBM_SETPOSNOTIFY exists since Windows 7
  SendMessageW(slider, TBM_SETPOS, TRUE, (LPARAM)pos);
}

//} ... class NumericMenuItemRegion

//{ class StaticMenuOverride ...

StaticMenuOverride StaticMenuOverride::singleton;

StaticMenuOverride::StaticMenuOverride() 
: initializing_popup_menu{nullptr},
  default_wnd_proc{nullptr}
{
}

void StaticMenuOverride::ensure_init() {
  if(singleton.default_wnd_proc)
    return;
  
  WNDCLASSW wc;
  
  if(GetClassInfoW(nullptr, L"#32768", &wc)) {
    singleton.default_wnd_proc = wc.lpfnWndProc;
    wc.lpfnWndProc = wnd_proc;
    RegisterClassW(&wc);
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

bool StaticMenuOverride::handle_child_window_mouse_message(HWND hwnd_menu, HWND hwnd_child, UINT msg, WPARAM wParam, const POINT &screen_pt) {
//  pmath_debug_print("[msg for slider ...]");
//  POINT pt = screen_pt;
//  ScreenToClient(hwnd_child, &pt);
//  SendMessageW(hwnd_child, msg, wParam, MAKELPARAM(pt.x, pt.y));
//  pmath_debug_print("[... msg for slider]");
//  return true;
  for(auto reg = singleton.popup_window_regions[hwnd_menu]; reg; reg = reg->next) {
    if(reg->slider == hwnd_child) {
      POINT pt = screen_pt;
      ScreenToClient(reg->slider, &pt);
      if(HMENU menu = singleton.popup_window_to_menu[hwnd_menu]) {
        return reg->handle_slider_mouse_message(msg, wParam, pt, menu);
      }
    }
  }
  return false;
}

void StaticMenuOverride::on_menu_connected(HWND hwnd, HMENU menu) {
  menu_to_popup_window.set(menu, hwnd);
  popup_window_to_menu.set(hwnd, menu);
//  pmath_debug_print("[menu %p uses hwnd=%p]\n", menu, hwnd);
  
  on_init_popupmenu(hwnd, menu);
}

void StaticMenuOverride::on_init_popupmenu(HWND hwnd, HMENU menu) {
  int count = GetMenuItemCount(menu);
  
  int region_start = -1;
  Expr region_lhs;
  Expr region_scope;
  
  NumericMenuItemRegion *new_regions = nullptr;
  NumericMenuItemRegion **first_reg_pos = popup_window_regions.search(hwnd);
  if(!first_reg_pos)
    first_reg_pos = &new_regions;
  
  NumericMenuItemRegion **next_reg_pos = first_reg_pos;
  
  for(int i = 0; i < count; ++i) {
    Expr scope;
    Expr lhs;
    
    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask = MIIM_ID;
    if(GetMenuItemInfoW(menu, i, TRUE, &mii)) {
      if(Expr cmd = Win32Menu::id_to_command(mii.wID)) {
        
        if(cmd[0] == richmath_FE_ScopedCommand) {
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
      }
    }
    
    if(region_start >= 0) {
      if(region_lhs != lhs || region_scope != scope) {
        if(region_start + 1 < i) {
          if(!*next_reg_pos)
            *next_reg_pos = new NumericMenuItemRegion();
          
          (*next_reg_pos)->lhs         = region_lhs;
          (*next_reg_pos)->scope       = region_scope;
          (*next_reg_pos)->start_index = region_start;
          (*next_reg_pos)->end_index   = i - 1;
          next_reg_pos = &((*next_reg_pos)->next);
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
    if(!*next_reg_pos)
      *next_reg_pos = new NumericMenuItemRegion();

    (*next_reg_pos)->lhs         = region_lhs;
    (*next_reg_pos)->scope       = region_scope;
    (*next_reg_pos)->start_index = region_start;
    (*next_reg_pos)->end_index   = count-1;
    next_reg_pos = &((*next_reg_pos)->next);
  }
  
  if(auto rest = *next_reg_pos) {
    *next_reg_pos = nullptr;
    rest->delete_all();
  }
  
  if(new_regions) {
    NumericMenuItemRegion::delete_all(popup_window_regions[hwnd]);
    popup_window_regions.set(hwnd, new_regions);
  }
  
  if(*first_reg_pos) {
    NumericMenuItemRegion::update_all_slider_rects(*first_reg_pos, hwnd, menu);
    
    if(Document *doc = Documents::current()) {
      for(auto reg = *first_reg_pos; reg; reg = reg->next) {
        StyledObject *obj = nullptr;
        if(reg->scope == richmath_System_Document) {
          obj = doc;
        }
        else if(Box *sel = doc->selection_box()) {
          if(reg->scope == richmath_System_Section)
            obj = sel->find_parent<Section>(true);
          else
            obj = sel;
        }
        
        if(obj) {
          StyleOptionName key = Style::get_key(reg->lhs);
          StyleType type = Style::get_type(key);
          if(type == StyleType::Number) {
            float val = obj->get_style((FloatStyleOptionName)key, NAN);
            Array<float> values;
            if(!std::isnan(val) && reg->collect_float_values(values, menu)) {
              float rel_idx = NumericMenuItemRegion::interpolation_index(values, val, true);
              if(0 <= rel_idx && rel_idx <= values.length() - 1) {
                int slider_pos = (int)round(rel_idx * 100);
                SendMessageW(reg->slider, TBM_SETPOS, TRUE, (LPARAM)slider_pos);
              }
            }
          }
        }
      }
    }
  }
}

void StaticMenuOverride::on_create(HWND hwnd) {
  if(Win32Themes::SetWindowTheme && Win32Menu::use_dark_mode)
    Win32Themes::SetWindowTheme(hwnd, L"DarkMode", nullptr);
}

void StaticMenuOverride::on_ncdestroy(HWND hwnd) {
  HMENU menu = popup_window_to_menu[hwnd];
  popup_window_to_menu.remove(hwnd);
  menu_to_popup_window.remove(menu);
  
  if(auto regions = popup_window_regions[hwnd]) {
    popup_window_regions.remove(hwnd);
    
    for(auto reg = regions; reg; reg = reg->next)
      reg->slider = nullptr; // already freed by parent window
    
    regions->delete_all();
  }
}

LRESULT StaticMenuOverride::on_find_menuwindow_from_point(HWND hwnd, WPARAM wParam, LPARAM lParam) {
  POINT pos { (short)LOWORD(lParam), (short)HIWORD(lParam) };
  POINT localpos = pos;
  ScreenToClient(hwnd, &localpos);
  
//  pmath_debug_print("[menu find from point %d,%d = %d,%d wparam=%p]", 
//    pos.x, pos.y, localpos.x, localpos.y, 
//    wParam);
  
  if(auto regs = popup_window_regions[hwnd]) {
    for(auto reg = regs; reg; reg = reg->next) {
      RECT slider_rect;
      if(reg->slider && GetWindowRect(reg->slider, &slider_rect)) {
        if(PtInRect(&slider_rect, pos)) {
          if(wParam)
            *(DWORD*)wParam = (DWORD)(-1);
          //pmath_debug_print("[-> slider %p, return %p @wparam=-1]\n", reg->slider, hwnd);
          return (LRESULT)hwnd;
        }
      }
    }
  }
  
  LRESULT result = default_wnd_proc(hwnd, MN_FINDMENUWINDOWFROMPOINT, wParam, lParam);
  //pmath_debug_print("[-> 0x%x, @wparam=0x%x]\n", result, wParam ? *(unsigned*)wParam : 0);
  return result;
}

LRESULT StaticMenuOverride::on_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  HMENU menu = popup_window_to_menu[hwnd];
  if(!menu && initializing_popup_menu) {
//    if(menu_to_popup_window[initializing_popup_menu]) {
//      pmath_debug_print("[menu %p is already using window %p ?!?]\n", initializing_popup_menu, hwnd);
//    }
    
    menu = initializing_popup_menu;
    initializing_popup_menu = nullptr;
    on_menu_connected(hwnd, menu);
    
//    HMENU hm = (HMENU)default_wnd_proc(hwnd, MN_GETHMENU, 0, 0);
//    pmath_debug_print("[hwnd %p MN_GETHMENU -> %p]\n", hwnd, hm);
  }
//  if(msg != WM_TIMER && msg != MN_SELECTITEM)
//    pmath_debug_print("[menu hwnd=%p menu=%p msg=0x%x, w=%x, l=%x]\n", hwnd, menu, msg, wParam, lParam);
  
  switch(msg) {
    case WM_CREATE:    on_create(hwnd); break;
    case WM_NCDESTROY: on_ncdestroy(hwnd); break;
    case WM_WINDOWPOSCHANGED: 
      NumericMenuItemRegion::update_all_slider_rects(popup_window_regions[hwnd], hwnd, menu);
      break;
    
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
  if(expr[0] != richmath_FE_KeyEvent || expr.expr_length() != 2)
    return false;
    
  Expr modifiers = expr[2];
  if(modifiers[0] != richmath_System_List)
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
  if(expr[0] != richmath_System_List)
    return nullptr;
    
  Array<ACCEL> accel(expr.expr_length());
  int j = 0;
  
  for(size_t i = 1; i <= expr.expr_length(); ++i) {
    Expr item(expr[i]);
    Expr cmd( item[2]);
    
    if( item[0] == richmath_System_MenuItem &&
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
