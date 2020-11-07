#include <eval/current-value.h>

#include <boxes/section.h>
#include <boxes/sectionlist.h>
#include <boxes/templatebox.h>
#include <eval/application.h>
#include <eval/dynamic.h>
#include <gui/document.h>
#include <gui/native-widget.h>


using namespace richmath;


namespace richmath {
  namespace strings {
    extern String ControlsFontFamily;
    extern String ControlsFontSize;
    extern String ControlsFontSlant;
    extern String ControlsFontWeight;
    extern String CurrentValueProviders;
    extern String DocumentScreenDpi;
    extern String MouseOver;
    extern String MouseOverBox;
    extern String SectionGroupOpen;
    extern String SelectedMenuCommand;
    extern String StyleDefinitionsOwner;
    extern String TemplateSlotCount;
  }
  
  class CurrentValueImpl {
    public:
      static Hashtable<Expr, Expr (*)(FrontEndObject*, Expr)>       providers;
      static Hashtable<Expr, bool (*)(FrontEndObject*, Expr, Expr)> setters;
      
      static FrontEndObject *object(Expr obj);
      
      static Expr get_CurrentValueProviders(FrontEndObject *obj, Expr item);
      static Expr get_MouseOver(FrontEndObject *obj, Expr item);
      static Expr get_DocumentScreenDpi(FrontEndObject *obj, Expr item);
      static Expr get_ControlFont_data(FrontEndObject *obj, Expr item);
      static Expr get_SectionGroupOpen(FrontEndObject *obj, Expr item);
      static bool put_SectionGroupOpen(FrontEndObject *obj, Expr item, Expr rhs);
      static Expr get_Selectable(FrontEndObject *obj, Expr item);
      static Expr get_SelectedMenuCommand(FrontEndObject *obj, Expr item);
      static Expr get_StyleDefinitionsOwner(FrontEndObject *obj, Expr item);
      static Expr get_WindowTitle(FrontEndObject *obj, Expr item);
  };
}

extern pmath_symbol_t richmath_System_Selectable;
extern pmath_symbol_t richmath_System_TemplateBox;
extern pmath_symbol_t richmath_System_TemplateSlot;
extern pmath_symbol_t richmath_System_WindowTitle;

Hashtable<Expr, Expr (*)(FrontEndObject*, Expr)>       CurrentValueImpl::providers;
Hashtable<Expr, bool (*)(FrontEndObject*, Expr, Expr)> CurrentValueImpl::setters;

Expr richmath_eval_FrontEnd_AssignCurrentValue(Expr expr);
Expr richmath_eval_FrontEnd_CurrentValue(Expr expr);


//{ class CurrentValue ...

void CurrentValue::init() {
  
  register_provider(strings::MouseOver,                   Impl::get_MouseOver);
  register_provider(strings::MouseOverBox,                Document::get_current_value_of_MouseOverBox);
  register_provider(strings::DocumentScreenDpi,           Impl::get_DocumentScreenDpi);
  register_provider(strings::ControlsFontFamily,          Impl::get_ControlFont_data);
  register_provider(strings::ControlsFontSlant,           Impl::get_ControlFont_data);
  register_provider(strings::ControlsFontWeight,          Impl::get_ControlFont_data);
  register_provider(strings::ControlsFontSize,            Impl::get_ControlFont_data);
  register_provider(strings::CurrentValueProviders,       Impl::get_CurrentValueProviders);
  register_provider(strings::SectionGroupOpen,            Impl::get_SectionGroupOpen,
                                                          Impl::put_SectionGroupOpen);
  register_provider(Symbol(richmath_System_Selectable),   Impl::get_Selectable,
                                                          Style::put_current_style_value);
  register_provider(strings::SelectedMenuCommand,         Impl::get_SelectedMenuCommand);
  register_provider(strings::StyleDefinitionsOwner,       Impl::get_StyleDefinitionsOwner);
  register_provider(Symbol(richmath_System_TemplateBox),  TemplateBox::get_current_value_of_TemplateBox);
  register_provider(strings::TemplateSlotCount,           TemplateBoxSlot::get_current_value_of_TemplateSlotCount);
  register_provider(Symbol(richmath_System_TemplateSlot), TemplateBoxSlot::get_current_value_of_TemplateSlot,
                                                          TemplateBoxSlot::put_current_value_of_TemplateSlot);
  register_provider(Symbol(richmath_System_WindowTitle),  Impl::get_WindowTitle,
                                                          Style::put_current_style_value);
  
  
}

