#ifndef RICHMATH__EVAL__PARTIAL_DYNAMIC_H__INCLUDED
#define RICHMATH__EVAL__PARTIAL_DYNAMIC_H__INCLUDED

#include <util/pmath-extra.h>

namespace richmath {
  class StyledObject;
  
  class PartialDynamic {
      class Impl;
    public:
      PartialDynamic();
      PartialDynamic(StyledObject *owner, Expr expr);
      
      void operator=(Expr expr);
      
      StyledObject *owner() { return _owner; }
      Expr expr() const { return _held_expr[1];  }
      
      Expr get_value_now();
      void get_value_later(Expr job_info);
      bool get_value(Expr *result, Expr job_info);
      Expr finish_dynamic(Expr dyn_eval_result);
      
    private:
      Expr _dyn_eval_template;
      Expr _held_expr;
      StyledObject *_owner;
  };
}

#endif // RICHMATH__EVAL__PARTIAL_DYNAMIC_H__INCLUDED
