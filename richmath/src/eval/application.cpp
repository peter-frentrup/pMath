#define __STDC_LIMIT_MACROS


#include <eval/application.h>

#include <cmath>
#include <stdint.h>
#include <cstdio>

#include <boxes/graphics/graphicsbox.h>
#include <boxes/box-factory.h>

#include <graphics/config-shaper.h>

#include <gui/menus.h>
#include <gui/messagebox.h>


#ifdef RICHMATH_USE_WIN32_GUI
#  include <gui/win32/api/win32-highdpi.h>
#  include <gui/win32/basic-win32-widget.h>
#  include <gui/win32/win32-document-window.h>
#  include <gui/win32/win32-filedialog.h>
#  include <gui/win32/menus/win32-menu.h>
#endif

#ifdef RICHMATH_USE_GTK_GUI
#  include <gui/gtk/mgtk-filedialog.h>
#  include <gui/gtk/mgtk-document-window.h>
#endif


#ifdef PMATH_OS_WIN32
#  include <windows.h>
#else
#  include <pthread.h>
#endif


#include <gui/document.h>
#include <gui/documents.h>
#include <gui/recent-documents.h>

#include <eval/binding.h>
#include <eval/current-value.h>
#include <eval/dynamic.h>
#include <eval/eval-contexts.h>
#include <eval/job.h>
#include <eval/server.h>

#include <util/autovaluereset.h>
#include <util/concurrent-queue.h>
#include <util/filesystem.h>


#ifdef RICHMATH_USE_WIN32_GUI

#  define WM_CLIENTNOTIFY  (WM_USER + 1)
#  define WM_ADDJOB        (WM_USER + 2)

#  define TID_DYNAMIC_UPDATE  1

#endif


using namespace richmath;

extern pmath_symbol_t richmath_System_DollarAborted;
extern pmath_symbol_t richmath_System_DollarApplicationFileName;
extern pmath_symbol_t richmath_System_DollarControlActiveSetting;
extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_DollarSessionID;
extern pmath_symbol_t richmath_System_Abort;
extern pmath_symbol_t richmath_System_Assign;
extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_EvaluationSequence;
extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_HoldComplete;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_OpenRead;
extern pmath_symbol_t richmath_System_OpenWrite;
extern pmath_symbol_t richmath_System_RawBoxes;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_SetAttributes;
extern pmath_symbol_t richmath_System_True;
extern pmath_symbol_t richmath_System_WindowTitle;

extern pmath_symbol_t richmath_FE_DollarControlActive;
extern pmath_symbol_t richmath_FE_DollarPaletteSearchPath;
extern pmath_symbol_t richmath_FE_DollarStylesheetDirectory;
extern pmath_symbol_t richmath_FE_BoxesToText;

namespace richmath { namespace strings {
  extern String EmptyString;
  extern String DollarContext_namespace;
  extern String PlainText;
  extern String Text;
}}

