#include <gui/fltk/basic-fltk-widget.h>

#include <pmath-cpp.h>

#include <FL/Fl.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Box.H>

namespace richmath {
  class BasicFltkWidget::Connector: public Fl_Callback_User_Data {
    public:
      explicit Connector(BasicFltkWidget *owner);
      virtual ~Connector();
      
      BasicFltkWidget *owner() { return _owner; }
      
      static void static_callback(Fl_Widget *sender, void *_this);
      
    private:
      BasicFltkWidget *_owner;
  };
}

using namespace richmath;

//{ class BasicFltkWidget ...

BasicFltkWidget::BasicFltkWidget()
  : Base(),
  _widget(nullptr),
  _initializing(true),
  _destroying(false)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

void BasicFltkWidget::after_construction() {
  if(!_widget) {
    _widget = new Fl_Box(FL_NO_BOX, 0, 0, 100, 100, nullptr);
  }
  
  _widget->callback(Connector::static_callback, new Connector(this), true);
}

BasicFltkWidget::~BasicFltkWidget() {
  pmath_debug_print("[~BasicFltkWidget {%s} %p widget: %p]\n", get_debug_tag(), this, _widget);
  if(!_destroying) {
    fprintf(stderr, "[Unexpected destruction]\n");
  }
  if(auto wid = _widget) {
    _widget = nullptr;
    
    delete wid;
  }
}

BasicFltkWidget *BasicFltkWidget::from_widget(Fl_Widget *wid) {
  if(!wid)
    return nullptr;
  
  return ((BasicFltkWidget::Connector*)wid->user_data())->owner();
}

void BasicFltkWidget::on_callback(Fl_Widget *sender) {
  pmath_debug_print("[BasicFltkWidget::on_callback %p sender %p reason %d]\n", this, sender, Fl::callback_reason());
}

//} ... class BasicFltkWidget

//{ class BasicFltkWidget::Connector ...

BasicFltkWidget::Connector::Connector(BasicFltkWidget *owner)
: _owner(owner)
{
}

BasicFltkWidget::Connector::~Connector() {
  pmath_debug_print("[~BasicFltkWidget::Connector %p owner: %p]\n", this, _owner);
  if(!_owner)
    return;
  
  if(!_owner->destroying()) {
    _owner->_widget = nullptr;
    _owner->destroy();
  }
}

void BasicFltkWidget::Connector::static_callback(Fl_Widget *sender, void *_this) {
  if(BasicFltkWidget *owner = ((Connector*)_this)->_owner) {
    //AutoMemorySuspension ams;
    owner->on_callback(sender);
  }
}

//} ... class BasicFltkWidget::Connector
