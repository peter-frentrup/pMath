#ifndef RICHMATH__BOXES__SECTION_H__INCLUDED
#define RICHMATH__BOXES__SECTION_H__INCLUDED

#include <boxes/sectionornament.h>
#include <util/array.h>


namespace richmath {
  class MathSequence;
  class TextSequence;
  
  class Section: public Box {
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
        
      virtual bool exitable() override { return false; }
      virtual bool remove_inserts_placeholder() override { return false; }
      
      virtual Box *normalize_selection(int *start, int *end) override;
      
      virtual Box *get_highlight_child(Box *src, int *start, int *end) override;
      virtual bool request_repaint(float x, float y, float w, float h) override;
      virtual void invalidate() override;
      virtual bool edit_selection(Context &context) override;
      
      virtual bool changes_children_style() override { return true; }
      
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
      String            label_string;
  };
  
  class ErrorSection: public Section {
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
      
      virtual VolatileSelection mouse_selection(float x, float y, bool *was_inside_start) override;
        
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
      
      virtual VolatileSelection mouse_selection(float x, float y, bool *was_inside_start) override;
        
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
  
  class MathSection: public AbstractSequenceSection {
    public:
      MathSection();
      explicit MathSection(SharedPtr<Style> _style);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      MathSequence *content() { return (MathSequence*)_content; }
  };
  
  class TextSection: public AbstractSequenceSection {
    public:
      TextSection();
      explicit TextSection(SharedPtr<Style> _style);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      TextSequence *content() { return (TextSequence*)_content; }
  };
  
  class EditSection: public MathSection {
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
  
  class StyleDataSection : public AbstractSequenceSection {
    public:
      StyleDataSection();
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual Box *item(int i) override { return nullptr; }
      virtual int count() override { return 0; }
      
      virtual bool edit_selection(Context &context) override { return false; }
      virtual bool selectable(int i) override { return i < 0; }
      
      virtual VolatileSelection mouse_selection(float x, float y, bool *was_inside_start) override;
    
    protected:
      virtual bool can_enter_content() override { return false; }
      
    private:
      Expr _style_data;
  };
}

#endif // RICHMATH__BOXES__SECTION_H__INCLUDED
