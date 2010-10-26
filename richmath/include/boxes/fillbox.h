#ifndef __BOXES__FILLBOX_H__
#define __BOXES__FILLBOX_H__

#include <boxes/ownerbox.h>

namespace richmath{
  class FillBox: public OwnerBox{
    public:
      explicit FillBox(MathSequence *content = 0, float _weight = 1);
      ~FillBox();
      
      static FillBox *create(Expr expr, int opts);
      
      virtual bool expand(const BoxSize &size);
      virtual void paint_content(Context *context);
      
      virtual Expr to_pmath(bool parseable);
      
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
    
    public:
      float weight;
  };
}

#endif // __BOXES__FILLBOX_H__
