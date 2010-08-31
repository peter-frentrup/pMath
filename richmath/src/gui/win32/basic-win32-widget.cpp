#include <gui/win32/basic-win32-widget.h>

#include <cstdio>

#include <gui/win32/win32-themes.h>

#include <resources.h>

using namespace richmath;

static ATOM win32_widget_class = 0;
static const WCHAR win32_widget_class_name[] = L"RichmathWin32";

static void add_remove_window(int count){
  static int window_count = 0;
  
  window_count+= count;
  
//  if(window_count <= 0)
//    PostQuitMessage(0);
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
  _hwnd(0),
  _initializing(true),
  init_data(new InitData)
{
  init_window_class();
  add_remove_window(+1);
  
  init_data->style_ex = style_ex;
  init_data->style    = style;
  init_data->x        = x;
  init_data->y        = y;
  init_data->width    = width;
  init_data->height   = height;
  init_data->parent   = parent;
}

void BasicWin32Widget::after_construction(){
  if(!CreateWindowExW(
    init_data->style_ex,
    win32_widget_class_name,
    L"",
    init_data->style,
    init_data->x,
    init_data->y,
    init_data->width,
    init_data->height,
    init_data->parent ? *init_data->parent : 0,
    0,
    GetModuleHandle(0),
    this)
  ){
    fprintf(stderr, "Error Creating Widget\n");
  }
  
  delete init_data;
}

BasicWin32Widget::~BasicWin32Widget(){
  // detach this from window handle:
  SetWindowLongPtr(_hwnd, GWLP_USERDATA, 0);
  
  DestroyWindow(_hwnd);
  add_remove_window(-1);
}

BasicWin32Widget *BasicWin32Widget::parent(){
  HWND p = GetParent(_hwnd);
  
  if(p)
    return (BasicWin32Widget*)GetWindowLongPtrW(p, GWLP_USERDATA);
  
  return 0;
}

BasicWin32Widget *BasicWin32Widget::from_hwnd(HWND hwnd){
  WINDOWINFO info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  
  if(GetWindowInfo(hwnd, &info)
  && info.atomWindowType == win32_widget_class){
    return (BasicWin32Widget*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  }
  
  return 0;
}

LRESULT BasicWin32Widget::callback(UINT message, WPARAM wParam, LPARAM lParam){
  switch(message){
    case WM_CREATE: {
      SetMenu(_hwnd, 0);
    } break;
    
    case WM_CLOSE: {
      delete this;
    } return 0;
    
    case WM_DESTROY: {
      SetWindowLongPtr(_hwnd, GWLP_USERDATA, 0);
      _hwnd = 0;
    } return 0;
  }
  
  return DefWindowProcW(_hwnd, message, wParam, lParam);
}

void BasicWin32Widget::init_window_class(){
  static bool window_class_initialized = false;
  if(window_class_initialized)
    return;
  
  WNDCLASSEXW wincl;
  
  memset(&wincl, 0, sizeof(wincl));
  
  wincl.cbSize = sizeof(wincl);
  wincl.hInstance = GetModuleHandle(0);
  wincl.hIcon     = LoadIcon(NULL, IDI_APPLICATION);
  wincl.hIconSm   = LoadIcon(NULL, IDI_APPLICATION);
  wincl.lpszClassName = win32_widget_class_name;
  wincl.lpfnWndProc = window_proc;
  wincl.style = 0;
  
  win32_widget_class = RegisterClassExW(&wincl);
  if(!win32_widget_class){
    perror("init_window_class() failed\n");
    abort();
  }
  
  window_class_initialized = true;
}

LRESULT CALLBACK BasicWin32Widget::window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
  BasicWin32Widget *widget = (BasicWin32Widget*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  
  if(!widget && message == WM_NCCREATE){
    widget = (BasicWin32Widget*)(((CREATESTRUCT*)lParam)->lpCreateParams);
    
    if(!widget)
      return FALSE;
    
    SetWindowLongPtr(
      hwnd, 
      GWLP_USERDATA, 
      (LONG)widget);
    
    widget->_hwnd = hwnd;
  }
  
  if(widget)
    return widget->callback(message, wParam, lParam);
  
  return DefWindowProcW(hwnd, message, wParam, lParam);
}

//} ... class BasicWin32Widget
