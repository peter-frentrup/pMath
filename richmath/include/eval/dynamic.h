#ifndef __PMATHRICHMATH__EVAL__DYNAMIC_H__INCLUDED
#define __PMATHRICHMATH__EVAL__DYNAMIC_H__INCLUDED

#include <boxes/box.h>

extern pmath_symbol_t richmath_System_Dynamic;

namespace richmath {
  class Dynamic: public Base {
    public:
      Dynamic();
      Dynamic(Box *owner, Expr expr);
      
      // when default constructor is used, init() must be called before use
      void init(Box *owner, Expr expr);
      
      Expr operator=(Expr expr);
      
      Expr pre_assignment_function();
      Expr middle_assignment_function();
      Expr post_assignment_function();
      bool has_pre_or_post_assignment();
      
      void assign(Expr value) { assign(std::move(value), true, true, true); }
      void assign(Expr value, bool pre, bool middle, bool post);
      Expr get_value_now();
      void get_value_later();
      bool get_value(Expr *result);
      
      Box *owner() { return _owner; }
      Expr expr() {  return _expr;  }
      
      bool is_dynamic() {
        return _expr.expr_length() >= 1 && _expr[0] == ::richmath_System_Dynamic;
      }
      bool is_unevaluated() {
        return _expr.expr_length() == 1 && _expr[0] == PMATH_SYMBOL_UNEVALUATED;
      }
      
      // 0 = False, 1 = True, 2 = Automatic
      int synchronous_updating() { return _synchronous_updating; }
      
      static FrontEndReference current_evaluation_box_id;
      
    private:
      Box *_owner;
      Expr _expr;
      
      int  _synchronous_updating; // 0 = False, 1 = True, 2 = Automatic
  };
};

#endif // __PMATHRICHMATH__EVAL__DYNAMIC_H__INCLUDED
