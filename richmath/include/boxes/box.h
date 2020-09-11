#ifndef RICHMATH__BOXES__BOX_H__INCLUDED
#define RICHMATH__BOXES__BOX_H__INCLUDED

#include <graphics/shapers.h>
#include <eval/observable.h>
#include <util/selections.h>
#include <util/sharedptr.h>
#include <util/styled-object.h>

#include <functional>


namespace richmath {
  enum class LogicalDirection {
    Forward,
    Backward
  };
  
  class Box;
  class SyntaxState;
  
  enum class DeviceKind : char {
    Mouse,
    Pen,
    Touch
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
    Default       = 0,
    Parseable     = 1, ///< no StyleBox with StripOnInput->True, ...
    Literal       = 2, ///< no DynamicBox or TemplateBox
    ShortNumbers  = 4, ///< not the internal representation of NumberBox, but the content()
    WithDebugInfo = 8, ///< attach DebugInfoSource() metadata to strings and expressions
    NoNewSymbols  = 16 ///< do not generate new symbols by effectively using MakeExpression(..., ParseSymbols->False)
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
  
  /** Suspending deletions of Boxes.
  
      While destruction suspended is in effect, boxes will be remembered in a free
      list (the limbo). When destruction mode is resumed, all objects in the limbo are
      actually deleted.
  
      During mouse_down()/paint()/... handlers, the document might change.
      This could cause a parent box to be removed. Since it is still referenced
      on the stack, such a box should not be wiped out until the call stack is clean.
  
      Hence, the widget which forwards all calls to Box/Document should suppress
      destruction of Boxes during event handling.
   */
  class AutoMemorySuspension {
    public:
      AutoMemorySuspension() { suspend_deletions(); }
      ~AutoMemorySuspension() { resume_deletions(); }
      
      static bool are_deletions_suspended();
      
    private:
      static void suspend_deletions();
      static void resume_deletions();
  };
  
  class Box: public StyledObject {
      friend class AutoMemorySuspension;
    protected:
      virtual ~Box();
      void delete_owned(Box *child) { 
        if(child) {
          assert(child->parent() == this);
          delete child;
        }
      }
    public:
      Box();
      
      virtual StyledObject *style_parent() override { return parent(); }
      virtual SharedPtr<Style> own_style() override { return style; };
      
      /// Usually called after the box was inserted into a document.
      ///
      /// This function allows for stylesheet-dependent initialization. The default implementation
      /// calls item(i)->after_insertion() recursively for all child boxes item(i).
      virtual void after_insertion();
      
      /// Usually called after parts have been inserted into this Box when this Box is already part of a document.
      ///
      /// This calls item(i)->after_insertion() for all child boxes item(i) whose index() is >= start and < end.
      void after_insertion(int start, int end);
      
      /// Mark the box for deletion.
      ///
      /// You should normally use this function instead of delete.
      /// \see AutoMemorySuspension
      void safe_destroy();
      
      template<class T>
      static T *try_create(Expr expr, BoxInputFlags options) {
        T *box = new T();
        
        if(!box->try_load_from_object(expr, options)) {
          Box *upcast_box = box;
          delete upcast_box;
          return nullptr;
        }
        
        return box;
      }
      
      Box *parent() { return _parent; }

      /// The selection index within parent().
      int index() {  return _index; }
      
      template<class T>
      T *find_parent(bool selfincluding) {
        Box *b = this;
        
        if(!selfincluding)
          b = _parent;
          
        while(b && !dynamic_cast<T*>(b))
          b = b->parent();
          
        return dynamic_cast<T*>(b);
      }
      
      bool is_parent_of(Box *child); // x->is_parent_of(x) is true
      
      static Box *common_parent(Box *a, Box *b);
      
      /// Get the next box after/before this one.
      /// \param direction          The search direction.
      /// \param restrict_to_parent (Optional) If set, restrict search to the sub-tree of child-nodes of that box.
      /// \return The subsequent box (child/sibling/other relative) or NULL if none exists in the given search direction.
      Box *next_box(LogicalDirection direction, Box *restrict_to_parent = nullptr);
      
      /// Find a child box after/before a given index.
      Box *next_child_or_null(int index, LogicalDirection direction);
      
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
      
      virtual bool expand(const BoxSize &size) { return false; }
      bool update_dynamic_styles(Context &context);
      virtual void resize(Context &context) = 0;
      virtual void colorize_scope(SyntaxState &state);
      virtual void paint(Context &context) = 0;
      virtual Box *get_highlight_child(Box *src, int *start, int *end);
      virtual void selection_path(Canvas &canvas, int start, int end);
      virtual void scroll_to(const RectangleF &rect);
      virtual void scroll_to(Canvas &canvas, const VolatileSelection &child);
      void default_scroll_to(Canvas &canvas, Box *parent, const VolatileSelection &child_sel);
      
      /// Remove a child box and get the new selection. May also delete this box itself.
      ///
      /// \param index [inout] On input, selection index of the child-to-be-removed. 
      ///              On output, new selection index relative to the returned box.
      virtual Box *remove(int *index) = 0;
      
      /// Get the head symbol of to_pmath()
      virtual Expr to_pmath_symbol() = 0;
      virtual Expr to_pmath(BoxOutputFlags flags) = 0;
      virtual Expr to_pmath(BoxOutputFlags flags, int start, int end) {
        return to_pmath(flags);
      }
      
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
      /// \param x The local x coordinate.
      /// \param y The local y coordinate.
      /// \param was_inside_start [out] Set to true if the returned [*start, *end] interval contains 
      ///                         the point and fals if it is only near the point.
      /// \return The box with and start and end index that contains the point.
      ///
      /// Note that further mouse event processing done by the result of ret->mouse_sensitive() where 
      /// ret is the returned box, but text selection is subject to ret->selectable()
      virtual VolatileSelection mouse_selection(float x, float y, bool *was_inside_start);
        
