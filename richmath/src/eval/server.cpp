#include <eval/server.h>

#include <cstdio>

#ifdef _WIN32
  #include <windows.h>
  #include <process.h>
#else
  #include <pthread.h>
#endif

#include <eval/client.h>
#include <util/concurrent-queue.h>

using namespace richmath;

Expr richmath::to_boxes(Expr obj){
  return Expr(
    pmath_evaluate(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TOBOXES), 1,
        pmath_ref(obj.get()))));
}

Expr richmath::generate_section(String style, Expr boxes){
  pmath_gather_begin(0);
  
  pmath_emit(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_BOXDATA), 1, 
      boxes.release()), 0);
  pmath_emit(pmath_ref(style.get()), 0);
  
  pmath_emit(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_RULE), 2,
      pmath_ref(PMATH_SYMBOL_SECTIONGENERATED),
      pmath_ref(PMATH_SYMBOL_TRUE)),
    0);
  
  if(style.equals("Output")){
    Expr line = Client::interrupt(Symbol(PMATH_SYMBOL_LINE));
    
    Expr dlvl = Client::interrupt(Symbol(PMATH_SYMBOL_DIALOGLEVEL));
    
    String label = String("out [");
    if(dlvl.instance_of(PMATH_TYPE_INTEGER)){
      if(pmath_integer_fits_ui(dlvl.get())){
        unsigned long ui = pmath_integer_get_ui(dlvl.get());
        
        if(ui == 1)
          label = String("(Dialog) ") + label;
        else if(ui != 0)
          label = String("(Dialog ") + dlvl.to_string() + String(") ") + label;
      }
      else
        label = String("(Dialog ") + dlvl.to_string() + String(") ") + label;
    }
    else
      label = String("(Dialog ") + dlvl.to_string() + String(") ") + label;
    
    label = label + line.to_string() + "]:";
    
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_SECTIONLABEL),
        pmath_ref(label.get())),
      0);
  }
  
  return Expr(
    pmath_expr_set_item(
      pmath_gather_end(), 
      0, 
      pmath_ref(PMATH_SYMBOL_SECTION)));
}

class LocalServer: public Server{
  public:
    class Token{
      public:
        typedef enum {Normal, Aborted, Returned} ResultKind;
      public:
        Expr object;
        bool boxes;
        
        ResultKind execute(Expr *return_from_dialog){
          pmath_bool_t aborted = FALSE;
          ResultKind rkind = Normal;
          
          if(boxes){
            if(object.instance_of(PMATH_TYPE_EXPRESSION)
            && object[0] == (pmath_t)0){
              for(size_t i = 1;i <= object.expr_length();++i){
                Expr result = Expr(
                  pmath_session_execute(
                    pmath_expr_new_extended(
                      pmath_ref(PMATH_SYMBOL_RELEASE), 1,
                      pmath_expr_new_extended(
                        pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1,
                        pmath_ref(object[i].get()))),
                    &aborted));
                
                if(return_from_dialog
                && !aborted
                && result[0] == PMATH_SYMBOL_RETURN
                && result.expr_length() <= 1){
                  *return_from_dialog = result[1];
                  rkind = Returned;
                  break;
                }
                
                Client::notify(CNT_RETURNBOX, to_boxes(result));
              }
            }
            else{
              Expr result = Expr(
                pmath_session_execute(
                  pmath_expr_new_extended(
                    pmath_ref(PMATH_SYMBOL_RELEASE), 1,
                    pmath_expr_new_extended(
                      pmath_ref(PMATH_SYMBOL_BOXESTOEXPRESSION), 1,
                      pmath_ref(object.get()))),
                  &aborted));
            
              if(return_from_dialog
              && !aborted
              && result[0] == PMATH_SYMBOL_RETURN
              && result.expr_length() <= 1){
                *return_from_dialog = result[1];
                rkind = Returned;
              }
              else
                Client::notify(CNT_RETURNBOX, to_boxes(result));
            }
          }
          else{
            Expr result = Expr(
              pmath_session_execute(
                pmath_ref(object.get()),
                &aborted));
        
            if(return_from_dialog
            && !aborted
            && result[0] == PMATH_SYMBOL_RETURN
            && result.expr_length() <= 1){
              *return_from_dialog = result[1];
              rkind = Returned;
            }
            else
              Client::notify(CNT_RETURN, result);
          }
          
          if(aborted){
            Client::notify(CNT_END, Symbol(PMATH_SYMBOL_ABORTED));
            rkind = Aborted;
          }
          else
            Client::notify(CNT_END, Expr());
          
          return rkind;
        }
    };
    
  public:
    LocalServer(){
      data = new Data(this);
      data->do_quit = false;
      
      message_queue = Expr(pmath_thread_fork_daemon(
        LocalServer::thread_proc, 
        LocalServer::kill, 
        data));
      
      if(!message_queue.is_valid()){
        printf("Cannot start server\n");
        delete data;
        data = 0;
      }
    }
    
    virtual ~LocalServer(){
      pmath_atomic_lock(&data_spin);
      if(data){
        data->owner = 0;
        
//        data->do_quit = true;
//        pmath_abort_please();
//        pmath_thread_wakeup(message_queue.get());
      }
      pmath_atomic_unlock(&data_spin);
    }
      
