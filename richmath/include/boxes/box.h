#ifndef __BOXES__BOX_H__
#define __BOXES__BOX_H__

#include <graphics/shapers.h>
#include <util/frontendobject.h>
#include <util/sharedptr.h>
#include <util/style.h>


namespace richmath {
  typedef enum {
    Forward,
    Backward
  } LogicalDirection;
  
  class Box;
  class SyntaxState;
  
  class MouseEvent {
    public:
      MouseEvent();
      
      void set_source(Box *new_source);
      
    public:
      float x, y;
      bool left;
      bool middle;
      bool right;
      
      Box *source;
  };
  
  typedef enum {
    KeyUnknown = 0,
    
    KeyLeft,
    KeyRight,
    KeyUp,
    KeyDown,
    KeyHome,
    KeyEnd,
    KeyPageUp,
    KeyPageDown,
    KeyBackspace,
    KeyDelete,
    KeyReturn,
    KeyTab,
    KeyEscape,
    KeyF1,
    KeyF2,
    KeyF3,
    KeyF4,
    KeyF5,
    KeyF6,
    KeyF7,
    KeyF8,
    KeyF9,
    KeyF10,
    KeyF11,
    KeyF12
  } SpecialKey;
  
  class SpecialKeyEvent {
    public:
      SpecialKey key;
      bool ctrl;
      bool alt;
      bool shift;
  };
  
  enum {
    BoxFlagDefault      = 0,
    BoxFlagParseable    = 1, // no StyleBox with StripOnInput->True, ...
    BoxFlagLiteral      = 2, // no DynamicBox
    BoxFlagShortNumbers = 4  // not the internal representation of NumberBox, but the content()
  };
  
  enum {
    BoxOptionDefault       = 0,
    BoxOptionFormatNumbers = 1
  };
  
  class Box: public FrontEndObject {
    public:
      Box();
      virtual ~Box();
      
      template<class T>
      static T *try_create(Expr expr, int options){
        T *box = new T();
        
        if(!box->try_load_from_object(expr, options)){
          delete box;
          return 0;
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
      virtual Expr to_pmath(int flags) = 0; // BoxFlagXXX
      virtual Expr to_pmath(int flags, int start, int end) {
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
      virtual void dynamic_updated();
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
      
      // ignore parents
      int    get_own_style(IntStyleOptionName    n, int    result = 0);
      float  get_own_style(FloatStyleOptionName  n, float  result = 0.0);
      String get_own_style(StringStyleOptionName n, String result);
      Expr   get_own_style(ObjectStyleOptionName n, Expr   result);
      String get_own_style(StringStyleOptionName n);
      Expr   get_own_style(ObjectStyleOptionName n);
      
      virtual void reset_style(){ if(style) style->clear(); }
      
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
      
      virtual bool try_load_from_object(Expr expr, int options) { return false; }
      
      virtual Box *item(int i) { return 0; }
      virtual int  count() {     return 0; }
      virtual int  length() {    return 0; }
      
      virtual void resize(Context *context) {}
      virtual void paint(Context *context) {}
      
      virtual Box *remove(int *index) { return this; }
      
      virtual Expr to_pmath_symbol() { return Expr(); }
      virtual Expr to_pmath(int flags) { return Expr(); }
  };
  
  class AbstractSequence: public Box {
    public:
      AbstractSequence();
      virtual ~AbstractSequence();
      
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
      virtual bool try_load_from_object(Expr object, int options);
      virtual void load_from_object(Expr object, int options) = 0; // BoxOptionXXX
      
      virtual Box *dynamic_to_literal(int *start, int *end);
      
      BoxSize &var_extents() { return _extents; }
      
      float get_em() { return em; }
      virtual int get_line(int index, int guide = 0) = 0; // 0, 1, ...
      
      virtual void get_line_heights(int line, float *ascent, float *descent) = 0;
      
      virtual bool request_repaint_range(int start, int end);
      
    protected:
      float em;
  };
}

#endif // __BOXES__BOX_H__
