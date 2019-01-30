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
      
      virtual void paint_content(Context *context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
    protected:
      virtual void resize_no_baseline(Context *context) override;
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::TemplateBox; }
      
    public:
      Expr arguments;
      
    private:
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
      
      virtual Expr prepare_dynamic(Expr expr) override;
      
      virtual bool selectable(int i = -1) override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
      
      virtual Box *remove(int *index) override;
      
      virtual void invalidate() override;
      
      virtual void paint_content(Context *context) override;
      
      virtual void on_exit() override;
      virtual void on_finish_editing() override;
      
    protected:
      virtual void resize_no_baseline(Context *context) override;
    
    private:
      int _argument;
      bool _is_content_loaded;
      bool _has_changed_content;
  };
}

#endif // RICHMATH_BOXES_TEMPLATEBOX_H_INCLUDED
