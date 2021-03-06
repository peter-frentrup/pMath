#include <gui/native-widget.h>

#include <climits>
#include <cmath>

#include <gui/document.h>


using namespace richmath;

namespace richmath {
  class NativeWidgetImpl {
    public:
      NativeWidgetImpl(NativeWidget &_self) : self(_self) {}
      
      void finish_idle_after_edit();
      void abort_idle_after_edit();
      
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
  
  class DummyNativeWidget final : public NativeWidget {
    public:
      DummyNativeWidget(): NativeWidget(nullptr) {
      }
      
      virtual Vector2F window_size() override { return Vector2F(0, 0); }
      virtual Vector2F page_size() override { return Vector2F(0, 0); }
      
      virtual bool is_scrollable() override { return false; }
      virtual bool autohide_vertical_scrollbar() override { return false; }
      virtual Point scroll_pos() override { return Point(0,0); }
      virtual void scroll_to(Point pos) override {}
      
      virtual void show_tooltip(Box *source, Expr boxes) override {}
      virtual void hide_tooltip() override {}
      
      virtual bool is_scaleable() override { return false; }
      
      virtual double double_click_time() override { return 0; }
      virtual double message_time() override { return 0; }
      virtual Vector2F double_click_dist() override { return Vector2F(0, 0); }
      
      virtual void do_drag_drop(const VolatileSelection &src, MouseEvent &event) override {}
      
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
      
      virtual String directory() override { return String(); }
      virtual void directory(String new_directory) override {}
      
      virtual String filename() override { return String(); }
      virtual void filename(String new_filename) override {}
      
      virtual String full_filename() override { return String(); }
      virtual void full_filename(String new_full_filename) override {}
      
      virtual void on_saved() override {}
      
      virtual bool is_foreground_window() override { return false; }
      virtual bool is_focused_widget() override { return false; }
      virtual bool is_using_dark_mode() override { return false; }
  };
}

static DummyNativeWidget staticdummy;

NativeWidget *NativeWidget::dummy = &staticdummy;

NativeWidget::NativeWidget(Document *doc)
  : FrontEndObject(),
    _custom_scale_factor(ScaleDefault),
    _dpi(96),
    _document(nullptr),
    _source_box(FrontEndReference::None),
    _owner_document(FrontEndReference::None),
    _stylesheet_document(FrontEndReference::None)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  adopt(doc);
}

NativeWidget::~NativeWidget() {
  if(_document)
    _document->_native = dummy;
  NativeWidgetImpl(*this).abort_idle_after_edit();
  delete _document;
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

bool NativeWidget::may_drop_into(const VolatileSelection &dst, bool self_is_source) {
  if(!dst || !dst.box->get_style(Editable) || !dst.box->selectable(dst.start))
    return false;
    
  if(self_is_source) {
    if(VolatileSelection src = drag_source_reference().get_all())
      return !src.visually_contains(dst);
  }
  
  return true;
}

CursorType NativeWidget::text_cursor(Vector2F dir) {
  int part = (int)round(atan2(dir.x, dir.y) * 4 / M_PI); // note: x and y are reversed
  if(part == -4)
    part = 4;
    
  return (CursorType)((int)CursorType::TextN + part);
}

CursorType NativeWidget::text_cursor(Box *box, int index) {
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  box->transformation(0, &mat);
  
  return text_cursor(Vector2F(mat.xy, mat.yy));
}

CursorType NativeWidget::size_cursor(Vector2F dir, CursorType base) {
  int delta = (int)base - (int)CursorType::SizeN;
  
  if(delta < -8 || delta > 8)
    return base;
    
  int part = (int)round(atan2(dir.x, dir.y) * 4 / M_PI); // note: x and y are reversed
  if(part == -4)
    part = 4;
    
  part += delta;
  
  if(part > 4)
    part -= 8;
  else if(part <= -4)
    part += 8;
    
  return (CursorType)((int)CursorType::SizeN + part);
}

CursorType NativeWidget::size_cursor(Box *box, CursorType base) {
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  box->transformation(0, &mat);
  
  return size_cursor(Vector2F(mat.xy, mat.yy), base);
}

void NativeWidget::on_editing() {
  if(TimedEvent *ev = _idle_after_edit_or_limbo_next.as_normal()) {
    ev->reset_timer();
  }
  else if(_idle_after_edit_or_limbo_next.is_normal()) {
    SharedPtr<TimedEvent> ev = new IdleAfterEditEvent(id());
    ev->ref();
    _idle_after_edit_or_limbo_next.set_to_normal(ev.ptr());
    register_timed_event(ev);
  }
}

void NativeWidget::on_idle_after_edit() {
  Document *owner = owner_document();
  if(owner && owner->native()->stylesheet_document() == _document) {
    Expr expr = _document->to_pmath(BoxOutputFlags::Default | BoxOutputFlags::NoNewSymbols);
    owner->style->set_pmath(StyleDefinitions, expr);
    owner->on_style_changed(true); // TODO: check if a layout-style was changed
  }
}

Box *NativeWidget::source_box() {
  return FrontEndObject::find_cast<Box>(_source_box);
}

bool NativeWidget::source_box(Box *box) {
  if(box) {
    if(_source_box.unobserved_equals(box->id()))
      return true;
    
    Document *doc = box->find_parent<Document>(true);
    while(doc) {
      if(doc->native() == this) {
        pmath_debug_print("[Cannot set source_box, because that would introduce a reference cycle]\n");
        return false;
      }
      
      Box *tmp = doc->native()->source_box();
      if(!tmp)
        break;
        
      doc = tmp->find_parent<Document>(true);
    }
    
    _source_box = box->id();
  }
  else
    _source_box = FrontEndReference::None;
    
  return true;
}
      

Document *NativeWidget::owner_document() {
  return FrontEndObject::find_cast<Document>(_owner_document); 
}

bool NativeWidget::owner_document(Document *owner) {
  return owner_document(owner ? owner->id() : FrontEndReference::None); 
}

bool NativeWidget::owner_document(FrontEndReference owner) {
  if(_owner_document) {
    pmath_debug_print("[NativeWidget: owner_document already set]\n");
    return false;
  }
  
  // TODO: beware of cyclic references
  _owner_document = owner;
  return true;
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

void NativeWidget::next_in_limbo(FrontEndObject *next) {
  RICHMATH_ASSERT( _idle_after_edit_or_limbo_next.is_normal() );
  NativeWidgetImpl(*this).abort_idle_after_edit();
  _idle_after_edit_or_limbo_next.set_to_tinted(next);
}
      
//{ class NativeWidgetImpl ...

void NativeWidgetImpl::finish_idle_after_edit() {
  abort_idle_after_edit();
  self.on_idle_after_edit();
}

void NativeWidgetImpl::abort_idle_after_edit() {
  if(TimedEvent *ev = self._idle_after_edit_or_limbo_next.as_normal()) {
    self._idle_after_edit_or_limbo_next.set_to_normal(nullptr);
    ev->unref();
  }
}
      
//} ... class NativeWidgetImpl
