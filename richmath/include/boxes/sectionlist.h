#ifndef RICHMATH__BOXES__SECTIONLIST_H__INCLUDED
#define RICHMATH__BOXES__SECTIONLIST_H__INCLUDED

#include <boxes/section.h>
#include <util/array.h>


namespace richmath {
  class SectionList: public Box {
      class Impl;
    protected:
      virtual ~SectionList();
    public:
      SectionList();
      
      static Expr group(Expr sections);
      
      Section *section(int i) { return _sections[i]; }
      const SectionGroupInfo &group_info(int i) { return _sections[i]->group_info(); }
      
      virtual Box *item(int i) override;
      virtual int count() override { return _sections.length(); }
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      virtual void selection_path(Canvas &canvas, int start, int end) override;
      virtual RectangleF range_rect(int start, int end) override;
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
        
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
        
      virtual VolatileSelection normalize_selection(int start, int end) override;
      
      void set_open_close_group(int i, bool open);
      void toggle_open_close_group(int i);
      
      Expr get_group_style(int pos, ObjectStyleOptionName n, Expr result);
      
      virtual void insert_pmath(int *pos, Expr boxes, int overwrite_until_index = 0);
      virtual void insert(int pos, Section *section);
      virtual Section *swap(int pos, Section *section);
      virtual void remove(int start, int end); // not including end
      virtual Box *remove(int *index) override;
      
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      virtual Expr to_pmath_impl(BoxOutputFlags flags, int start, int end) override;
      
      void internal_insert_pmath(int *pos, Expr boxes, int overwrite_until_index);
      void internal_remove(int start, int end); // not including end
      
      void emit_pmath_impl(BoxOutputFlags flags, int start, int end); // exclusive
      
      void recalc_group_info();
      void recalc_group_info_part(int *pos);
      void update_group_nesting();
      void update_group_nesting_part(int *pos, int current_nesting);
      void update_section_visibility();
      
      virtual bool request_repaint_range(int start, int end) override;
      
      float get_content_scroll_correction_x(int i);
      
      void init_section_bracket_sizes(Context &context);
      void resize_section(Context &context, int i);
      void finish_resize(Context &context);
      void paint_section(Context &context, int i);
      
      void paint_section_brackets(Context &context, int i, float right, float top);
      
      static void paint_single_section_bracket(
        Context &context,
        float    x1,
        float    y1,
        float    x2,
        float    y2,
        int      style); // int BorderXXX constants
        
    public:
      float section_bracket_width;
      float section_bracket_right_margin;
      float unfilled_width;
      
    protected:
      float _scrollx;
      float _page_width;
      float _window_width;
      bool _must_recalc_group_info;
      
    private:
      Array<Section*> _sections;
  };
  
  const int BorderDefault     =   0;
  const int BorderNoTop       =   1;
  const int BorderNoBottom    =   2;
  const int BorderTopArrow    =   4;
  const int BorderBottomArrow =   8;
  const int BorderEval        =  16;
  const int BorderSession     =  32;
  const int BorderNoEditable  =  64;
  const int BorderText        = 128;
  const int BorderInput       = 256;
  const int BorderOutput      = 512;
}

#endif // RICHMATH__BOXES__SECTIONLIST_H__INCLUDED
