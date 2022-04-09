#include <util/styled-object.h>

#include <boxes/abstractsequence.h>
#include <gui/common-document-windows.h>
#include <gui/document.h>


using namespace richmath;

namespace {
  template<typename N, typename T>
  struct Stylesheet_get {
    static bool impl(SharedPtr<Stylesheet> all, SharedPtr<Style> style, N n, T *result);
  };
  
  struct Stylesheet_get_pmath {
    static bool impl(SharedPtr<Stylesheet> all, SharedPtr<Style> style, StyleOptionName n, Expr *result);
  };
}

extern pmath_symbol_t richmath_System_Inherited;
extern pmath_symbol_t richmath_System_SingleMatch;

static bool StyleName_is_FontSize(StyleOptionName name);
static bool StyleName_is_ScriptLevel(StyleOptionName name);

template<typename T>
static bool StyledObject_get_FontSize(StyledObject &self, T *result);

template<> bool StyledObject_get_FontSize(StyledObject &self, Color *result) { return false; }
template<> bool StyledObject_get_FontSize(StyledObject &self, String *result) { return false; }

template<typename T>
static bool StyledObject_get_ScriptLevel(StyledObject &self, T *result);

template<> bool StyledObject_get_ScriptLevel(StyledObject &self, Color *result) { return false; }
template<> bool StyledObject_get_ScriptLevel(StyledObject &self, float *result) { return false; }
template<> bool StyledObject_get_ScriptLevel(StyledObject &self, String *result) { return false; }

template<class T>
static T *StyledObject_find_style_parent(StyledObject &self, bool selfincluding);

namespace richmath {
  class StyledObject::Impl {
      StyledObject &self;
    public:
      Impl(StyledObject &self) : self(self) {}
     
      template<typename N, typename T>
      bool try_get_own_style_with_stylesheet(
        N                      n, 
        T                     *result,
        SharedPtr<Stylesheet>  all,
        bool (*Stylesheet_get_)(SharedPtr<Stylesheet>, SharedPtr<Style>, N, T *) = Stylesheet_get<N, T>::impl
      );
    
      template<typename N, typename T>
      bool try_get_own_style(
        N      n, 
        T     *result,
        bool (*Stylesheet_get_)(SharedPtr<Stylesheet>, SharedPtr<Style>, N, T *) = Stylesheet_get<N, T>::impl
      ) {
        return try_get_own_style_with_stylesheet(n, result, self.stylesheet(), Stylesheet_get_);
      }

      template<typename N, typename T>
      T get_style(
        N      n,
        T      result,
        bool (*Stylesheet_get_)(SharedPtr<Stylesheet>, SharedPtr<Style>, N, T *) = Stylesheet_get<N, T>::impl);
      
  };
}

//{ class StyledObject ...

SharedPtr<Stylesheet> StyledObject::stylesheet() {
  if(StyledObject *parent = style_parent())
    return parent->stylesheet();
  
  return Stylesheet::Default;
}

bool StyledObject::enabled() {
  StyledObject *tmp = this;
  while(tmp) {
    switch(tmp->get_own_style(Enabled, AutoBoolAutomatic)) {
      case AutoBoolTrue:
        return true;
      
      case AutoBoolFalse:
        return false;
    }
    tmp = tmp->style_parent();
  }
  return true;
}

Color StyledObject::get_style(ColorStyleOptionName n, Color result) {
  return Impl(*this).get_style(n, result);
}

int StyledObject::get_style(IntStyleOptionName n, int result) {
  return Impl(*this).get_style(n, result);
}

float StyledObject::get_style(FloatStyleOptionName n, float result) {
  return Impl(*this).get_style(n, result);
}

String StyledObject::get_style(StringStyleOptionName n, String result) {
  return Impl(*this).get_style(n, result);
}

Expr StyledObject::get_style(ObjectStyleOptionName n, Expr result) {
  return Impl(*this).get_style(n, result);
}

