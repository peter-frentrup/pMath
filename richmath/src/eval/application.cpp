#ifdef RICHMATH_USE_WIN32_GUI
#define _WIN32_WINNT 0x0600
#endif

#define __STDC_LIMIT_MACROS


#include <eval/application.h>

#include <cmath>
#include <stdint.h>
#include <cstdio>

#include <boxes/graphics/graphicsbox.h>
#include <boxes/section.h>
#include <boxes/mathsequence.h>

#include <graphics/config-shaper.h>

#ifdef RICHMATH_USE_WIN32_GUI
#  include <gui/win32/basic-win32-widget.h>
#  include <gui/win32/win32-colordialog.h>
#  include <gui/win32/win32-document-window.h>
#  include <gui/win32/win32-filedialog.h>
#  include <gui/win32/win32-fontdialog.h>
#  include <gui/win32/win32-menu.h>
#endif

#ifdef RICHMATH_USE_GTK_GUI
#  include <gui/gtk/mgtk-colordialog.h>
#  include <gui/gtk/mgtk-filedialog.h>
#  include <gui/gtk/mgtk-fontdialog.h>
#  include <gui/gtk/mgtk-document-window.h>
#endif


#ifdef PMATH_OS_WIN32
#  include <windows.h>
#else
#  include <pthread.h>
#endif


#include <gui/document.h>

#include <eval/binding.h>
#include <eval/dynamic.h>
#include <eval/job.h>
#include <eval/server.h>

#include <util/concurrent-queue.h>


#ifdef RICHMATH_USE_WIN32_GUI

#  define WM_CLIENTNOTIFY  (WM_USER + 1)
#  define WM_ADDJOB        (WM_USER + 2)

#  define TID_DYNAMIC_UPDATE  1

#endif


using namespace richmath;

namespace {
  class ClientNotificationData {
    public:
      ClientNotificationData(): finished(0), result_ptr(0) {}
      
      void done() {
        if(finished) {
          result_ptr = nullptr;
          pmath_atomic_write_release(finished, 1);
          pmath_thread_wakeup(notify_queue.get());
        }
      }
      
    public:
      ClientNotification  type;
      Expr                    data;
      Expr                    notify_queue;
      pmath_atomic_t         *finished;
      
      pmath_t                *result_ptr;
  };
  
  class Session: public Shareable {
    public:
      explicit Session(SharedPtr<Session> _next)
        : Shareable(),
          next(_next)
      {
        SET_BASE_DEBUG_TAG(typeid(*this).name());
      }
      
    public:
      ConcurrentQueue< SharedPtr<Job> > jobs;
      SharedPtr<Job>                    current_job;
      
      SharedPtr<Session> next;
  };
}

enum ClientState {
  Starting = 0,
  Running = 1,
  Quitting = 2
};

static pmath_atomic_t state = { Starting }; // ClientState

static ConcurrentQueue<ClientNotificationData> notifications;

static SharedPtr<Session> session = new Session(0);

static Hashtable<Expr, bool              ( *)(Expr)>                  menu_commands;
static Hashtable<Expr, MenuCommandStatus ( *)(Expr)>                  menu_command_testers;
static Hashtable<Expr, Expr              ( *)(FrontEndObject*, Expr)> currentvalue_providers;

static Hashtable<int, Void, cast_hash> pending_dynamic_updates;
static bool dynamic_update_delay = false;
static bool dynamic_update_delay_timer_active = false;
static double last_dynamic_evaluation = 0.0;

static bool is_executing_for_sth = false;

static void execute(ClientNotificationData &cn);

static pmath_atomic_t     print_pos_lock = PMATH_ATOMIC_STATIC_INIT;
static EvaluationPosition print_pos;

static EvaluationPosition old_job;
static Expr               main_message_queue;
static double total_time_waited_for_gui = 0.0;

// also a GSourceFunc, must return 0
static int on_client_notify(void *data) {
  ClientNotificationData cn;
  
  if(notifications.get(&cn) && session)
    execute(cn);
    
  return 0;
}

// also a GSourceFunc, must return 0
static int on_add_job(void *data) {
  if(session && !session->current_job) {
    while(session->jobs.get(&session->current_job)) {
      if(session->current_job) {
        pmath_atomic_lock(&print_pos_lock);
        {
          old_job = print_pos;
        }
        pmath_atomic_unlock(&print_pos_lock);
        
        if(session->current_job->start()) {
          pmath_atomic_lock(&print_pos_lock);
          {
            print_pos = session->current_job->position();
          }
          pmath_atomic_unlock(&print_pos_lock);
          
          break;
        }
      }
      
      session->current_job = 0;
    }
  }
  
  return 0;
}

// also a GSourceFunc, must return 0
static int on_dynamic_update_delay_timeout(void *data) {
  dynamic_update_delay_timer_active = false;
  
  if(!dynamic_update_delay)
    Application::delay_dynamic_updates(false);
    
  return 0;
}

#ifdef RICHMATH_USE_WIN32_GUI
static HWND hwnd_message = HWND_MESSAGE;

class ClientInfoWindow: public BasicWin32Widget {
  public:
    ClientInfoWindow()
      : BasicWin32Widget(0, 0, 0, 0, 0, 0, &hwnd_message)
    {
      // total exception!!! normally not callable in constructor, but we do not
      // subclass this class, so this will still happen after the object is
      // fully initialized
      init();
    }
    
  protected:
    virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override {
      if(!initializing()) {
        switch(message) {
          case WM_CLIENTNOTIFY:
            on_client_notify(0);
            return 0;
            
          case WM_ADDJOB:
            on_add_job(0);
            return 0;
            
          case WM_TIMER:
            on_dynamic_update_delay_timeout(nullptr);
            KillTimer(_hwnd, TID_DYNAMIC_UPDATE);
            return 0;
        }
      }
      
      return BasicWin32Widget::callback(message, wParam, lParam);
    }
};

static ClientInfoWindow info_window;
#endif


#ifdef PMATH_OS_WIN32
static DWORD main_thread_id = 0;
#else
static pthread_t main_thread = 0;
#endif


double Application::edit_interrupt_timeout      = 2.0;
double Application::interrupt_timeout           = 0.3;
double Application::button_timeout              = 4.0;
double Application::dynamic_timeout             = 4.0;
double Application::min_dynamic_update_interval = 0.05;
String Application::application_filename;
String Application::application_directory;

Hashtable<Expr, Expr, object_hash> Application::eval_cache;

void Application::notify(ClientNotification type, Expr data) {
  if(pmath_atomic_read_aquire(&state) == Quitting)
    return;
    
  ClientNotificationData cn;
  cn.type = type;
  cn.data = data;
  
  notifications.put(cn);
  pmath_thread_wakeup(main_message_queue.get());
  
#ifdef RICHMATH_USE_WIN32_GUI
  PostMessage(info_window.hwnd(), WM_CLIENTNOTIFY, 0, 0);
#endif
  
#ifdef RICHMATH_USE_GTK_GUI
  g_idle_add_full(G_PRIORITY_DEFAULT, on_client_notify, nullptr, nullptr);
#endif
}