void CurrentValue::done() {
  Impl::providers.clear();
  Impl::setters.clear();
}

Expr CurrentValue::get(Expr item) {
  return get(Application::get_evaluation_box(), std::move(item));
}

Expr CurrentValue::get(FrontEndObject *obj, Expr item) {
  auto func = Impl::providers[item];
  if(!func && item[0] == PMATH_SYMBOL_LIST)
    func = Impl::providers[item[1]];
    
  if(!func)
    return Symbol(PMATH_SYMBOL_FAILED);
    
  return func(obj, std::move(item));
}

bool CurrentValue::put(FrontEndObject *obj, Expr item, Expr rhs) {
  auto func = Impl::setters[item];
  if(!func && item[0] == PMATH_SYMBOL_LIST)
    func = Impl::setters[item[1]];
    
  if(!func)
    return false;
    
  return func(obj, std::move(item), std::move(rhs));
}

bool CurrentValue::register_provider(
  Expr   item,
  Expr (*get)(FrontEndObject *obj, Expr item),
  bool (*put)(FrontEndObject *obj, Expr item, Expr rhs))
{
  assert(get != nullptr);
  
  if(Impl::providers.search(item))
    return false;
  
  if(Impl::setters.search(item))
    return false;
  
  Impl::providers.set(item, get);
  
  if(put)
    Impl::setters.set(item, put);
  
  return true;
}

//} ... class CurrentValue

//{ class CurrentValueImpl ...

FrontEndObject *CurrentValueImpl::object(Expr obj) {
  if(obj == PMATH_SYMBOL_AUTOMATIC)
    return Application::get_evaluation_box();
  
  return FrontEndObject::find(FrontEndReference::from_pmath(std::move(obj)));
}

Expr CurrentValueImpl::get_CurrentValueProviders(FrontEndObject *obj, Expr item) {
  Gather g;
  for(auto &&key : providers.keys())
    Gather::emit(std::move(key));
  
  return g.end();
}

Expr CurrentValueImpl::get_MouseOver(FrontEndObject *obj, Expr item) {
  Box *box = dynamic_cast<Box*>(obj);
  if(!box)
    return Symbol(PMATH_SYMBOL_FALSE);
    
  Document *doc = box->find_parent<Document>(true);
  if(!doc)
    return Symbol(PMATH_SYMBOL_FALSE);
    
  if(auto observer_id = Dynamic::current_observer_id) {
    // ensure that get/set of InternalUsesCurrentValueOfMouseOver below will not cause reevaluation
    Dynamic::current_observer_id = FrontEndReference::None;
    if(!box->style)
      box->style = new Style();
      
    int observer_kind = ObserverKindNone;
    box->style->get(InternalUsesCurrentValueOfMouseOver, &observer_kind);
    if(box->id() == observer_id) 
      observer_kind |= ObserverKindSelf;
    else 
      observer_kind |= ObserverKindOther;
    
    box->style->set(InternalUsesCurrentValueOfMouseOver, observer_kind);
    
    if(box->id() != observer_id)
      box->style->register_observer(observer_id);
    
    Dynamic::current_observer_id = observer_id;
  }
  
  Box *mo = FrontEndObject::find_cast<Box>(doc->mouseover_box_id());
  while(mo && mo != box)
    mo = mo->parent();
    
  if(mo)
    return Symbol(PMATH_SYMBOL_TRUE);
  return Symbol(PMATH_SYMBOL_FALSE);
}

Expr CurrentValueImpl::get_DocumentScreenDpi(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  return Expr(doc->native()->dpi());
}

Expr CurrentValueImpl::get_ControlFont_data(FrontEndObject *obj, Expr item) {
  SharedPtr<Style> style = new Style();
  ControlPainter::std->system_font_style(ControlContext::find(dynamic_cast<Box*>(obj)), style.ptr());
  
  AutoResetCurrentObserver guard;
  if(item == strings::ControlsFontFamily)
    return style->get_pmath(FontFamilies);
  if(item == strings::ControlsFontSlant)
    return style->get_pmath(FontSlant);
  if(item == strings::ControlsFontWeight)
    return style->get_pmath(FontWeight);
  if(item == strings::ControlsFontSize)
    return style->get_pmath(FontSize);
    
  return Symbol(PMATH_SYMBOL_FAILED);
}

