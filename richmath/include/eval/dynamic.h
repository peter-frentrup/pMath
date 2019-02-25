#ifndef __PMATHRICHMATH__EVAL__DYNAMIC_H__INCLUDED
#define __PMATHRICHMATH__EVAL__DYNAMIC_H__INCLUDED

#include <boxes/box.h>

extern pmath_symbol_t richmath_System_Dynamic;

namespace richmath {
  class Dynamic: public Base {
      friend class DynamicImpl;
    public:
      Dynamic();
      Dynamic(Box *owner, Expr expr);
      
      // when default constructor is used, init() must be called before use
      void init(Box *owner, Expr expr);
      
      Expr operator=(Expr expr);
      
      bool has_pre_or_post_assignment();
      
      void assign(Expr value) { assign(std::move(value), true, true, true); }
      void assign(Expr value, bool pre, bool middle, bool post);
      Expr get_value_now();
      void get_value_later() { get_value_later(Expr()); }
      void get_value_later(Expr job_info);
      bool get_value(Expr *result) { return get_value(result, Expr()); }
      bool get_value(Expr *result, Expr job_info);
      
      Box *owner() { return _owner; }
      Expr expr() const {  return _expr;  }
      
      bool is_dynamic() { return is_dynamic(_expr); }
      static bool is_dynamic(Expr expr) {
        if(!expr.is_expr())
          return false;
        
        size_t len = expr.expr_length();
        Expr head = expr[0];
        return (head == ::richmath_System_Dynamic && len >= 1) ||
               (head == PMATH_SYMBOL_PUREARGUMENT && len == 1);
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