Expr Application::notify_wait(ClientNotification type, Expr data) {
  if(pmath_atomic_read_aquire(&state) != Running)
    return Symbol(PMATH_SYMBOL_FAILED);
    
  pmath_t result = PMATH_UNDEFINED;
  
  if(
#ifdef PMATH_OS_WIN32
    GetCurrentThreadId() == main_thread_id
#else
    pthread_equal(pthread_self(), main_thread)
#endif
  ) {
    notify(type, data);
    return Symbol(PMATH_SYMBOL_FAILED);
    
//    ClientNotificationData cn;
//    cn.type = type;
//    cn.data = data;
//    cn.result_ptr = &result;
//    execute(cn);
//    return Expr(result);
  }
  
  pmath_atomic_t finished = PMATH_ATOMIC_STATIC_INIT;
  ClientNotificationData cn;
  cn.finished = &finished;
  cn.notify_queue = Expr(pmath_thread_get_queue());
  cn.type = type;
  cn.data = data;
  cn.result_ptr = &result;
  
  notifications.put(cn);
  pmath_thread_wakeup(main_message_queue.get());
  
#ifdef RICHMATH_USE_WIN32_GUI
  PostMessage(info_window.hwnd(), WM_CLIENTNOTIFY, 0, 0);
#endif
  
#ifdef RICHMATH_USE_GTK_GUI
  g_idle_add_full(G_PRIORITY_DEFAULT, on_client_notify, nullptr, nullptr);
#endif
  
  while(!pmath_atomic_read_aquire(&finished)) {
    pmath_thread_sleep();
    pmath_debug_print("w");
  }
  pmath_debug_print("W");
  
  return Expr(result);
}

Expr Application::current_value(Expr item) {
  return current_value(Application::get_evaluation_box(), item);
}

Expr Application::current_value(FrontEndObject *obj, Expr item) {
  auto func = currentvalue_providers[item];
  if(!func && item[0] == PMATH_SYMBOL_LIST)
    func = currentvalue_providers[item[1]];
    
  if(!func)
    return Symbol(PMATH_SYMBOL_FAILED);
    
  return func(obj, item);
}

bool Application::run_recursive_menucommand(Expr cmd) {
  bool (*func)(Expr);
  
  func = menu_commands[cmd];
  if(func && func(cmd))
    return true;
    
  func = menu_commands[cmd[0]];
  if(func && func(cmd))
    return true;
    
  return false;
}

MenuCommandStatus Application::test_menucommand_status(Expr cmd) {
  MenuCommandStatus (*func)(Expr);
  
  if(cmd.is_null())
    return MenuCommandStatus(true);
    
  if(cmd.is_string()) {
    String scmd(cmd);
    
    scmd = scmd.trim();
    
    if(scmd.starts_with("@shaper=")) {
      scmd = scmd.part(sizeof("@shaper=") - 1, -1);
      
      if(!MathShaper::available_shapers.search(scmd))
        return MenuCommandStatus(false);
    }
    
    cmd = scmd;
  }
  
  func = menu_command_testers[cmd];
  if(func) {
    MenuCommandStatus status(func(cmd));
    if(status.enabled)
      return status;
  }
  
  func = menu_command_testers[cmd[0]];
  if(func) {
    MenuCommandStatus status(func(cmd));
    if(status.enabled)
      return status;
  }
  
  return MenuCommandStatus(true);
}

void Application::register_menucommand(
  Expr cmd,
  bool              (*func)(Expr cmd),
  MenuCommandStatus (*test)(Expr cmd)
) {
  if(cmd.is_null()) {
    menu_commands.default_value       = func;
    menu_command_testers.default_value = test;
    return;
  }
  
  if(func)
    menu_commands.set(cmd, func);
  else
    menu_commands.remove(cmd);
    
  if(test)
    menu_command_testers.set(cmd, test);
  else
    menu_command_testers.remove(cmd);
}

void Application::register_currentvalue_provider(
  Expr   item,
  Expr (*func)(FrontEndObject *obj, Expr item))
{
  if(func)
    currentvalue_providers.set(item, func);
  else
    currentvalue_providers.remove(item);
}

static void write_data(void *user, const uint16_t *data, int len) {
  FILE *file = (FILE *)user;
  
#define BUFSIZE 200
  char buf[BUFSIZE];
  while(len > 0) {
    int tmplen = len < BUFSIZE ? len : BUFSIZE;
    char *bufptr = buf;
    while(tmplen-- > 0) {
      if(*data <= 0xFF) {
        *bufptr++ = (unsigned char) * data++;
      }
      else {
        ++data;
        *bufptr++ = '?';
      }
    }
    if(pmath_aborting())
      return;
      
    fwrite(buf, 1, len < BUFSIZE ? len : BUFSIZE, file);
    len -= BUFSIZE;
  }
#undef BUFSIZE
}

