#include <pmath-core/expressions.h>
#include <pmath-core/symbols.h>

#include <pmath-util/concurrency/threadlocks.h>
#include <pmath-util/line-writer.h>

#include <pmath-builtins/all-symbols-private.h>

#include <stdio.h>
#include <string.h>


extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Row;
extern pmath_symbol_t pmath_System_SectionPrint;
extern pmath_symbol_t pmath_System_Unevaluated;

static void write_to_file(void *file, const char *cstr) {
  fwrite(cstr, 1, strlen(cstr), (FILE*)file);
}

static pmath_threadlock_t print_lock = NULL;

static void sectionprint_locked_callback(void *p) {
  pmath_expr_t expr = *(pmath_expr_t*)p;
  pmath_cstr_writer_info_t info;
  size_t i;
  
  info.user = stdout;
  info._pmath_write_cstr = write_to_file;
  
  for(i = 2; i <= pmath_expr_length(expr); i++) {
    pmath_t item = pmath_expr_get_item(expr, i);
    pmath_write_with_pagewidth(item, 0, pmath_native_writer, &info, -1, 0);
    pmath_unref(item);
  }
  printf("\n\n");
  fflush(stdout);
}

PMATH_PRIVATE pmath_t builtin_sectionprint(pmath_expr_t expr) {
  pmath_thread_call_locked(
    &print_lock,
    sectionprint_locked_callback,
    &expr);
    
  pmath_unref(expr);
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_print(pmath_expr_t expr) {
  if(pmath_expr_length(expr) == 1) {
    pmath_t item = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    expr = item;
  }
  else {
    expr = pmath_expr_set_item(expr, 0, pmath_ref(pmath_System_List));
    
    expr = pmath_expr_new_extended(
            pmath_ref(pmath_System_Row), 1,
            expr);
  }       
  return pmath_expr_new_extended(
           pmath_ref(pmath_System_SectionPrint), 2,
           PMATH_C_STRING("Print"),
           pmath_expr_new_extended(pmath_ref(pmath_System_Unevaluated), 1, expr));
}