Expr CurrentValueImpl::get_SectionGroupOpen(FrontEndObject *obj, Expr item) {
  Box *box = dynamic_cast<Box*>(obj);
  Section *sec = box ? box->find_parent<Section>(true) : nullptr;
  if(!sec)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  SectionList *slist = dynamic_cast<SectionList*>(sec->parent());
  if(!slist)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  int close_rel = slist->group_info(sec->index()).close_rel;
  if(close_rel < 0)
    return Symbol(PMATH_SYMBOL_TRUE);
  else
    return Symbol(PMATH_SYMBOL_FALSE);
}

bool CurrentValueImpl::put_SectionGroupOpen(FrontEndObject *obj, Expr item, Expr rhs) {
  Box *box = dynamic_cast<Box*>(obj);
  Section *sec = box ? box->find_parent<Section>(true) : nullptr;
  if(!sec)
    return false;
    
  SectionList *slist = dynamic_cast<SectionList*>(sec->parent());
  if(!slist)
    return false;
  
  if(rhs == PMATH_SYMBOL_TRUE) {
    slist->set_open_close_group(sec->index(), true);
    return true;
  }
  
  if(rhs == PMATH_SYMBOL_FALSE) {
    slist->set_open_close_group(sec->index(), false);
    return true;
  }
  
  return false;
}

Expr CurrentValueImpl::get_Selectable(FrontEndObject *obj, Expr item) {
  if(Box *box = dynamic_cast<Box*>(obj)) 
    return box->selectable() ? Symbol(PMATH_SYMBOL_TRUE) : Symbol(PMATH_SYMBOL_FALSE);
  
  return Style::get_current_style_value(obj, std::move(item));
}

Expr CurrentValueImpl::get_SelectedMenuCommand(FrontEndObject *obj, Expr item) {
  Expr cmd = Menus::selected_item_command();
  if(cmd.is_null())
    return Symbol(PMATH_SYMBOL_NONE);
  
  return Call(Symbol(PMATH_SYMBOL_HOLD), std::move(cmd));
}

Expr CurrentValueImpl::get_StyleDefinitionsOwner(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  Document *owner = doc->native()->owner_document();
  while(!owner) {
    doc = doc->native()->working_area_document();
    if(!doc)
      return Symbol(PMATH_SYMBOL_NONE);
    
    owner = doc->native()->owner_document();
  }
  return owner->to_pmath_id();
}

Expr CurrentValueImpl::get_WindowTitle(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  
  if(doc) {
    if(item == richmath_System_WindowTitle) {
      auto result = doc->native()->window_title();
      if(!result.is_null())
        return result;
    }
  }
  
  return Style::get_current_style_value(obj, std::move(item));
}

//} ... class CurrentValueImpl

Expr richmath_eval_FrontEnd_AssignCurrentValue(Expr expr) {
  /*  FrontEnd`AssignCurrentValue(obj,       item, HoldComplete(rhs))
      FrontEnd`AssignCurrentValue(Automatic, item, HoldComplete(rhs))
   */
  
  if(expr.expr_length() != 3)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  Expr rhs = expr[3];
  if(rhs[0] != PMATH_SYMBOL_HOLDCOMPLETE || rhs.expr_length() != 1)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  rhs = rhs[1];
  if(CurrentValue::put(CurrentValueImpl::object(expr[1]), expr[2], rhs))
    return rhs;
  
  return Symbol(PMATH_SYMBOL_FAILED);
}

Expr richmath_eval_FrontEnd_CurrentValue(Expr expr) {
  /*  FrontEnd`CurrentValue(obj, item)
      FrontEnd`CurrentValue(Automatic, item)
   */
  if(expr.expr_length() != 2)
    return Symbol(PMATH_SYMBOL_FAILED);
  
  return CurrentValue::get(CurrentValueImpl::object(expr[1]), expr[2]);
}