String StyledObject::get_style(StringStyleOptionName n) {
  return get_style(n, String());
}

Expr StyledObject::get_style(ObjectStyleOptionName n) {
  return get_style(n, Expr());
}

Expr StyledObject::get_pmath_style(StyleOptionName n) {
  return Impl(*this).get_style(
           n,
           Symbol(richmath_System_Inherited),
           Stylesheet_get_pmath::impl);
}

Expr StyledObject::get_finished_flatlist_style(ObjectStyleOptionName n) {
  return Style::finish_style_merge(n, get_pmath_style(n));
}

Color StyledObject::get_own_style(ColorStyleOptionName n, Color fallback_result) {
  auto all = stylesheet();
  
  Color result;
  if(Impl(*this).try_get_own_style_with_stylesheet(n, &result, all))
    return result;
    
  if(all && all->base && all->get(all->base, n, &result))
    return result;
    
  return fallback_result;
}

int StyledObject::get_own_style(IntStyleOptionName n, int fallback_result) {
  auto all = stylesheet();
  
  int result;
  if(Impl(*this).try_get_own_style_with_stylesheet(n, &result, all))
    return result;
    
  if(all && all->base && all->get(all->base, n, &result))
    return result;
    
  return fallback_result;
}

float StyledObject::get_own_style(FloatStyleOptionName n, float fallback_result) {
  auto all = stylesheet();
  
  float result;
  if(Impl(*this).try_get_own_style_with_stylesheet(n, &result, all))
    return result;
    
  if(all && all->base && all->get(all->base, n, &result))
    return result;
    
  return fallback_result;
}

String StyledObject::get_own_style(StringStyleOptionName n, String fallback_result) {
  auto all = stylesheet();
  
  String result;
  if(Impl(*this).try_get_own_style_with_stylesheet(n, &result, all))
    return result;
    
  if(all && all->base && all->get(all->base, n, &result))
    return result;
    
  return fallback_result;
}

Expr StyledObject::get_own_style(ObjectStyleOptionName n, Expr fallback_result) {
  auto all = stylesheet();
  
  Expr result;
  if(Impl(*this).try_get_own_style_with_stylesheet(n, &result, all))
    return result;
    
  if(all && all->base && all->get(all->base, n, &result))
    return result;
    
  return fallback_result;
}

String StyledObject::get_own_style(StringStyleOptionName n) {
  return get_own_style(n, String());
}

Expr StyledObject::get_own_style(ObjectStyleOptionName n) {
  return get_own_style(n, Expr());
}

void StyledObject::reset_style() {
  if(auto style = own_style())
    style->clear();
}

//} ... class StyledObject

//{ class ActiveStyledObject ...

Expr ActiveStyledObject::update_cause() {
  if(!style)
    return Expr();
  
  return get_own_style(InternalUpdateCause);
}

void ActiveStyledObject::update_cause(Expr cause) {
  if(!style) {
    if(!cause)
      return;
    
    style = new Style;
  }
  
  style->set(InternalUpdateCause, std::move(cause));
}

//} ... class ActiveStyledObject

//{ class FrontEndSession ...

FrontEndSession::FrontEndSession(StyledObject *owner)
  : base{},
    _owner_or_limbo_next{owner}
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  style = new Style();
}

Expr FrontEndSession::allowed_options() {
  // {~ -> Inherited}
  return List(Rule(Call(Symbol(richmath_System_SingleMatch)), Symbol(richmath_System_Inherited)));
}

void FrontEndSession::dynamic_updated() {
}

void FrontEndSession::on_style_changed(bool layout_affected) {
  for(auto win : CommonDocumentWindow::All)
    win->content()->on_style_changed(layout_affected);
}

void FrontEndSession::next_in_limbo(ObjectWithLimbo *next) {
  RICHMATH_ASSERT( _owner_or_limbo_next.is_normal() );
  _owner_or_limbo_next.set_to_tinted(next);
}

