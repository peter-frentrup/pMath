#define _WIN32_WINNT 0x0600

#include <eval/client.h>

#include <cmath>
#include <cstdio>

#include <boxes/section.h>

#include <graphics/config-shaper.h>

#include <gui/win32/basic-win32-widget.h>
#include <gui/document.h>

#include <eval/binding.h>
#include <eval/job.h>
#include <eval/server.h>

#include <util/concurrent-queue.h>
#include <util/semaphore.h>

#include <windows.h>
#include <resources.h>

using namespace richmath;

#define WM_CLIENTNOTIFY  (WM_USER + 1)
#define WM_ADDJOB        (WM_USER + 2)

namespace{
  class ClientNotification {
    public:
      ClientNotification(): finished(0), result_ptr(0) {}
      
      void done(){
        if(finished){
          result_ptr = NULL;
          *finished = true;
          pmath_thread_wakeup(notify_queue.get());
        }
      }
      
    public:  
      ClientNotificationType  type;
      Expr                    data;
      Expr                    notify_queue;
      volatile bool          *finished;
      
      pmath_t *result_ptr;
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

static DWORD  main_thread_id = 0;
static volatile enum ClientState state = Starting;

static ConcurrentQueue<ClientNotification>   notifications;

static SharedPtr<Session>   session = new Session(0);

static Hashtable<Expr, bool (*)(Expr)> menu_commands;
static Hashtable<Expr, bool (*)(Expr)> menu_command_testers;

static void execute(ClientNotification &cn);

static EvaluationPosition print_pos;
static EvaluationPosition old_job;

static HACCEL keyboard_accelerators;
static HWND hwnd_message = HWND_MESSAGE;
static Expr main_message_queue;

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
          case WM_CLIENTNOTIFY: {
            ClientNotification cn;
            
            if(notifications.get(&cn) && session)
              execute(cn);
          } return 0;
          
          case WM_ADDJOB: {
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
          } return 0;
        }
      }
      
      return BasicWin32Widget::callback(message, wParam, lParam);
    }
};

static ClientInfoWindow info_window;

double Client::edit_interrupt_timeout = 2.0;
double Client::interrupt_timeout      = 0.3;
double Client::button_timeout         = 4.0;
double Client::dynamic_timeout        = 4.0;
String Client::application_filename;
String Client::application_directory;

Hashtable<Expr, Expr, object_hash> Client::eval_cache;

void Client::notify(ClientNotificationType type, Expr data){
  if(state == Quitting)
    return;
  
  ClientNotification cn;
  cn.type = type;
  cn.data = data;
  
  notifications.put(cn);
  pmath_thread_wakeup(main_message_queue.get());
  PostMessage(info_window.hwnd(), WM_CLIENTNOTIFY, 0, 0);
}

Expr Client::notify_wait(ClientNotificationType type, Expr data){
  if(state != Running)
    return Symbol(PMATH_SYMBOL_FAILED);
    
  if(GetCurrentThreadId() == main_thread_id){
    notify(type, data);
    return Symbol(PMATH_SYMBOL_FAILED);
  }
  
  volatile bool finished = false;
  pmath_t result = PMATH_UNDEFINED;
  ClientNotification cn;
  cn.finished = &finished;
  cn.notify_queue = Expr(pmath_thread_get_queue());
  cn.type = type;
  cn.data = data;
  cn.result_ptr = &result;
  
  notifications.put(cn);
  pmath_thread_wakeup(main_message_queue.get());
  PostMessage(info_window.hwnd(), WM_CLIENTNOTIFY, 0, 0);
  
  while(!finished){
    pmath_thread_sleep();
    pmath_debug_print("w");
  }
  pmath_debug_print("W");
  
  return Expr(result);
}

bool Client::is_menucommand_runnable(Expr cmd){
  bool (*func)(Expr);
  
  func = menu_command_testers[cmd];
  if(func && !func(cmd))
    return false;
  
  func = menu_command_testers[cmd];
  if(func && !func(cmd))
    return false;
  
  return true;
}

