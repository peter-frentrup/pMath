#ifdef RICHMATH_USE_WIN32_GUI
  #define _WIN32_WINNT 0x0600
#endif


#include <eval/application.h>

#include <cmath>
#include <cstdio>

#include <boxes/section.h>

#include <graphics/config-shaper.h>

#ifdef RICHMATH_USE_WIN32_GUI
  #include <gui/win32/basic-win32-widget.h>
  #include <gui/win32/win32-document-window.h>
  #include <gui/win32/win32-menu.h>
#endif

#ifdef RICHMATH_USE_GTK_GUI
  #include <gui/gtk/mgtk-document-window.h>
#endif


#ifdef PMATH_OS_WIN32
  #include <windows.h>
#else
  #include <pthread.h>
#endif


#include <gui/document.h>

#include <eval/binding.h>
#include <eval/dynamic.h>
#include <eval/job.h>
#include <eval/server.h>

#include <util/concurrent-queue.h>
#include <util/semaphore.h>


#ifdef RICHMATH_USE_WIN32_GUI

  #define WM_CLIENTNOTIFY  (WM_USER + 1)
  #define WM_ADDJOB        (WM_USER + 2)

#endif


using namespace richmath;

namespace{
  class ClientNotification {
    public:
      ClientNotification(): finished(0), result_ptr(0) {}

      void done(){
        if(finished){
          result_ptr = NULL;
          pmath_atomic_write_release(finished, 1);
          pmath_thread_wakeup(notify_queue.get());
        }
      }

    public:
      ClientNotificationType  type;
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
      }

    public:
      ConcurrentQueue< SharedPtr<Job> > jobs;
      SharedPtr<Job>                    current_job;

      SharedPtr<Session> next;
  };
}

enum ClientState{
  Starting = 0,
  Running = 1,
  Quitting = 2
};

static pmath_atomic_t state = { Starting }; // ClientState

static ConcurrentQueue<ClientNotification>   notifications;

static SharedPtr<Session>   session = new Session(0);

static Hashtable<Expr, bool (*)(Expr)> menu_commands;
static Hashtable<Expr, bool (*)(Expr)> menu_command_testers;

static Hashtable<int, Void, cast_hash> pending_dynamic_updates;
static bool dynamic_update_delay = false;

static void execute(ClientNotification &cn);

static EvaluationPosition print_pos;
static EvaluationPosition old_job;
static Expr main_message_queue;

  // also a GSourceFunc, must return 0
  static int on_client_notify(void *data){
    ClientNotification cn;

    if(notifications.get(&cn) && session)
      execute(cn);

    return 0;
  }

  // also a GSourceFunc, must return 0
  static int on_add_job(void *data){
    if(session && !session->current_job){
      while(session->jobs.get(&session->current_job)){
        if(session->current_job){
          old_job = print_pos;

          if(session->current_job->start()){
            print_pos = session->current_job->position();

            break;
          }
        }

        session->current_job = 0;
      }
    }

    return 0;
  }