void Application::gui_print_section(Expr expr) {
  EvaluationPosition pos;
  
  pmath_atomic_lock(&print_pos_lock);
  {
    pos = print_pos;
  }
  pmath_atomic_unlock(&print_pos_lock);
  
  Document *doc = FrontEndObject::find_cast<Document>(pos.document_id);
  Section *sect = FrontEndObject::find_cast<Section>( pos.section_id);
  
  if(doc && doc->get_own_style(Editable)) {
    int index;
    if(sect && sect->parent() == doc) {
      index = sect->index() + 1;
      
      while(index < doc->count()) {
        Section *s = doc->section(index);
        if(!s || !s->get_style(SectionGenerated))
          break;
          
        if(session && session->current_job && session->current_job->default_graphics_options.is_null()) {
          if(MathSection *msect = dynamic_cast<MathSection *>(s)) {
            if( msect->content()->length() == 1 &&
                msect->content()->count()  == 1)
            {
              if(GraphicsBox *gb = dynamic_cast<GraphicsBox *>(msect->content()->item(0))) {
                session->current_job->default_graphics_options = gb->get_user_options();
              }
            }
          }
        }
        
        if(doc->selection_box() == doc) {
          int start = doc->selection_start();
          int end   = doc->selection_end();
          
          if(start > index) --start;
          if(end > index)   --end;
          
          doc->select(doc, start, end);
        }
        
        doc->remove(index, index + 1);
      }
    }
    else
      index = doc->length();
      
    sect = Section::create_from_object(expr);
    if(sect) {
      String base_style_name;
      
      if(session && session->current_job) {
        if(MathSection *msect = dynamic_cast<MathSection *>(sect)) {
          if( msect->content()->length() == 1 &&
              msect->content()->count()  == 1)
          {
            if(GraphicsBox *gb = dynamic_cast<GraphicsBox *>(msect->content()->item(0))) {
              gb->set_user_default_options(session->current_job->default_graphics_options);
            }
          }
        }
        
        if(sect->style && sect->style->get(BaseStyleName, &base_style_name)) {
          Section *eval_sect = FrontEndObject::find_cast<Section>(session->current_job->position().section_id);
          
          if(eval_sect) {
            Gather g;
            Expr   rules;
            
            SharedPtr<Stylesheet> all   = eval_sect->stylesheet();
            SharedPtr<Style>      style = eval_sect->style;
            
            for(int count = 20; count && style; --count) {
              if(style->get(GeneratedSectionStyles, &rules))
                Gather::emit(rules);
                
              String inherited;
              if(all && style->get(BaseStyleName, &inherited))
                style = all->styles[inherited];
              else
                break;
            }
            
            rules = g.end();
            Expr base_style = Evaluate(
                                Parse(
                                  "Try(Replace(`1`, Flatten(`2`)))",
                                  base_style_name,
                                  rules));
                                  
            if(base_style != PMATH_SYMBOL_FAILED) {
              sect->style->remove(BaseStyleName);
              sect->style->add_pmath(base_style);
            }
          }
        }
      }
      
      doc->insert(index, sect);
      
      pos = EvaluationPosition(sect);
      pmath_atomic_lock(&print_pos_lock);
      {
        print_pos = pos;
      }
      pmath_atomic_unlock(&print_pos_lock);
      
      if(doc->selection_box() == doc) {
        int s = doc->selection_start();
        int e = doc->selection_end();
        
        if(s >= index) ++s;
        if(e >= index) ++e;
        
        doc->select(doc, s, e);
      }
    }
  }
  else {
    if(expr[0] == PMATH_SYMBOL_SECTION) {
      Expr boxes = expr[1];
      if(boxes[0] == PMATH_SYMBOL_BOXDATA)
        boxes = boxes[1];
        
      expr = Call(Symbol(PMATH_SYMBOL_RAWBOXES), boxes);
    }
    
    printf("\n");
    pmath_write(expr.get(), 0, write_data, stdout);
    printf("\n");
  }
}

static void update_control_active(bool value) {
  static bool original_value = false;
  
  if(value == original_value)
    return;
    
  original_value = value;
  if(value) {
    Application::interrupt_wait(
      /*Parse("FE`$ControlActiveSymbol:= True; Print(FE`$ControlActiveSymbol)")*/
      Call(Symbol(PMATH_SYMBOL_ASSIGN),
           GetSymbol(FESymbolIndex::ControlActive),
           Symbol(PMATH_SYMBOL_TRUE)));
  }
  else {
    Application::interrupt_wait(
      /*Parse("FE`$ControlActiveSymbol:= False; SetAttributes($ControlActiveSetting,{}); Print(FE`$ControlActiveSymbol)")*/
      Call(Symbol(PMATH_SYMBOL_EVALUATIONSEQUENCE),
           Call(Symbol(PMATH_SYMBOL_ASSIGN),
                GetSymbol(FESymbolIndex::ControlActive),
                Symbol(PMATH_SYMBOL_FALSE)),
           Call(Symbol(PMATH_SYMBOL_SETATTRIBUTES),
                Symbol(PMATH_SYMBOL_CONTROLACTIVESETTING),
                List())));
  }
}

static Hashtable<int, Void, cast_hash> active_controls;

void Application::activated_control(Box *box) {
  active_controls.set(box->id(), Void());
  
  update_control_active(true);
}

void Application::deactivated_control(Box *box) {
  active_controls.remove(box->id());
  
  if(active_controls.size() == 0)
    update_control_active(false);
}

void Application::deactivated_all_controls() {
  active_controls.clear();
  update_control_active(false);
}

static Expr get_current_value_of_MouseOver(FrontEndObject *obj, Expr item);
static Expr get_current_value_of_Filename(FrontEndObject *obj, Expr item);
static Expr get_current_value_of_ControlFont_data(FrontEndObject *obj, Expr item);

void Application::init() {
  main_message_queue = Expr(pmath_thread_get_queue());
  
#ifdef RICHMATH_USE_WIN32_GUI
  if(!info_window.hwnd())
    PostQuitMessage(1);
#endif
    
#ifdef PMATH_OS_WIN32
  main_thread_id = GetCurrentThreadId();
#else
  main_thread = pthread_self();
#endif
  
  register_currentvalue_provider(String("MouseOver"),         get_current_value_of_MouseOver);
  register_currentvalue_provider(String("Filename"),          get_current_value_of_Filename);
  register_currentvalue_provider(String("ControlFontFamily"), get_current_value_of_ControlFont_data);
  register_currentvalue_provider(String("ControlFontSlant"),  get_current_value_of_ControlFont_data);
  register_currentvalue_provider(String("ControlFontWeight"), get_current_value_of_ControlFont_data);
  register_currentvalue_provider(String("ControlFontSize"),   get_current_value_of_ControlFont_data);
  
  
  application_filename = String(Evaluate(Symbol(PMATH_SYMBOL_APPLICATIONFILENAME)));
  int             i   = application_filename.length() - 1;
  const uint16_t *buf = application_filename.buffer();
  while(i > 0 && buf[i] != '\\' && buf[i] != '/')
    --i;
    
  if(i > 0)
    application_directory = application_filename.part(0, i);
  else
    application_directory = application_filename;
    
  total_time_waited_for_gui = 0.0;
}

void Application::doevents() {
  // ClientState
  intptr_t old_state = pmath_atomic_fetch_set(&state, Running);
  
#ifdef RICHMATH_USE_WIN32_GUI
  {
    MSG msg;
    while(PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if(msg.message == WM_QUIT) {
        PostQuitMessage(0);
        break;
      }
      
      if(Win32AcceleratorTable::main_table.is_valid()) {
        if(!TranslateAcceleratorW(GetFocus(), Win32AcceleratorTable::main_table->haccel(), &msg)) {
          TranslateMessage(&msg);
          DispatchMessageW(&msg);
        }
      }
    }
  }
#endif
  
#ifdef RICHMATH_USE_GTK_GUI
  while(gtk_events_pending())
    gtk_main_iteration();
#endif
    
  pmath_atomic_write_release(&state, old_state);
}

int Application::run() {
  int result = 0;
  
  if(pmath_atomic_read_aquire(&state) == Running)
    return 1;
    
  pmath_atomic_write_release(&state, Running);
  
#ifdef RICHMATH_USE_WIN32_GUI
  {
    MSG msg;
    BOOL bRet;
    while((bRet = GetMessageW(&msg, nullptr, 0, 0)) != 0) {
      if(bRet == -1) {
        pmath_atomic_write_release(&state, Quitting);
        return 1;
      }
      
      if(Win32AcceleratorTable::main_table.is_valid()) {
        if(!TranslateAcceleratorW(GetFocus(), Win32AcceleratorTable::main_table->haccel(), &msg)) {
          TranslateMessage(&msg);
          DispatchMessageW(&msg);
        }
      }
    }
    
    result = msg.wParam;
  }
#endif
  
#ifdef RICHMATH_USE_GTK_GUI
  gtk_main();
#endif
  
  pmath_atomic_write_release(&state, Quitting);
  return result;
}