void Client::register_menucommand(
  Expr cmd,
  bool (*func)(Expr cmd), 
  bool (*test)(Expr cmd)
){
  if(!cmd.is_valid()){
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

  static void write_data(FILE *file, const uint16_t *data, int len){
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

void Client::gui_print_section(Expr expr){
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
        
        doc->remove(index, index + 1);
      }
    }
    else
      index = doc->length();
    
    sect = Section::create_from_object(expr);
    if(sect){
      doc->insert(index, sect);
      
      print_pos = EvaluationPosition(sect);
    
      if(doc->selection_box() == doc
      && doc->selection_start() >= index
      && doc->selection_end() >= index)
        doc->move_to(doc, index + 1);
    }
  }
  else{
    printf("\n");
    pmath_write(expr.get(), 0, (pmath_write_func_t)write_data, stdout);
    printf("\n");
  }
}

void Client::init(){
  main_message_queue = Expr(pmath_thread_get_queue());
  
  main_thread_id = GetCurrentThreadId();
  if(!info_window.hwnd())
    PostQuitMessage(1);
  
  Array<WCHAR> filename(256);
  while(GetModuleFileNameW(0, filename.items(), filename.length()) == (DWORD)filename.length()
  && GetLastError() == ERROR_INSUFFICIENT_BUFFER
  && filename.length() < 4096){
    filename.length(2 * filename.length());
  }
  
  filename[filename.length() - 1] = L'\0';
  
  application_filename = String::FromUcs2((uint16_t*)filename.items());
  int i = application_filename.length() - 1;
  while(i > 0 && application_filename[i] != '\\')
    --i;
  
  if(i > 0)
    application_directory = application_filename.part(0, i);
  else
    application_directory = application_filename;
  
  keyboard_accelerators = LoadAcceleratorsW(GetModuleHandle(0), MAKEINTRESOURCEW(ACC_TABLE));
}

void Client::doevents(){
  MSG msg;
  
  ClientState old_state = state;
  state = Running;
  
  while(PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)){
    if(!TranslateAcceleratorW(GetFocus(), keyboard_accelerators, &msg)){
      TranslateMessage(&msg); 
      DispatchMessageW(&msg); 
    }
  }
  
  state = old_state;
}

int Client::run(){
  MSG msg;
  BOOL bRet;
  
  if(state == Running)
    return 1;
  
  state = Running;
  
  while((bRet = GetMessageW(&msg, NULL, 0, 0)) != 0){
    if(bRet == -1) {
      state = Quitting;
      return 1;
    }
    
    if(!TranslateAcceleratorW(GetFocus(), keyboard_accelerators, &msg)){
      TranslateMessage(&msg); 
      DispatchMessageW(&msg); 
    }
  }
  
  state = Quitting;
  return msg.wParam;
}

