#include <eval/server.h>

#include <cstdio>

#include <eval/application.h>
#include <util/concurrent-queue.h>

using namespace richmath;

Expr richmath::to_boxes(Expr obj) {
  return Expr(
           pmath_evaluate(
             pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_TOBOXES), 1,
               pmath_ref(obj.get()))));
}

Expr richmath::generate_section(String style, Expr boxes) {
  Gather gather;
  
  Gather::emit(Call(Symbol(PMATH_SYMBOL_BOXDATA), boxes));
  Gather::emit(style);
  
  Gather::emit(
    Rule(Symbol(PMATH_SYMBOL_SECTIONGENERATED), Symbol(PMATH_SYMBOL_TRUE)));
    
  if(style.equals("Output")) {
    Expr line = Application::interrupt(Symbol(PMATH_SYMBOL_LINE));
    Expr dlvl = Application::interrupt(Symbol(PMATH_SYMBOL_DIALOGLEVEL));
    
    if(line == PMATH_UNDEFINED)
      line = Symbol(PMATH_SYMBOL_ABORTED);
      
    if(dlvl == PMATH_UNDEFINED)
      dlvl = Symbol(PMATH_SYMBOL_ABORTED);
      
    String label = String("out [");
    if(dlvl == PMATH_FROM_INT32(1))
      label = String("(Dialog) ") + label;
    else if(dlvl != PMATH_FROM_INT32(0))
      label = String("(Dialog ") + dlvl.to_string() + String(") ") + label;
      
    label = label + line.to_string() + "]:";
    
    Gather::emit(Rule(Symbol(PMATH_SYMBOL_SECTIONLABEL), label));
  }
  
  Expr result = gather.end();
  result.set(0, Symbol(PMATH_SYMBOL_SECTION));
  return result;
}

class LocalServer: public Server {
  public:
    class Token {
      public:
        typedef enum {Normal, Aborted, Returned} ResultKind;
      public:
        Expr object;
        bool boxes;
        
        ResultKind execute(Expr *return_from_dialog) {
          pmath_bool_t aborted = FALSE;
          ResultKind rkind = Normal;
          
          pmath_debug_print("\n[Start token]\n");
          
          pmath_resume_all();
          
          if(boxes) {
            if(object.is_expr() && object[0].is_null()) {
              for(size_t i = 1; i <= object.expr_length(); ++i) {
                Expr result = Expr(
                                pmath_session_execute(
                                  pmath_expr_new_extended(
                                    pmath_ref(PMATH_SYMBOL_TOEXPRESSION), 1,
                                    pmath_ref(object[i].get())),
                                  &aborted));
                                  
                if( return_from_dialog               &&
                    !aborted                         &&
                    result[0] == PMATH_SYMBOL_RETURN &&
                    result.expr_length() <= 1)
                {
                  *return_from_dialog = result[1];
                  rkind = Returned;
                  break;
                }
                
                Application::notify_wait(CNT_RETURNBOX, to_boxes(result));
              }
            }
            else {
              Expr result = Expr(
                              pmath_session_execute(
                                pmath_expr_new_extended(
                                  pmath_ref(PMATH_SYMBOL_TOEXPRESSION), 1,
                                  pmath_ref(object.get())),
                                &aborted));
                                
              if( return_from_dialog               &&
                  !aborted                         &&
                  result[0] == PMATH_SYMBOL_RETURN &&
                  result.expr_length() <= 1)
              {
                *return_from_dialog = result[1];
                rkind = Returned;
              }
              else
                Application::notify_wait(CNT_RETURNBOX, to_boxes(result));
            }
          }
          else {
            Expr result = Expr(
                            pmath_session_execute(
                              pmath_ref(object.get()),
                              &aborted));
                              
            pmath_debug_print_object("\n[token result=", result.get(), "]\n");
            
            if( return_from_dialog               &&
                !aborted                         &&
                result[0] == PMATH_SYMBOL_RETURN &&
                result.expr_length() <= 1)
            {
              *return_from_dialog = result[1];
              rkind = Returned;
            }
            else
              Application::notify_wait(CNT_RETURN, result);
          }
          
          if(aborted) {
            pmath_debug_print("\n[Aborted token]\n");
            //pmath_resume_all();
            Application::notify(CNT_END, Symbol(PMATH_SYMBOL_ABORTED));
            rkind = Aborted;
          }
          else {
            pmath_debug_print("\n[End token]\n");
            Application::notify(CNT_END, Expr());
          }
          
          return rkind;
        }
    };
    
  public:
    LocalServer() {
      data = new Data(this);
      pmath_atomic_write_release(&data->do_quit, false);
      
      message_queue = Expr(pmath_thread_fork_daemon(
                             LocalServer::thread_proc,
                             LocalServer::kill,
                             data));
                             
      if(message_queue.is_null()) {
        printf("Cannot start server\n");
        delete data;
        data = 0;
      }
    }
    
    virtual ~LocalServer() {
      pmath_atomic_lock(&data_spin);
      if(data) {
        data->owner = 0;
        
//        data->do_quit = true;
//        pmath_abort_please();
//        pmath_thread_wakeup(message_queue.get());
      }
      pmath_atomic_unlock(&data_spin);
    }
    
    virtual void run_boxes(Expr boxes) {
      if(data && !pmath_atomic_read_aquire(&data->do_quit)) {
        Token t;
        
        t.boxes  = true;
        t.object = boxes;
        data->tokens.put(t);
        pmath_thread_wakeup(message_queue.get());
      }
    }
    