void Application::done() {
  Server::local_server->async_interrupt(Call(Symbol(PMATH_SYMBOL_ABORT)));
  
  while(session) {
    SharedPtr<Job> job;
    
    while(session->jobs.get(&job)) {
      if(job)
        job->dequeued();
    }
    
    Server::local_server->async_interrupt(Call(Symbol(PMATH_SYMBOL_ABORT)));
    //Server::local_server->abort_all();
    
    session = session->next;
  }
  
  ClientNotificationData cn;
  while(notifications.get(&cn)) {
    cn.done();
//    if(cn.sem)
//      cn.sem->post();
  }
  
  eval_cache.clear();
  menu_commands.clear();
  menu_command_testers.clear();
  currentvalue_providers.clear();
  application_filename = String();
  application_directory = String();
  main_message_queue = Expr();
}

void Application::add_job(SharedPtr<Job> job) {
  if(session && job) {
    session->jobs.put(job);
    job->enqueued();
    
#ifdef RICHMATH_USE_WIN32_GUI
    PostMessage(info_window.hwnd(), WM_ADDJOB, 0, 0);
#endif
    
#ifdef RICHMATH_USE_GTK_GUI
    g_idle_add_full(G_PRIORITY_DEFAULT, on_add_job, nullptr, nullptr);
#endif
  }
}

Box *Application::find_current_job() {
  assert(main_message_queue == Expr(pmath_thread_get_queue()));
  
  Session *s = session.ptr();
  
  while(s && !s->current_job)
    s = s->next.ptr();
    
  if(s) {
    EvaluationPosition pos = s->current_job->position();
    
    Box *box = FrontEndObject::find_cast<Box>(pos.box_id);
    if(box)
      return box;
      
    box = FrontEndObject::find_cast<Box>(pos.section_id);
    if(box)
      return box;
      
    box = FrontEndObject::find_cast<Box>(pos.document_id);
    if(box)
      return box;
  }
  
  return 0;
}

bool Application::remove_job(Box *input_box, bool only_check_possibility) {
  assert(main_message_queue == Expr(pmath_thread_get_queue()));
  
  if(!input_box)
    return false;
    
  ConcurrentQueue< SharedPtr<Job> > tested_jobs;
  SharedPtr<Job> tmp;
  
  bool result = false;
  SharedPtr<Session> s = session;
  while(s) {
    while(s->jobs.get(&tmp)) {
      Box *sect = FrontEndObject::find_cast<Box>(tmp->position().section_id);
      
      if(sect == input_box) {
        if(only_check_possibility) {
          tested_jobs.put_front(tmp);
        }
        else
          tmp->dequeued();
          
        result = true;
        break;
      }
      
      tested_jobs.put_front(tmp);
    }
    
    while(tested_jobs.get(&tmp)) {
      s->jobs.put_front(tmp);
    }
    
    if(result)
      return true;
      
    s = s->next;
  }
  
  return false;
}

void Application::abort_all_jobs() {
  if(session) {
    SharedPtr<Job> job;
    
    while(session->jobs.get(&job)) {
      if(job)
        job->dequeued();
    }
  }
  
  Server::local_server->async_interrupt(Call(Symbol(PMATH_SYMBOL_ABORT)));
  //Server::local_server->abort_all();
}

Box *Application::get_evaluation_box() {
  Box *box = FrontEndObject::find_cast<Box>(Dynamic::current_evaluation_box_id);
  
  if(box)
    return box;
    
  box = Application::find_current_job();
  if(box)
    return box;
    
  EvaluationPosition pos;
  pmath_atomic_lock(&print_pos_lock);
  {
    pos = print_pos;
  }
  pmath_atomic_unlock(&print_pos_lock);
  
  box = FrontEndObject::find_cast<Box>(pos.box_id);
  if(box)
    return box;
    
  box = FrontEndObject::find_cast<Box>(pos.section_id);
  if(box)
    return box;
    
  box = FrontEndObject::find_cast<Box>(pos.document_id);
  if(box)
    return box;
    
  return nullptr;
}

Document *Application::create_document() {
  Document *doc = nullptr;
  
#ifdef RICHMATH_USE_WIN32_GUI
  {
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int w = 500;
    int h = 550;
    
    doc = get_current_document();
    if(doc) {
      auto wid = dynamic_cast<Win32Widget*>(doc->native());
      if(wid) {
        HWND hwnd = wid->hwnd();
        while(GetParent(hwnd) != nullptr)
          hwnd = GetParent(hwnd);
          
        int dx = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CXSIZEFRAME);
        int dy = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYSIZEFRAME);
        
        RECT rect;
        if(GetWindowRect(hwnd, &rect)) {
          x = rect.left + dx;
          y = rect.top  + dy;
        }
        
        MONITORINFO monitor_info;
        memset(&monitor_info, 0, sizeof(monitor_info));
        monitor_info.cbSize = sizeof(monitor_info);
        
        HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if(GetMonitorInfo(hmon, &monitor_info)) {
        
          if(y + h > monitor_info.rcWork.bottom) {
            y = monitor_info.rcWork.top;
          }
          
          if(x + w > monitor_info.rcWork.right) {
            x = monitor_info.rcWork.left;
            y = monitor_info.rcWork.top;
          }
        }
      }
    }
    
    Win32DocumentWindow *wnd = new Win32DocumentWindow(
      new Document,
      0, WS_OVERLAPPEDWINDOW,
      x,
      y,
      w,
      h);
    wnd->init();
    
    doc = wnd->document();
  }
#endif
  
#ifdef RICHMATH_USE_GTK_GUI
  {
    MathGtkDocumentWindow *wnd = new MathGtkDocumentWindow();
    wnd->init();
    
    doc = wnd->document();
  }
#endif
  
  doc->style->set(StyleDefinitions, String("Default.pmathdoc"));
  return doc;
}

