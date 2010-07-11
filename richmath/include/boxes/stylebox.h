#ifndef __BOXES__STYLEBOX_H__
#define __BOXES__STYLEBOX_H__

#include <boxes/ownerbox.h>

namespace richmath{
  class AbstractStyleBox: public OwnerBox{
    public:
      explicit AbstractStyleBox(MathSequence *content = 0);
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      
      virtual void colorize_scope(SyntaxState *state);
      
      virtual Box *move_logical(
        LogicalDirection  direction, 
        bool              jumping, 
        int              *index);
      
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
      
    protected:
      void paint_or_resize(Context *context, bool paint);
      
    private:
      bool show_auto_styles;
  };
  
  class StyleBox: public AbstractStyleBox{
    public:
      explicit StyleBox(MathSequence *content = 0);
      
      static StyleBox *create(Expr expr, int opts); // returns 0 on error
      
      virtual bool expand(const BoxSize &size);
      virtual pmath_t to_pmath(bool parseable);
      
      virtual bool changes_children_style(){ return true; }
      
  };
  
  class TagBox: public AbstractStyleBox{
    public:
      TagBox();
      explicit TagBox(MathSequence *content);
      TagBox(MathSequence *content, Expr _tag);
      
      virtual bool expand(const BoxSize &size);
      virtual void resize(Context *context);
      
      virtual pmath_t to_pmath(bool parseable);
      
    public:
      Expr tag;
  };
}

#endif // __BOXES__STYLEBOX_H__
