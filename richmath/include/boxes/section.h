#ifndef RICHMATH__BOXES__SECTION_H__INCLUDED
#define RICHMATH__BOXES__SECTION_H__INCLUDED

#include <boxes/sectionornament.h>
#include <util/array.h>


namespace richmath {
  class MathSequence;
  class TextSequence;
  
  class Section: public Box {
      using base = Box;
    protected:
      virtual ~Section();
    public:
      Section(SharedPtr<Style> _style);
      
      static Section *create_from_object(const Expr expr);
      
      float label_width();
      void resize_label(Context &context);
      void paint_label(Context &context);
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual bool selection_exitable(bool vertical) override { return vertical; }
      virtual bool remove_inserts_placeholder() override { return false; }
      
      virtual VolatileSelection normalize_selection(int start, int end) override;
      
      virtual VolatileSelection get_highlight_child(const VolatileSelection &src) override;
      virtual bool request_repaint(const RectangleF &rect) override;
      virtual bool visible_rect(RectangleF &rect, Box *top_most) override;
      virtual void invalidate() override;
      virtual bool edit_selection(SelectionReference &selection) override;
      
      virtual bool changes_children_style() override { return true; }
      
    public:
      float y_offset;
      float top_margin;
      float bottom_margin;
      float unfilled_width;
      
      int evaluating;
      
      bool must_resize;
      bool visible;
      bool dialog_start;
      
    private:
      Array<GlyphInfo>  label_glyphs;
      String            label_string;
  };
  
  class ErrorSection final : public Section {
    public:
      ErrorSection(const Expr object);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Box *item(int i) override { return nullptr; }
      virtual int count() override { return 0; }
      
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      
      virtual Box *remove(int *index) override { return this; }
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual Expr to_pmath(BoxOutputFlags flags) override { return _object; }
      
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
    private:
      Expr _object;
  };
  
  class AbstractSequenceSection: public Section {
      using base = Section;
    protected:
      virtual ~AbstractSequenceSection();
    public:
      AbstractSequenceSection(AbstractSequence *content, SharedPtr<Style> _style);
      
      AbstractSequence *abstract_content() {return _content; };
      
      virtual Box *item(int i) override;
      virtual int count() override;
      
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      
      virtual Box *remove(int *index) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
        
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
      
      virtual Box *mouse_sensitive() override;
      virtual void on_mouse_up(MouseEvent &event) override;
      
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
    
    protected:
      virtual bool can_enter_content() { return true; }
      void adopt_all();
    
    protected:
      AbstractSequence *_content; // TextSequence or MathSequence
      SectionOrnament   _dingbat;
      float cx, cy;
  };
  
  class MathSection : public AbstractSequenceSection {
    public:
      MathSection();
      explicit MathSection(SharedPtr<Style> _style);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      MathSequence *content() { return (MathSequence*)_content; }
  };
  
  class TextSection final : public AbstractSequenceSection {
    public:
      TextSection();
      explicit TextSection(SharedPtr<Style> _style);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      TextSequence *content() { return (TextSequence*)_content; }
  };
  
  class EditSection final : public MathSection {
    protected:
      virtual ~EditSection();
    public:
      EditSection();
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
    public:
      Section *original;
  };
  
  class StyleDataSection final : public AbstractSequenceSection {
    public:
      StyleDataSection();
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual Box *item(int i) override { return nullptr; }
      virtual int count() override { return 0; }
      
      virtual bool edit_selection(SelectionReference &selection) override { return false; }
      virtual bool selectable(int i) override { return i < 0; }
      
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
    
    protected:
      virtual bool can_enter_content() override { return false; }
      
    public:
      Expr style_data; // StyleData("name")
  };
}

#endif // RICHMATH__BOXES__SECTION_H__INCLUDED
