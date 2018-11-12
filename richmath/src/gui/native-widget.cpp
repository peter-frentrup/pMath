#include <gui/native-widget.h>

#include <climits>
#include <cmath>

#include <gui/document.h>

using namespace richmath;

namespace richmath {
  class NativeWidgetImpl {
    public:
      NativeWidgetImpl(NativeWidget &_self) : self(_self) {}
      
      void finish_idle_after_edit() {
        self._idle_after_edit = nullptr;
        self.on_idle_after_edit();
      }
      
      void abort_idle_after_edit() {
        self._idle_after_edit = nullptr;
      }
      
    private:
      NativeWidget &self;
  };
}

namespace {
  class IdleAfterEditEvent : public TimedEvent {
    public:
      explicit IdleAfterEditEvent(FrontEndReference _id) 
        : TimedEvent(0.25), 
          widget_id(_id) 
      {
      }
      
      virtual ~IdleAfterEditEvent() {
        NativeWidget *wid = FrontEndObject::find_cast<NativeWidget>(widget_id);
        if(wid) 
          NativeWidgetImpl(*wid).abort_idle_after_edit();
      }
      
      virtual void execute_event() override {
        NativeWidget *wid = FrontEndObject::find_cast<NativeWidget>(widget_id);
        if(wid) {
          NativeWidgetImpl(*wid).finish_idle_after_edit();
          widget_id = FrontEndReference::None;
        }
      }
      
    private:
      FrontEndReference widget_id;
  };
  
  class DummyNativeWidget: public NativeWidget {
    public:
      DummyNativeWidget(): NativeWidget(nullptr) {
      }
      
      virtual void window_size(float *w, float *h) override {
        *w = *h = 0;
      }
      
      virtual void page_size(float *w, float *h) override {
        *w = *h = 0;
      }
      
      virtual bool is_scrollable() override { return false; }
      virtual bool autohide_vertical_scrollbar() override { return false; }
      virtual void scroll_pos(float *x, float *y) override {
        *x = *y = 0;
      }
      
      virtual void scroll_to(float x, float y) override {}
      
      virtual void show_tooltip(Expr boxes) override {}
      virtual void hide_tooltip() override {}
      
      virtual bool is_scaleable() override { return false; }
      
      virtual double double_click_time() override { return 0; }
      virtual double message_time() override { return 0; }
      virtual void double_click_dist(float *dx, float *dy) override {
        *dx = *dy = 0;
      }
      virtual void do_drag_drop(Box *src, int start, int end) override {
      }
      virtual bool cursor_position(float *x, float *y) override {
        *x = *y = 0;
        return false;
      }
      
      virtual void bring_to_front() override {}
      
      virtual void close() override {}
      
      virtual void invalidate() override {}
      
      virtual void invalidate_options() override {}
      
      virtual void force_redraw() override {}
      
      virtual void set_cursor(CursorType type) override {}
      
      virtual void running_state_changed() override {}
      
      virtual bool is_mouse_down() override { return false; }
      
      virtual void beep() override {};
      
      virtual bool register_timed_event(SharedPtr<TimedEvent> event) override {
        return false;
      }
      
      virtual String filename() override { return String(); }
      virtual void filename(String new_filename) override {}
      
      virtual void on_saved() override {}
  };
}

static DummyNativeWidget staticdummy;

NativeWidget *NativeWidget::dummy = &staticdummy;

NativeWidget::NativeWidget(Document *doc)
  : FrontEndObject(),
    _custom_scale_factor(ScaleDefault),
    _dpi(96),
    _document(0),
    _owner_document(FrontEndReference::None),
    _stylesheet_document(FrontEndReference::None)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  adopt(doc);
}

NativeWidget::~NativeWidget() {
  if(_document)
    _document->_native = dummy;
  delete _document;
}

void NativeWidget::scroll_by(float dx, float dy) {
  float x, y;
  scroll_pos(&x, &y);
  scroll_to(x + dx, y + dy);
}

void NativeWidget::scale_by(float ds) {
  set_custom_scale(custom_scale_factor() * ds);
}

void NativeWidget::set_custom_scale(float s) {

  if( !is_scaleable()           ||
      s == _custom_scale_factor ||
      s < ScaleMin              ||
      s > ScaleMax)
  {
    return;
  }
  
  _custom_scale_factor = s;
  if(fabs(_custom_scale_factor - ScaleDefault) < 0.0001)
    _custom_scale_factor = ScaleDefault;
  
  Document *doc = document();
  if(doc && doc->style){
    SharedPtr<Stylesheet> stylesheet = doc->stylesheet();
    SharedPtr<Style> doc_base_style = stylesheet->find_parent_style(doc->style);
    
    float docScaleDefault = ScaleDefault;
    stylesheet->get(doc_base_style, Magnification, &docScaleDefault);
    
    if(docScaleDefault == _custom_scale_factor) {
      pmath_debug_print("[skip Magnification -> %f]\n", _custom_scale_factor);
      doc->style->remove(Magnification);
    }
    else {
      doc->style->set(Magnification, _custom_scale_factor);
    }
  }
  
  if(_document)
    _document->invalidate_all();
}

