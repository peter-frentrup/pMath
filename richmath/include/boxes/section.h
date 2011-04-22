#ifndef __BOXES__SECTION_H__
#define __BOXES__SECTION_H__

#include <boxes/box.h>
#include <util/array.h>

namespace richmath{
  class MathSequence;
  class TextSequence;
  
  class Section: public Box {
    public:
      Section(SharedPtr<Style> _style);
      virtual ~Section();
      
      static Section *create_from_object(const Expr object);
      
      const String label(){ return get_style(SectionLabel); } // { return style->section_label; }
      void label(const String str);
      
      float label_width();
      void resize_label(Context *context);
      void paint_label(Context *context);
      
      virtual Box *move_vertical(
        LogicalDirection  direction, 
        float            *index_rel_x,
        int              *index);
        
      virtual bool exitable(){ return false; }
      virtual bool remove_inserts_placeholder(){ return false; }
      
      virtual bool selectable(int i = -1);
      
      virtual Box *get_highlight_child(Box *src, int *start, int *end);
      virtual bool request_repaint(float x, float y, float w, float h);
      virtual void invalidate();
      virtual bool edit_selection(Context *context);
      
      virtual bool changes_children_style(){ return true; }
      
    public:
      float y_offset;
      float top_margin;
      float bottom_margin;
      float unfilled_width;
      
      bool must_resize;
      bool visible;
      bool evaluating;
      bool dialog_start;
      
    private:
      Array<GlyphInfo>  label_glyphs;
  };
  
  class ErrorSection: public Section {
    public:
      ErrorSection(const Expr object);
      
      virtual Box *item(int i){ return 0; }
      virtual int count(){ return 0; }
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      
      virtual Box *remove(int *index){ return this; }
      
      virtual Expr to_pmath(bool parseable){ return _object; }
      
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start);
        
    private:
      Expr _object;
  };
  
  class AbstractSequenceSection: public Section {
    public:
      AbstractSequenceSection(AbstractSequence *content, SharedPtr<Style> style);
      virtual ~AbstractSequenceSection();
      
      virtual Box *item(int i);
      virtual int count(){ return 1; }
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      
      virtual Box *remove(int *index);
      
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
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix);
      
    protected:
      AbstractSequence *_content; // TextSequence or MathSequence
      float cx, cy;
  };
  
  class MathSection: public AbstractSequenceSection {
    public:
      explicit MathSection(SharedPtr<Style> style);
      
      MathSequence *content(){ return (MathSequence*)_content; }
  };
  
  class TextSection: public AbstractSequenceSection {
    public:
      explicit TextSection(SharedPtr<Style> style);
      
      TextSection *content(){ return (TextSection*)_content; }
  };
  
  class EditSection: public MathSection {
    public:
      EditSection();
      virtual ~EditSection();
      
      virtual Expr to_pmath(bool parseable);
      
    public:
      Section *original;
  };
}

#endif // __BOXES__SECTION_H__
