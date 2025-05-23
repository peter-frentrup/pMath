#ifndef RICHMATH__EVAL__DYNAMIC_H__INCLUDED
#define RICHMATH__EVAL__DYNAMIC_H__INCLUDED

#include <util/pmath-extra.h>
#include <util/style.h>

extern pmath_symbol_t richmath_System_Dynamic;
extern pmath_symbol_t richmath_System_PureArgument;
extern pmath_symbol_t richmath_System_Unevaluated;

namespace richmath {
  class StyledObject;
  
  class Dynamic: public Base {
      class Impl;
    public:
      Dynamic();
      Dynamic(StyledObject *owner, Expr expr);
      
      // when default constructor is used, init() must be called before use
      void init(StyledObject *owner, Expr expr);
      
      Expr operator=(Expr expr);
      
      bool has_pre_or_post_assignment();
      bool has_temporary_assignment();
      
      void assign(Expr value) { assign(PMATH_CPP_MOVE(value), true, true, true); }
      void assign(Expr value, bool pre, bool middle, bool post);
      Expr get_value_unevaluated();
      Expr get_value_now();
      bool try_get_simple_value(Expr *result);
      void get_value_later() { get_value_later(Expr()); }
      void get_value_later(Expr job_info);
      bool get_value(Expr *result) { return get_value(result, Expr()); }
      bool get_value(Expr *result, Expr job_info);
      
      StyledObject *owner() { return _owner; }
      Expr expr() const {  return _expr;  }
      
      bool is_dynamic_of(Expr sym);
      bool is_dynamic() { return is_dynamic(_expr); }
      static bool is_dynamic(Expr expr) {
        if(!expr.is_expr())
          return false;
        
        size_t len = expr.expr_length();
        Expr head = expr[0];
        return (head == ::richmath_System_Dynamic && len >= 1) ||
               (head == richmath_System_PureArgument && len == 1);
      }
      bool is_unevaluated() {
        return _expr.expr_length() == 1 && _expr[0] == richmath_System_Unevaluated;
      }
      
      AutoBoolValues synchronous_updating() { return _synchronous_updating; }
      void synchronous_updating(AutoBoolValues setting) { _synchronous_updating = setting; }
      
      static FrontEndReference current_observer_id;
      
    private:
      Expr _expr;
      StyledObject *_owner;
      
      AutoBoolValues _synchronous_updating;
  };
  
  class AutoResetCurrentObserver {
    public:
      AutoResetCurrentObserver()
        : old_id{Dynamic::current_observer_id}
      {
        Dynamic::current_observer_id = FrontEndReference();
      }
      ~AutoResetCurrentObserver() {
        Dynamic::current_observer_id = old_id;
      }
      
      AutoResetCurrentObserver(const AutoResetCurrentObserver&) = delete;
      AutoResetCurrentObserver &operator=(const AutoResetCurrentObserver&) = delete;

    private:
      FrontEndReference old_id;
  };
};

#endif // RICHMATH__EVAL__DYNAMIC_H__INCLUDED
