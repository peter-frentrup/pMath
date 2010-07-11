#ifndef __BOXES__OWNERBOX_H__
#define __BOXES__OWNERBOX_H__

#include <boxes/box.h>

namespace richmath{
  class MathSequence;
  
  class OwnerBox: public Box {
    public:
      explicit OwnerBox(MathSequence *content = 0);
      ~OwnerBox();
      
      MathSequence *content(){ return _content; }
      
      virtual Box *item(int i);
      virtual int count(){ return 1; }
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      virtual void paint_content(Context *context);
      
      virtual Box *remove(int *index);
      
      virtual pmath_t to_pmath(bool parseable);
      
      virtual Box *move_vertical(
        LogicalDirection  direction, 
        float            *index_rel_x,
        int              *index);
      
      virtual Box *mouse_selection(
        float x,
        float y,
        int   *start,
        int   *end,
        bool  *eol);
      
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix);
      
      virtual bool edit_selection(Context *context);
      
    protected:
      MathSequence *_content;
      float     cx;
      float     cy;
  };
}

#endif // __BOXES__OWNERBOX_H__