Document *Application::create_document(Expr data) {
  // CreateDocument({sections...}, options...)
  
  // TODO: respect window-related options (WindowTitle...)
  
  Document *doc = Application::create_document();
  
  if(!doc)
    return 0;
    
  if(data.expr_length() >= 1) {
    Expr options(pmath_options_extract(data.get(), 1));
    if(options.is_expr())
      doc->style->add_pmath(options);
      
    Expr sections = data[1];
    if(sections[0] != PMATH_SYMBOL_LIST)
      sections = List(sections);
      
    for(size_t i = 1; i <= sections.expr_length(); ++i) {
      Expr item = sections[i];
      
      if( item[0] != PMATH_SYMBOL_SECTION      &&
          item[0] != PMATH_SYMBOL_SECTIONGROUP)
      {
        item = Call(Symbol(PMATH_SYMBOL_SECTION),
                    Call(Symbol(PMATH_SYMBOL_BOXDATA),
                         Application::interrupt_wait(Call(Symbol(PMATH_SYMBOL_TOBOXES), item))),
                    String("Input"));
      }
      
      int pos = doc->length();
      doc->insert_pmath(&pos, item);
    }
  }
  
  if(!doc->selectable())
    doc->select(0, 0, 0);
    
  doc->invalidate_options();
  
  return doc;
}

Expr Application::run_filedialog(Expr data) {
// FE`FileOpenDialogSymbol("initialfile", {"filter1" -> {"*.ext1", ...}, ...}, WindowTitle -> ....)
// FE`FileSaveDialogSymbol("initialfile", {"filter1" -> {"*.ext1", ...}, ...}, WindowTitle -> ....)
  String title;
  String filename;
  Expr   filter;
  
  Expr head = data[0];
  
  size_t argi = 1;
  if(data[argi].is_string()) {
    filename = String(data[argi]);
    ++argi;
  }
  
  if(data[argi][0] == PMATH_SYMBOL_LIST) {
    filter = data[argi];
    ++argi;
  }
  
  if(data.is_expr()) {
    Expr options(pmath_options_extract(data.get(), argi - 1));
    
    if(options.is_valid()) {
      Expr title_value(pmath_option_value(
                         head.get(),
                         PMATH_SYMBOL_WINDOWTITLE,
                         options.get()));
                         
      if(title_value.is_string())
        title = String(title_value);
    }
  }
  
  Expr result = Symbol(PMATH_SYMBOL_FAILED);
  double gui_start_time = pmath_tickcount();
  
  pmath_debug_print("run_filedialog...\n");
  
#if RICHMATH_USE_WIN32_GUI
  Win32FileDialog
#elif RICHMATH_USE_GTK_GUI
  MathGtkFileDialog
#else
#  error "No GUI"
#endif
  dialog(head == GetSymbol(FESymbolIndex::FileSaveDialog));
  
  pmath_debug_print("  set_title...\n");
  dialog.set_title(title);
  pmath_debug_print("  set_initial_file...\n");
  dialog.set_initial_file(filename);
  pmath_debug_print("  set_filter...\n");
  dialog.set_filter(filter);
  pmath_debug_print("  show_dialog...\n");
  result = dialog.show_dialog();
  pmath_debug_print("...run_filedialog\n");
  
  double gui_end_time = pmath_tickcount();
  if(gui_start_time < gui_end_time)
    total_time_waited_for_gui += gui_end_time - gui_start_time;
    
  return result;
}

bool Application::is_idle() {
  return !is_executing_for_sth && !session->current_job.is_valid();
}

bool Application::is_running_job_for(Box *box) {
  if(!box)
    return false;
    
  Section *sect = box->find_parent<Section>(true);
  
  SharedPtr<Session> s = session;
  if(sect) {
    while(s) {
      SharedPtr<Job> job = s->current_job;
      if(job && job->position().section_id == box->id())
        return true;
        
      s = s->next;
    }
    
    return false;
  }
  
  while(s) {
    SharedPtr<Job> job = s->current_job;
    if(job && job->position().document_id == box->id())
      return true;
      
    s = s->next;
  }
  
  return false;
}

void Application::async_interrupt(Expr expr) {
  Server::local_server->async_interrupt(expr);
}

static pmath_bool_t interrupt_wait_idle(double *end_tick, void *data) {
  double gui_start_time = total_time_waited_for_gui;
  
  ConcurrentQueue<ClientNotificationData> *suppressed_notifications;
  suppressed_notifications = (ConcurrentQueue<ClientNotificationData> *)data;
  
  ClientNotificationData cn;
  while(notifications.get(&cn)) {
//    if(cn.type == ClientNotification::End || cn.type == ClientNotification::EndSession) {
//      //notifications.put_front(cn);
//      //break;
//
//      suppressed_notifications->put_front(cn);
//      continue;
//    }

    /* We must filter out ClientNotification::DynamicUpdate because that could update a parent
       DynamicBox of the DynamicBox that is currently updated during its
       paint() event. That would cause a memory corruption/crash.
     */
    if( cn.type == ClientNotification::DynamicUpdate) {
      suppressed_notifications->put_front(cn);
      continue;
    }
    
    execute(cn);
  }
  
  double gui_end_time = total_time_waited_for_gui;
  if(gui_start_time < gui_end_time) {
    pmath_debug_print("[interrupt_wait_idle: delay timeout by %f sec]\n", gui_end_time - gui_start_time);
    *end_tick += gui_end_time - gui_start_time;
  }
  
  return FALSE; // not busy
}

Expr Application::interrupt_wait(Expr expr, double seconds) {
  ConcurrentQueue<ClientNotificationData>  suppressed_notifications;
  
  last_dynamic_evaluation = pmath_tickcount();
  
  Expr result = Server::local_server->interrupt_wait(expr, seconds, interrupt_wait_idle, &suppressed_notifications);
  ClientNotificationData cn;
  while(suppressed_notifications.get(&cn)) {
    notifications.put_front(cn);
  }
  
  return result;
}

Expr Application::interrupt_wait(Expr expr) {
  return interrupt_wait(expr, interrupt_timeout);
}

Expr Application::interrupt_wait_cached(Expr expr, double seconds) {
  if(!expr.is_pointer_of(PMATH_TYPE_SYMBOL | PMATH_TYPE_EXPRESSION))
    return expr;
    
  Expr *cached = eval_cache.search(expr);
  if(cached)
    return *cached;
    
  Expr result = interrupt_wait(expr, seconds);
  if( result != PMATH_SYMBOL_ABORTED &&
      result != PMATH_SYMBOL_FAILED)
  {
    eval_cache.set(expr, result);
  }
  else if(result == PMATH_SYMBOL_ABORTED) {
#ifdef RICHMATH_USE_WIN32_GUI
    MessageBeep(-1);
#endif
    
#ifdef RICHMATH_USE_GTK_GUI
    gdk_beep();
#endif
  }
  
  return result;
}

Expr Application::interrupt_wait_cached(Expr expr) {
  return interrupt_wait_cached(expr, interrupt_timeout);
}

Expr Application::interrupt_wait_for(Expr expr, Box *box, double seconds) {
  EvaluationPosition pos(box);
  
  expr = Call(
           GetSymbol(FESymbolIndex::InternalExecuteFor),
           expr,
           pos.document_id,
           pos.section_id,
           pos.box_id);
           
  bool old_is_executing_for_sth = is_executing_for_sth;
  is_executing_for_sth = true;
  
  Expr result = interrupt_wait(expr, seconds);
  
  is_executing_for_sth = old_is_executing_for_sth;
  return result;
}