    virtual void run(Expr obj) {
      if(data && !pmath_atomic_read_aquire(&data->do_quit)) {
        Token t;
        
        t.boxes  = false;
        t.object = obj;
        data->tokens.put(t);
        pmath_thread_wakeup(message_queue.get());
      }
    }
    
    virtual Expr interrupt_wait(
      Expr           expr,
      double         timeout_seconds,
      pmath_bool_t (*idle_function)(double *end_tick,void *idle_data),
      void          *idle_data
    ) {
      if(data && !pmath_atomic_read_aquire(&data->do_quit)) {
        return Expr(pmath_thread_send_wait(
                      message_queue.get(),
                      expr.release(),
                      timeout_seconds,
                      idle_function,
                      idle_data));
      }
      else
        return Expr();
    }
    
    virtual void interrupt(Expr expr) {
      if(data && !pmath_atomic_read_aquire(&data->do_quit)) {
        pmath_thread_send(message_queue.get(), expr.release());
      }
    }
    
    virtual void abort_all() {
      pmath_abort_please();
    }
    
    virtual bool is_accessable() {
      return data && !pmath_atomic_read_aquire(&data->do_quit);
    }
    
  protected:
    static pmath_atomic_t data_spin;
    Expr message_queue;
    
    class Data {
      public:
        Data(LocalServer *ls)
          : owner(ls)
        {
        }
        
        ~Data() {
          pmath_atomic_lock(&data_spin);
          if(owner) {
            owner->data = 0;
            owner = 0;
          }
          pmath_atomic_unlock(&data_spin);
        }
        
      public:
        LocalServer *owner;
        
        ConcurrentQueue< Token > tokens;
        pmath_atomic_t do_quit;
    };
    
    Data *data;
    
    static void kill(void *arg) {
      Data *me = (Data *)arg;
      pmath_atomic_write_release(&me->do_quit, TRUE);
      
      LocalServer *ls = dynamic_cast<LocalServer *>(local_server.ptr());
      
      if(ls)
        pmath_thread_wakeup(ls->message_queue.get());
    }
    
    static pmath_t builtin_dialog(pmath_expr_t expr) {
      if(pmath_expr_length(expr) > 1) {
        pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
        return expr;
      }
      
      LocalServer *ls = dynamic_cast<LocalServer *>(local_server.ptr());
      
      if(!ls || !ls->is_accessable())
        return expr;
        
      if(pmath_thread_queue_is_blocked_by(pmath_ref(ls->message_queue.get()), pmath_thread_get_queue())) {
        // already in main thread
        
        Expr firsteval;
        if(pmath_expr_length(expr) == 1) {
          firsteval = Expr(pmath_expr_get_item(expr, 1));
        }
        pmath_unref(expr);
        
        return dialog(ls->data, firsteval).release();
      }
      
      // in another thread => send to main thread
      return pmath_thread_send_wait(ls->message_queue.get(), expr, Infinity, NULL, NULL);
    }
    
    static Expr dialog(Data *me, Expr firsteval) {
      static pmath_atomic_t dialog_depth = PMATH_ATOMIC_STATIC_INIT;
      Expr result = Expr();
      
      pmath_atomic_fetch_add(&dialog_depth, 1);
      {
        pmath_t old_dialog = pmath_session_start();
        Application::notify(CNT_STARTSESSION, Expr());
        
        firsteval = Expr(pmath_evaluate(firsteval.release()));
        if( firsteval[0] == PMATH_SYMBOL_RETURN &&
            firsteval.expr_length() <= 1)
        {
          result = firsteval[1];
          goto FINISH;
        }
        
        while(!pmath_atomic_read_aquire(&me->do_quit)) {
          pmath_thread_sleep();
          
          pmath_debug_print("S");
          
          Token token;
          while(me->tokens.get(&token)) {
            Token::ResultKind rk = token.execute(&result);
            
            if(rk == Token::Returned)
              goto FINISH;
              
            if(rk == Token::Aborted) {
              // clear remaining tokens:
              while(me->tokens.get(&token)) {
                Application::notify(CNT_END, Symbol(PMATH_SYMBOL_ABORTED));
              }
            }
          }
        }
        
      FINISH:
      
        Application::notify(CNT_ENDSESSION, Expr());
        pmath_session_end(old_dialog);
      }
      pmath_atomic_fetch_add(&dialog_depth, -1);
      
      if(pmath_atomic_read_aquire(&me->do_quit)) {
        pmath_abort_please();
      }
      
      return result;
    }
    
    static void thread_proc(void *arg)
    {
      Data *me = (Data *)arg;
      
      pmath_register_code(PMATH_SYMBOL_DIALOG, builtin_dialog, PMATH_CODE_USAGE_DOWNCALL);
      
      while(!pmath_atomic_read_aquire(&me->do_quit)) {
        pmath_thread_sleep();
        
        pmath_debug_print("s");
        
        Token token;
        while(me->tokens.get(&token)) {
          Token::ResultKind rk = token.execute(0);
          
          if(rk == Token::Aborted) {
            // clear remaining tokens:
            while(me->tokens.get(&token)) {
              Application::notify(CNT_END, Symbol(PMATH_SYMBOL_ABORTED));
            }
          }
        }
      }
      
      delete me;
    }
};

pmath_atomic_t LocalServer::data_spin = PMATH_ATOMIC_STATIC_INIT;

bool Server::init_local_server() {
  local_server = new LocalServer;
  return local_server->is_accessable();
}

SharedPtr<Server> Server::local_server = 0;
