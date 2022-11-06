#ifndef RICHMATH__BOXES__BOX_H__INCLUDED
#define RICHMATH__BOXES__BOX_H__INCLUDED

#include <graphics/shapers.h>
#include <eval/observable.h>
#include <util/heterogeneous-stack.h>
#include <util/selections.h>
#include <util/sharedptr.h>
#include <util/styled-object.h>

#include <functional>


namespace richmath {
  class Box;
  class MathSequence;
  class TextSequence;
  class SyntaxState;
  
  enum class DeviceKind : char {
    Mouse,
    Pen,
    Touch
  };
  
  enum class EditAction : char {
    DryRun,
    DoIt
  };
  
  enum  class DisplayStage: char {
    Layout,
    Paint
  };
  
  class MouseEvent {
    public:
      MouseEvent();
      
      void set_origin(Box *new_origin);
      
    public:
      Point      position;
      int        id;
      DeviceKind device;
      bool       left;
      bool       middle;
      bool       right;
      
      Box *origin;
  };
  
  enum class SpecialKey: uint8_t {
    Unknown = 0,
    
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    PageUp,
    PageDown,
    Backspace,
    Delete,
    Return,
    Tab,
    Escape,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12
  };
  
  class SpecialKeyEvent {
    public:
      SpecialKey key;
      bool ctrl;
      bool alt;
      bool shift;
  };
  
  enum class BoxOutputFlags {
    Default           =  0,
    Parseable         =  1, ///< no StyleBox with StripOnInput->True, ...
    Literal           =  2, ///< no DynamicBox or TemplateBox
    ShortNumbers      =  4, ///< not the internal representation of NumberBox, but the content()
    WithDebugMetadata =  8, ///< attach Language`SourceLocation() metadata to strings and expressions
    NoNewSymbols      = 16, ///< do not generate new symbols by effectively using MakeExpression(..., ParseSymbols->False)
    LimitedDepth      = 32, ///< output the id() for boxas at relative depth > Box::max_box_output_depth 
  };
  
  inline bool has(BoxOutputFlags lhs, BoxOutputFlags rhs) {
    return ((int)lhs & (int)rhs) == (int)rhs;
  }
  inline BoxOutputFlags operator |(BoxOutputFlags lhs, BoxOutputFlags rhs) {
    return (BoxOutputFlags)((int)lhs | (int)rhs);
  }
  inline BoxOutputFlags &operator |=(BoxOutputFlags &lhs, BoxOutputFlags rhs) {
    lhs = (BoxOutputFlags)((int)lhs | (int)rhs);
    return lhs;
  }
  inline BoxOutputFlags operator -(BoxOutputFlags lhs, BoxOutputFlags rhs) {
    return (BoxOutputFlags)((int)lhs & ~(int)rhs);
  }
  inline BoxOutputFlags &operator -=(BoxOutputFlags &lhs, BoxOutputFlags rhs) {
    lhs = (BoxOutputFlags)((int)lhs & ~(int)rhs);
    return lhs;
  }
  
  enum class BoxInputFlags {
    Default            = 0,
    FormatNumbers      = 1,
    AllowTemplateSlots = 2,
    ForceResetDynamic  = 4
  };
  
  inline bool has(BoxInputFlags lhs, BoxInputFlags rhs) {
    return ((int)lhs & (int)rhs) == (int)rhs;
  }
  inline BoxInputFlags operator |(BoxInputFlags lhs, BoxInputFlags rhs) {
    return (BoxInputFlags)((int)lhs | (int)rhs);
  }
  inline BoxInputFlags &operator |=(BoxInputFlags &lhs, BoxInputFlags rhs) {
    lhs = (BoxInputFlags)((int)lhs | (int)rhs);
    return lhs;
  }
  inline BoxInputFlags operator -(BoxInputFlags lhs, BoxInputFlags rhs) {
    return (BoxInputFlags)((int)lhs & ~(int)rhs);
  }
  inline BoxInputFlags &operator -=(BoxInputFlags &lhs, BoxInputFlags rhs) {
    lhs = (BoxInputFlags)((int)lhs & ~(int)rhs);
    return lhs;
  }
  
  template< class... Args >
  struct FunctionChain {
    std::function<void(Args...)> func;
    FunctionChain<Args...> *next;
      
    FunctionChain(std::function<void(Args...)> _func, FunctionChain<Args...> *_next)
      : func{_func},    
        next{_next}
    {
    }
  };
  