Expr Application::interrupt_wait_for(Expr expr, Box *box) {
  return interrupt_wait_for(expr, box, interrupt_timeout);
}

Expr Application::internal_execute_for(Expr expr, int doc, int sect, int box) {
  EvaluationPosition old_print_pos;
  
  pmath_atomic_lock(&print_pos_lock);
  {
    old_print_pos = print_pos;
    print_pos.document_id = doc;
    print_pos.section_id  = sect;
    print_pos.box_id      = box;
  }
  pmath_atomic_unlock(&print_pos_lock);
  
  expr = Evaluate(expr);
  
  pmath_atomic_lock(&print_pos_lock);
  {
    print_pos = old_print_pos;
  }
  pmath_atomic_unlock(&print_pos_lock);
  
  return expr;
}

void Application::delay_dynamic_updates(bool delay) {
  assert(main_message_queue == Expr(pmath_thread_get_queue()));
  
  dynamic_update_delay = delay;
  
  if(!delay) {
    decltype(pending_dynamic_updates)  old_pending;
    swap(pending_dynamic_updates, old_pending);
    for(auto &e : old_pending.entries()) {
      auto feo = FrontEndObject::find(e.key);
      if(feo)
        feo->dynamic_updated();
    }
  }
}

static void cnt_startsession() {
  if(session->current_job) {
    Section *sect = FrontEndObject::find_cast<Section>(
                      session->current_job->position().section_id);
                      
    if(sect) {
      sect->dialog_start = true;
      sect->request_repaint_all();
    }
  }
  
  session = new Session(session);
}

static void cnt_endsession() {
  SharedPtr<Job> job;
  while(session->jobs.get(&job)) {
    if(job)
      job->dequeued();
  }
  
  if(session->next) {
    session = session->next;
  }
  
  if(session->current_job) {
    Section *sect = FrontEndObject::find_cast<Section>(
                      session->current_job->position().section_id);
                      
    if(sect) {
      sect->dialog_start = false;
      sect->request_repaint_all();
    }
  }
}

static void cnt_end(Expr data) {
  SharedPtr<Job> job = session->current_job;
  session->current_job = 0;
  
  if(job) {
    job->end();
    job->dequeued();
    
    {
      EvaluationPosition pos;
      pmath_atomic_lock(&print_pos_lock);
      {
        pos = print_pos;
      }
      pmath_atomic_unlock(&print_pos_lock);
      
      Document *doc = FrontEndObject::find_cast<Document>(pos.document_id);
      Section *sect = FrontEndObject::find_cast<Section>( pos.section_id);
      
      if(doc) {
        if(sect && sect->parent() == doc) {
          int index = sect->index() + 1;
          
          while(index < doc->count()) {
            Section *s = doc->section(index);
            if(!s || !s->get_style(SectionGenerated))
              break;
              
            doc->remove(index, index + 1);
            if(doc->selection_box() == doc
                && doc->selection_start() > index) {
              doc->select(doc,
                          doc->selection_start() - 1,
                          doc->selection_end()  - 1);
            }
          }
        }
      }
    }
    
    pmath_atomic_lock(&print_pos_lock);
    {
      print_pos = old_job;
    }
    pmath_atomic_unlock(&print_pos_lock);
  }
  
  bool more = false;
  if(data == PMATH_SYMBOL_ABORTED) {
    while(session->jobs.get(&job)) {
      if(job) {
        job->end();
        job->dequeued();
      }
    }
  }
  
  while(session->jobs.get(&session->current_job)) {
    if(session->current_job) {
      pmath_atomic_lock(&print_pos_lock);
      {
        old_job = print_pos;
      }
      pmath_atomic_unlock(&print_pos_lock);
      
      if(session->current_job->start()) {
        pmath_atomic_lock(&print_pos_lock);
        {
          print_pos = session->current_job->position();
        }
        pmath_atomic_unlock(&print_pos_lock);
        
        more = true;
        break;
      }
    }
    
    session->current_job = 0;
  }
  
  if(!more) {
    for(auto &e : all_document_ids.entries()) {
      Document *doc = FrontEndObject::find_cast<Document>(e.key);
      
      assert(doc);
      
      for(int s = 0; s < doc->count(); ++s) {
        auto math = dynamic_cast<MathSection*>(doc->section(s));
        if(math && math->get_style(ShowAutoStyles)) {
          math->invalidate();
        }
      }
    }
  }
  
  Application::eval_cache.clear();
}

static void cnt_return(Expr data) {
  if(session->current_job) {
    session->current_job->returned(data);
  }
}

static void cnt_returnbox(Expr data) {
  if(session->current_job) {
    session->current_job->returned_boxes(data);
  }
}

static void cnt_printsection(Expr data) {
  Application::gui_print_section(data);
}

static Expr cnt_getdocuments() {
  Gather gather;
  
  for(auto &e : all_document_ids.entries())
    Gather::emit(Call(Symbol(PMATH_SYMBOL_FRONTENDOBJECT), e.key));
    
  return gather.end();
}

static void cnt_menucommand(Expr data) {
  Application::run_recursive_menucommand(data);
}

static void cnt_addconfigshaper(Expr data) {
  SharedPtr<ConfigShaper> shaper = ConfigShaper::try_register(data);
  
  if(shaper) {
    MathShaper::available_shapers.set(shaper->name(), shaper);
    
    pmath_debug_print_object("loaded ", shaper->name().get(), "\n");
  }
  else {
    pmath_debug_print("adding config shaper failed.\n");
  }
}

static Expr cnt_getoptions(Expr data) {
  Box *box = FrontEndObject::find_cast<Box>(data);
  
  if(box) {
    Gather gather;
    
    if(box->style)
      box->style->emit_to_pmath(true);
      
    Expr options = gather.end();
    if(!box->to_pmath_symbol().is_symbol())
      return options;
      
    Expr default_options =
      Call(Symbol(PMATH_SYMBOL_UNION),
           options,
           Call(Symbol(PMATH_SYMBOL_FILTERRULES),
                Call(Symbol(PMATH_SYMBOL_OPTIONS), box->to_pmath_symbol()),
                Call(Symbol(PMATH_SYMBOL_EXCEPT),
                     options)));
                     
    default_options = Expr(pmath_evaluate(default_options.release()));
    return default_options;
  }
  
  return Symbol(PMATH_SYMBOL_FAILED);
}

static Expr cnt_setoptions(Expr data) {
  Box *box = FrontEndObject::find_cast<Box>(data[1]);
  
  if(box) {
    Expr options = Expr(pmath_expr_get_item_range(data.get(), 2, SIZE_MAX));
    options.set(0, Symbol(PMATH_SYMBOL_LIST));
    
    if(!box->style)
      box->style = new Style();
      
    box->style->add_pmath(options);
    box->invalidate_options();
    
    return options;
  }
  
  return Symbol(PMATH_SYMBOL_FAILED);
}