      /// Append the child-to-parent coordinate transformation to a matrix (multiply from right). 
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix);
        
      /// Append the child-to-parent coordinate transformation to a matrix.
      ///
      /// \param parent An owning box or this (otherwise nullptr is assumed, meaning 
      ///               "all the way up to userspace coordinates")
      void transformation(
        Box            *parent,
        cairo_matrix_t *matrix);
        
      virtual bool remove_inserts_placeholder() { return true; }
      
      /// Decides whether the cursor can exit any (direct) child box via Left/Right/... keys
      ///
      /// Note that mouse and e.g. TAB selection are not subject to this restriction.
      virtual bool exitable() { return true; }
      
      /// Decides whether the selection can exit any (direct) child box via Shift+Left/Right/... keys
      ///
      /// Note that mouse and e.g. TAB selection are not subject to this restriction.
      virtual bool selection_exitable() { return true; }

      /// Decides whether this box (i = -1) or the child box at index i >= 0 may be selected.
      ///
      /// This usually defaults to parent()->selectable(index()) unless an explicit Selectable style is set.
      virtual bool selectable(int i = -1);
      
      /// Adjust the to be selected range.
      ///
      /// On input, this box is to be selected between *start and *end.
      /// On output, the returned box will be selected between *start and *end.
      virtual Box *normalize_selection(int *start, int *end);
      
      /// Modify an expression before it gets evaluated in a child box' Dynamic() context. 
      virtual Expr prepare_dynamic(Expr expr);

      /// Notifies that a Dynamic value changed which this box is tracking.
      virtual void dynamic_updated() override;
      
      /// An asynchronous dynamic evaluation was finished.
      ///
      /// \param info   The info argument given to the corresponding DynamicEvaluationJob. Usually null.
      /// \param result The evaluation result.
      virtual void dynamic_finished(Expr info, Expr result) {}
      
      virtual bool try_load_from_object(Expr object, BoxInputFlags options) = 0;
      virtual Box *dynamic_to_literal(int *start, int *end);
      
      bool         request_repaint_all();
      virtual bool request_repaint_range(int start, int end);
      virtual bool request_repaint(float x, float y, float w, float h);

      /// Something inside this box changed and needs a resize().
      virtual void invalidate();

      /// A style option changed. Calls invalidate().
      ///
      /// TODO: Refactor to be more efficient, need not always call invalidate(),
      ///       sometimes only need request_repaint_all(), depending on option.
      virtual void invalidate_options();

      /// Perform any cleanup before the user edits the selection or block the operation.
      /// 
      /// This function usually first calls parent()->edit_selection(). It is only called before
      /// user initiated operations.
      virtual bool edit_selection(SelectionReference &selection); // *not* automatically called
      
      /// Get the box that captures mouse input when the a mouse button is pressed at this box.
      /// Capturing takes place until the last mouse button is released or until explicitly cancelled.
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
      
      /// A keyboard key was pressed. Key events do bubble up if unhandled.
      virtual void on_key_down(SpecialKeyEvent &event);
      
      /// A keyboard key was released. Key events do bubble up if unhandled.
      virtual void on_key_up(SpecialKeyEvent &event);
      
      /// A keyboard character key was pressed. Key events do bubble up if unhandled.
      virtual void on_key_press(uint32_t unichar);
      
    public:
      SharedPtr<Style> style;
      
      static FunctionChain<Box*, Expr> *on_finish_load_from_object;
      
    protected:
      void adopt(Box *child, int i);
      void abandon(Box *child);
      
      void finish_load_from_object(Expr expr);
      
    protected:
      BoxSize  _extents;
      Box     *_parent;
      int      _index;
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
      virtual Expr to_pmath(BoxOutputFlags flags) override { return Expr(); }
  };
  
  class AbstractSequence: public Box {
    protected:
      virtual ~AbstractSequence();
    public:
      AbstractSequence();
      
      virtual AbstractSequence *create_similar() = 0;
      
      virtual String raw_substring(int start, int length) = 0;
      virtual uint32_t char_at(int pos) = 0; // return 0 on Out-Of-Range
      virtual bool is_placeholder(int i) = 0;
      
      virtual void ensure_boxes_valid() = 0;
      
      virtual int insert(int pos, uint16_t chr) { return insert(pos, String::FromChar(chr)); }
      virtual int insert(int pos, AbstractSequence *seq, int start, int end);
      
      virtual int insert(int pos, const String &s) = 0;
      virtual int insert(int pos, Box *box) = 0;
      virtual void remove(int start, int end) = 0;
      
      virtual Box *extract_box(int boxindex) = 0;
      virtual bool try_load_from_object(Expr object, BoxInputFlags options) override;
      virtual void load_from_object(Expr object, BoxInputFlags options) = 0;
      
      virtual Box *dynamic_to_literal(int *start, int *end) override;
      
      BoxSize &var_extents() { return _extents; }
      
      float get_em() { return em; }
      virtual int get_line(int index, int guide = 0) = 0; // 0, 1, ...
      
      virtual void get_line_heights(int line, float *ascent, float *descent) = 0;
      
      virtual bool request_repaint_range(int start, int end) override;
      
    protected:
      ObservableValue<float> em;
  };
}

#endif // RICHMATH__BOXES__BOX_H__INCLUDED
