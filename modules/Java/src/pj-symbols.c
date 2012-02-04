#include "pj-symbols.h"
#include "pj-threads.h"
#include "pj-objects.h"
#include "pjvm.h"

#include <string.h>


pmath_symbol_t _pj_symbols[PJ_SYMBOLS_COUNT];


extern pmath_t pj_builtin__pmath_Core_execute(pmath_t expr);


pmath_bool_t pj_symbols_init(void){
  size_t i;
  for(i = 0;i < PJ_SYMBOLS_COUNT;++i){
    _pj_symbols[i] = PMATH_NULL;
  }
    
  #define VERIFY(x)             do{ if(pmath_is_null(x)) goto FAIL; }while(0)
  #define NEW_SYMBOL(name)      pmath_symbol_get(PMATH_C_STRING((name)), TRUE)

  #define BIND(sym, func, use)  do{ if(!pmath_register_code((sym), (func), (use))) goto FAIL; }while(0)
  #define BIND_DOWN(sym, func)  BIND((sym), (func), PMATH_CODE_USAGE_DOWNCALL)
  #define BIND_UP(sym, func)  BIND((sym), (func), PMATH_CODE_USAGE_UPCALL)

  #define PROTECT(sym)   pmath_symbol_set_attributes((sym), pmath_symbol_get_attributes((sym)) | PMATH_SYMBOL_ATTRIBUTE_PROTECTED)

  VERIFY(PJ_SYMBOL_INTERNAL_STOPPEDCOTHREAD = NEW_SYMBOL("Java`Internal`StoppedCothread"));
  VERIFY(PJ_SYMBOL_INTERNAL_JAVACALL        = NEW_SYMBOL("Java`Internal`JavaCall"));
  VERIFY(PJ_SYMBOL_INTERNAL_JAVANEW         = NEW_SYMBOL("Java`Internal`JavaNew"));
  VERIFY(PJ_SYMBOL_INTERNAL_RETURN          = NEW_SYMBOL("Java`Internal`Return"));
  VERIFY(PJ_SYMBOL_INTERNAL_CALLFROMJAVA    = NEW_SYMBOL("Java`Internal`CallFromJava"));
  VERIFY(PJ_SYMBOL_INTERNAL_SUCCEEDED       = NEW_SYMBOL("Java`Internal`Succeeded"));
  VERIFY(PJ_SYMBOL_INTERNAL_FAILED          = NEW_SYMBOL("Java`Internal`Failed"));
  
  VERIFY(PJ_SYMBOL_ISJAVAOBJECT        = NEW_SYMBOL("Java`IsJavaObject"));
  VERIFY(PJ_SYMBOL_CLASSNAME           = NEW_SYMBOL("Java`ClassName"));
  VERIFY(PJ_SYMBOL_DEFAULTCLASSPATH    = NEW_SYMBOL("Java`$DefaultClassPath"));
  VERIFY(PJ_SYMBOL_GETCLASS            = NEW_SYMBOL("Java`GetClass"));
  VERIFY(PJ_SYMBOL_INSTANCEOF          = NEW_SYMBOL("Java`InstanceOf"));
  VERIFY(PJ_SYMBOL_PARENTCLASS         = NEW_SYMBOL("Java`ParentClass"));
    
  VERIFY(PJ_SYMBOL_JAVA                = NEW_SYMBOL("Java`Java"));
  VERIFY(PJ_SYMBOL_JAVACALL            = NEW_SYMBOL("Java`JavaCall"));
  VERIFY(PJ_SYMBOL_JAVACLASS           = NEW_SYMBOL("Java`JavaClass"));
  VERIFY(PJ_SYMBOL_JAVACLASSASOBJECT   = NEW_SYMBOL("Java`JavaClassAsObject"));
  VERIFY(PJ_SYMBOL_JAVAEXCEPTION       = NEW_SYMBOL("Java`JavaException"));
  VERIFY(PJ_SYMBOL_JAVAFIELD           = NEW_SYMBOL("Java`JavaField"));
  VERIFY(PJ_SYMBOL_JAVANEW             = NEW_SYMBOL("Java`JavaNew"));
  VERIFY(PJ_SYMBOL_JAVAOBJECT          = NEW_SYMBOL("Java`JavaObject"));
  VERIFY(PJ_SYMBOL_JAVASTARTVM         = NEW_SYMBOL("Java`JavaStartVM"));
  VERIFY(PJ_SYMBOL_JAVAVMLIBRARYNAME   = NEW_SYMBOL("Java`$JavaVMLibraryName"));
    
  VERIFY(PJ_SYMBOL_TYPE_ARRAY          = NEW_SYMBOL("Java`Type`Array"));
  VERIFY(PJ_SYMBOL_TYPE_BOOLEAN        = NEW_SYMBOL("Java`Type`Boolean"));
  VERIFY(PJ_SYMBOL_TYPE_BYTE           = NEW_SYMBOL("Java`Type`Byte"));
  VERIFY(PJ_SYMBOL_TYPE_CHAR           = NEW_SYMBOL("Java`Type`Char"));
  VERIFY(PJ_SYMBOL_TYPE_DOUBLE         = NEW_SYMBOL("Java`Type`Double"));
  VERIFY(PJ_SYMBOL_TYPE_FLOAT          = NEW_SYMBOL("Java`Type`Float"));
  VERIFY(PJ_SYMBOL_TYPE_INT            = NEW_SYMBOL("Java`Type`Int"));
  VERIFY(PJ_SYMBOL_TYPE_LONG           = NEW_SYMBOL("Java`Type`Long"));
  VERIFY(PJ_SYMBOL_TYPE_SHORT          = NEW_SYMBOL("Java`Type`Short"));
  
  BIND_UP(PJ_SYMBOL_JAVAFIELD, pj_builtin_assign_javafield);
  
  BIND_DOWN(PJ_SYMBOL_ISJAVAOBJECT,      pj_builtin_isjavaobject);
  BIND_DOWN(PJ_SYMBOL_PARENTCLASS,       pj_builtin_parentclass);
  
  BIND_DOWN(PJ_SYMBOL_JAVACALL,          pj_builtin_javacall);
  BIND_DOWN(PJ_SYMBOL_JAVACLASSASOBJECT, pj_builtin_javaclassasobject);
  BIND_DOWN(PJ_SYMBOL_JAVAFIELD,         pj_builtin_javafield);
  BIND_DOWN(PJ_SYMBOL_INSTANCEOF,        pj_builtin_instanceof);
  BIND_DOWN(PJ_SYMBOL_JAVANEW,           pj_builtin_javanew);
  BIND_DOWN(PJ_SYMBOL_JAVASTARTVM,       pj_builtin_startvm);
  
  BIND_DOWN(PJ_SYMBOL_INTERNAL_CALLFROMJAVA,    pj_builtin__pmath_Core_execute);
  BIND_DOWN(PJ_SYMBOL_INTERNAL_JAVACALL,        pj_builtin_internal_javacall);
  BIND_DOWN(PJ_SYMBOL_INTERNAL_JAVANEW,         pj_builtin_internal_javanew);
  BIND_DOWN(PJ_SYMBOL_INTERNAL_RETURN,          pj_builtin_internal_return);
  BIND_DOWN(PJ_SYMBOL_INTERNAL_STOPPEDCOTHREAD, pj_builtin_internal_stoppedcothread);
  
  pmath_symbol_set_attributes(
    PJ_SYMBOL_INTERNAL_CALLFROMJAVA, 
    PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE);
    
  pmath_symbol_set_attributes(
    PJ_SYMBOL_INTERNAL_RETURN, 
    PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE);
    
  pmath_symbol_set_attributes(
    PJ_SYMBOL_INTERNAL_STOPPEDCOTHREAD, 
    PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE);
    
  for(i = 0;i < PJ_SYMBOLS_COUNT;++i){
    if(pmath_is_null(_pj_symbols[i]))
      pmath_debug_print("Symbol %d not defined.\n", (int)i);
      
    PROTECT(_pj_symbols[i]);
  }
  
  
  return TRUE;
  
 FAIL:
  {
    size_t i;
    for(i = 0;i < PJ_SYMBOLS_COUNT;++i){
      pmath_unref(_pj_symbols[i]);
      _pj_symbols[i] = PMATH_NULL;
    }
  }
  return FALSE;

#undef VERIFY
#undef NEW_SYMBOL
#undef BIND
#undef BIND_DOWN
#undef PROTECT
}

void pj_symbols_done(void){
  size_t i;
  for(i = 0;i < PJ_SYMBOLS_COUNT;++i){
    pmath_unref(_pj_symbols[i]);
    _pj_symbols[i] = PMATH_NULL;
  }
}