static void cnt_dynamicupate(Expr data) {

  double now = pmath_tickcount();
  double next_eval = last_dynamic_evaluation + Application::min_dynamic_update_interval - now;
  bool need_timer = (next_eval > 0);
  
  if(need_timer || dynamic_update_delay) {
    for(size_t i = data.expr_length(); i > 0; --i) {
      Expr id_obj = data[i];
      
      if(!id_obj.is_int32())
        continue;
        
      auto obj = FrontEndObject::find(PMATH_AS_INT32(id_obj.get()));
      if(obj)
        pending_dynamic_updates.set(obj->id(), Void());
    }
    
    if(need_timer && !dynamic_update_delay_timer_active) {
      int milliseconds = (int)(next_eval * 1000);
      
#ifdef RICHMATH_USE_WIN32_GUI
      if(SetTimer(info_window.hwnd(), TID_DYNAMIC_UPDATE, milliseconds, 0))
        dynamic_update_delay_timer_active = true;
#endif
        
#ifdef RICHMATH_USE_GTK_GUI
      if(g_timeout_add_full(G_PRIORITY_DEFAULT, milliseconds, on_dynamic_update_delay_timeout, nullptr, nullptr))
        dynamic_update_delay_timer_active = true;
#endif
    }
    
    return;
  }
  
  for(size_t i = data.expr_length(); i > 0; --i) {
    Expr id_obj = data[i];
    
    if(!id_obj.is_int32())
      continue;
      
    auto obj = FrontEndObject::find(PMATH_AS_INT32(id_obj.get()));
    if(obj)
      obj->dynamic_updated();
  }
}

static Expr cnt_createdocument(Expr data) {
  Document *doc = Application::create_document(data);
  
  if(doc) {
    doc->invalidate_options();
    
    return Call(Symbol(PMATH_SYMBOL_FRONTENDOBJECT), doc->id());
  }
  
  return Symbol(PMATH_SYMBOL_FAILED);
}

static Expr cnt_currentvalue(Expr data) {
  Expr item;
  FrontEndObject *obj = nullptr;
  
  if(data.expr_length() == 1) {
    obj = Application::get_evaluation_box();
    item = data[1];
  }
  else if(data.expr_length() == 2) {
    obj = FrontEndObject::find(data[1]);
    item = data[2];
  }
  else
    return Symbol(PMATH_SYMBOL_FAILED);
  
//  Document *doc = box->find_parent<Document>(true);
//  if(item.is_string()) {
//    String item_string { item };
//    
//    if(item_string.equals("MousePosition")){
//      if(box && doc){
//        if(!box->style)
//          box->style = new Style();
//
//        box->style->set(InternalUsesCurrentValueOfMousePosition, true);
//
//        MouseEvent ev;
//        if(doc->native()->cursor_position(&ev.x, &ev.y)){
//          ev.set_source(box);
//
//          return List(ev.x, ev.y);
//        }
//      }
//
//      return Symbol(PMATH_SYMBOL_NONE);
//    }
//  }
  
  return Application::current_value(obj, item);
}

static Expr cnt_getevaluationdocument(Expr data) {
  Document *doc = 0;
  Box      *box = Application::get_evaluation_box();
  
  if(box)
    doc = box->find_parent<Document>(true);
    
  if(doc->main_document)
    doc = doc->main_document;
    
  if(doc)
    return Call(Symbol(PMATH_SYMBOL_FRONTENDOBJECT), doc->id());
    
  return Symbol(PMATH_SYMBOL_FAILED);
}

static Expr cnt_documentget(Expr data) {
  Box *box;
  
  if(data.expr_length() == 0)
    box = get_current_document();
  else
    box = FrontEndObject::find_cast<Box>(data[1]);
    
  if(box == 0)
    return Symbol(PMATH_SYMBOL_FAILED);
    
  return box->to_pmath(BoxOutputFlags::Default);
}

static Expr cnt_documentread(Expr data) {
  Document *doc = 0;
  
  if(data.expr_length() == 0) {
    doc = get_current_document();
  }
  else {
    Box *box = FrontEndObject::find_cast<Box>(data[1]);
    
    if(box)
      doc = box->find_parent<Document>(true);
  }
  
  if(!doc || !doc->selection_box() || doc->selection_length() == 0)
    return String("");
    
  return doc->selection_box()->to_pmath(BoxOutputFlags::Default,
                                        doc->selection_start(),
                                        doc->selection_end());
}

static Expr cnt_colordialog(Expr data) {
  int initcolor = -1;
  
  if(data.expr_length() >= 1)
    initcolor = pmath_to_color(data[1]);
    
  Expr result = Symbol(PMATH_SYMBOL_FAILED);
  double gui_start_time = pmath_tickcount();
  
#if RICHMATH_USE_WIN32_GUI
  result = Win32ColorDialog::show(initcolor);
#endif
  
#ifdef RICHMATH_USE_GTK_GUI
  result = MathGtkColorDialog::show(initcolor);
#endif
  
  double gui_end_time = pmath_tickcount();
  if(gui_start_time < gui_end_time)
    total_time_waited_for_gui += gui_end_time - gui_start_time;
    
  return result;
}

static Expr cnt_fontdialog(Expr data) {
  SharedPtr<Style> initial_style;
  
  if(data.expr_length() > 0)
    initial_style = new Style(data);
    
  Expr result = Symbol(PMATH_SYMBOL_FAILED);
  double gui_start_time = pmath_tickcount();
  
#if RICHMATH_USE_WIN32_GUI
  result = Win32FontDialog::show(initial_style);
#endif
  
#ifdef RICHMATH_USE_GTK_GUI
  result = MathGtkFontDialog::show(initial_style);
#endif
  
  double gui_end_time = pmath_tickcount();
  if(gui_start_time < gui_end_time)
    total_time_waited_for_gui += gui_end_time - gui_start_time;
    
  return result;
}

namespace {
  class SaveOperation {
    private:
      static String section_to_string(Section *sec) {
        // TODO: convert only the first line to boxes
        Expr boxes = sec->to_pmath(BoxOutputFlags::Default);
        Expr text = Application::interrupt_wait(
                      Parse("FE`BoxesToText(`1`, \"PlainText\")", boxes),
                      Application::edit_interrupt_timeout);
                      
        String str = text.to_string();
        const uint16_t *buf = str.buffer();
        const int len = str.length();
        for(int i = 0; i < len; ++i) {
          if(buf[i] == L'\n')
            return str.part(0, i);
        }
        return str;
      }
      
      static String guess_best_title(Document *doc) {
        assert(doc != nullptr);
        
        for(int i = 0; i < doc->count(); ++i) {
          Section *sec = doc->section(i);
          String stylename = sec->get_style(BaseStyleName);
          if(stylename.equals("Title"))
            return section_to_string(sec);
        }
        
        return String();
      }
      