bool NativeWidget::may_drop_into(Box *dst, int start, int end, bool self_is_source) {
  if(!dst || !dst->get_style(Editable) || !dst->selectable(start))
    return false;
    
  if(self_is_source) {
    if(Box *src = drag_source_reference().get()) {
      Box *box = Box::common_parent(src, dst);
      if(box == src) {
        int s = start;
        int e = end;
        box = dst;
        
        if( box == src &&
            s <= drag_source_reference().end &&
            e >= drag_source_reference().start)
        {
          return false;
        }
        
        while(box != src) {
          s = box->index();
          e = s + 1;
          box = box->parent();
        }
        
        if(s < drag_source_reference().end && e > drag_source_reference().start)
          return false;
      }
      else if(box == dst) {
        int s = drag_source_reference().start;
        int e = drag_source_reference().end;
        box = src;
        
        while(box != dst) {
          s = box->index();
          e = s + 1;
          box = box->parent();
        }
        
        if(s < end && e > start)
          return false;
      }
    }
  }
  
  return true;
}

CursorType NativeWidget::text_cursor(float dx, float dy) {
  int part = (int)floor(atan2(dx, dy) * 4 / M_PI + 0.5);
  if(part == -4)
    part = 4;
    
  return (CursorType)(TextNCursor + part);
}

CursorType NativeWidget::text_cursor(Box *box, int index) {
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  box->transformation(0, &mat);
  
  return text_cursor(mat.xy, mat.yy);
}

CursorType NativeWidget::size_cursor(float dx, float dy, CursorType base) {
  int delta = base - SizeNCursor;
  
  if(delta < -8 || delta > 8)
    return base;
    
  int part = (int)floor(atan2(dx, dy) * 4 / M_PI + 0.5);
  if(part == -4)
    part = 4;
    
  part += delta;
  
  if(part > 4)
    part -= 8;
  else if(part <= -4)
    part += 8;
    
  return (CursorType)(SizeNCursor + part);
}

CursorType NativeWidget::size_cursor(Box *box, CursorType base) {
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  box->transformation(0, &mat);
  
  return size_cursor(mat.xy, mat.yy, base);
}

void NativeWidget::on_editing() {
  if(_idle_after_edit) {
    _idle_after_edit->reset_timer();
  }
  else {
    _idle_after_edit = new IdleAfterEditEvent(id());
    register_timed_event(_idle_after_edit);
  }
}

void NativeWidget::on_idle_after_edit() {
  Document *owner = owner_document();
  if(owner && owner->native()->stylesheet_document() == _document) {
    Expr expr = _document->to_pmath(BoxOutputFlags::Default);
    owner->style->set_pmath(StyleDefinitions, expr);
    owner->invalidate_options();
  }
}

Document *NativeWidget::owner_document(){
  return FrontEndObject::find_cast<Document>(_owner_document); 
}

Document *NativeWidget::stylesheet_document() {
  if(!_stylesheet_document) {
    auto wa = working_area_document();
    if(wa)
      return wa->native()->stylesheet_document();
  }
  
  return FrontEndObject::find_cast<Document>(_stylesheet_document); 
}

bool NativeWidget::stylesheet_document(Document *doc) {
  FrontEndReference doc_id = doc->id();
  
  if(_owner_document)
    return false;
  
  Document *old_sd = stylesheet_document();
  if(doc) {
    NativeWidget *wid = doc->native();
    if(wid->_owner_document && wid->_owner_document != _document->id()) 
      return false;
    
    wid->_owner_document = _document->id();
    _stylesheet_document = doc->id();
  }
  else
    _stylesheet_document = FrontEndReference::None;
  
  if(old_sd && old_sd != doc)
    old_sd->native()->close();
  
  return true;
}

void NativeWidget::adopt(Document *doc) {
  if(_document) {
    _document->_native = dummy;
    _document->safe_destroy();
  }
  
  assert(!doc || doc->_native == dummy || doc->_native == this);
  if(doc)
    doc->_native = this;
  _document = doc;
}

Context *NativeWidget::document_context() {
  if(_document)
    return &_document->context;
    
  return 0;
}

SelectionReference &NativeWidget::drag_source_reference() {
  return _document->drag_source;
}