void Client::done(){
  DestroyAcceleratorTable(keyboard_accelerators);
  while(session){
    SharedPtr<Job> job = session->current_job;
    session->current_job = 0;
    if(job){
      job->end();
    }
    
    while(session->jobs.get(&job)){
      if(job)
        job->end();
    }
    
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

void Client::add_job(SharedPtr<Job> job){
  if(session && job){
    session->jobs.put(job);
    job->enqueued();
    PostMessage(info_window.hwnd(), WM_ADDJOB, 0, 0);
  }
}

void Client::abort_all_jobs(){
  Server::local_server->interrupt(Call(Symbol(PMATH_SYMBOL_ABORT)));
  
  if(session){
    SharedPtr<Job> job = session->current_job;
    session->current_job = 0;
    if(job){
      job->end();
    }
    
    while(session->jobs.get(&job)){
      if(job)
        job->end();
    }
  }
}

bool Client::is_idle(){
  return !session->current_job.is_valid();
}

bool Client::is_idle(int document_id){
  SharedPtr<Session> s = session;
  
  while(s){
    SharedPtr<Job> job = s->current_job;
    if(job && job->position().document_id == document_id)
      return false;
    
    s = s->next;
  }
  
  return true;
}

  static void interrupt_wait_idle(void *data){
    // do nothing currently
    // We cannot process all notifications here, because boxes must not be 
    // changed when we hit this code path (we could be between resize() and 
    // paint() of a box)
    // On the other hand, some blocking notifications should be regarded e.g. to 
    // answer questions to the frontend (Documents() calls and more)
  }
  
Expr Client::interrupt(Expr expr, double seconds){
  return Server::local_server->interrupt_wait(expr, seconds, interrupt_wait_idle, NULL);
}

Expr Client::interrupt(Expr expr){
  return interrupt(expr, interrupt_timeout);
}

void Client::execute_for(Expr expr, Box *box, double seconds){
//  if(box)
//    print_pos = EvaluationPosition(box);
  
  Server::local_server->interrupt(expr, seconds);
}

void Client::execute_for(Expr expr, Box *box){
  execute_for(expr, box, interrupt_timeout);
}

Expr Client::interrupt_cached(Expr expr, double seconds){
  if(!expr.instance_of(PMATH_TYPE_SYMBOL | PMATH_TYPE_EXPRESSION))
    return expr;
  
  Expr *cached = eval_cache.search(expr);
  if(cached)
    return *cached;
  
  Expr result = interrupt(expr, seconds);
  if(result != PMATH_SYMBOL_ABORTED
  && result != PMATH_SYMBOL_FAILED)
    eval_cache.set(expr, result);
  else if(result == PMATH_SYMBOL_ABORTED)
    MessageBeep(-1);
    
  return result;
}

Expr Client::interrupt_cached(Expr expr){
  return interrupt_cached(expr, interrupt_timeout);
}

static void execute(ClientNotification &cn){
  switch(cn.type){
    case CNT_STARTSESSION: {
      if(session->current_job){
        Section *sect = dynamic_cast<Section*>(
          Box::find(session->current_job->position().section_id));
        
        if(sect){
          sect->dialog_start = true;
          sect->request_repaint_all();
        }
      }
      
      session = new Session(session);
    } break;
    
    case CNT_ENDSESSION: {
      SharedPtr<Job> job;
      while(session->jobs.get(&job)){
        if(job){
          job->end();
        }
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
    } break;
    
    case CNT_END: {
      SharedPtr<Job> job = session->current_job;
      session->current_job = 0;
      
      if(job){
        job->end();
        
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
              }
            }
          }
        }
        
        print_pos = old_job;
      }
      
      bool more = false;
      if(cn.data == PMATH_SYMBOL_ABORTED){
        while(session->jobs.get(&job)){
          if(job)
            job->end();
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
      
      Client::eval_cache.clear();
    } break;
    
    case CNT_RETURN: {
      if(session->current_job){
        session->current_job->returned(cn.data);
      }
    } break;
    
    case CNT_RETURNBOX: {
      if(session->current_job){
        session->current_job->returned_boxes(cn.data);
      }
    } break;
    
    case CNT_PRINTSECTION: {
      Client::gui_print_section(cn.data);
    } break;
  
    case CNT_GETDOCUMENTS: {
      if(cn.result_ptr){
        pmath_gather_begin(NULL);
        
        for(unsigned int count = 0, i = 0;count < all_document_ids.size();++i)
          if(all_document_ids.entry(i)){
            ++count;
            pmath_emit(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_FRONTENDOBJECT), 1,
                pmath_integer_new_si(
                  all_document_ids.entry(i)->key)),
              NULL);
          }
        
        *cn.result_ptr = pmath_gather_end();
      }
    } break;
  
    case CNT_MENUCOMMAND: {
      bool (*func)(Expr);
      
      func = menu_commands[cn.data];
      if(func && func(cn.data))
        break;
      
      func = menu_commands[cn.data[0]];
      if(func && func(cn.data))
        break;
      
    } break;
    
    case CNT_ADDCONFIGSHAPER: {
      SharedPtr<ConfigShaperDB> db = ConfigShaperDB::load_from_object(cn.data);
      
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
        pmath_debug_print_object("failed with ", cn.data.get(), "\n");
      }
    } break;
  
    case CNT_GETOPTIONS: {
      if(cn.result_ptr){
        Box *obj = Box::find(cn.data);
        
        if(obj){
          pmath_gather_begin(NULL);
          
          if(obj->style)
            obj->style->emit_to_pmath(0 != dynamic_cast<Section*>(obj), true);
          
          *cn.result_ptr = pmath_gather_end();
        }
        else
          *cn.result_ptr = pmath_ref(PMATH_SYMBOL_FAILED);
      }
    } break;
    
    case CNT_DYNAMICUPDATE: {
      for(size_t i = cn.data.expr_length();i > 0;--i){
        Expr id_obj = cn.data[i];
        
        if(id_obj.instance_of(PMATH_TYPE_INTEGER)
        && pmath_integer_fits_si(id_obj.get())){
          Box *box = Box::find(pmath_integer_get_si(id_obj.get()));
          
          if(box)
            box->dynamic_updated();
        }
      }
    } break;
  }
  
  cn.done();
//  if(cn.sem)
//    cn.sem->post();
}
