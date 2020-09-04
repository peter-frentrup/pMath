#ifndef RICHMATH_BOXES_TEMPLATEBOX_H_INCLUDED
#define RICHMATH_BOXES_TEMPLATEBOX_H_INCLUDED

#include <boxes/ownerbox.h>
#include <eval/observable.h>


namespace richmath {
  class TemplateBox: public Observable, public ExpandableOwnerBox {
      using base = ExpandableOwnerBox;
      friend class TemplateBoxImpl;
      
    public:
      TemplateBox();
      
      virtual void after_insertion() override;
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual bool edit_selection(SelectionReference &selection) override;
      virtual bool selectable(int i = -1) override;
      virtual Box *normalize_selection(int *start, int *end) override;
      virtual VolatileSelection mouse_selection(float x, float y, bool *was_inside_start) override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,  // [in/out]
        int              *index,        // [in/out], -1 if called from parent
        bool              called_from_child) override;
      
      virtual void paint_content(Context &context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void on_mouse_enter() override;
      virtual void on_mouse_exit() override;
      
      void reset_argument(int index, Expr new_arg);
      
      static Expr get_current_value_of_TemplateBox(FrontEndObject *obj, Expr item);

    protected:
      virtual void resize_default_baseline(Context &context) override;
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::TemplateBox; }
      
    public:
      Expr arguments;
      
    private:
      Expr _tag;
      Expr _cached_display_function;
      bool _is_content_loaded;
  };
  
  class TemplateBoxSlot: public ExpandableOwnerBox {
      using base = ExpandableOwnerBox;
      friend class TemplateBoxSlotImpl;
      
    public:
      TemplateBoxSlot();
      
      virtual void after_insertion() override;
      
      TemplateBox *find_owner();
      int argument() { return _argument; }
      
      static Expr prepare_boxes(Expr boxes);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr prepare_dynamic(Expr expr) override;
      
      virtual bool edit_selection(SelectionReference &selection) override;
      virtual bool selectable(int i = -1) override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
      
      virtual Box *remove(int *index) override;
      
      virtual float fill_weight() override;
      virtual void invalidate() override;
      virtual void paint_content(Context &context) override;
      
      virtual void on_exit() override;
      virtual void on_finish_editing() override;
      
      static Expr get_current_value_of_TemplateSlotCount(FrontEndObject *obj, Expr item);
      static Expr get_current_value_of_TemplateSlot(FrontEndObject *obj, Expr item);
      static bool put_current_value_of_TemplateSlot(FrontEndObject *obj, Expr item, Expr rhs);

    protected:
      virtual void resize_default_baseline(Context &context) override;
    
    private:
      int _argument;
      bool _is_content_loaded;
      bool _has_changed_content;
  };
}

#endif // RICHMATH_BOXES_TEMPLATEBOX_H_INCLUDED
