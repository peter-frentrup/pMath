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
      
      virtual bool selectable(int i = -1) override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
      
      virtual void resize(Context *context) override;
      virtual void paint_content(Context *context) override;
      
      virtual Expr to_pmath_symbol() override { return Symbol(PMATH_SYMBOL_TEMPLATEBOX); }
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      const Expr &arguments() { return _arguments; }
      
    protected:
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::TemplateBox; }
      
    private:
      Expr _arguments;
      Expr _tag;
      bool _is_content_loaded;
  };
  
  class TemplateBoxSlot: public OwnerBox {
      typedef OwnerBox base;
      friend class TemplateBoxSlotImpl;
      
    public:
      TemplateBoxSlot();
      
      TemplateBox *find_owner();
      
      static Expr prepare_boxes(Expr boxes);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual bool selectable(int i = -1) override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
      
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
      
      TemplateBox *find_owner();
      
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