    virtual void run_boxes(Expr boxes){
      if(data && !data->do_quit){
        Token t;
        
        t.boxes  = true;
        t.object = boxes;
        data->tokens.put(t);
        pmath_thread_wakeup(message_queue.get());
      }
    }
    
    virtual void run(Expr obj){
      if(data && !data->do_quit){
        Token t;
        
        t.boxes  = false;
        t.object = obj;
        data->tokens.put(t);
        pmath_thread_wakeup(message_queue.get());
      }
    }
      
    virtual Expr interrupt_wait(Expr expr, double timeout_seconds, void (*idle_function)(void*), void *idle_data){
      if(data && !data->do_quit){
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
      
    virtual void interrupt(Expr expr, double timeout_seconds){
      if(data && !data->do_quit){
        if(timeout_seconds < Infinity){
          expr = Expr(pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_TIMECONSTRAINED), 2,
            expr.release(),
            pmath_float_new_d(timeout_seconds)));
        }
        
        pmath_thread_send(message_queue.get(), expr.release());
      }
    }
    
    virtual bool is_accessable(){
      return data && !data->do_quit;
    }
    
  protected:
    static PMATH_DECLARE_ATOMIC(data_spin);
    Expr message_queue;
    
    static pmath_thread_t main_thread;
    
    class Data {
      public:
        Data(LocalServer *ls)
        : owner(ls)
        {
        }
        
        ~Data(){
          pmath_atomic_lock(&data_spin);
          if(owner){
            owner->data = 0;
            owner = 0;
          }
          pmath_atomic_unlock(&data_spin);
        }
      
      public:
        LocalServer *owner;
        
        ConcurrentQueue< Token > tokens;
        volatile bool do_quit;
    };
    
    Data *data;
    
    static void kill(void *arg){
      Data *me = (Data*)arg;
      me->do_quit = TRUE;
      
      LocalServer *ls = dynamic_cast<LocalServer*>(local_server.ptr());
      
      if(ls)
        pmath_thread_wakeup(ls->message_queue.get());
    }
    
    static pmath_t builtin_dialog(pmath_expr_t expr){
      if(pmath_expr_length(expr) > 1){
        pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
        return expr;
      }
      
      LocalServer *ls = dynamic_cast<LocalServer*>(local_server.ptr());
      
      if(!ls || !ls->is_accessable())
        return expr;
      
      if(pmath_thread_is_parent(main_thread, pmath_thread_get_current())){
        // already in main thread
        
        Expr firsteval;
        if(pmath_expr_length(expr) == 1){
          firsteval = Expr(pmath_expr_get_item(expr, 1));
        }
        pmath_unref(expr);
        
        return dialog(ls->data, firsteval).release();
      }
      
      // in another thread => send to main thread
      return pmath_thread_send_wait(ls->message_queue.get(), expr, Infinity, NULL, NULL);
    }
    
    static Expr dialog(Data *me, Expr firsteval){
      static PMATH_DECLARE_ATOMIC(dialog_depth) = 0;
      Expr result = Expr();
      
      pmath_atomic_fetch_add(&dialog_depth, 1);
      {
        pmath_t old_dialog = pmath_session_start();
        Client::notify(CNT_STARTSESSION, Expr());
        
        firsteval = Expr(pmath_evaluate(firsteval.release()));
        if(firsteval[0] == PMATH_SYMBOL_RETURN
        && firsteval.expr_length() <= 1){
          result = firsteval[1];
          goto FINISH;
        }
        
        while(!me->do_quit){
          pmath_thread_sleep();
          
          printf("S");
          
          Token token;
          while(me->tokens.get(&token)){
            Token::ResultKind rk = token.execute(&result);
            
            if(rk == Token::Returned)
              goto FINISH;
            
            if(rk == Token::Aborted) {
              // clear remaining tokens:
              while(me->tokens.get(&token)){
                Client::notify(CNT_END, Symbol(PMATH_SYMBOL_ABORTED));
              }
            }
          }
        }
        
       FINISH:
        
        Client::notify(CNT_ENDSESSION, Expr());
        pmath_session_end(old_dialog);
      }
      pmath_atomic_fetch_add(&dialog_depth, -1);
      
      return result;
    }
    
    static void thread_proc(void *arg)
    {
      Data *me = (Data*)arg;
      main_thread = pmath_thread_get_current();
      
      pmath_register_code(PMATH_SYMBOL_DIALOG, builtin_dialog, PMATH_CODE_USAGE_DOWNCALL);
      
      while(!me->do_quit){
        pmath_thread_sleep();
        
        printf("S");
        
        Token token;
        while(me->tokens.get(&token)){
          Token::ResultKind rk = token.execute(0);
          
          if(rk == Token::Aborted){
            // clear remaining tokens:
            while(me->tokens.get(&token)){
              Client::notify(CNT_END, Symbol(PMATH_SYMBOL_ABORTED));
            }
          }
        }
      }
      
      delete me;
    }
};

pmath_thread_t LocalServer::main_thread = 0;
PMATH_DECLARE_ATOMIC(LocalServer::data_spin) = 0;

bool Server::init_local_server(){
  local_server = new LocalServer;
  return local_server->is_accessable();
}

SharedPtr<Server> Server::local_server = 0;