#ifdef RICHMATH_USE_WIN32_GUI
  static HWND hwnd_message = HWND_MESSAGE;

  class ClientInfoWindow: public BasicWin32Widget{
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
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam){
        if(!initializing()){
          switch(message){
            case WM_CLIENTNOTIFY:
              on_client_notify(0);
              return 0;

            case WM_ADDJOB:
              on_add_job(0);
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


double Application::edit_interrupt_timeout = 2.0;
double Application::interrupt_timeout      = 0.3;
double Application::button_timeout         = 4.0;
double Application::dynamic_timeout        = 4.0;
String Application::application_filename;
String Application::application_directory;

Hashtable<Expr, Expr, object_hash> Application::eval_cache;

void Application::notify(ClientNotificationType type, Expr data){
  if(pmath_atomic_read_aquire(&state) == Quitting)
    return;

  ClientNotification cn;
  cn.type = type;
  cn.data = data;

  notifications.put(cn);
  pmath_thread_wakeup(main_message_queue.get());

  #ifdef RICHMATH_USE_WIN32_GUI
    PostMessage(info_window.hwnd(), WM_CLIENTNOTIFY, 0, 0);
  #endif

  #ifdef RICHMATH_USE_GTK_GUI
    g_idle_add_full(G_PRIORITY_DEFAULT, on_client_notify, NULL, NULL);
  #endif
}

Expr Application::notify_wait(ClientNotificationType type, Expr data){
  if(pmath_atomic_read_aquire(&state) != Running)
    return Symbol(PMATH_SYMBOL_FAILED);

  if(
    #ifdef PMATH_OS_WIN32
      GetCurrentThreadId() == main_thread_id
    #else
      pthread_equal(pthread_self(), main_thread)
    #endif
  ){
    notify(type, data);
    return Symbol(PMATH_SYMBOL_FAILED);
  }

  pmath_atomic_t finished = PMATH_ATOMIC_STATIC_INIT;
  pmath_t result = PMATH_UNDEFINED;
  ClientNotification cn;
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
    g_idle_add_full(G_PRIORITY_DEFAULT, on_client_notify, NULL, NULL);
  #endif

  while(!pmath_atomic_read_aquire(&finished)){
    pmath_thread_sleep();
    pmath_debug_print("w");
  }
  pmath_debug_print("W");

  return Expr(result);
}

bool Application::is_menucommand_runnable(Expr cmd){
  bool (*func)(Expr);

  func = menu_command_testers[cmd];
  if(func && !func(cmd))
    return false;

  func = menu_command_testers[cmd];
  if(func && !func(cmd))
    return false;
  
  if(cmd.is_string()){
    String scmd(cmd);
    
    if(scmd.starts_with("@shaper=")){
      scmd = scmd.part(sizeof("@shaper=") - 1, -1);
      
      if(!MathShaper::available_shapers.search(scmd))
        return false;
    }
  }

  return true;
}

void Application::register_menucommand(
  Expr cmd,
  bool (*func)(Expr cmd),
  bool (*test)(Expr cmd)
){
  if(cmd.is_null()){
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

  static void write_data(void *user, const uint16_t *data, int len){
    FILE *file = (FILE*)user;

    #define BUFSIZE 200
    char buf[BUFSIZE];
    while(len > 0){
      int tmplen = len < BUFSIZE ? len : BUFSIZE;
      char *bufptr = buf;
      while(tmplen-- > 0){
        if(*data <= 0xFF){
          *bufptr++ = (unsigned char)*data++;
        }
        else{
          ++data;
          *bufptr++ = '?';
        }
      }
      if(pmath_aborting())
        return;
      
      fwrite(buf, 1, len < BUFSIZE ? len : BUFSIZE, file);
      len-= BUFSIZE;
    }
    #undef BUFSIZE
  }

void Application::gui_print_section(Expr expr){
  Document *doc = dynamic_cast<Document*>(
    Box::find(print_pos.document_id));

  Section *sect = dynamic_cast<Section*>(
    Box::find(print_pos.section_id));

  if(doc){
    int index;
    if(sect && sect->parent() == doc){
      index = sect->index() + 1;

      while(index < doc->count()){
        Section *s = doc->section(index);
        if(!s || !s->get_style(SectionGenerated))
          break;

        if(doc->selection_box() == doc){
          int s = doc->selection_start();
          int e = doc->selection_end();

          if(s > index) --s;
          if(e > index) --e;

          doc->select(doc, s, e);
        }

        doc->remove(index, index + 1);
      }
    }
    else
      index = doc->length();

    sect = Section::create_from_object(expr);
    if(sect){

      doc->insert(index, sect);

      print_pos = EvaluationPosition(sect);

      if(doc->selection_box() == doc){
        int s = doc->selection_start();
        int e = doc->selection_end();

        if(s >= index) ++s;
        if(e >= index) ++e;

        doc->select(doc, s, e);
      }
    }
  }
  else{
    printf("\n");
    pmath_write(expr.get(), 0, write_data, stdout);
    printf("\n");
  }
}

void Application::init(){
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

  application_filename = String(Evaluate(Symbol(PMATH_SYMBOL_APPLICATIONFILENAME)));
  int             i   = application_filename.length() - 1;
  const uint16_t *buf = application_filename.buffer();
  while(i > 0 && buf[i] != '\\' && buf[i] != '/')
    --i;

  if(i > 0)
    application_directory = application_filename.part(0, i);
  else
    application_directory = application_filename;
}

void Application::doevents(){
  // ClientState
  intptr_t old_state = pmath_atomic_fetch_set(&state, Running);

  #ifdef RICHMATH_USE_WIN32_GUI
  {
    MSG msg;
    while(PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)){
      if(msg.message == WM_QUIT){
        PostQuitMessage(0);
        break;
      }

      if(Win32AcceleratorTable::main_table.is_valid()){
        if(!TranslateAcceleratorW(GetFocus(), Win32AcceleratorTable::main_table->haccel(), &msg)){
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

int Application::run(){
  int result = 0;

  if(pmath_atomic_read_aquire(&state) == Running)
    return 1;

  pmath_atomic_write_release(&state, Running);

  #ifdef RICHMATH_USE_WIN32_GUI
  {
    MSG msg;
    BOOL bRet;
    while((bRet = GetMessageW(&msg, NULL, 0, 0)) != 0){
      if(bRet == -1) {
        pmath_atomic_write_release(&state, Quitting);
        return 1;
      }

      if(Win32AcceleratorTable::main_table.is_valid()){
        if(!TranslateAcceleratorW(GetFocus(), Win32AcceleratorTable::main_table->haccel(), &msg)){
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

void Application::done(){
  Server::local_server->interrupt(Call(Symbol(PMATH_SYMBOL_ABORT)));
  
  while(session){
    SharedPtr<Job> job;
    
    while(session->jobs.get(&job)){
      if(job)
        job->dequeued();
    }

    Server::local_server->interrupt(Call(Symbol(PMATH_SYMBOL_ABORT)));
    //Server::local_server->abort_all();
  
    session = session->next;
  }
  
  ClientNotification cn;
  while(notifications.get(&cn)){
    cn.done();
//    if(cn.sem)
//      cn.sem->post();
  }

  eval_cache.clear();
  menu_commands.clear();
  menu_command_testers.clear();
  application_filename = String();
  application_directory = String();
  main_message_queue = Expr();
}

void Application::add_job(SharedPtr<Job> job){
  if(session && job){
    session->jobs.put(job);
    job->enqueued();

    #ifdef RICHMATH_USE_WIN32_GUI
      PostMessage(info_window.hwnd(), WM_ADDJOB, 0, 0);
    #endif

    #ifdef RICHMATH_USE_GTK_GUI
      g_idle_add_full(G_PRIORITY_DEFAULT, on_add_job, NULL, NULL);
    #endif
  }
}

Box *Application::find_current_job(){
  assert(main_message_queue == Expr(pmath_thread_get_queue()));
  
  Session *s = session.ptr();
  
  while(s && !s->current_job)
    s = s->next.ptr();
  
  if(s){
    EvaluationPosition pos = s->current_job->position();
    
    Box *box = Box::find(pos.box_id);
    if(box)
      return box;
    
    box = Box::find(pos.section_id);
    if(box)
      return box;
      
    box = Box::find(pos.document_id);
    if(box)
      return box;
  }
  
  return 0;
}

bool Application::remove_job(Box *input_box, bool only_check_possibility){
  assert(main_message_queue == Expr(pmath_thread_get_queue()));
  
  if(!input_box)
    return false;
  
  ConcurrentQueue< SharedPtr<Job> > tested_jobs;
  SharedPtr<Job> tmp;
  
  bool result = false;
  SharedPtr<Session> s = session;
  while(s){
    while(s->jobs.get(&tmp)){
      Box *sect = Box::find(tmp->position().section_id);
      
      if(sect == input_box){
        if(only_check_possibility){
          tested_jobs.put_front(tmp);
        }
        else
          tmp->dequeued();
          
        result = true;
        break;
      }
      
      tested_jobs.put_front(tmp);
    }
    
    while(tested_jobs.get(&tmp)){
      s->jobs.put_front(tmp);
    }
    
    if(result)
      return true;
    
    s = s->next;
  }
  
  return false;
}

void Application::abort_all_jobs(){
  if(session){
    SharedPtr<Job> job;
    
    while(session->jobs.get(&job)){
      if(job)
        job->dequeued();
    }
  }
  
  Server::local_server->interrupt(Call(Symbol(PMATH_SYMBOL_ABORT)));
  //Server::local_server->abort_all();
}

bool Application::is_idle(){
  return !session->current_job.is_valid();
}

bool Application::is_idle(Box *box){
  if(!box)
    return true;
  
  Section *sect = box->find_parent<Section>(true);
  
  SharedPtr<Session> s = session;
  if(sect){
    while(s){
      SharedPtr<Job> job = s->current_job;
      if(job && job->position().section_id == box->id())
        return false;

      s = s->next;
    }
    
    return true;
  }
  
  while(s){
    SharedPtr<Job> job = s->current_job;
    if(job && job->position().document_id == box->id())
      return false;

    s = s->next;
  }
  
  return true;
}

  static void interrupt_wait_idle(void *data){
    pmath_debug_print("[idle ");
    ConcurrentQueue<ClientNotification> *suppressed_notifications;
    suppressed_notifications = (ConcurrentQueue<ClientNotification>*)data;

    ClientNotification cn;
    while(notifications.get(&cn)){
      if(cn.type == CNT_END 
      || cn.type == CNT_ENDSESSION){
        notifications.put_front(cn);
        return;
      }

      /* We must filter out CNT_DYNAMICUPDATE because that could update a parent
         DynamicBox of the DynamicBox that is currently updated during its
         paint() event. That would cause a memory corruption/crash.
       */
      if(cn.type == CNT_DYNAMICUPDATE){
        suppressed_notifications->put_front(cn);
        continue;
      }

      execute(cn);
    }
    pmath_debug_print(" idle]\n");
  }

Expr Application::interrupt(Expr expr, double seconds){
  ConcurrentQueue<ClientNotification>  suppressed_notifications;

  Expr result = Server::local_server->interrupt_wait(expr, seconds, interrupt_wait_idle, &suppressed_notifications);

  ClientNotification cn;
  while(suppressed_notifications.get(&cn)){
    notifications.put_front(cn);
  }

  return result;
}

Expr Application::interrupt(Expr expr){
  return interrupt(expr, interrupt_timeout);
}

Expr Application::interrupt_cached(Expr expr, double seconds){
  if(!expr.is_pointer_of(PMATH_TYPE_SYMBOL | PMATH_TYPE_EXPRESSION))
    return expr;

  Expr *cached = eval_cache.search(expr);
  if(cached)
    return *cached;

  Expr result = interrupt(expr, seconds);
  if(result != PMATH_SYMBOL_ABORTED
  && result != PMATH_SYMBOL_FAILED){
    eval_cache.set(expr, result);
  }
  else if(result == PMATH_SYMBOL_ABORTED){
    #ifdef RICHMATH_USE_WIN32_GUI
      MessageBeep(-1);
    #endif

    #ifdef RICHMATH_USE_GTK_GUI
      gdk_beep();
    #endif
  }

  return result;
}

Expr Application::interrupt_cached(Expr expr){
  return interrupt_cached(expr, interrupt_timeout);
}

void Application::execute_for(Expr expr, Box *box, double seconds){
//  if(box)
//    print_pos = EvaluationPosition(box);

  EvaluationPosition pos(box);

  Server::local_server->interrupt(
    Call(GetSymbol(InternalExecuteForSymbol), expr, pos.document_id, pos.section_id, pos.box_id),
    seconds);
}

void Application::execute_for(Expr expr, Box *box){
  execute_for(expr, box, interrupt_timeout);
}

Expr Application::internal_execute_for(Expr expr, int doc, int sect, int box){
  static pmath_atomic_t lock = PMATH_ATOMIC_STATIC_INIT;
  EvaluationPosition old_print_pos;

  pmath_atomic_lock(&lock);
  {
    old_print_pos = print_pos;
    print_pos.document_id = doc;
    print_pos.section_id  = sect;
    print_pos.box_id      = box;
  }
  pmath_atomic_unlock(&lock);

  expr = Evaluate(expr);

  pmath_atomic_lock(&lock);
  {
    print_pos = old_print_pos;
  }
  pmath_atomic_unlock(&lock);

  return expr;
}

void Application::delay_dynamic_updates(bool delay){
  assert(main_message_queue == Expr(pmath_thread_get_queue()));
  
  dynamic_update_delay = delay;
  
  if(!delay){
    for(unsigned i = 0, cnt = 0;cnt < pending_dynamic_updates.size();++i){
      Entry<int, Void> *e = pending_dynamic_updates.entry(i);
      
      if(e){
        ++cnt;
        
        Box *box = Box::find(e->key);
        if(box)
          box->dynamic_updated();
      }
    }
    pending_dynamic_updates.clear();
  }
}

static void cnt_startsession(){
  if(session->current_job){
    Section *sect = dynamic_cast<Section*>(
      Box::find(session->current_job->position().section_id));

    if(sect){
      sect->dialog_start = true;
      sect->request_repaint_all();
    }
  }

  session = new Session(session);
}

static void cnt_endsession(){
  SharedPtr<Job> job;
  while(session->jobs.get(&job)){
    if(job)
      job->dequeued();
  }

  if(session->next){
    session = session->next;
  }

  if(session->current_job){
    Section *sect = dynamic_cast<Section*>(
      Box::find(session->current_job->position().section_id));

    if(sect){
      sect->dialog_start = false;
      sect->request_repaint_all();
    }
  }
}

static void cnt_end(Expr data){
  SharedPtr<Job> job = session->current_job;
  session->current_job = 0;

  if(job){
    job->end();
    job->dequeued();

    {
      Document *doc = dynamic_cast<Document*>(
        Box::find(print_pos.document_id));

      Section *sect = dynamic_cast<Section*>(
        Box::find(print_pos.section_id));

      if(doc){
        if(sect && sect->parent() == doc){
          int index = sect->index() + 1;

          while(index < doc->count()){
            Section *s = doc->section(index);
            if(!s || !s->get_style(SectionGenerated))
              break;

            doc->remove(index, index + 1);
            if(doc->selection_box() == doc
            && doc->selection_start() > index){
              doc->select(doc,
                doc->selection_start()-1,
                doc->selection_end()  -1);
            }
          }
        }
      }
    }

    print_pos = old_job;
  }

  bool more = false;
  if(data == PMATH_SYMBOL_ABORTED){
    while(session->jobs.get(&job)){
      if(job){
        job->end();
        job->dequeued();
      }
    }
  }

  while(session->jobs.get(&session->current_job)){
    if(session->current_job){
      old_job = print_pos;

      if(session->current_job->start()){
        print_pos = session->current_job->position();

        more = true;
        break;
      }
    }

    session->current_job = 0;
  }

  if(!more){
    for(unsigned int count = 0, i = 0;count < all_document_ids.size();++i){
      if(all_document_ids.entry(i)){
        ++count;

        Document *doc = dynamic_cast<Document*>(
          Box::find(all_document_ids.entry(i)->key));

        assert(doc);

        for(int s = 0;s < doc->count();++s){
          MathSection *math = dynamic_cast<MathSection*>(doc->section(s));

          if(math && math->get_style(ShowAutoStyles)){
            math->invalidate();
          }
        }
      }
    }
  }

  Application::eval_cache.clear();
}

static void cnt_return(Expr data){
  if(session->current_job){
    session->current_job->returned(data);
  }
}

static void cnt_returnbox(Expr data){
  if(session->current_job){
    session->current_job->returned_boxes(data);
  }
}

static void cnt_printsection(Expr data){
  Application::gui_print_section(data);
}

static Expr cnt_getdocuments(){
  Gather gather;

  for(unsigned int count = 0, i = 0;count < all_document_ids.size();++i)
    if(all_document_ids.entry(i)){
      ++count;
      Gather::emit(
        Call(Symbol(PMATH_SYMBOL_FRONTENDOBJECT),
          all_document_ids.entry(i)->key));
    }

  return gather.end();
}

static void cnt_menucommand(Expr data){
  bool (*func)(Expr);

  func = menu_commands[data];
  if(func && func(data))
    return;

  func = menu_commands[data[0]];
  if(func && func(data))
    return;

  // ...
}

static void cnt_addconfigshaper(Expr data){
  SharedPtr<ConfigShaperDB> db = ConfigShaperDB::load_from_object(data);

  if(db){
    SharedPtr<ConfigShaperDB> *olddb;

    olddb = ConfigShaperDB::registered.search(db->shaper_name);
    if(olddb){
      olddb->ptr()->clear_cache();
    }

    ConfigShaperDB::registered.set(db->shaper_name, db);
    MathShaper::available_shapers.set(db->shaper_name, db->find(NoStyle));

    pmath_debug_print_object("loaded ", db->shaper_name.get(), "\n");
  }
  else{
    pmath_debug_print("adding config shaper failed.\n");
  }
}

static Expr cnt_getoptions(Expr data){
  Box *obj = Box::find(data);

  if(obj){
    Gather gather;

    if(obj->style)
      obj->style->emit_to_pmath(0 != dynamic_cast<Section*>(obj), true);

    return gather.end();
  }

  return Symbol(PMATH_SYMBOL_FAILED);
}

static void cnt_dynamicupate(Expr data){
  for(size_t i = data.expr_length();i > 0;--i){
    Expr id_obj = data[i];

    if(id_obj.is_int32()){
      Box *box = Box::find(PMATH_AS_INT32(id_obj.get()));

      if(box){
        if(dynamic_update_delay > 0){
          pending_dynamic_updates.set(box->id(), Void());
        }
        else{
          box->dynamic_updated();
        }
      }
    }
  }
}

static Expr cnt_createdocument(Expr data){
  // CreateDocument({sections...})

  Document *doc = 0;

  #ifdef RICHMATH_USE_WIN32_GUI
  {
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;

    doc = get_current_document();
    if(doc){
      Win32Widget *wid = dynamic_cast<Win32Widget*>(doc->native());
      if(wid){
        HWND hwnd = wid->hwnd();
        while(GetParent(hwnd) != NULL)
          hwnd = GetParent(hwnd);

        RECT rect;
        if(GetWindowRect(hwnd, &rect)){
          x = rect.left + GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CXSIZEFRAME);
          y = rect.top  + GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYSIZEFRAME);
        }
      }
    }

    Win32DocumentWindow *wnd = new Win32DocumentWindow(
      new Document,
      0, WS_OVERLAPPEDWINDOW,
      x,
      y,
      500,
      550);
    wnd->init();

    doc = wnd->document();

    ShowWindow(wnd->hwnd(), SW_SHOWNORMAL);
  }
  #endif

  #ifdef RICHMATH_USE_GTK_GUI
  {
    MathGtkDocumentWindow *wnd = new MathGtkDocumentWindow();
    wnd->init();

    doc = wnd->document();

    if(wnd->widget())
      gtk_window_present(GTK_WINDOW(wnd->widget()));
  }
  #endif

  if(doc){
    if(data.expr_length() >= 1){
      Expr sections = data[1];
      if(sections[0] != PMATH_SYMBOL_LIST)
        sections = List(sections);

      for(size_t i = 1;i <= sections.expr_length();++i){
        Expr item = sections[i];

        if(item[0] == PMATH_SYMBOL_SECTION){
          Section *sect = Section::create_from_object(item);
          if(sect)
            doc->insert(doc->length(), sect);
        }
        else{
          item = Call(Symbol(PMATH_SYMBOL_SECTION),
            Call(Symbol(PMATH_SYMBOL_BOXDATA),
              Application::interrupt(Call(Symbol(PMATH_SYMBOL_TOBOXES), item))),
            String("Input"));

          Section *sect = Section::create_from_object(item);
          if(sect)
            doc->insert(doc->length(), sect);
        }
      }
    }

    return Call(Symbol(PMATH_SYMBOL_FRONTENDOBJECT), doc->id());
  }

  return Symbol(PMATH_SYMBOL_FAILED);
}

static Expr cnt_currentvalue(Expr data){
  Expr item;
  Box *box = 0;

  if(data.expr_length() == 1){
    box = Box::find(Dynamic::current_evaluation_box_id);
    item = data[1];
  }
  else if(data.expr_length() == 2){
    box = Box::find(data[1]);
    item = data[2];
  }
  else
    return Symbol(PMATH_SYMBOL_FAILED);

  Document *doc = box->find_parent<Document>(true);

  if(String(item).equals("MouseOver")){
    if(box && doc){
      if(!box->style)
        box->style = new Style();

      box->style->set(InternalUsesCurrentValueOfMouseOver, true);

      Box *mo = Box::find(doc->mouseover_box_id());
      while(mo && mo != box)
        mo = mo->parent();

      if(mo)
        return Symbol(PMATH_SYMBOL_TRUE);
    }

    return Symbol(PMATH_SYMBOL_FALSE);
  }

//  if(String(item).equals("MousePosition")){
//    if(box && doc){
//      if(!box->style)
//        box->style = new Style();
//
//      box->style->set(InternalUsesCurrentValueOfMousePosition, true);
//
//      MouseEvent ev;
//      if(doc->native()->cursor_position(&ev.x, &ev.y)){
//        ev.set_source(box);
//
//        return List(ev.x, ev.y);
//      }
//    }
//
//    return Symbol(PMATH_SYMBOL_NONE);
//  }

  return Symbol(PMATH_SYMBOL_FAILED);
}

static void execute(ClientNotification &cn){
  switch(cn.type){
    case CNT_STARTSESSION:
      cnt_startsession();
      break;

    case CNT_ENDSESSION:
      cnt_endsession();
      break;

    case CNT_END:
      cnt_end(cn.data);
      break;

    case CNT_RETURN:
      cnt_return(cn.data);
      break;

    case CNT_RETURNBOX:
      cnt_returnbox(cn.data);
      break;

    case CNT_PRINTSECTION:
      cnt_printsection(cn.data);
      break;

    case CNT_GETDOCUMENTS:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_getdocuments().release();
      break;

    case CNT_MENUCOMMAND:
      cnt_menucommand(cn.data);
      break;

    case CNT_ADDCONFIGSHAPER:
      cnt_addconfigshaper(cn.data);
      break;

    case CNT_GETOPTIONS:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_getoptions(cn.data).release();
      break;

    case CNT_DYNAMICUPDATE:
      cnt_dynamicupate(cn.data);
      break;

    case CNT_CREATEDOCUMENT:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_createdocument(cn.data).release();
      break;

    case CNT_CURRENTVALUE:
      if(cn.result_ptr)
        *cn.result_ptr = cnt_currentvalue(cn.data).release();
      break;
  }

  cn.done();
}
