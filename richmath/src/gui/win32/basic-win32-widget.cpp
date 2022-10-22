#include <gui/win32/basic-win32-widget.h>
#include <gui/win32/ole/dataobject.h>
#include <gui/win32/api/win32-highdpi.h>
#include <gui/win32/api/win32-themes.h>
#include <gui/win32/api/win32-touch.h>
#include <gui/win32/api/win32-version.h>
#include <gui/win32/win32-clipboard.h>
#include <boxes/box.h>
#include <resources.h>

#include <cstdio>

#include <shlguid.h>

using namespace richmath;

static ATOM win32_widget_class = 0;
static const wchar_t win32_widget_class_name[] = L"RichmathWin32";


static void add_remove_window(int count) {
  static int global_window_count = 0;
  
  if(global_window_count == 0) {
    HRESULT ole_status = OleInitialize(nullptr);
    
    if(ole_status != S_OK && ole_status != S_FALSE) {
      fprintf(stderr, "OleInitialize failed.\n");
    }
    
    Win32Touch::init();
    
    if(Win32Touch::EnableMouseInPointer)
      Win32Touch::EnableMouseInPointer(TRUE);
    else
      fprintf(stderr, "[no EnableMouseInPointer]\n");
  }
  
  global_window_count += count;
  
  if(global_window_count <= 0) {
    OleUninitialize();
  }
}

//{ class BasicWin32Widget ...

BasicWin32Widget::BasicWin32Widget(
  DWORD style_ex,
  DWORD style,
  int x,
  int y,
  int width,
  int height,
  HWND *parent)
  : ObjectWithLimbo(),
    _hwnd(nullptr),
    _limbo_next(nullptr),
    init_data(new InitData),
    _initializing(true)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  //_drag_source_helper = _drop_target_helper.as<IDragSourceHelper>();
  //CoCreateInstance(
  //  CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER,
  //  _drag_source_helper.iid(),
  //  (void**)_drag_source_helper.get_address_of());
  
  init_window_class();
  add_remove_window(+1);
  
  init_data->style_ex          = style_ex;
  init_data->style             = style;
  init_data->x                 = x;
  init_data->y                 = y;
  init_data->width             = width;
  init_data->height            = height;
  init_data->parent            = parent;
  init_data->window_class_name = nullptr;
  
}

void BasicWin32Widget::set_window_class_name(const wchar_t *static_name) {
  if(init_data)
    init_data->window_class_name = static_name;
}

void BasicWin32Widget::after_construction() {
  const wchar_t *cls_name = init_data->window_class_name;
  if(!cls_name)
    cls_name = win32_widget_class_name;
    
  if(!CreateWindowExW(
        init_data->style_ex,
        cls_name,
        L"",
        init_data->style,
        init_data->x,
        init_data->y,
        init_data->width,
        init_data->height,
        init_data->parent ? *init_data->parent : nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        this) ||
    _hwnd == nullptr)
  {
    fprintf(stderr, "Error Creating Widget\n");
  }
  
  delete init_data;
  init_data = nullptr;
}

BasicWin32Widget::~BasicWin32Widget() {
  if(_hwnd) {
    SetWindowLongPtrW(_hwnd, GWLP_USERDATA, 0);
    WIN32report(DestroyWindow(_hwnd)); 
    _hwnd = nullptr;
  }
  add_remove_window(-1);
}

void BasicWin32Widget::safe_destroy() {
  if(_hwnd) {
    // detach this from window handle:
    SetWindowLongPtrW(_hwnd, GWLP_USERDATA, 0);
  }
  
  ObjectWithLimbo::safe_destroy();
}

//
// IUnknown::AddRef
//
STDMETHODIMP_(ULONG) BasicWin32Widget::AddRef(void) {
  return 1;
}

//
// IUnknown::Release
//
STDMETHODIMP_(ULONG) BasicWin32Widget::Release(void) {
  return 1;
}