  class Box;
  class BoxAdopter {
      friend class Box;
      explicit BoxAdopter(Box &owner) : _owner(owner) {}
    public:
      void adopt(Box *child, int i);
      void abandon(Box *child);
      
      Box &owner() { return _owner; }
      
    private:
      Box &_owner;
  };
  
  class Box: public ActiveStyledObject {
      friend class BoxAdopter;
    protected:
      virtual ~Box();
      void delete_owned(Box *child) { 
        if(child) {
          RICHMATH_ASSERT(child->parent() == this || child->parent() == nullptr);
          child->safe_destroy();
        }
      }
    public:
      Box();
      
      virtual StyledObject *style_parent() override { return parent(); }
      virtual Expr allowed_options() override;
      
      /// Usually called after the box was inserted into a document.
      ///
      /// This function allows for stylesheet-dependent initialization. The default implementation
      /// calls item(i)->after_insertion() recursively for all child boxes item(i).
      virtual void after_insertion();
      
      /// Usually called after parts have been inserted into this Box when this Box is already part of a document.
      ///
      /// This calls item(i)->after_insertion() for all child boxes item(i) whose index() is >= start and < end.
      void after_insertion(int start, int end);
      
      static int depth(Box *box);

      Box *parent() { return _parent_or_limbo_next.as_normal(); }
      
      /// The selection index within parent().
      int index() {  return _index; }
      
      template<class T>
      T *find_parent(bool selfincluding) {
        Box *b = this;
        
        if(!selfincluding)
          b = parent();
          
        while(b && !dynamic_cast<T*>(b))
          b = b->parent();
          
        return dynamic_cast<T*>(b);
      }
      
      bool is_parent_of(Box *child); // x->is_parent_of(x) is true
      
      static Box *common_parent(Box *a, Box *b);
      
      static Box *find_nearest_box(FrontEndObject *obj);
      
      template<class T>
      static T *find_nearest_parent(FrontEndObject *obj, bool selfincluding = true) {
        Box *box = find_nearest_box(obj);
        if(!box)
          return nullptr;
        
        return box->find_parent<T>(selfincluding || box != obj);
      }
      
      int index_in_ancestor(Box *ancestor, int fallback);
      
      /// Get the next box after/before this one.
      /// \param direction          The search direction.
      /// \param restrict_to_parent (Optional) If set, restrict search to the sub-tree of child-nodes of that box.
      /// \return The subsequent box (child/sibling/other relative) or NULL if none exists in the given search direction.
      Box *next_box(LogicalDirection direction, Box *restrict_to_parent = nullptr);
      
      /// Find a child box after/before a given index.
      Box *next_child_or_null(int index, LogicalDirection direction);
      
      virtual MathSequence *as_inline_span() { return nullptr; }
      
      virtual TextSequence *as_inline_text_span() { return nullptr; }
      
      /// Get the i-th child box for i bewteen 0 to count()-1.
      ///
      /// Note that i does not necessarily correspond to index() of the returned box.
      virtual Box *item(int i) = 0;

      /// The number child boxes.
      virtual int count() = 0;
      
      /// The maximum selecion index.
      virtual int length() { return count(); }
      
      virtual float fill_weight() { return 0.0f; }
      const BoxSize &extents() { return _extents; }
      virtual float first_glyph_width() { return 0.0f; }
      virtual float last_glyph_width() { return 0.0f; }
      
      virtual int child_script_level(int index, const int *opt_ambient_script_level);
      virtual bool expand(const BoxSize &size) { return false; }
      bool update_dynamic_styles(Context &context);
      virtual void resize_inline(Context &context) {}
      virtual void resize(Context &context) = 0;
      virtual void colorize_scope(SyntaxState &state);
      virtual void before_paint_inline(Context &context);
      virtual void paint(Context &context) = 0;
      virtual void begin_paint_inline_span(Context &context, BasicHeterogeneousStack &context_stack, DisplayStage stage) {}
      virtual void end_paint_inline_span(Context &context, BasicHeterogeneousStack &context_stack, DisplayStage stage) {}
      virtual VolatileSelection get_highlight_child(const VolatileSelection &src);
      virtual void selection_path(Canvas &canvas, int start, int end);
      virtual bool scroll_to(const RectangleF &rect);
      virtual bool scroll_to(Canvas &canvas, const VolatileSelection &child);
      bool default_scroll_to(Canvas &canvas, Box *scroll_view, const VolatileSelection &child_sel);
      
