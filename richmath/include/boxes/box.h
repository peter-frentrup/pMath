#ifndef __BOXES__BOX_H__
#define __BOXES__BOX_H__

#include <graphics/shapers.h>
#include <util/frontendobject.h>
#include <util/sharedptr.h>
#include <util/style.h>


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
      float      x, y;
      int        id;
      DeviceKind device;
      bool       left;
      bool       middle;
      bool       right;
      
      Box *origin;
  };
  
  enum class SpecialKey {
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
  
  enum class BoxFlags {
    Default      = 0,
    Parseable    = 1, // no StyleBox with StripOnInput->True, ...
    Literal      = 2, // no DynamicBox
    ShortNumbers = 4  // not the internal representation of NumberBox, but the content()
  };
  
  inline bool has(BoxFlags lhs, BoxFlags rhs) {
    return ((int)lhs & (int)rhs) == (int)rhs;
  }
  inline BoxFlags operator |(BoxFlags lhs, BoxFlags rhs) {
    return (BoxFlags)((int)lhs | (int)rhs);
  }
  inline BoxFlags &operator |=(BoxFlags &lhs, BoxFlags rhs) {
    lhs = (BoxFlags)((int)lhs | (int)rhs);
    return lhs;
  }
  inline BoxFlags operator -(BoxFlags lhs, BoxFlags rhs) {
    return (BoxFlags)((int)lhs & ~(int)rhs);
  }
  inline BoxFlags &operator -=(BoxFlags &lhs, BoxFlags rhs) {
    lhs = (BoxFlags)((int)lhs & ~(int)rhs);
    return lhs;
  }
  
  enum {
    BoxOptionDefault       = 0,
    BoxOptionFormatNumbers = 1
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
  
  class Box: public FrontEndObject {
      friend class AutoMemorySuspension;
    public:
      Box();
      virtual ~Box();
      
      /** Mark the box for deletion.
      
          You should normally use this function instead of delete.
          \see AutoMemorySuspension
       */
      void safe_destroy();
      
      template<class T>
      static T *try_create(Expr expr, int options) {
        T *box = new T();
        
        if(!box->try_load_from_object(expr, options)) {
          delete box;
          return nullptr;
        }
        
        return box;
      }
      
      Box *parent() { return _parent; }
      int  index() {  return _index; }
      
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
      
      virtual Box *item(int i) = 0;
      virtual int count() = 0;
      virtual int length() { return count(); }
      
      const BoxSize &extents() { return _extents; }
      
      virtual bool expand(const BoxSize &size) { return false; }
      virtual void resize(Context *context) = 0;
      virtual void colorize_scope(SyntaxState *state);
      virtual void paint(Context *context) = 0;
      virtual Box *get_highlight_child(Box *src, int *start, int *end);
      virtual void selection_path(Canvas *canvas, int start, int end);
      virtual void scroll_to(float x, float y, float w, float h);
      virtual void scroll_to(Canvas *canvas, Box *child, int start, int end);
      void default_scroll_to(Canvas *canvas, Box *parent, Box *child, int start, int end);
      
      virtual Box *remove(int *index) = 0;
      
      virtual Expr to_pmath_symbol() = 0;
      virtual Expr to_pmath(BoxFlags flags) = 0;
      virtual Expr to_pmath(BoxFlags flags, int start, int end) {
        return to_pmath(flags);
      }
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index);
        
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,  // [in/out]
        int              *index,        // [in/out], -1 if called from parent
        bool              called_from_child);
        
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start);
        
      // ?todo: rename to transform_from_child()
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix);
        
      // ?todo: rename to transform_to_parent()
      void transformation(
        Box            *parent,
        cairo_matrix_t *matrix);
        
      virtual bool remove_inserts_placeholder() { return true; }
      
      virtual bool exitable() { return true; }
      virtual bool selectable(int i = -1);
      
      virtual Box *normalize_selection(int *start, int *end);
      
      virtual Expr prepare_dynamic(Expr expr);
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) {}
      
      virtual bool try_load_from_object(Expr object, int options) = 0; // BoxOptionXXX
      virtual Box *dynamic_to_literal(int *start, int *end);
      
      bool         request_repaint_all();
      virtual bool request_repaint_range(int start, int end);
      virtual bool request_repaint(float x, float y, float w, float h);
      virtual void invalidate();
      virtual void invalidate_options();
      virtual bool edit_selection(Context *context); // *not* automatically called
      
      virtual bool changes_children_style() { return false; }
      
      virtual SharedPtr<Stylesheet> stylesheet();
      
      int    get_style(IntStyleOptionName    n, int    result = 0);
      float  get_style(FloatStyleOptionName  n, float  result = 0.0);
      String get_style(StringStyleOptionName n, String result);
      Expr   get_style(ObjectStyleOptionName n, Expr   result);
      String get_style(StringStyleOptionName n);
      Expr   get_style(ObjectStyleOptionName n);
      Expr   get_pmath_style(Expr n);
      
      // ignore parents
      int    get_own_style(IntStyleOptionName    n, int    result = 0);
      float  get_own_style(FloatStyleOptionName  n, float  result = 0.0);
      String get_own_style(StringStyleOptionName n, String result);
      Expr   get_own_style(ObjectStyleOptionName n, Expr   result);
      String get_own_style(StringStyleOptionName n);
      Expr   get_own_style(ObjectStyleOptionName n);
      
      virtual void reset_style() { if(style) style->clear(); }
      
      virtual Box *mouse_sensitive();
      virtual void on_mouse_enter();
      virtual void on_mouse_exit();
      virtual void on_mouse_down(MouseEvent &event);
      virtual void on_mouse_move(MouseEvent &event);
      virtual void on_mouse_up(MouseEvent &event);
      virtual void on_mouse_cancel();
      
      virtual void on_enter();
      virtual void on_exit();
      virtual void on_finish_editing();
      
      virtual void on_key_down(SpecialKeyEvent &event);
      virtual void on_key_up(SpecialKeyEvent &event);
      virtual void on_key_press(uint32_t unichar);
      
    public:
      SharedPtr<Style> style;
      
    protected:
      void adopt(Box *child, int i);
      void abandon(Box *child);
      
    protected:
      BoxSize  _extents;
      Box     *_parent;
      int      _index;
  };
  
  class DummyBox: public Box {
    public:
      DummyBox(): Box() {}
      virtual ~DummyBox() {}
      
      virtual bool try_load_from_object(Expr expr, int options) override { return false; }
      
      virtual Box *item(int i) override { return nullptr; }
      virtual int  count() override {     return 0; }
      virtual int  length() override {    return 0; }
      
      virtual void resize(Context *context) override {}
      virtual void paint(Context *context) override {}
      
      virtual Box *remove(int *index) override { return this; }
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual Expr to_pmath(BoxFlags flags) override { return Expr(); }
  };
  
  class AbstractSequence: public Box {
    public:
      AbstractSequence();
      virtual ~AbstractSequence();
      
      virtual AbstractSequence *create_similar() = 0;
      
      virtual String raw_substring(int start, int length) = 0;
      virtual uint32_t char_at(int pos) = 0; // return 0 on Out-Of-Range
      virtual bool is_placeholder(int i) = 0;
      
      virtual void ensure_boxes_valid() = 0;
      
      virtual int insert(int pos, uint16_t chr) { return insert(pos, String::FromChar(chr)); }
      virtual int insert(int pos, AbstractSequence *seq, int start, int end);
      
      virtual int insert(int pos, const String &s) = 0; // unsafe: allows PMATH_BOX_CHAR
      virtual int insert(int pos, Box *box) = 0;
      virtual void remove(int start, int end) = 0;
      
      virtual Box *extract_box(int boxindex) = 0;
      virtual bool try_load_from_object(Expr object, int options) override;
      virtual void load_from_object(Expr object, int options) = 0; // BoxOptionXXX
      
      virtual Box *dynamic_to_literal(int *start, int *end) override;
      
      BoxSize &var_extents() { return _extents; }
      
      float get_em() { return em; }
      virtual int get_line(int index, int guide = 0) = 0; // 0, 1, ...
      
      virtual void get_line_heights(int line, float *ascent, float *descent) = 0;
      
      virtual bool request_repaint_range(int start, int end) override;
      
    protected:
      float em;
  };
}

#endif // __BOXES__BOX_H__
