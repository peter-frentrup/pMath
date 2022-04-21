#ifndef RICHMATH__BOXES__SECTION_H__INCLUDED
#define RICHMATH__BOXES__SECTION_H__INCLUDED

#include <boxes/sectionornament.h>
#include <util/array.h>


namespace richmath {
  class AbstractSequence;
  class MathSequence;
  class TextSequence;
  
  /* Every section has a SectionGroupPrecedence (SGP) option to specify how deep
     it will be nested in the section groups. When a section X is followed by a
     section Y, three possible situations arise:
  
     [ notation:
       First(X) = SectionGroupInfo.first for section X
       LFirst   = previous group start (index of the first section in the
                  current group).
     ]
  
     SGP(X) < SGP(Y): A new group starts with X. First(Y) = X, First(X) = LFirst
  
     SGP(X) = SGP(Y): X and Y are both in the current group.
                      First(X) = First(Y) = LFirst
  
     SGP(X) > SGP(Y): The previous group ends with X. A new group starts with Y.
                      First(X) = LFirst, First(Y) = the last section Z with
                      SGP(Z) < SGP(Y) or Y itself iff there is no such section.
                      LFIG:= -1
  
   */
  class SectionGroupInfo {
    public:
      SectionGroupInfo()
        : precedence(0.0),
        nesting(0),
        first(-1),
        end(-1),
        close_rel(-1)
      {
      }
      
      float precedence;
      int nesting;
      int first;                      // always < section index, may be -1
      int end;                        // index of last section of the group that starts here
      ObservableValue<int> close_rel; // group closed => rel. index of the only open section, else: -1
  };
  
  class Section: public Box {
      using base = Box;
      friend class SectionList;
    protected:
      virtual ~Section();
    public:
      Section(SharedPtr<Style> _style);
      
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
      virtual bool edit_selection(SelectionReference &selection, EditAction action) override;
      
      virtual bool changes_children_style() override { return true; }
      
      const SectionGroupInfo &group_info() { return _group_info; }
      
      virtual float get_em();
    
    protected:
      enum {
        MustResizeBit = base::NumFlagsBits,
        VisibleBit,
        DialogStartBit,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
    
    public:
      float y_offset;
      float top_margin;
      float bottom_margin;
      float unfilled_width;
      
      int evaluating;
      
      bool must_resize() {        return get_flag(MustResizeBit); }
      void must_resize(bool value) {  change_flag(MustResizeBit, value); }
      bool visible() {            return get_flag(VisibleBit); }
      void visible(bool value) {      change_flag(VisibleBit, value); }
      bool dialog_start() {       return get_flag(DialogStartBit); }
      void dialog_start(bool value) { change_flag(DialogStartBit, value); }
    
    private:
      SectionGroupInfo  _group_info; // Managed by SectionList parent() only
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
      explicit AbstractSequenceSection(AbstractSequence *content, SharedPtr<Style> _style);
      
      AbstractSequence *content() {return _content; };
      
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
      
      virtual float get_em() override;
    
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
      explicit MathSection();
      explicit MathSection(SharedPtr<Style> _style);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      MathSequence *content() { return (MathSequence*)_content; }
  };
  
  class TextSection final : public AbstractSequenceSection {
    public:
      explicit TextSection();
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
      
      virtual bool edit_selection(SelectionReference &selection, EditAction action) override { return false; }
      virtual bool selectable(int i = -1) override { return i < 0; }
      
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
    
    protected:
      virtual bool can_enter_content() override { return false; }
      
    public:
      Expr style_data; // StyleData("name")
  };
}

#endif // RICHMATH__BOXES__SECTION_H__INCLUDED
