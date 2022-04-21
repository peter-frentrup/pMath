#ifndef RICHMATH__BOXES__OWNERBOX_H__INCLUDED
#define RICHMATH__BOXES__OWNERBOX_H__INCLUDED

#include <boxes/box.h>


namespace richmath {
  class AbstractSequence;
  
  class OwnerBox: public Box {
      using base = Box;
      class Impl;
    protected:
      virtual ~OwnerBox();
    public:
      explicit OwnerBox(AbstractSequence *content);
      
      AbstractSequence *content() { return _content; }
      
      virtual Box *item(int i) override;
      virtual int count() override { return 1; }
      
      virtual void resize_inline(Context &context) override;
      virtual void resize(Context &context) final override { resize_default_baseline(context); adjust_baseline_after_resize(context); }
      virtual void before_paint_inline(Context &context) override;
      virtual void paint(Context &context) override;
      virtual void paint_content(Context &context);
      
      virtual Box *remove(int *index) override;
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
        
      virtual bool edit_selection(SelectionReference &selection, EditAction action) override;
    
    protected:
      virtual void resize_default_baseline(Context &context);
      virtual void adjust_baseline_after_resize(Context &context);
      float calculate_scaled_baseline(double scale) const;
    
    protected:
      AbstractSequence *_content;
      float     cx;
      float     cy;
  };
  
  class ExpandableOwnerBox : public OwnerBox {
    public:
      explicit ExpandableOwnerBox(AbstractSequence *content = nullptr) : OwnerBox(content) {}
      
      virtual bool expand(const BoxSize &size) override;
  };
  
  class InlineSequenceBox final : public OwnerBox {
      using base = OwnerBox;
    public:
      explicit InlineSequenceBox(AbstractSequence *content) : OwnerBox(content) {}
      
      virtual MathSequence *as_inline_span() override;
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags options) override;
      
      virtual void paint(Context &context) override;
      
      virtual void on_enter() override;
      virtual void on_exit() override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      bool has_explicit_head() {       return get_flag(ExplicitHeadBit); }
      void has_explicit_head(bool value) { change_flag(ExplicitHeadBit, value); }
    
    protected:
      virtual void resize_default_baseline(Context &context) override;
      
    protected:
      enum {
        ExplicitHeadBit = base::NumFlagsBits,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
  };
}

#endif // RICHMATH__BOXES__OWNERBOX_H__INCLUDED
