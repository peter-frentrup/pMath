#include <eval/eval-contexts.h>
#include <eval/current-value.h>

#include <boxes/section.h>
#include <boxes/sectionlist.h>

#include <gui/document.h>


namespace richmath { namespace strings {
  extern String DollarContext_namespace;
  extern String Global_namespace;
  extern String System_namespace;
}}

extern pmath_symbol_t richmath_System_Document;
extern pmath_symbol_t richmath_System_EvaluationSequence;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_SectionGroup;
extern pmath_symbol_t richmath_FE_Private_DeleteEvaluationContext;
extern pmath_symbol_t richmath_FE_Private_EvaluationContextBlock;
extern pmath_symbol_t richmath_FE_Private_SwitchEvaluationContext;

using namespace richmath;

namespace richmath {
  class EvaluationContexts::Impl {
    public:
      template<class T> 
      static T *find_parent_or_self(StyledObject *obj) {
        while(obj) {
          if(T *result = dynamic_cast<T*>(obj))
            return result;
          
          obj = obj->style_parent();
        }
        
        return nullptr;
      }
      
      static String generate_context_name(StyledObject *obj);
      static String generate_context_name_and_remember(StyledObject *obj) { remember_that_object_defines_context(obj); return generate_context_name(obj); }
    
    private:
      static void remember_that_object_defines_context(StyledObject *obj);
      
    public:
      static String current_context;
  };
}

namespace {
  class SymbolNamespaceReplacer {
    public:
      SymbolNamespaceReplacer(String old_ns, String new_ns);
      
      pmath_t run(pmath_t expr);
      
      pmath_symbol_attributes_t extra_attributes = 0;
      bool copy_old_attributes = false;
      
    private:
      const uint16_t *old_ns_buf;
      int             old_ns_len;
      String old_ns;
      String new_ns;
  };
}

String EvaluationContexts::Impl::current_context;

//{ class EvaluationContexts ...

void EvaluationContexts::init() {
  Impl::current_context = strings::Global_namespace;
  
  CurrentValue::register_provider(
    String("ResolvedEvaluationContext"), 
    [](FrontEndObject *obj, Expr item) -> Expr { return resolve_context(dynamic_cast<StyledObject*>(obj)); });
}

void EvaluationContexts::done() {
  Impl::current_context = String();
}

void EvaluationContexts::context_source_deleted(StyledObject *obj) {
  String ctx = Impl::generate_context_name(obj);
  
  if(ctx == strings::Global_namespace)
    return;
  
  if(ctx == Impl::current_context) 
    set_context(strings::Global_namespace);
  
  Application::async_interrupt(Call(Symbol(richmath_FE_Private_DeleteEvaluationContext), std::move(ctx)));
}

void EvaluationContexts::set_context(String context) {
  if(auto set_ctx = prepare_set_context(context))
    Application::interrupt_wait(std::move(set_ctx));
}

Expr EvaluationContexts::prepare_set_context(String context) {
  if(!context.is_namespace())
    return Expr();
  
  if(context == Impl::current_context)
    return Expr();
  
  auto old_ctx = std::move(Impl::current_context);
  Impl::current_context = context;
  pmath_debug_print_object("[prepare_set_context ", old_ctx.get(), "");
  pmath_debug_print_object(" -> ", context.get(), "]\n");
  return Call(Symbol(richmath_FE_Private_SwitchEvaluationContext), std::move(old_ctx), std::move(context));
}

Expr EvaluationContexts::make_context_block(Expr expr, String context) {
  return Call(Symbol(richmath_FE_Private_EvaluationContextBlock), std::move(expr), std::move(context));
}

Section *EvaluationContexts::find_section_group_header(Section *section) {
  if(!section)
    return nullptr;
  
  SectionList *slist = dynamic_cast<SectionList*>(section->parent());
  if(!slist)
    return nullptr;
  
  int i = section->group_info().first; // index of owner of section.
  
  if(i < 0)
    return nullptr;
  
  if(section->get_own_style(SectionGenerated, false)) {
    // i was the input, go one step further.
    i = slist->group_info(i).first;
  }
  
  if(i < 0)
    return nullptr;
  
  return slist->section(i);
}

