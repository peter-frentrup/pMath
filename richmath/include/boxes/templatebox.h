#ifndef RICHMATH_BOXES_TEMPLATEBOX_H_INCLUDED
#define RICHMATH_BOXES_TEMPLATEBOX_H_INCLUDED

#include <boxes/ownerbox.h>
#include <eval/observable.h>


namespace richmath {
  class TemplateBoxImpl;
  class TemplateBoxSlotImpl;
  
  class TemplateBox final : public Observable, public ExpandableOwnerBox {
      using base = ExpandableOwnerBox;
      friend class TemplateBoxImpl;
      using Impl = TemplateBoxImpl;
      
    public:
      explicit TemplateBox(AbstractSequence *content);
      
      virtual void after_insertion() override;
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual MathSequence *as_inline_span() override;
      
      virtual bool edit_selection(SelectionReference &selection, EditAction action) override;
      virtual bool selectable(int i = -1) override;
      virtual VolatileSelection normalize_selection(int start, int end) override;
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
      virtual void after_inline_span_mouse_selection(Box *top, VolatileSelection &sel, bool &was_inside_start) override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,  // [in/out]
        int              *index,        // [in/out], -1 if called from parent
        bool              called_from_child) override;
      
      virtual void after_paint_inline(Context &context) override;
      virtual void paint_content(Context &context) override;
      
      virtual Expr to_pmath_symbol() override;
      
      virtual void on_mouse_enter() override;
      virtual void on_mouse_exit() override;
      
      void reset_argument(int index, Expr new_arg);
      
      static FrontEndObject *get_current_value_of_TemplateBox(FrontEndObject *obj, Expr item);

    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      virtual void resize_default_baseline(Context &context) override;
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::TemplateBox; }
      
    private:
      enum {
        IsContentLoadedBit = base::NumFlagsBits,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool is_content_loaded() {         return get_flag(IsContentLoadedBit); }
      void is_content_loaded(bool value) {   change_flag(IsContentLoadedBit, value); }
      
      void base_after_insertion() { base::after_insertion(); }
    
    public:
      Expr arguments;
      
    private:
      Expr _tag;
      Expr _cached_display_function;
  };
  
  class TemplateBoxSlot final : public ExpandableOwnerBox {
      using base = ExpandableOwnerBox;
      friend class TemplateBoxSlotImpl;
      using Impl = TemplateBoxSlotImpl;
      
    public:
      explicit TemplateBoxSlot(AbstractSequence *content);
      
      virtual void after_insertion() override;
      
      TemplateBox *find_owner();
      TemplateBox *find_owner_in_same_document();
      int argument() { return _argument; }
      
      static Expr prepare_boxes(Expr boxes);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr prepare_dynamic(Expr expr) override;
      
      virtual MathSequence *as_inline_span() override;
      
      virtual bool edit_selection(SelectionReference &selection, EditAction action) override;
      virtual bool selectable(int i = -1) override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
      
      virtual Box *remove(int *index) override;
      
      virtual float fill_weight() override;
      virtual void invalidate() override;
      virtual void after_paint_inline(Context &context) override;
      virtual void paint_content(Context &context) override;
      
      virtual void on_exit() override;
      virtual void on_finish_editing() override;
      
      virtual Expr to_pmath_symbol() override;
      
      static Expr get_current_value_of_TemplateSlotCount(FrontEndObject *obj, Expr item);
      static Expr get_current_value_of_HeldTemplateSlot(FrontEndObject *obj, Expr item);
      static Expr get_current_value_of_TemplateSlot(FrontEndObject *obj, Expr item);
      static bool put_current_value_of_TemplateSlot(FrontEndObject *obj, Expr item, Expr rhs);

    protected:
      virtual void resize_default_baseline(Context &context) override;
    
      enum {
        IsContentLoadedBit = base::NumFlagsBits,
        HasChangedContentBit,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool is_content_loaded() {         return get_flag(IsContentLoadedBit); }
      void is_content_loaded(bool value) {   change_flag(IsContentLoadedBit, value); }
      bool has_changed_content() {       return get_flag(HasChangedContentBit); }
      void has_changed_content(bool value) { change_flag(HasChangedContentBit, value); }
    
      void base_after_insertion() { base::after_insertion(); }
    
    private:
      int _argument;
  };
}

#endif // RICHMATH_BOXES_TEMPLATEBOX_H_INCLUDED
