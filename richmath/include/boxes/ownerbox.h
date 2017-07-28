#ifndef __BOXES__OWNERBOX_H__
#define __BOXES__OWNERBOX_H__

#include <boxes/box.h>


namespace richmath {
  class MathSequence;
  
  class OwnerBox: public Box {
    public:
      explicit OwnerBox(MathSequence *content = 0);
      ~OwnerBox();
      
      MathSequence *content() { return _content; }
      
      virtual Box *item(int i) override;
      virtual int count() override { return 1; }
      
      virtual void resize(Context *context) override;
      virtual void paint(Context *context) override;
      virtual void paint_content(Context *context);
      
      virtual Box *remove(int *index) override;
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual Expr to_pmath(BoxFlags flags) override;
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
        
      virtual bool edit_selection(Context *context) override;
      
    protected:
      MathSequence *_content;
      float     cx;
      float     cy;
  };
  
  class InlineSequenceBox: public OwnerBox {
    public:
      virtual bool try_load_from_object(Expr expr, int options) override;
      
      virtual void resize(Context *context) override;
      virtual void paint(Context *context) override;
      
      virtual void on_enter() override;
      virtual void on_exit() override;
  };
}

#endif // __BOXES__OWNERBOX_H__
