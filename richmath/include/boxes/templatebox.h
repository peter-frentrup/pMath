#ifndef RICHMATH_BOXES_TEMPLATEBOX_H_INCLUDED
#define RICHMATH_BOXES_TEMPLATEBOX_H_INCLUDED

#include <boxes/ownerbox.h>


namespace richmath {
  class TemplateBox: public OwnerBox {
      typedef OwnerBox base;
      friend class TemplateBoxImpl;
      
    public:
      TemplateBox();
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void resize(Context *context) override;
      virtual void paint_content(Context *context) override;
      
      virtual Expr to_pmath_symbol() override { return Symbol(PMATH_SYMBOL_TEMPLATEBOX); }
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      const Expr &arguments() { return _arguments; }
      
    private:
      Expr _arguments;
      Expr _tag;
      Expr _display_function;
  };
  
  class TemplateBoxSlot: public OwnerBox {
      typedef OwnerBox base;
      friend class TemplateBoxSlotImpl;
      
    public:
      TemplateBoxSlot();
      
      static Expr prepare_boxes(Expr boxes);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void resize(Context *context) override;
      virtual void paint_content(Context *context) override;
      
    private:
      int _argument;
      bool _is_content_loaded;
  };
  
  class TemplateBoxSlotSequence: public OwnerBox {
      typedef OwnerBox base;
      friend class TemplateBoxSlotSequenceImpl;
      
    public:
      TemplateBoxSlotSequence();
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
    
      virtual void resize(Context *context) override;
      virtual void paint_content(Context *context) override;
      
    private:
      Expr _separator;
      Expr _modifier_function;
      int _first_arg;
      int _last_arg;
      bool _is_content_loaded;
  };
}

#endif // RICHMATH_BOXES_TEMPLATEBOX_H_INCLUDED