String EvaluationContexts::resolve_context(StyledObject *obj) {
  if(!obj)
    return strings::Global_namespace;
  
  Expr ctx = obj->get_style(EvaluationContext);
  if(ctx.is_namespace())
    return String(ctx);
  
  if(ctx == richmath_System_Document) {
    Document *doc = Impl::find_parent_or_self<Document>(obj);
    return Impl::generate_context_name_and_remember(doc);
  }
  
  Section *sect = Impl::find_parent_or_self<Section>(obj);
  if(ctx == richmath_System_Section) {
    return Impl::generate_context_name_and_remember(sect);
  }
  
  if(ctx == richmath_System_SectionGroup) {
    sect = find_section_group_header(sect);
    return Impl::generate_context_name_and_remember(sect);
  }
  
  return strings::Global_namespace;
}

Expr EvaluationContexts::prepare_namespace_for_current_context(Expr expr) {
  return replace_symbol_namespace(std::move(expr), strings::DollarContext_namespace, Impl::current_context);
}

Expr EvaluationContexts::prepare_namespace_for(Expr expr, StyledObject *obj) {
  return replace_symbol_namespace(std::move(expr), strings::DollarContext_namespace, resolve_context(obj));
}

String EvaluationContexts::current() {
  return Impl::current_context;
}

Expr EvaluationContexts::replace_symbol_namespace(Expr expr, String old_ns, String new_ns) {
  if(old_ns == new_ns)
    return expr;
  
  if(old_ns == strings::System_namespace)
    return expr;

  if(old_ns.is_namespace() && new_ns.is_namespace()) {
    SymbolNamespaceReplacer repl(old_ns, new_ns);
    
    if(new_ns == strings::DollarContext_namespace) {
      repl.extra_attributes = PMATH_SYMBOL_ATTRIBUTE_TEMPORARY;
      //repl.copy_old_attributes = true;
    }
    
    return Expr(repl.run(expr.release()));
  }
  return expr;
}

//} ... class EvaluationContexts

//{ class EvaluationContexts::Impl ...

void EvaluationContexts::Impl::remember_that_object_defines_context(StyledObject *obj) {
  if(!obj)
    return;
  
  auto style = obj->own_style();
  if(!style) // TODO: create if necessary 
    return;
  
  style->set(InternalDefinesEvaluationContext, true);
}

String EvaluationContexts::Impl::generate_context_name(StyledObject *obj) {
  if(!obj)
    return strings::Global_namespace;
  
  String prefix;
  if(dynamic_cast<Document*>(obj))
    prefix = "Document$$";
  else if(dynamic_cast<Section*>(obj))
    prefix = "Section$$";
  else
    prefix = "Object$$";
  
  prefix+= obj->id().to_pmath_raw().to_string();
  
  return prefix + "`";
}

//} ... class EvaluationContexts::Impl

//{ class SymbolNamespaceReplacer ...

SymbolNamespaceReplacer::SymbolNamespaceReplacer(String old_ns, String new_ns)
 : old_ns{old_ns},
   old_ns_buf{old_ns.buffer()},
   old_ns_len{old_ns.length()},
   new_ns{new_ns}
{
}

pmath_t SymbolNamespaceReplacer::run(pmath_t expr) {
  if(pmath_is_symbol(expr)) {
    pmath_string_t name = pmath_symbol_name(expr);
    
    int len = pmath_string_length(name);
    if(old_ns_len < len) {
      const uint16_t *name_buf = pmath_string_buffer(&name);
      if(memcmp(name_buf, old_ns_buf, (size_t)old_ns_len * sizeof(uint16_t)) == 0) {
        name = pmath_string_part(name, old_ns_len, -1);
        name = pmath_string_concat(pmath_ref(new_ns.get()), name);
        
        pmath_symbol_t newsym = pmath_symbol_find(name, TRUE);
        name = PMATH_NULL;
        
        if(copy_old_attributes) {
          pmath_symbol_set_attributes(newsym, pmath_symbol_get_attributes(expr) | extra_attributes);
        }
        
        if(extra_attributes) {
          pmath_symbol_set_attributes(newsym, pmath_symbol_get_attributes(newsym) | extra_attributes);
        }
        
        pmath_unref(expr);
        expr = newsym;
      }
    }
    
    pmath_unref(name);
    return expr;
  }
  
  if(pmath_is_expr(expr)) {
    if(pmath_is_packed_array(expr))
      return expr;
    
    pmath_t debug_info = pmath_get_debug_info(expr);
    size_t len = pmath_expr_length(expr);
    for(size_t i = 0; i <= len; ++i) {
      pmath_t item = pmath_expr_extract_item(expr, i);
      item = run(item);
      expr = pmath_expr_set_item(expr, i, item);
    }
    
    expr = pmath_try_set_debug_info(expr, debug_info);
    return expr;
  }
  
  return expr;
}

//} ... class SymbolNamespaceReplacer