//} ... class FrontEndSession

//{ class StyledObject::Impl ...

template<typename N, typename T>
bool StyledObject::Impl::try_get_own_style_with_stylesheet(
  N                      n, 
  T                     *result,
  SharedPtr<Stylesheet>  all,
  bool (*Stylesheet_get_)(SharedPtr<Stylesheet>, SharedPtr<Style>, N, T *)
) {
  if(Stylesheet_get_(all, self.own_style(), n, result))
    return true;
  
  N defn = self.get_default_key(n);
  if(defn != n) {
    if(Stylesheet_get_(all, self.own_style(), defn, result))
      return true;
    
    StyledObject *obj = self.style_parent();
    while(obj) {
      if(obj->changes_children_style()) {
        if(Stylesheet_get_(all, obj->own_style(), defn, result))
          return result;
      }
      
      obj = obj->style_parent();
    }
  }
  
  return false;
}

template<typename N, typename T>
T StyledObject::Impl::get_style(
  N      n,
  T      result,
  bool (*Stylesheet_get_)(SharedPtr<Stylesheet>, SharedPtr<Style>, N, T *)
) {
  SharedPtr<Stylesheet> all = self.stylesheet();  
  if(try_get_own_style_with_stylesheet(n, &result, all, Stylesheet_get_))
    return result;
    
  if(StyleName_is_FontSize(n)) {
    if(StyledObject_get_FontSize(self, &result))
      return result;
  }
  else if(StyleName_is_ScriptLevel(n)) {
    if(StyledObject_get_ScriptLevel(self, &result))
      return result;
  }
  
  StyledObject *obj = self.style_parent();
  while(obj) {
    if(obj->changes_children_style()) {
      if(Impl(*obj).try_get_own_style_with_stylesheet(n, &result, all, Stylesheet_get_))
        return result;
    }
    
    obj = obj->style_parent();
  }
  
  if(all && all->base && Stylesheet_get_(all, all->base, n, &result))
    return result;
  
  return result;
}

//} ... class StyledObject::Impl

template<typename N, typename T>
bool Stylesheet_get<N,T>::impl(SharedPtr<Stylesheet> all, SharedPtr<Style> style, N n, T *result){
  if(all)
    return all->get(style, n, result);
  else if(style)
    return style->get(n, result);
  else
    return false;
}

bool Stylesheet_get_pmath::impl(SharedPtr<Stylesheet> all, SharedPtr<Style> style, StyleOptionName n, Expr *result) {
  if(all) 
    *result = Style::merge_style_values(n, std::move(*result), all->get_pmath(style, n));
  else if(style)
    *result = Style::merge_style_values(n, std::move(*result), style->get_pmath(n));
  else
    return false;
  return !Style::contains_inherited(*result);
}

static bool StyleName_is_FontSize(StyleOptionName name) {
  return name == FloatStyleOptionName::FontSize;
}

static bool StyleName_is_ScriptLevel(StyleOptionName name) {
  return name == IntStyleOptionName::ScriptLevel;
}

template<typename T>
static bool StyledObject_get_FontSize(StyledObject &self, T *result) {
  if(auto seq = StyledObject_find_style_parent<AbstractSequence>(self, true)) {
    *result = seq->get_em();
    return true;
  }
  return false;
}

template<typename T>
static bool StyledObject_get_ScriptLevel(StyledObject &self, T *result) {
  if(auto box = StyledObject_find_style_parent<Box>(self, true)) {
    *result = box->child_script_level(-1, nullptr);
    return true;
  }
  return false;
}

template<class T>
static T *StyledObject_find_style_parent(StyledObject &self, bool selfincluding) {
  StyledObject *obj = &self;
  if(!selfincluding)
    obj = self.style_parent();
    
  while(obj) {
    if(T *result = dynamic_cast<T*>(obj))
      return result;
    
    obj = obj->style_parent();
  }
  
  return nullptr;
}
