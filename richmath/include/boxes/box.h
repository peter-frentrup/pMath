#ifndef __BOXES__BOX_H__
#define __BOXES__BOX_H__

#include <graphics/shapers.h>
#include <util/pmath-extra.h>
#include <util/sharedptr.h>
#include <util/style.h>

namespace richmath{
  typedef enum {
    Forward, 
    Backward
  } LogicalDirection;
  
  class Box;
  class SyntaxState;
  
  class MouseEvent{
    public:
      MouseEvent(): source(0){}
      
      void set_source(Box *new_source);
      
    public:
      float x;
      float y;
      bool left;
      bool middle;
      bool right;
      
      Box *source;
  };
  
  typedef enum{
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
  }SpecialKey;
  
  class SpecialKeyEvent{
    public:
      SpecialKey key;
      bool ctrl;
      bool alt;
      bool shift;
  };
  
  class Box: public Base{
    public:
      Box();
      virtual ~Box();
      
      Box *parent(){ return _parent; }
      int  index(){  return _index; }
      
      int id(){ return _id; }
      static Box *find(int id);
      static Box *find(Expr frontendobject);
      void swap_id(Box *other);
      
      template<class T>
      T *find_parent(bool selfincluding){
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
      virtual int length(){ return count(); }
      
      const BoxSize &extents(){ return _extents; }
      
      virtual bool expand(const BoxSize &size){ return false; }
      virtual void resize(Context *context) = 0;
      virtual void colorize_scope(SyntaxState *state);
      virtual void clear_coloring();
      virtual void paint(Context *context) = 0;
      
      virtual Box *remove(int *index) = 0;
      
      virtual pmath_t to_pmath(bool parseable) = 0;
      virtual pmath_t to_pmath(bool parseable, int start, int end){
        return to_pmath(parseable);
      }
      
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
      
      // ?todo: rename to transform_from_child()
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix);
      
      // ?todo: rename to transform_to_parent()
      void transformation(
        Box            *parent, 
        cairo_matrix_t *matrix);
      
      virtual bool remove_inserts_placeholder(){ return true; }
      
      virtual bool exitable(){ return true; }
      virtual bool selectable(int i = -1);
      
      virtual Box *normalize_selection(int *start, int *end);
      
      virtual void dynamic_updated(){}
      bool request_repaint_all();
      virtual bool request_repaint(float x, float y, float w, float h);
      virtual void invalidate();
      virtual bool edit_selection(Context *context); // *not* automatically called
      
      virtual bool changes_children_style(){ return false; }
      
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
      
      virtual Box *mouse_sensitive();
      virtual void on_mouse_enter();
      virtual void on_mouse_exit();
      virtual void on_mouse_down(MouseEvent &event);
      virtual void on_mouse_move(MouseEvent &event);
      virtual void on_mouse_up(  MouseEvent &event);
      virtual void on_mouse_cancel();
      
      virtual void on_enter();
      virtual void on_exit();
      
      virtual void on_key_down(SpecialKeyEvent &event);
      virtual void on_key_up(  SpecialKeyEvent &event);
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
      
    private:
      int _id;
  };
  
  class DummyBox: public Box {
    public:
      DummyBox(): Box() {}
      ~DummyBox(){}
      
      Box *item(int i){ return 0; }
      int count(){ return 0; }
      int length(){ return 0; }
      
      void resize(Context *context){}
      void paint(Context *context){}
      
      Box *remove(int *index){ return this; }
      
      pmath_t to_pmath(bool parseable){ return NULL; }
  };

  class AbstractSequence: public Box {
    public:
      AbstractSequence();
      virtual ~AbstractSequence();
      
      virtual void selection_path(Context *context, int start, int end) = 0;
      
      virtual int insert(int pos, uint16_t chr){ return insert(pos, String::FromChar(chr)); }
      virtual int insert(int pos, const String &s) = 0; // unsafe: allows PMATH_BOX_CHAR
      virtual int insert(int pos, Box *box) = 0;
      virtual void remove(int start, int end) = 0;
      
      virtual Box *extract_box(int boxindex) = 0;
      virtual void load_from_object(Expr object, int options) = 0; // BoxOptionXXX
      
      BoxSize &var_extents(){ return _extents; }
      
      float get_em(){ return em; }
      
    protected:
      float em;
  };
}

#endif // __BOXES__BOX_H__
