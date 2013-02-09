#ifndef __EVAL__SERVER_H__
#define __EVAL__SERVER_H__

#include <util/pmath-extra.h>
#include <util/sharedptr.h>


namespace richmath {
  Expr to_boxes(Expr obj);
  Expr generate_section(String style, Expr boxes);
  
  class Server: public Shareable {
    public:
      virtual void run_boxes(Expr boxes) = 0;
      
      virtual void run(Expr obj) = 0;
      
      virtual Expr interrupt_wait(
        Expr           expr, 
        double         timeout_seconds, 
        pmath_bool_t (*idle_function)(double *end_tick,void *idle_data), 
        void          *idle_data) = 0;
      virtual void interrupt(Expr expr) = 0;
      
      virtual void abort_all() = 0;
      
      virtual bool is_accessable() = 0;
      
      static bool init_local_server();
      static SharedPtr<Server> local_server;
  };
}

#endif // __EVAL__SERVER_H__
