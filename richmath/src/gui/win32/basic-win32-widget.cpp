//#define _WIN32_WINNT  0x0501 /* for CS_DROPSHADOW */

#include <gui/win32/basic-win32-widget.h>
#include <gui/win32/win32-themes.h>
#include <boxes/box.h>
#include <resources.h>

#include <cstdio>

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
  : Base(),
    _hwnd(nullptr),
    _allow_drop(true),
    _is_dragging_over(false),
    init_data(new InitData),
    _initializing(true),
    freeThreadedMarshaller(nullptr)
{
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
  
  
  HRESULT hr = CoCreateFreeThreadedMarshaler(
                 static_cast<IStylusSyncPlugin*>(this), 
                 &freeThreadedMarshaller);
  if(FAILED(hr)) {
    fprintf(stderr, "BasicWin32Widget: cannot create free-threaded marshaller for IStylusSyncPlugin");
    freeThreadedMarshaller = nullptr;
  }
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
        init_data->parent ? *init_data->parent : 0,
        0,
        GetModuleHandle(0),
        this) ||
    _hwnd == nullptr)
  {
    fprintf(stderr, "Error Creating Widget\n");
  }
  
  delete init_data;
  init_data = 0;
}

BasicWin32Widget::~BasicWin32Widget() {
  // detach this from window handle:
  SetWindowLongPtr(_hwnd, GWLP_USERDATA, 0);
  
  DestroyWindow(_hwnd);
  add_remove_window(-1);
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
  
  if(iid == IID_IStylusSyncPlugin) {
    AddRef();
    *ppvObject = static_cast<IStylusSyncPlugin *>(this);
    return S_OK;
  }
  
  if((iid == IID_IMarshal) && (freeThreadedMarshaller != NULL)) {
    return freeThreadedMarshaller->QueryInterface(iid, ppvObject);
  }
  
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

//
// IDropTarget::DragEnter
//
STDMETHODIMP BasicWin32Widget::DragEnter(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect) {
  _is_dragging_over = true;
  _allow_drop = is_data_droppable(data_object);
  
  if(_allow_drop) {
    *effect = drop_effect(key_state, pt, *effect);
    SetFocus(_hwnd);
    position_drop_cursor(pt);
  }
  else {
    *effect = DROPEFFECT_NONE;
  }
  
  return S_OK;
}

//
// IDropTarget::DragOver
//
STDMETHODIMP BasicWin32Widget::DragOver(DWORD key_state, POINTL pt, DWORD *effect) {
  if(_allow_drop) {
    *effect = drop_effect(key_state, pt, *effect);
    position_drop_cursor(pt);
  }
  else {
    *effect = DROPEFFECT_NONE;
  }
  
  return S_OK;
}

//
// IDropTarget::DragLeave
//
STDMETHODIMP BasicWin32Widget::DragLeave() {
  _is_dragging_over = false;
  return S_OK;
}

//
// IDropTarget::Drop
//
STDMETHODIMP BasicWin32Widget::Drop(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect) {
  position_drop_cursor(pt);
  
  if(_allow_drop) {
    SetFocus(_hwnd);
    *effect = drop_effect(key_state, pt, *effect);
    
    do_drop_data(data_object, *effect);
  }
  else {
    *effect = DROPEFFECT_NONE;
  }
  
  return S_OK;
}

BasicWin32Widget *BasicWin32Widget::parent() {
  HWND p = GetParent(_hwnd);
  
  if(p)
    return (BasicWin32Widget *)GetWindowLongPtrW(p, GWLP_USERDATA);
    
  return 0;
}

BasicWin32Widget *BasicWin32Widget::from_hwnd(HWND hwnd) {
  DWORD pid = 0;
  
  if(!hwnd ||
      GetCurrentThreadId()  != GetWindowThreadProcessId(hwnd, &pid) ||
      GetCurrentProcessId() != pid)
  {
    return 0;
  }
  
  WINDOWINFO info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  
  if(GetWindowInfo(hwnd, &info) && info.atomWindowType == win32_widget_class) {
    return (BasicWin32Widget *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  }
  
  return 0;
}

LRESULT BasicWin32Widget::callback(UINT message, WPARAM wParam, LPARAM lParam) {
  AutoMemorySuspension ams;
  
  switch(message) {
    case WM_CREATE: {
        SetMenu(_hwnd, 0);
        RegisterDragDrop(_hwnd, static_cast<IDropTarget *>(this));
      } break;
      
    case WM_CLOSE: {
        delete this;
      } return 0;
      
    case WM_DESTROY: {
        RevokeDragDrop(_hwnd);
        SetWindowLongPtr(_hwnd, GWLP_USERDATA, 0);
        _hwnd = 0;
      } return 0;
  }
  
  return DefWindowProcW(_hwnd, message, wParam, lParam);
}

bool BasicWin32Widget::is_data_droppable(IDataObject *data_object) {
  return false;
}

DWORD BasicWin32Widget::drop_effect(DWORD key_state, POINTL pt, DWORD allowed_effects) {
  DWORD effect = 0;
  
  if(key_state & MK_CONTROL) {
    effect = allowed_effects & DROPEFFECT_COPY;
  }
  else if(key_state & MK_SHIFT) {
    effect = allowed_effects & DROPEFFECT_MOVE;
  }
  
  if(effect == 0) {
    if(allowed_effects & DROPEFFECT_COPY) effect = DROPEFFECT_COPY;
    if(allowed_effects & DROPEFFECT_MOVE) effect = DROPEFFECT_MOVE;
  }
  
  return effect;
}

void BasicWin32Widget::do_drop_data(IDataObject *data_object, DWORD effect) {
}

void BasicWin32Widget::position_drop_cursor(POINTL pt) {
}

void BasicWin32Widget::init_window_class() {
  static bool window_class_initialized = false;
  if(window_class_initialized)
    return;
    
  WNDCLASSEXW wincl;
  
  memset(&wincl, 0, sizeof(wincl));
  
  wincl.cbSize = sizeof(wincl);
  wincl.hInstance = GetModuleHandle(0);
  wincl.hIcon = LoadIcon(wincl.hInstance, MAKEINTRESOURCE(ICO_APP_MAIN));
  wincl.hIconSm   = (HICON)LoadImage(wincl.hInstance, MAKEINTRESOURCE(ICO_APP_MAIN),
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
