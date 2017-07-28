#ifndef __BOYES__DYNAMICLOCALBOX_H__
#define __BOYES__DYNAMICLOCALBOX_H__

#include <boxes/dynamicbox.h>


namespace richmath {
  class DynamicLocalBox: public AbstractDynamicBox {
    public:
      DynamicLocalBox();
      virtual ~DynamicLocalBox();
      
      // Box::try_create<DynamicLocalBox>(expr, options)
      virtual bool try_load_from_object(Expr expr, BoxOptions options) override;
      
      virtual void paint(Context *context) override;
      
      virtual Expr to_pmath_symbol() override { return Symbol(PMATH_SYMBOL_DYNAMICLOCALBOX); }
      virtual Expr to_pmath(BoxFlags flags) override;
      
      virtual Expr prepare_dynamic(Expr expr) override;
      
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