      /// Remove a child box and get the new selection. May also delete this box itself.
      ///
      /// \param index [inout] On input, selection index of the child-to-be-removed. 
      ///              On output, new selection index relative to the returned box.
      virtual Box *remove(int *index) = 0;
      
      /// Get the head symbol of to_pmath()
      virtual Expr to_pmath_symbol() = 0;
      Expr to_pmath(BoxOutputFlags flags);
      Expr to_pmath(BoxOutputFlags flags, int start, int end);
      
      /// Move the text cursor forward/backward in reading direction.
      ///
      /// \param direction     Forward or backward.
      /// \param jumping       Whether to move by token/word (true) or by character (false).
      /// \param index [inout] On input, where the cursor comes from. 0 <= *index <= length() means 
      ///                      "from inside" and otherwise "from outside" ("from before" if *index < 0, 
      ///                      "from after" if *index > length()).
      ///                      On output, the new cursor position relative to the returned box.
      /// \return The box that contains the new cursor position.
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index);
      
      /// Move the text cursor up/down.
      ///
      /// \param direction           Forward means down, backward means up.
      /// \param index_rel_x [inout] The x-displacement relative to *index.
      /// \param index       [input] On input, where the cursor comes from: *index < 0 means "from outside"
      ///                            and *index >= 0 means from the specified position inside.
      ///                            On output, the new cursor position relative to the returned box.
      /// \param called_from_child   Whether this function was called from a child during its own move_vertical() handling.
      ///                            This is used by SectionList to allow the cursor to rest between sections.
      /// \return The box that contains the new cursor position.
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,  // [in/out]
        int              *index,        // [in/out], -1 if called from parent
        bool              called_from_child);
      
      /// Get the box/possible selection at a point.
      ///
      /// \param pos              The position in local coordinates.
      /// \param was_inside_start [out] Set to true if the returned [*start, *end] interval contains 
      ///                         the point and false if it is only near the point.
      /// \return The box with start and end index that contains the point.
      ///
      /// Note that further mouse event processing done by the result of Box::mouse_sensitive() where 
      /// ret is the returned box, but text selection is subject to Box::selectable()
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start);
        
      /// Append the child-to-parent coordinate transformation to a matrix (multiply from right). 
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix);
        
      /// Append the child-to-parent coordinate transformation to a matrix.
      ///
      /// \param ancestor An owning box or this (otherwise nullptr is assumed, meaning 
      ///                 "all the way up to userspace coordinates")
      void transformation(
        Box            *ancestor,
        cairo_matrix_t *matrix);
        
      virtual bool remove_inserts_placeholder() { return true; }
      
      /// Decides whether the cursor can exit any (direct) child box via Left/Right/... keys
      ///
      /// Note that mouse and e.g. TAB selection are not subject to this restriction.
      virtual bool exitable() { return true; }
      
      /// Decides whether the selection can exit any (direct) child box via Shift+Left/Right/... keys
      ///
      /// Note that mouse and e.g. TAB selection are not subject to this restriction.
      virtual bool selection_exitable(bool vertical) { return true; }

      /// Decides whether this box (i = -1) or the child box at index i >= 0 may be selected.
      ///
      /// This usually defaults to parent()->selectable(index()) unless an explicit Selectable style is set.
      virtual bool selectable(int i = -1);
      
      /// Adjust the to be selected range.
      virtual VolatileSelection normalize_selection(int start, int end);
      
      /// Modify an expression before it gets evaluated, e.g. in a child box' Dynamic() context. 
      virtual Expr prepare_dynamic(Expr expr) override;

      /// Notifies that a Dynamic value changed which this box is tracking.
      virtual void dynamic_updated() override;
      
      virtual bool try_load_from_object(Expr object, BoxInputFlags options) = 0;
      
      /// Convert Dynamic content in this box to literal boxes.
      VolatileSelection all_dynamic_to_literal() { return dynamic_to_literal(0, length()); }
      
      /// Convert Dynamic content in the specified range of this box to literal boxes.
      virtual VolatileSelection dynamic_to_literal(int start, int end);
      
      virtual bool request_repaint_all();
      virtual bool request_repaint_range(int start, int end);
      virtual bool request_repaint(const RectangleF &rect);
      
      virtual RectangleF range_rect(int start, int end) { return extents().to_rectangle(); }
      
      bool visible_rect(RectangleF &rect) { return visible_rect(rect, nullptr); }
      virtual bool visible_rect(RectangleF &rect, Box *top_most);

      /// Something inside this box changed and needs a resize().
      virtual void invalidate();
      
      /// A style option changed. Calls invalidate() or request_repaint_all(), depending on layout_affected.
      virtual void on_style_changed(bool layout_affected) override;

      /// Perform any cleanup before the user edits the selection or block the operation.
      /// 
      /// This function usually first calls parent()->edit_selection(). It is only called before
      /// user initiated operations.
      virtual bool edit_selection(SelectionReference &selection, EditAction action); // *not* automatically called
      
      /// Test whether this box' content is editable.
      bool editable();
      
      /// Get the box that captures mouse input when the a mouse button is pressed at this box.
      /// Capturing takes place until the last mouse button is released or until explicitly canceled.
      virtual Box *mouse_sensitive();
      
      /// The mouse enters this box (with no button pressed).
      virtual void on_mouse_enter();
      
      /// The mouse exits this box (with no button pressed).
      virtual void on_mouse_exit();

      /// A mouse button was pressed somewhere. Mouse events do not bubble up.
      virtual void on_mouse_down(MouseEvent &event);
      
      /// The mouse moved. Either over this box or somewhere else while this box has mouse capture.
      /// Mouse events do not bubble up.
      virtual void on_mouse_move(MouseEvent &event);

      /// A mouse button was released while mouse input is captured. Mouse events do not bubble up.
      virtual void on_mouse_up(MouseEvent &event);
      
      /// Mouse capturing was canceled (e.g. via ESCAPE key while the mouse was pressed).
      virtual void on_mouse_cancel();
      
      /// Called when the selection moves from outside (possibly surounding) to inside this box.
      virtual void on_enter();
      
      /// Called when the selection moves from inside to outside (possibly surounding) of this box.
      virtual void on_exit();

      /// Called when this box contains the text cursor and (mouse) input focus moves elsewhere.
      ///
      /// The text cursor (selection) may be in a child box. 
      /// This is not called if the mouse input focus is a parent or child of this box.
      virtual void on_finish_editing();
      
      /// A keyboard key was pressed. Key events do bubble up if not handled.
      virtual void on_key_down(SpecialKeyEvent &event);
      
      /// A keyboard key was released. Key events do bubble up if unhandled.
      virtual void on_key_up(SpecialKeyEvent &event);
      
      /// A keyboard character key was pressed. Key events do bubble up if unhandled.
      virtual void on_key_press(uint32_t unichar);
      
    public:
      static FunctionChain<Box*, Expr> *on_finish_load_from_object;
      static int max_box_output_depth;
      
    protected:
      void adopt(Box *child, int i);
      void abandon(Box *child);
      BoxAdopter make_adoptor() { return BoxAdopter(*this); }
      
      void finish_load_from_object(Expr expr);
      
      virtual ObjectWithLimbo *next_in_limbo() final override { return _parent_or_limbo_next.as_tinted(); }
      virtual void next_in_limbo(ObjectWithLimbo *next) final override;
      
      virtual Expr to_pmath_impl(BoxOutputFlags flags) = 0;
      virtual Expr to_pmath_impl(BoxOutputFlags flags, int start, int end) {
        return to_pmath_impl(flags);
      }
      
    protected:
      TintedPtr<Box, ObjectWithLimbo> _parent_or_limbo_next;
      int                             _index;
      BoxSize                         _extents;
  };
  
  class DummyBox: public Box {
    protected:
      virtual ~DummyBox() {}
    public:
      DummyBox(): Box() {}
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags options) override { return false; }
      
      virtual Box *item(int i) override { return nullptr; }
      virtual int  count() override {     return 0; }
      virtual int  length() override {    return 0; }
      
      virtual void resize(Context &context) override {}
      virtual void paint(Context &context) override {}
      
      virtual Box *remove(int *index) override { return this; }
    
      virtual Expr to_pmath_symbol() override { return Expr(); }
      
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override { return Expr(); }
  };

  inline void BoxAdopter::adopt(Box *child, int i) { _owner.adopt(child, i); }
  inline void BoxAdopter::abandon(Box *child) {      _owner.abandon(child);  }
}

#endif // RICHMATH__BOXES__BOX_H__INCLUDED
