#ifndef RICHMATH__EVAL__CURRENT_VALUE_H__INCLUDED
#define RICHMATH__EVAL__CURRENT_VALUE_H__INCLUDED


#include <util/pmath-extra.h>

namespace richmath {
  class FrontEndObject;
  
  class CurrentValueImpl;
  class CurrentValue final {
      friend class CurrentValueImpl;
      using Impl = CurrentValueImpl;
    public:
      static void init();
      static void done();
      
      static Expr get(Expr item);
      static Expr get(FrontEndObject *obj, Expr item);
      static bool put(FrontEndObject *obj, Expr item, Expr rhs);
      
      static bool register_provider(
        Expr   item,
        Expr (*get)(FrontEndObject *obj, Expr item),
        bool (*put)(FrontEndObject *obj, Expr item, Expr rhs) = nullptr);
      
    private:
      CurrentValue() = delete;
      CurrentValue(const CurrentValue&) = delete;
      
  };
}

#endif // RICHMATH__EVAL__CURRENT_VALUE_H__INCLUDED