namespace {
  class ClientNotificationData {
    public:
      ClientNotificationData(): finished(nullptr), result_ptr(nullptr) {}
      
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

static SharedPtr<Session> session = new Session(nullptr);

static Hashset<FrontEndReference> pending_dynamic_updates;
static bool dynamic_update_delay = false;
static bool dynamic_update_delay_timer_active = false;
static double last_dynamic_evaluation = 0.0;

static bool is_executing_for_sth = false;

static void execute(ClientNotificationData &cn);

static pmath_atomic_t     print_pos_lock = PMATH_ATOMIC_STATIC_INIT;
static EvaluationPosition print_pos;
static FrontEndReference  current_evaluation_object_id;

static EvaluationPosition old_job_print_pos;
static Expr               main_message_queue;
static double total_time_waited_for_gui = 0.0;

static double gui_start_time = 0.0;
static int gui_wait_depth = 0;

AutoGuiWait::AutoGuiWait() {
  RICHMATH_ASSERT(gui_wait_depth >= 0);
  if(gui_wait_depth++ == 0) {
    gui_start_time = pmath_tickcount();
  }
}

AutoGuiWait::~AutoGuiWait() {
  RICHMATH_ASSERT(gui_wait_depth > 0);
  if(--gui_wait_depth == 0) {
    double end_time = pmath_tickcount();
    if(gui_start_time < end_time)
      total_time_waited_for_gui += end_time - gui_start_time;
  }
}

// also a GSourceFunc, must return G_SOURCE_REMOVE (=0)
static int on_client_notify(void *data) {
  ClientNotificationData cn;
  
  if(notifications.get(&cn) && session)
    execute(cn);
    
  return 0;
}

// also a GSourceFunc, must return G_SOURCE_REMOVE (=0)
static int on_add_job(void *data) {
  if(session && !session->current_job) {
    while(session->jobs.get(&session->current_job)) {
      if(session->current_job) {
        pmath_atomic_lock(&print_pos_lock);
        {
          old_job_print_pos = print_pos;
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
      
      session->current_job = nullptr;
    }
  }
  
  return 0;
}

// also a GSourceFunc, must return G_SOURCE_REMOVE (=0)
static int on_dynamic_update_delay_timeout(void *data) {
  dynamic_update_delay_timer_active = false;
  
  if(!dynamic_update_delay)
    Application::delay_dynamic_updates(false);
    
  return 0;
}

#ifdef RICHMATH_USE_WIN32_GUI
static HWND hwnd_message = HWND_MESSAGE;

class ClientInfoWindow final: public BasicWin32Widget {
  public:
    ClientInfoWindow()
      : BasicWin32Widget(0, 0, 0, 0, 0, 0, &hwnd_message)
    {
      init(); // total exception!!! Calling init in consructor is only allowd since this class is final
    }
    
  protected:
    virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam) override {
      if(!initializing()) {
        switch(message) {
          case WM_CLIENTNOTIFY:
            on_client_notify(nullptr);
            return 0;
            
          case WM_ADDJOB:
            on_add_job(nullptr);
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


FrontEndSession *Application::front_end_session = nullptr;
double Application::edit_interrupt_timeout      = 2.0;
double Application::interrupt_timeout           = 0.3;
double Application::button_timeout              = 20.0;
double Application::dynamic_timeout             = 4.0;
double Application::min_dynamic_update_interval = 0.05;
String Application::application_filename;
String Application::application_directory;
String Application::stylesheet_path_base;
Expr Application::palette_search_path;
Expr Application::session_id;
pmath_atomic_uint8_t Application::track_dynamic_update_causes = PMATH_ATOMIC_STATIC_INIT;

Hashtable<Expr, Expr> Application::eval_cache;

bool Application::is_running_on_gui_thread() {
  if(pmath_atomic_read_aquire(&state) == Quitting)
    return false;
  
#ifdef PMATH_OS_WIN32
  return GetCurrentThreadId() == main_thread_id;
#else
  return pthread_equal(pthread_self(), main_thread);
#endif
}

void Application::notify(ClientNotification type, Expr data) {
  if(pmath_atomic_read_aquire(&state) == Quitting)
    return;
    
  ClientNotificationData cn;
  cn.type = type;
  cn.data = std::move(data);
  
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
  if(pmath_atomic_read_aquire(&state) == Quitting)
    return Symbol(richmath_System_DollarFailed);
  
  if(is_running_on_gui_thread()) {
    notify(type, data);
    return Symbol(richmath_System_DollarFailed);
  }  
  
  pmath_t result = PMATH_UNDEFINED;
  pmath_atomic_t finished = PMATH_ATOMIC_STATIC_INIT;
  ClientNotificationData cn;
  cn.finished = &finished;
  cn.notify_queue = Expr(pmath_thread_get_queue());
  cn.type = type;
  cn.data = std::move(data);
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
  
  String ctx;
  if(sect)
    ctx = EvaluationContexts::resolve_context(sect);
  else if(doc)
    ctx = EvaluationContexts::resolve_context(doc);
  else
    ctx = EvaluationContexts::current();
  
  expr = EvaluationContexts::replace_symbol_namespace(
           std::move(expr), 
           ctx,
           strings::DollarContext_namespace);
  
  if(doc && doc->editable()) {
    int index;
    if(sect && sect->parent() == doc) {
      index = sect->index() + 1;
      
      while(index < doc->count()) {
        Section *s = doc->section(index);
        if(!s || !s->get_style(SectionGenerated))
          break;
          
        if(session && session->current_job && session->current_job->default_graphics_options.is_null()) {
          if(auto seq_sect = dynamic_cast<AbstractSequenceSection *>(s)) {
            if(seq_sect->content()->length() == 1 && seq_sect->content()->count() == 1) {
              if(GraphicsBox *gb = dynamic_cast<GraphicsBox *>(seq_sect->content()->item(0))) {
                session->current_job->default_graphics_options = gb->get_user_options();
              }
            }
          }
        }
        
        if(doc->selection_box() == doc) {
          int start = doc->selection_start();
          int end   = doc->selection_end();
          
          if(index < start || index < end) {
            if(start > index) --start;
            if(end > index)   --end;
            
            doc->select(doc, start, end);
          }
        }
        
        doc->remove(index, index + 1);
      }
    }
    else
      index = doc->length();
      
    sect = BoxFactory::create_section(expr);
    if(sect) {
      String base_style_name;
      
      if(session && session->current_job) {
        if(auto seq_sect = dynamic_cast<AbstractSequenceSection *>(sect)) {
          if(seq_sect->content()->length() == 1 && seq_sect->content()->count() == 1) {
            if(GraphicsBox *gb = dynamic_cast<GraphicsBox *>(seq_sect->content()->item(0))) {
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
            Expr base_style = Evaluate(Parse("Try(Replace(`1`, Flatten(`2`)))", base_style_name, rules));
            if(base_style != richmath_System_DollarFailed) {
              sect->style->remove(BaseStyleName);
              sect->style->add_pmath(base_style);
            }
          }
        }
        
        if(!sect->style)
          sect->style = new Style();
        
        if(!sect->style->contains(SectionGenerated))
          sect->style->set(SectionGenerated, true);
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
        
        if(index <= s || index <= e) {
          if(s >= index) ++s;
          if(e >= index) ++e;
          
          doc->select(doc, s, e);
        }
      }
    }
  }
  else {
    if(expr[0] == richmath_System_Section) {
      Expr boxes = expr[1];
      if(boxes[0] == richmath_System_BoxData)
        boxes = boxes[1];
        
      expr = Call(Symbol(richmath_System_RawBoxes), boxes);
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
      Call(Symbol(richmath_System_Assign),
           Symbol(richmath_FE_DollarControlActive),
           Symbol(richmath_System_True)));
  }
  else {
    Application::interrupt_wait(
      /*Parse("FE`$ControlActive:= False; SetAttributes($ControlActiveSetting,{}); Print(FE`$ControlActive)")*/
      Call(Symbol(richmath_System_EvaluationSequence),
           Call(Symbol(richmath_System_Assign),
                Symbol(richmath_FE_DollarControlActive),
                Symbol(richmath_System_False)),
           Call(Symbol(richmath_System_SetAttributes),
                Symbol(richmath_System_DollarControlActiveSetting),
                List())));
  }
}

static Hashset<FrontEndReference> active_controls;

void Application::activated_control(Box *box) {
  active_controls.add(box->id());
  
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
  
  CurrentValue::init();
  if(!front_end_session)
    front_end_session = new FrontEndSession(nullptr);
  
  application_filename = String(Evaluate(Symbol(richmath_System_DollarApplicationFileName)));
  application_directory = FileSystem::get_directory_path(application_filename);
    
  stylesheet_path_base = String(Evaluate(Symbol(richmath_FE_DollarStylesheetDirectory)));
  palette_search_path = Evaluate(Symbol(richmath_FE_DollarPaletteSearchPath));
  session_id = Evaluate(Symbol(richmath_System_DollarSessionID));
  
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
        if(!Win32AcceleratorTable::main_table->translate_accelerator(GetFocus(), &msg)) {
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
        if(!Win32AcceleratorTable::main_table->translate_accelerator(GetFocus(), &msg)) {
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
  Server::local_server->async_interrupt(Call(Symbol(richmath_System_Abort)));
  
  while(session) {
    SharedPtr<Job> job;
    
    while(session->jobs.get(&job)) {
      if(job)
        job->dequeued();
    }
    
    Server::local_server->async_interrupt(Call(Symbol(richmath_System_Abort)));
    //Server::local_server->abort_all();
    
    session = session->next;
  }
  
  delete front_end_session;
  front_end_session = nullptr;
  
  ClientNotificationData cn;
  while(notifications.get(&cn)) {
    cn.done();
//    if(cn.sem)
//      cn.sem->post();
  }
  
  eval_cache.clear();
  CurrentValue::done();
  application_filename = String();
  application_directory = String();
  stylesheet_path_base = String();
  palette_search_path = Expr();
  session_id = Expr();
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

FrontEndObject *Application::find_current_job() {
  RICHMATH_ASSERT(main_message_queue == Expr(pmath_thread_get_queue()));
  
  Session *s = session.ptr();
  
  while(s && !s->current_job)
    s = s->next.ptr();
    
  if(s) {
    EvaluationPosition pos = s->current_job->position();
    
    FrontEndObject *obj = FrontEndObject::find(pos.object_id);
    if(obj)
      return obj;
      
    obj = FrontEndObject::find(pos.section_id);
    if(obj)
      return obj;
      
    obj = FrontEndObject::find(pos.document_id);
    if(obj)
      return obj;
  }
  
  return nullptr;
}

bool Application::remove_job(Box *input_box, bool only_check_possibility) {
  RICHMATH_ASSERT(main_message_queue == Expr(pmath_thread_get_queue()));
  
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
  
  Server::local_server->async_interrupt(Call(Symbol(richmath_System_Abort)));
  //Server::local_server->abort_all();
}

FrontEndObject *Application::get_evaluation_object() {
  //FrontEndObject *obj = FrontEndObject::find(Dynamic::current_observer_id);
  FrontEndObject *obj = FrontEndObject::find(current_evaluation_object_id);
  
  if(obj)
    return obj;
    
  obj = Application::find_current_job();
  if(obj)
    return obj;
    
  EvaluationPosition pos;
  pmath_atomic_lock(&print_pos_lock);
  {
    pos = print_pos;
  }
  pmath_atomic_unlock(&print_pos_lock);
  
  obj = FrontEndObject::find(pos.object_id);
  if(obj)
    return obj;
    
  obj = FrontEndObject::find(pos.section_id);
  if(obj)
    return obj;
    
  obj = FrontEndObject::find(pos.document_id);
  if(obj)
    return obj;
    
  return nullptr;
}

Document *Application::try_create_document() {
  Document *doc = nullptr;
  
#ifdef RICHMATH_USE_WIN32_GUI
  {
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int w = 500;
    int h = 550;
    
    doc = Documents::selected_document();
    if(doc) {
      auto wid = dynamic_cast<Win32Widget*>(doc->native());
      if(wid) {
        HWND hwnd = wid->hwnd();
        while(GetParent(hwnd) != nullptr)
          hwnd = GetParent(hwnd);
        
        int dpi = Win32HighDpi::get_dpi_for_window(hwnd); //doc->native()->dpi();
        
        w = MulDiv(w, dpi, 96);
        h = MulDiv(h, dpi, 96);
        
        int dx = Win32HighDpi::get_system_metrics_for_dpi(SM_CXSMICON, dpi) + 
                 Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi) + 
                 Win32HighDpi::get_system_metrics_for_dpi(SM_CXSIZEFRAME, dpi);
        int dy = Win32HighDpi::get_system_metrics_for_dpi(SM_CYCAPTION, dpi) + 
                 Win32HighDpi::get_system_metrics_for_dpi(SM_CXPADDEDBORDER, dpi) + 
                 Win32HighDpi::get_system_metrics_for_dpi(SM_CYSIZEFRAME, dpi);
        
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
    else {
      HMONITOR mon = Win32HighDpi::get_startup_monitor();
      int dpi = Win32HighDpi::get_dpi_for_monitor(mon);
      Win32HighDpi::get_system_metrics_for_dpi(SM_CYCAPTION, dpi);
      
      w = MulDiv(w, dpi, 96);
      h = MulDiv(h, dpi, 96);
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
  
  if(doc) {
    doc->style->set(StyleDefinitions, String("Default.pmathdoc"));
  }
  return doc;
}

Document *Application::try_create_document(Expr data) {
  // CreateDocument({sections...}, options...)
  
  // TODO: respect window-related options (WindowTitle...)
  
  Document *doc = Application::try_create_document();
  if(!doc)
    return nullptr;
    
  if(data.expr_length() >= 1) {
    Expr options(pmath_options_extract_ex(data.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
    if(options.is_expr())
      doc->style->add_pmath(options);
      
    Expr sections = data[1];
    if(sections[0] != richmath_System_List)
      sections = List(sections);
    
    for(auto item : sections.items()) {
      int pos = doc->length();
      doc->insert_pmath(&pos, Documents::make_section_boxes(std::move(item), doc));
    }
  }
  
  if(!doc->selectable())
    doc->select(nullptr, 0, 0);
  
  return doc;
}

Document *Application::find_open_document(String full_filename) {
  if(full_filename.is_null())
    return nullptr;
  
  for(auto win : CommonDocumentWindow::All) {
    Document *doc = win->content();
    
    if(doc->native()->full_filename() == full_filename)
      return doc;
  }
  
  return nullptr;
}

Document *Application::open_new_document(String full_filename) {
  if(full_filename.is_null())
    return nullptr;
  
  Document *doc = Application::try_create_document();
  if(!doc)
    return nullptr;
    
  doc->native()->full_filename(full_filename);
  
  do {
    if(full_filename.part(full_filename.length() - 9).equals(".pmathdoc")) {
      Expr held_boxes = Application::interrupt_wait(
                          Parse("Get(`1`, Head->HoldComplete)", full_filename),
                          Application::button_timeout);
                          
                          
      if( held_boxes.expr_length() == 1 &&
          held_boxes[0] == richmath_System_HoldComplete &&
          doc->try_load_from_object(held_boxes[1], BoxInputFlags::Default))
      {
        break;
      }
    }
    
    ReadableTextFile file(Evaluate(Call(Symbol(richmath_System_OpenRead), full_filename)));
    String s;
    
    while(!pmath_aborting() && file.status() == PMATH_FILE_OK) {
      if(s.is_valid())
        s += "\n";
      s += file.readline();
    }
    
    if(s) {
      int pos = 0;
      Expr section_expr = Call(Symbol(richmath_System_Section), s, strings::Text);
      doc->insert_pmath(&pos, section_expr);
    }
  } while(false);

  if(!doc->selectable())
    doc->select(nullptr, 0, 0);
    
  doc->style->set(Visible,                         true);
  doc->style->set(InternalHasModifiedWindowOption, true);
  doc->on_style_changed(true);
  //doc->native()->bring_to_front();
  return doc;
}

extern pmath_symbol_t richmath_FE_FileSaveDialog;

Expr Application::run_filedialog(Expr data) {
// FE`FileOpenDialogSymbol("initialfile", {"filter1" -> {"*.ext1", ...}, ...}, WindowTitle -> ....)
// FE`FileSaveDialogSymbol("initialfile", {"filter1" -> {"*.ext1", ...}, ...}, WindowTitle -> ....)
  String title;
  String full_filename;
  Expr   filter;
  
  Expr head = data[0];
  
  size_t argi = 1;
  if(data[argi].is_string()) {
    full_filename = String(data[argi]);
    ++argi;
  }
  
  if(data[argi][0] == richmath_System_List) {
    filter = data[argi];
    ++argi;
  }
  
  if(data.is_expr()) {
    Expr options(pmath_options_extract(data.get(), argi - 1));
    
    if(options.is_valid()) {
      Expr title_value(pmath_option_value(
                         head.get(),
                         richmath_System_WindowTitle,
                         options.get()));
                         
      if(title_value.is_string())
        title = String(title_value);
    }
  }
  
  Expr result = Symbol(richmath_System_DollarFailed);
  AutoGuiWait timer;
  
#if RICHMATH_USE_WIN32_GUI
  Win32FileDialog
#elif RICHMATH_USE_GTK_GUI
  MathGtkFileDialog
#else
#  error "No GUI"
#endif
  dialog(head == richmath_FE_FileSaveDialog);
  
  dialog.set_title(title);
  dialog.set_initial_file(full_filename);
  dialog.set_filter(filter);
  result = dialog.show_dialog();
  
  return result;
}

bool Application::is_idle() {
  return !is_executing_for_sth && !session->current_job.is_valid();
}

bool Application::is_running_job_for(Box *section_or_document) {
  if(!section_or_document)
    return false;
  
  if(dynamic_cast<Section*>(section_or_document)) {
    SharedPtr<Session> s = session;
    while(s) {
      SharedPtr<Job> job = s->current_job;
      if(job && job->position().section_id == section_or_document->id())
        return true;
        
      s = s->next;
    }
    
    return false;
  }
  
  if(dynamic_cast<Document*>(section_or_document)) {
    SharedPtr<Session> s = session;
    while(s) {
      SharedPtr<Job> job = s->current_job;
      if(job && job->position().document_id == section_or_document->id())
        return true;
        
      s = s->next;
    }
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
  if( result != richmath_System_DollarAborted &&
      result != richmath_System_DollarFailed)
  {
    eval_cache.set(expr, result);
  }
  else if(result == richmath_System_DollarAborted) {
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

void Application::with_evaluation_box(FrontEndObject *obj, void(*callback)(void*), void *arg) {
  AutoValueReset<Document*>         auto_menu_redirect{ Menus::current_document_redirect };
  AutoValueReset<FrontEndReference> auto_reset(current_evaluation_object_id);
  
  current_evaluation_object_id = obj ? obj->id() : FrontEndReference::None;
  
  if(auto box = dynamic_cast<Box*>(obj))
    Menus::current_document_redirect = box->find_parent<Document>(true);
  else
    Menus::current_document_redirect = nullptr;
  
  callback(arg);
}

Expr Application::interrupt_wait_for(Expr expr, FrontEndObject *obj, double seconds) {
  auto old_evaluation_object_id = current_evaluation_object_id;
  auto old_is_executing_for_sth = is_executing_for_sth;
  
  current_evaluation_object_id = obj ? obj->id() : FrontEndReference::None;
  is_executing_for_sth = true;
  
  Expr result = interrupt_wait(expr, seconds);
  
  current_evaluation_object_id = old_evaluation_object_id;
  is_executing_for_sth         = old_is_executing_for_sth;
  
  return result;
}

Expr Application::interrupt_wait_for_interactive(Expr expr, FrontEndObject *obj, double seconds) {
  EvaluationPosition old_print_pos(obj);
  
  pmath_atomic_lock(&print_pos_lock);
  {
    using std::swap;
    swap(old_print_pos, print_pos);
  }
  pmath_atomic_unlock(&print_pos_lock);
  
  Expr result = interrupt_wait_for(std::move(expr), obj, seconds);

  pmath_atomic_lock(&print_pos_lock);
  {
    using std::swap;
    swap(old_print_pos, print_pos);
  }
  pmath_atomic_unlock(&print_pos_lock);
  
  return result;
}

void Application::delay_dynamic_updates(bool delay) {
  RICHMATH_ASSERT(main_message_queue == Expr(pmath_thread_get_queue()));
  
  dynamic_update_delay = delay;
  
  if(!delay) {
    decltype(pending_dynamic_updates)  old_pending;
    swap(pending_dynamic_updates, old_pending);
    for(auto id : old_pending) {
      auto feo = FrontEndObject::find(id);
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
      sect->dialog_start(true);
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
      sect->dialog_start(false);
      sect->request_repaint_all();
    }
  }
}

static void cnt_end(Expr data) {
  SharedPtr<Job> job = session->current_job;
  session->current_job = nullptr;
  
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
      print_pos = old_job_print_pos;
    }
    pmath_atomic_unlock(&print_pos_lock);
  }
  
  bool more = false;
  if(data == richmath_System_DollarAborted) {
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
        old_job_print_pos = print_pos;
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
    
    session->current_job = nullptr;
  }
  
  if(!more) {
    for(auto win : CommonDocumentWindow::All) {
      Document *doc = win->content();
      
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

static Expr cnt_callfrontend(Expr data) {
  AutoValueReset<FrontEndReference> auto_reset_eval(current_evaluation_object_id);
  if(FrontEndReference source = FrontEndReference::from_pmath_raw(data[0])) 
    current_evaluation_object_id = source;
  
  Expr expr = data[1];
  data = {};
  return Evaluate(std::move(expr));
}

static void cnt_dynamicupdate(Expr data) {
  Expr cause {};
  if(data.is_rule()) {
    cause = data[1];
    data = data[2];
  }

  double now = pmath_tickcount();
  double next_eval = last_dynamic_evaluation + Application::min_dynamic_update_interval - now;
  bool need_timer = (next_eval > 0);
  
  if(need_timer || dynamic_update_delay) {
    for(size_t i = data.expr_length(); i > 0; --i) {
      auto ref = FrontEndReference::from_pmath_raw(data[i]);
      auto obj = FrontEndObject::find(ref);
      if(obj) {
        if(cause) {
          obj->update_cause(cause);
        }
        
        pending_dynamic_updates.add(obj->id());
      }
    }
    
    if(need_timer && !dynamic_update_delay_timer_active) {
      int milliseconds = (int)(next_eval * 1000);
      
#ifdef RICHMATH_USE_WIN32_GUI
      if(SetTimer(info_window.hwnd(), TID_DYNAMIC_UPDATE, milliseconds, nullptr))
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
    auto ref = FrontEndReference::from_pmath_raw(data[i]);
    auto obj = FrontEndObject::find(ref);
    if(obj) {
      if(cause) {
        obj->update_cause(cause);
      }
      
      obj->dynamic_updated();
    }
  }
}

static Expr cnt_documentread(Expr data) {
  Document *doc = nullptr;
  BoxOutputFlags flags = BoxOutputFlags::WithDebugMetadata;
  int depth = INT_MAX;
  
  if(data.expr_length() == 0 || data[1] == richmath_System_Automatic) {
    doc = Documents::selected_document();
  }
  else {
    Box *box = FrontEndObject::find_cast<Box>(
                 FrontEndReference::from_pmath(data[1]));
    
    if(box)
      doc = box->find_parent<Document>(true);
  }
  
  if(data.expr_length() == 2) {
    depth = -1;
    Expr d_obj = data[2];
    if(d_obj.is_int32()) {
      depth = PMATH_AS_INT32(d_obj.get());
      flags |= BoxOutputFlags::LimitedDepth;
    }
    
    if(depth < 0)
      return Symbol(richmath_System_DollarFailed);
  }
  
  if(!doc || !doc->selection_box())
    return strings::EmptyString;
  
  if(doc->selection_length() == 0) {
    if(depth == 0)
      return doc->selection().to_pmath();
      
    return Expr(pmath_try_set_debug_metadata(PMATH_C_STRING(""), doc->selection().to_pmath().release()));
  }

  AutoValueReset<int> auto_mbod(Box::max_box_output_depth);
  Box::max_box_output_depth = depth;
  return doc->selection_box()->to_pmath(flags, doc->selection_start(), doc->selection_end());
}

namespace {
  class SaveOperation {
    private:
      static String section_to_string(Section *sec) {
        // TODO: convert only the first line to boxes
        Expr boxes = sec->to_pmath(BoxOutputFlags::Default);
        Expr text = Application::interrupt_wait(
                      Call(Symbol(richmath_FE_BoxesToText), std::move(boxes), strings::PlainText),
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
        RICHMATH_ASSERT(doc != nullptr);
        
        Section *best_section = nullptr;
        float lowest_precedence = Infinity;
        
        for(int i = 0; i < doc->count(); ++i) {
          Section *sec = doc->section(i);
          if(sec->group_info().precedence < lowest_precedence) {
            lowest_precedence = sec->group_info().precedence;
            best_section = sec;
          }
          
          if(i < sec->group_info().end)
            i = sec->group_info().end;
        }
        
        if(best_section)
          return section_to_string(best_section);
        return String();
      }
      
    public:
      static Expr do_save(Expr data) {
        // data = {document, filename}
        // data = {document, None} = always ask for new file name
        // data = {document}       = use document file name if available, ask otherwise
        // data = {Automatic, ...}
        
        Document *doc = nullptr;
        
        if(data[1].is_expr()) {
          Box *box = FrontEndObject::find_cast<Box>(FrontEndReference::from_pmath(data[1]));
          
          if(box)
            doc = box->find_parent<Document>(true);
        }
        else
          doc = Documents::selected_document();
          
        if(!doc)
          return Symbol(richmath_System_DollarFailed);
        
        Document *owner_doc = doc->native()->owner_document();
        if(owner_doc)
          doc = owner_doc;
          
        Expr filename = data[2];
        
        if(!filename.is_string() && filename != richmath_System_None)
          filename = doc->native()->full_filename();
          
        if(!filename.is_string()) {
          String initialfile = doc->native()->full_filename();
          
          if(initialfile.is_null()) {
            initialfile = doc->native()->filename();
            
            if(initialfile.is_null()) {
              String title = guess_best_title(doc);
              
              if(!FileSystem::is_filename_without_directory(title))
                title = String();
              
              if(title.length() == 0)
                title = String("untitled");
              initialfile = std::move(title) + ".pmathdoc";
              
              String dir = doc->native()->directory();
              if(!dir.is_null())
                initialfile = FileSystem::file_name_join(std::move(dir), std::move(initialfile));
            }
          }
          
          Expr filter = List(
                          Rule(String("pMath Documents (*.pmathdoc)"), String("*.pmathdoc"))/*,
                    Rule(String("All Files (*.*)"),              String("*.*"))*/);

          filename = Application::run_filedialog(
                       Call(
                         Symbol(richmath_FE_FileSaveDialog),
                         std::move(initialfile),
                         std::move(filter)));
        }
        
        if(!filename.is_string())
          return Symbol(richmath_System_DollarFailed);
          
        WriteableTextFile file(Evaluate(Call(Symbol(richmath_System_OpenWrite), filename)));
        if(!file.is_file())
          return Symbol(richmath_System_DollarFailed);
          
        Expr boxes = doc->to_pmath(BoxOutputFlags::Default);
        
        file.write("/* pMath Document */\n\n");
        
        boxes.write_to_file(file, 
          PMATH_WRITE_OPTIONS_INPUTEXPR | 
          PMATH_WRITE_OPTIONS_FULLSTR | 
          PMATH_WRITE_OPTIONS_FULLNAME_NONSYSTEM);
        
        file.close();
        doc->native()->full_filename(filename);
        doc->native()->on_saved();
        
        String stylesheet_name = Stylesheet::name_from_path(filename);
        if(stylesheet_name.is_valid()) {
          SharedPtr<Stylesheet> stylesheet = Stylesheet::find_registered(stylesheet_name);
          if(stylesheet) 
            reload_stylesheet(stylesheet.ptr(), boxes);
        }
        
        return filename;
      }
      
    private:
      static void reload_stylesheet(Stylesheet *stylesheet, Expr boxes) {
        RICHMATH_ASSERT(stylesheet != nullptr);
        
        Hashset<FrontEndReference> done;
        done.add(stylesheet->id());
        
        Array<FrontEndReference> work_list;
        
        for(auto &id : stylesheet->enum_users()) 
          work_list.add(id);
        
        stylesheet->reload(boxes);
        
        for(int next_index = 0; next_index < work_list.length(); ++next_index) {
          auto next = work_list[next_index];
          if(!done.add(next)) 
            continue;
          
          auto feo = FrontEndObject::find(next);
          if((stylesheet = dynamic_cast<Stylesheet*>(feo)) != nullptr) {
            for(auto &id : stylesheet->enum_users()) 
              work_list.add(id);
              
            stylesheet->reload();
          }
          else if(Document *doc = dynamic_cast<Document*>(feo)){
            // update document ...
            
            doc->style->set(InternalLastStyleDefinitions, Expr());
            doc->style->set(InternalHasModifiedWindowOption, true);
            doc->on_style_changed(true);
          }
          else {
            // ???
          }
        }
      }
  };
}

Expr Application::save(Document *doc) {
  if(doc)
    return SaveOperation::do_save(List(doc->to_pmath_id()));
  else
    return SaveOperation::do_save(List(Symbol(richmath_System_Automatic))); 
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
      cnt_end(std::move(cn.data));
      break;
      
    case ClientNotification::Return:
      cnt_return(std::move(cn.data));
      break;
      
    case ClientNotification::ReturnBox:
      cnt_returnbox(std::move(cn.data));
      break;
      
    case ClientNotification::PrintSection:
      cnt_printsection(std::move(cn.data));
      break;
    
    case ClientNotification::CallFrontEnd:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_callfrontend(std::move(cn.data)).release();
      else
        cnt_callfrontend(std::move(cn.data));
      break;
      
    case ClientNotification::MenuCommand:
      Menus::run_command_now(std::move(cn.data));
      break;
      
    case ClientNotification::DynamicUpdate:
      cnt_dynamicupdate(std::move(cn.data));
      break;
      
    case ClientNotification::DocumentRead:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_documentread(std::move(cn.data)).release();
      break;
      
    case ClientNotification::FileDialog:
      if(cn.result_ptr)
        *cn.result_ptr = Application::run_filedialog(std::move(cn.data)).release();
      break;
      
    case ClientNotification::Save:
      if(cn.result_ptr)
        *cn.result_ptr = SaveOperation::do_save(std::move(cn.data)).release();
      else
        SaveOperation::do_save(std::move(cn.data));
      break;
    
    case ClientNotification::AskInterrupt:
      if(cn.result_ptr)
        *cn.result_ptr = ask_interrupt(std::move(cn.data)).release();
      break;
  }
  
  cn.done();
}

Expr richmath_eval_FrontEnd_EvaluationBox(Expr expr) {
  if(expr.expr_length() != 0)
    return expr;
  
  if(FrontEndObject *obj = Application::get_evaluation_object())
    return obj->to_pmath_id();
  
  return Symbol(richmath_System_DollarFailed);
}

Expr richmath_eval_FrontEnd_EvaluationDocument(Expr expr) {
  if(expr.expr_length() != 0)
    return expr;
  
  Document *doc = Box::find_nearest_parent<Document>(Application::get_evaluation_object());
  if(doc) {
    if(auto working_doc = doc->native()->working_area_document())
      doc = working_doc;
  }
    
  if(doc)
    return doc->to_pmath_id();
  
  return Symbol(richmath_System_DollarFailed);
}
