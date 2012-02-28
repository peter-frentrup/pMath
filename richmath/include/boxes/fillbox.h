#ifndef __BOXES__FILLBOX_H__
#define __BOXES__FILLBOX_H__

#include <boxes/ownerbox.h>


namespace richmath {
  class FillBox: public OwnerBox {
    public:
      explicit FillBox(MathSequence *content = 0, float _weight = 1);
      ~FillBox();
      
      // Box::try_create<FillBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, int opts);
      
      virtual bool expand(const BoxSize &size);
      virtual void paint_content(Context *context);
      
      virtual Expr to_pmath_symbol() { return Symbol(PMATH_SYMBOL_FILLBOX); }
      virtual Expr to_pmath(int flags);
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index);
        
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start);
        
        
      virtual bool request_repaint(float x, float y, float w, float h);
        
    public:
      float weight;
  };
}

#endif // __BOXES__FILLBOX_H__