//
// IUnknown::QueryInterface
//
STDMETHODIMP BasicWin32Widget::QueryInterface(REFIID iid, void **ppvObject) {
  if(iid == IID_IDropTarget || iid == IID_IUnknown) {
    AddRef();
    *ppvObject = static_cast<IDropTarget *>(this);
    return S_OK;
  }
  
  if(iid == IID_IStylusAsyncPlugin || iid == IID_IStylusPlugin) {
    AddRef();
    *ppvObject = static_cast<IStylusAsyncPlugin *>(this);
    return S_OK;
  }
  
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

BasicWin32Widget *BasicWin32Widget::parent() {
  if(HWND p = GetParent(_hwnd))
    return (BasicWin32Widget *)GetWindowLongPtrW(p, GWLP_USERDATA);
    
  return nullptr;
}

BasicWin32Widget *BasicWin32Widget::from_hwnd(HWND hwnd) {
  DWORD pid = 0;
  
  if(!hwnd ||
      GetCurrentThreadId()  != GetWindowThreadProcessId(hwnd, &pid) ||
      GetCurrentProcessId() != pid)
  {
    return nullptr;
  }
  
  WINDOWINFO info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  
  if(GetWindowInfo(hwnd, &info) && info.atomWindowType == win32_widget_class) {
    return (BasicWin32Widget *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  }
  
  return nullptr;
}

LRESULT BasicWin32Widget::callback(UINT message, WPARAM wParam, LPARAM lParam) {
  switch(message) {
    case WM_CREATE: {
        WIN32report(SetMenu(_hwnd, nullptr));
        RegisterDragDrop(_hwnd, static_cast<IDropTarget *>(this));
      } break;
      
    case WM_CLOSE: {
        on_close();
      } return 0;
      
    case WM_DESTROY: {
        RevokeDragDrop(_hwnd);
        SetWindowLongPtrW(_hwnd, GWLP_USERDATA, 0);
        _hwnd = nullptr;
      } return 0;
  }
  
  return DefWindowProcW(_hwnd, message, wParam, lParam);
}

void BasicWin32Widget::on_close() {
  safe_destroy();
}

void BasicWin32Widget::init_window_class() {
  static bool window_class_initialized = false;
  if(window_class_initialized)
    return;
    
  WNDCLASSEXW wincl;
  
  memset(&wincl, 0, sizeof(wincl));
  
  wincl.cbSize = sizeof(wincl);
  wincl.hInstance = GetModuleHandleW(nullptr);
  wincl.hIcon = LoadIconW(wincl.hInstance, MAKEINTRESOURCEW(ICO_APP_MAIN));
  wincl.hIconSm   = (HICON)LoadImageW(wincl.hInstance, MAKEINTRESOURCEW(ICO_APP_MAIN),
                                     IMAGE_ICON,
                                     GetSystemMetrics(SM_CXSMICON),
                                     GetSystemMetrics(SM_CYSMICON),
                                     LR_DEFAULTCOLOR);
//  wincl.hIcon     = LoadIcon(nullptr, IDI_APPLICATION);
//  wincl.hIconSm   = (HICON)LoadImage(nullptr, IDI_APPLICATION,
//                                     IMAGE_ICON,
//                                     GetSystemMetrics(SM_CXSMICON),
//                                     GetSystemMetrics(SM_CYSMICON),
//                                     LR_DEFAULTCOLOR);
  wincl.lpszClassName = win32_widget_class_name;
  wincl.lpfnWndProc = window_proc;
  wincl.style = 0;//CS_DROPSHADOW;
  
  win32_widget_class = RegisterClassExW(&wincl);
  if(!win32_widget_class) {
    perror("init_window_class() failed\n");
    abort();
  }
  
  window_class_initialized = true;
}

LRESULT CALLBACK BasicWin32Widget::window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  AutoMemorySuspension ams;
  
  BasicWin32Widget *widget = (BasicWin32Widget *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  
  if(!widget && message == WM_NCCREATE) {
    widget = (BasicWin32Widget *)(((CREATESTRUCT *)lParam)->lpCreateParams);
    
    if(!widget)
      return FALSE;
      
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)widget);
    
    widget->_hwnd = hwnd;
  }
  
  if(widget)
    return widget->callback(message, wParam, lParam);
    
  return DefWindowProcW(hwnd, message, wParam, lParam);
}

//} ... class BasicWin32Widget
