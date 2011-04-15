#include <gui/native-widget.h>

#include <climits>
#include <cmath>

#include <gui/document.h>

using namespace richmath;

class DummyNativeWidget: public NativeWidget{
  public:
    DummyNativeWidget(): NativeWidget(0){
    }
    
    virtual void window_size(float *w, float *h){
      *w = *h = 0;
    }
    
    virtual void page_size(float *w, float *h){
      *w = *h = 0;
    }
    
    virtual bool is_scrollable(){ return false; }
    virtual bool autohide_vertical_scrollbar(){ return false; }
    virtual void scroll_pos(float *x, float *y){
      *x = *y = 0;
    }
    
    virtual void scroll_to(float x, float y){}
    
    virtual bool is_scaleable(){ return false; }
    
    virtual double double_click_time(){ return 0; }
    virtual double message_time(){ return 0; }
    virtual void double_click_dist(float *dx, float *dy){
      *dx = *dy = 0;
    }
    virtual void do_drag_drop(Box *src, int start, int end){
    }
    virtual bool cursor_position(float *x, float *y){
      *x = *y = 0;
      return false;
    }
    
    virtual void close(){}
    
    virtual void invalidate(){}
    
    virtual void force_redraw(){}
      
    virtual void set_cursor(CursorType type){}
    
    virtual void running_state_changed(){}
    
    virtual bool is_mouse_down(){ return false; }
    
    virtual void beep(){};
    
    virtual bool register_timed_event(SharedPtr<TimedEvent> event){
      return false;
    }
    
};

static DummyNativeWidget staticdummy;

NativeWidget *NativeWidget::dummy = &staticdummy;

NativeWidget::NativeWidget(Document *doc)
: Base(),
  _scale_factor(ScaleDefault),
  _document(0)
{
  adopt(doc);
}

NativeWidget::~NativeWidget(){
  if(_document)
    _document->_native = dummy;
  delete _document;
}

void NativeWidget::scroll_by(float dx, float dy){
  float x, y;
  scroll_pos(&x, &y);
  scroll_to(x + dx, y + dy);
}
     
void NativeWidget::scale_by(float ds){
  set_scale(scale_factor() * ds);
}

void NativeWidget::set_scale(float s){
  if(!is_scaleable() 
  || s == _scale_factor 
  || s < ScaleMin 
  || s > ScaleMax )
    return;
  
  _scale_factor = s;
  if(fabs(_scale_factor - ScaleDefault) < 0.0001)
    _scale_factor = ScaleDefault;
  
  if(_document)
    _document->invalidate_all();
}

CursorType NativeWidget::text_cursor(float dx, float dy){
  int part = (int)floor(atan2(dx, dy) * 4 / M_PI + 0.5);
  if(part == -4)
    part = 4;
            
  return (CursorType)(TextNCursor + part);
}

CursorType NativeWidget::text_cursor(Box *box, int index){
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  box->transformation(0, &mat);
  
  return text_cursor(mat.xy, mat.yy);
}

void NativeWidget::adopt(Document *doc){
  if(_document)
    _document->_native = dummy;
  delete _document;
  
  assert(!doc || doc->_native == dummy || doc->_native == this);
  if(doc)
    doc->_native = this;
  _document = doc;
}

Context *NativeWidget::document_context(){
  if(_document)
    return &_document->context;
  
  return 0;
}

SelectionReference &NativeWidget::drag_source_reference(){
  return _document->drag_source;
}
