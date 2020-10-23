#ifndef RICHMATH__BOXES__OWNERBOX_H__INCLUDED
#define RICHMATH__BOXES__OWNERBOX_H__INCLUDED

#include <boxes/box.h>


namespace richmath {
  class MathSequence;
  
  class OwnerBox: public Box {
      using base = Box;
      class Impl;
    protected:
      virtual ~OwnerBox();
    public:
      explicit OwnerBox(MathSequence *content = nullptr);
      
      MathSequence *content() { return _content; }
      
      virtual Box *item(int i) override;
      virtual int count() override { return 1; }
      
      virtual void resize(Context &context) final override { resize_default_baseline(context); adjust_baseline_after_resize(context); }
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
        
      virtual bool edit_selection(SelectionReference &selection) override;
    
    protected:
      virtual void resize_default_baseline(Context &context);
      virtual void adjust_baseline_after_resize(Context &context);
      float calculate_scaled_baseline(double scale) const;
    
    protected:
      MathSequence *_content;
      float     cx;
      float     cy;
  };
  
  class ExpandableOwnerBox : public OwnerBox {
    public:
      explicit ExpandableOwnerBox(MathSequence *content = nullptr)
        : OwnerBox(content)
      {
      }
      
      virtual bool expand(const BoxSize &size) override;
  };
  
  class InlineSequenceBox final : public OwnerBox {
    public:
      virtual bool try_load_from_object(Expr expr, BoxInputFlags options) override;
      
      virtual void paint(Context &context) override;
      
      virtual void on_enter() override;
      virtual void on_exit() override;
    
    protected:
      virtual void resize_default_baseline(Context &context) override;
  };
}

#endif // RICHMATH__BOXES__OWNERBOX_H__INCLUDED
