#ifndef __BOYES__DYNAMICLOCALBOX_H__
#define __BOYES__DYNAMICLOCALBOX_H__

#include <boxes/dynamicbox.h>


namespace richmath {
  class DynamicLocalBox: public AbstractDynamicBox {
    public:
      DynamicLocalBox();
      virtual ~DynamicLocalBox();
      
      // Box::try_create<DynamicLocalBox>(expr, options)
      virtual bool try_load_from_object(Expr expr, int options);
      
      virtual void paint(Context *context);
      
      virtual Expr to_pmath_symbol() { return Symbol(PMATH_SYMBOL_DYNAMICLOCALBOX); }
      virtual Expr to_pmath(int flags);
      
      virtual Expr prepare_dynamic(Expr expr);
      
    protected:
      void ensure_init();
      void emit_values(Expr symbol);
      
    private:
      Expr _public_symbols;
      Expr _private_symbols;
      
      Expr _initialization;
      Expr _deinitialization;
      Expr _unsaved_variables;
      
      Expr _init_call;
  };
}

#endif // __BOYES__DYNAMICLOCALBOX_H__
