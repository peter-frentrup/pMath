#ifndef __PMATH__EVAL__DYNAMIC_H__
#define __PMATH__EVAL__DYNAMIC_H__

#include <boxes/box.h>

namespace richmath{
  class Dynamic: public Base {
    public:
      Dynamic();
      Dynamic(Box *owner, Expr expr);
      
      // when default constructor is used, init() must be called before use
      void init(Box *owner, Expr expr);
      
      Expr operator=(Expr expr);
      
      void assign(Expr value);
      Expr get_value_now();
      void get_value_later();
      bool get_value(Expr *result);
      
      Box *owner(){ return _owner; }
      Expr expr(){  return _expr;  }
      
      bool is_dynamic(){ 
        return _expr[0] == PMATH_SYMBOL_DYNAMIC
            && _expr.expr_length() >= 1; 
      }
    
    private:
      Box *_owner;
      Expr _expr;
      
      int  synchronous_updating; // 0 = False, 1 = True, 2 = Automatic
  };
};

#endif // __PMATH__EVAL__DYNAMIC_H__