    public:
      static Expr do_save(Expr data) {
        // data = {document, filename}
        // data = {document, None} = always ask for new file name
        // data = {document}       = use document file name if available, ask otherwise
        // data = {Automatic, ...}
        
        Document *doc = 0;
        
        if(data[1].is_expr()) {
          Box *box = FrontEndObject::find_cast<Box>(data[1]);
          
          if(box)
            doc = box->find_parent<Document>(true);
        }
        else
          doc = get_current_document();
          
        if(!doc)
          return Symbol(PMATH_SYMBOL_FAILED);
          
        Expr filename = data[2];
        
        if(!filename.is_string() && filename != PMATH_SYMBOL_NONE)
          filename = doc->native()->filename();
          
        if(!filename.is_string()) {
          String initialfile = doc->native()->filename();
          
          if(initialfile.is_null()) {
            String title = guess_best_title(doc);
            if(title.length() == 0)
              title = String("untitled");
            initialfile = title + ".pmathdoc";
          }
          
          Expr filter = List(
                          Rule(String("pMath Documents (*.pmathdoc)"), String("*.pmathdoc"))/*,
                    Rule(String("All Files (*.*)"),              String("*.*"))*/);

          filename = Application::run_filedialog(
                       Call(
                         GetSymbol(FESymbolIndex::FileSaveDialog),
                         initialfile,
                         filter));
        }
        
        if(!filename.is_string())
          return Symbol(PMATH_SYMBOL_FAILED);
          
        WriteableTextFile file(Evaluate(Call(Symbol(PMATH_SYMBOL_OPENWRITE), filename)));
        if(!file.is_file())
          return Symbol(PMATH_SYMBOL_FAILED);
          
        Expr nsp(pmath_symbol_get_value(PMATH_SYMBOL_NAMESPACEPATH));
        Expr ns( pmath_symbol_get_value(PMATH_SYMBOL_NAMESPACE));
        Expr boxes = doc->to_pmath(BoxOutputFlags::Default);
        
        file.write("/* pMath Document */\n\n");
        
        pmath_symbol_set_value(PMATH_SYMBOL_NAMESPACEPATH, List(String("System`")).release());
        pmath_symbol_set_value(PMATH_SYMBOL_NAMESPACE,     String("System`").release());
        
        boxes.write_to_file(file, PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLSTR);
        
        pmath_symbol_set_value(PMATH_SYMBOL_NAMESPACEPATH, nsp.release());
        pmath_symbol_set_value(PMATH_SYMBOL_NAMESPACE,     ns.release());
        
        file.close();
        doc->native()->filename(filename);
        doc->native()->on_saved();
        return filename;
      }
  };
}
static void execute(ClientNotificationData &cn) {
  AutoMemorySuspension ams;
  
  switch(cn.type) {
    case ClientNotification::StartSession:
      cnt_startsession();
      break;
      
    case ClientNotification::EndSession:
      cnt_endsession();
      break;
      
    case ClientNotification::End:
      cnt_end(cn.data);
      break;
      
    case ClientNotification::Return:
      cnt_return(cn.data);
      break;
      
    case ClientNotification::ReturnBox:
      cnt_returnbox(cn.data);
      break;
      
    case ClientNotification::PrintSection:
      cnt_printsection(cn.data);
      break;
      
    case ClientNotification::GetDocuments:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_getdocuments().release();
      break;
      
    case ClientNotification::MenuCommand:
      cnt_menucommand(cn.data);
      break;
      
    case ClientNotification::AddConfigShaper:
      cnt_addconfigshaper(cn.data);
      break;
      
    case ClientNotification::GetOptions:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_getoptions(cn.data).release();
      break;
      
    case ClientNotification::SetOptions:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_setoptions(cn.data).release();
      else
        cnt_setoptions(cn.data);
      break;
      
    case ClientNotification::DynamicUpdate:
      cnt_dynamicupate(cn.data);
      break;
      
    case ClientNotification::CreateDocument:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_createdocument(cn.data).release();
      else
        cnt_createdocument(cn.data);
      break;
      
    case ClientNotification::CurrentValue:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_currentvalue(cn.data).release();
      break;
      
    case ClientNotification::GetEvaluationDocument:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_getevaluationdocument(cn.data).release();
      break;
      
    case ClientNotification::DocumentGet:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_documentget(cn.data).release();
      break;
      
    case ClientNotification::DocumentRead:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_documentread(cn.data).release();
      break;
      
    case ClientNotification::ColorDialog:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_colordialog(cn.data).release();
      break;
      
    case ClientNotification::FontDialog:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_fontdialog(cn.data).release();
      break;
      
    case ClientNotification::FileDialog:
      if(cn.result_ptr)
        *cn.result_ptr = Application::run_filedialog(cn.data).release();
      break;
      
    case ClientNotification::Save:
      if(cn.result_ptr)
        *cn.result_ptr = SaveOperation::do_save(cn.data).release();
      else
        SaveOperation::do_save(cn.data);
      break;
  }
  
  cn.done();
}

static Expr get_current_value_of_MouseOver(FrontEndObject *obj, Expr item) {
  Box *box = dynamic_cast<Box*>(obj);
  if(!box)
    return Symbol(PMATH_SYMBOL_FALSE);
    
  Document *doc = box->find_parent<Document>(true);
  if(!doc)
    return Symbol(PMATH_SYMBOL_FALSE);
    
  if(!box->style)
    box->style = new Style();
    
  box->style->set(InternalUsesCurrentValueOfMouseOver, true);
  
  Box *mo = FrontEndObject::find_cast<Box>(doc->mouseover_box_id());
  while(mo && mo != box)
    mo = mo->parent();
    
  if(mo)
    return Symbol(PMATH_SYMBOL_TRUE);
  return Symbol(PMATH_SYMBOL_FALSE);
}

static Expr get_current_value_of_Filename(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return Symbol(PMATH_SYMBOL_FALSE);
    
  String result = doc->native()->filename();
  if(!result.is_valid())
    return Symbol(PMATH_SYMBOL_NONE);
  return result;
}

static Expr get_current_value_of_ControlFont_data(FrontEndObject *obj, Expr item) {
  SharedPtr<Style> style = new Style();
  ControlPainter::std->system_font_style(style.ptr());
  
  String item_string {item};
  if(item_string.equals("ControlsFontFamily")) 
    return style->get_pmath(FontFamilies);
  if(item_string.equals("ControlsFontSlant")) 
    return style->get_pmath(FontSlant);
  if(item_string.equals("ControlsFontWeight")) 
    return style->get_pmath(FontWeight);
  if(item_string.equals("ControlsFontSize")) 
    return style->get_pmath(FontSize);
  
  return Symbol(PMATH_SYMBOL_FAILED);
}


