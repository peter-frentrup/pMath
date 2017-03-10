#include <pmath-builtins/all-symbols-private.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/debug.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <string.h>


PMATH_PRIVATE pmath_symbol_t _pmath_builtin_symbol_array[PMATH_BUILTIN_SYMBOL_COUNT];

PMATH_API pmath_symbol_t pmath_symbol_builtin(int index) {
  if(index < 0 || (size_t)index >= PMATH_BUILTIN_SYMBOL_COUNT)
    return PMATH_NULL;
    
  return _pmath_builtin_symbol_array[index];
}

/*============================================================================*/
//{ up/down/sub code hashtables ...

typedef struct {
  pmath_symbol_t   key;
  void           (*function)();
} func_entry_t;

#define CODE_TABLES_COUNT  5

static pmath_atomic_t _code_tables[CODE_TABLES_COUNT]; // index: pmath_code_usage_t

#define LOCK_CODE_TABLE(USAGE)           (pmath_hashtable_t)_pmath_atomic_lock_ptr(&_code_tables[(USAGE)])
#define UNLOCK_CODE_TABLE(USAGE, TABLE)  _pmath_atomic_unlock_ptr(&_code_tables[(USAGE)], (TABLE))

static void destroy_func_entry(void *e) {
  func_entry_t *entry = (func_entry_t*)e;
  pmath_unref(entry->key);
  pmath_mem_free(entry);
}

static unsigned int hash_func_entry(void *e) {
  func_entry_t *entry = (func_entry_t*)e;
  return _pmath_hash_pointer(PMATH_AS_PTR(entry->key));
}

static pmath_bool_t func_entry_keys_equal(
  void *e1,
  void *e2
) {
  func_entry_t *entry1 = (func_entry_t*)e1;
  func_entry_t *entry2 = (func_entry_t*)e2;
  return pmath_same(entry1->key, entry2->key);
}

static unsigned int hash_func_key(void *k) {
  pmath_symbol_t key = *(pmath_t*)k;
  return pmath_hash(key);
}

static pmath_bool_t func_entry_equals_key(
  void *e,
  void *k
) {
  func_entry_t   *entry = (func_entry_t*)e;
  pmath_symbol_t  key   = *(pmath_t*)k;
  return pmath_same(entry->key, key);
}

static const pmath_ht_class_t function_table_class = {
  destroy_func_entry,
  hash_func_entry,
  func_entry_keys_equal,
  hash_func_key,
  func_entry_equals_key
};

//} ============================================================================

//{ builtins from src/pmath-builtins/arithmetic/ ...
PMATH_PRIVATE pmath_t builtin_abs(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_approximate(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_arcsin(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_arctan(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_arg(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_binomial(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_chop(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_complex(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_conjugate(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_cos(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_cosh(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_directedinfinity(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_exp(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_factorial(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_factorial2(      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_fractionalpart(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_gamma(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_im(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_log(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_loggamma(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_mod(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_plus(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_polygamma(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_power(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_powermod(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_precision(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_product(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_quotient(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_re(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_rescale(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_round_functions( pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_setprecision(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sign(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sin(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sinh(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sqrt(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sum(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_tan(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_tanh(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_times(           pmath_expr_t expr);

PMATH_PRIVATE pmath_bool_t builtin_approximate_e(               pmath_t *obj, double prec);
PMATH_PRIVATE pmath_bool_t builtin_approximate_eulergamma(      pmath_t *obj, double prec);
PMATH_PRIVATE pmath_bool_t builtin_approximate_machineprecision(pmath_t *obj, double prec);
PMATH_PRIVATE pmath_bool_t builtin_approximate_pi(              pmath_t *obj, double prec);
PMATH_PRIVATE pmath_bool_t builtin_approximate_power(           pmath_t *obj, double prec);

PMATH_PRIVATE pmath_t builtin_assign_maxextraprecision(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_assign_setprecision(     pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_internal_copysign(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_nexttoward(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_realballbounds(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_signbit(pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/control/ ...
PMATH_PRIVATE pmath_t builtin_developer_hasbuiltincode(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_isheld(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_call_isheld(     pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_bytecount(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_count(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_filterrules(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_hash(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_history(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_loadlibrary(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_match(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_memoryusage(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_position(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_releasehold(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_replace(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_replacelist(     pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/control/definitions/ ...
PMATH_PRIVATE pmath_t builtin_assign(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_tagassign(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_unassign(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_tagunassign(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_dec_or_inc_or_postdec_or_postinc(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_divideby_or_timesby(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_clear(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_update(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_list(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_ownrules(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_assign_symbol_rules(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_ownrules(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_symbol_rules(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_attributes(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_attributes(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_clearattributes(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_setattributes(    pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_protect_or_unprotect(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_default(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_default(       pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_options(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isoption(      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_options(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_optionvalue(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_setoptions(    pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_showdefinition(pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/control/flow/ ...
PMATH_PRIVATE pmath_t builtin_internal_abortmessage(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_throw(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_catch(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_finally(              pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_evaluate(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_evaluatedelayed(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_evaluationsequence(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_function(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_call_function(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_block(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_local(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_with( pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_if(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_do(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_for(      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_piecewise(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_switch(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_while(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_which(    pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_begin(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_beginpackage(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_end(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_endpackage(  pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_stack(      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_synchronize(pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/control/messages/ ...
PMATH_PRIVATE pmath_t builtin_assign_messagename(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_messagename(       pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_messages(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_messages(       pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_criticalmessagetag(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isvalidargumentcount(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_message(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_messagecount(        pmath_expr_t expr); // in message.c
PMATH_PRIVATE pmath_t builtin_on_or_off(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_try(                 pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/datetime/ ...
PMATH_PRIVATE pmath_t builtin_datelist(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_pause(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_timeconstrained( pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_timing(          pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/formating/ ...
PMATH_PRIVATE pmath_t builtin_baseform(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_makeboxes(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_parenthesizeboxes(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_toboxes(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_tostring(         pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_makeboxes_or_format(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_assign_syntaxinformation(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_syntaxinformation(         pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/gui/ ...
PMATH_PRIVATE pmath_t builtin_button( pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_clock(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_refresh(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_internal_dynamicevaluate(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_dynamicevaluatemultiple(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_dynamicremove(          pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/io/ ...
PMATH_PRIVATE pmath_t builtin_developer_fileinformation(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_environment(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_binaryread(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_binaryreadlist(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_binarywrite(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_characters(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_close(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_compress(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_compressstream(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_copydirectory_and_copyfile(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_createdirectory(               pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_deletedirectory_and_deletefile(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_directory(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_directoryname(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_environment(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_filebytecount(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_filenames(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_filetype(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_find(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_findlist(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_fromcharactercode(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_get(                           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_open(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_parentdirectory(               pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_print(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_read(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_readlist(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_renamedirectory_and_renamefile(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_resetdirectory(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sectionprint(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_setdirectory(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_setstreamposition(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_streamposition(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringcases(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringcount(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringdrop(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringexpression(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringmatch(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringposition(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringreplace(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringsplit(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringtake(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_tocharactercode(               pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_tofilename(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_uncompress(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_uncompressstream(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_write(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_writestring(                   pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/language/ ...
PMATH_PRIVATE pmath_t builtin_developer_frompackedarray(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_developer_getdebuginfo(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_developer_ispackedarray(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_developer_setdebuginfoat( pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_developer_topackedarray(  pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_namespace(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_assign_namespacepath(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_isatom(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_iscomplex(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_iseven(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isexactnumber(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isfloat(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isinexactnumber(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isimaginary(      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isinteger(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_ismachinenumber(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isnumber(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isodd(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_ispos_or_isneg(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isquotient(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isrational(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isreal(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isstring(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_issymbol(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_makeexpression(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_names(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_namespace(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_remove(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringtoboxes(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_symbolname(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_toexpression(     pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/lists/ ...
PMATH_PRIVATE pmath_t builtin_part(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_assign_part(  pmath_expr_t expr); // in replacepart.c
PMATH_PRIVATE pmath_t builtin_replacepart(  pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_call_linearsolvefunction(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_apply(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_append(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_array(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_cases(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_complement(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_constantarray(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_depth(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_det(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_diagonalmatrix(               pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_dimensions(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_dot(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_drop(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_emit(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_extract(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_first(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_fixedpoint_and_fixedpointlist(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_flatten(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_fold(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_foldlist(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_gather(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_head(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_identitymatrix(               pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_inner(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_intersection(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isarray(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isfreeof(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isordered(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_ismatrix(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isvector(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_join(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_last(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_leafcount(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_length(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_lengthwhile(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_level(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_linearsolve(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_listconvolve(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_ludecomposition(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_map(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_mapindexed(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_mapthread(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_max(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_mean(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_min(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_minmax(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_most(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_nest(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_nestlist(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_nestwhile_and_nestwhilelist(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_norm(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_operate(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_ordering(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_padleft_and_padright(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_partition(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_prepend(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_quantile(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_range(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_regather(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_rest(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_reverse(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_riffle(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_scan(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_select(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sort(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sortby(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_split(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_table(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_take(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_takewhile(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_thread(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_through(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_total(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_union(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_unitvector(                   pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/logic/ ...
PMATH_PRIVATE pmath_t builtin_and(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_boole(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_conditionalexpression(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_equal(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_greater(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_greaterequal(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_identical(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_inequation(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_less(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_lessequal(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_not(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_or(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_unequal(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_unidentical(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_xor(                  pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_operate_conditionalexpression(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_operate_undefined(pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/manipulate/ ...
PMATH_PRIVATE pmath_t builtin_expand(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_expandall(pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/number-theory/ ...
PMATH_PRIVATE pmath_t builtin_assign_isnumeric(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isnumeric(       pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_bitand(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitclear(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitget(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitlength(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitnot(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitor(                           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitset(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitshiftleft(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitshiftright(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitxor(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_extendedgcd(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_gcd(                             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isprime(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_jacobisymbol_and_kroneckersymbol(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_lcm(                             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_nextprime(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_randominteger(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_randomreal(                      pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/parallel/ ...
PMATH_PRIVATE pmath_t builtin_abort(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_getthreadid(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_newtask(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_parallelmap(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_parallelmapindexed(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_parallelscan(      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_paralleltry(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_wait(              pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_internal_threadidle(pmath_expr_t expr);
//} ============================================================================

//{ general purpose builtin functions ...

static pmath_t general_builtin_zeroargs(pmath_expr_t expr) {
  if(pmath_expr_length(expr) != 0)
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
  return expr;
}

static pmath_t general_builtin_onearg(pmath_expr_t expr) {
  if(pmath_expr_length(expr) != 1)
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
  return expr;
}

static pmath_t general_builtin_zeroonearg(pmath_expr_t expr) {
  if(pmath_expr_length(expr) > 1)
    pmath_message_argxxx(pmath_expr_length(expr), 0, 1);
  return expr;
}

static pmath_t general_builtin_zerotwoarg(pmath_expr_t expr) {
  if(pmath_expr_length(expr) > 2)
    pmath_message_argxxx(pmath_expr_length(expr), 0, 2);
  return expr;
}

static pmath_t general_builtin_nofront(pmath_expr_t expr) {
  pmath_message(PMATH_NULL, "nofront", 0);
  pmath_unref(expr);
  return pmath_ref(PMATH_SYMBOL_FAILED);
}

//} ============================================================================

PMATH_PRIVATE
pmath_bool_t _pmath_have_code(
  pmath_t            key, // wont be freed
  pmath_code_usage_t usage
) {
  void              *entry;
  pmath_hashtable_t  table;
  
  if((unsigned)usage >= CODE_TABLES_COUNT)
    return FALSE;
    
  table = LOCK_CODE_TABLE(usage);
  
  entry = pmath_ht_search(table, &key);
  
  UNLOCK_CODE_TABLE(usage, table);
  
  return entry != NULL;
}

PMATH_PRIVATE
pmath_bool_t _pmath_run_code(
  pmath_t             key,   // wont be freed
  pmath_code_usage_t  usage,
  pmath_t            *in_out
) {
  func_entry_t         *entry;
  pmath_hashtable_t     table;
  pmath_builtin_func_t  result = NULL;
  
  assert((unsigned)usage <= (unsigned)PMATH_CODE_USAGE_EARLYCALL);
  
  table = LOCK_CODE_TABLE(usage);
  
  entry = pmath_ht_search(table, &key);
  if(entry)
    result = (pmath_builtin_func_t)entry->function;
    
  UNLOCK_CODE_TABLE(usage, table);
  
  if(result && !pmath_aborting()) {
    *in_out = result(*in_out);
    
    return TRUE;
  }
  
  return FALSE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_run_approx_code(
  pmath_t       key,   // wont be freed
  pmath_t      *in_out,
  double        prec
) {
  func_entry_t         *entry;
  pmath_hashtable_t     table;
  pmath_bool_t        (*func)(pmath_t*, double) = NULL;
  
  table = LOCK_CODE_TABLE(PMATH_CODE_USAGE_APPROX);
  
  entry = pmath_ht_search(table, &key);
  if(entry)
    func = (pmath_bool_t(*)(pmath_t*, double))entry->function;
    
  UNLOCK_CODE_TABLE(PMATH_CODE_USAGE_APPROX, table);
  
  if(func) 
    return func(in_out, prec);
  
  return FALSE;
}

/*----------------------------------------------------------------------------*/

PMATH_API
pmath_bool_t pmath_register_code(
  pmath_symbol_t         symbol,
  pmath_builtin_func_t   func,
  pmath_code_usage_t     usage
) {
  func_entry_t       *entry;
  pmath_hashtable_t   table;
  
  if((unsigned)usage >= CODE_TABLES_COUNT)
    return FALSE;
    
  if(func) {
    entry = (func_entry_t*)pmath_mem_alloc(sizeof(func_entry_t));
    if(!entry)
      return FALSE;
      
    entry->key      = pmath_ref(symbol);
    entry->function = (void(*)())func;
  }
  else
    entry = NULL;
    
  table = LOCK_CODE_TABLE(usage);
  
  if(entry)
    entry = pmath_ht_insert(table, entry);
  else
    entry = pmath_ht_remove(table, &symbol);
    
  UNLOCK_CODE_TABLE(usage, table);
  
  
  if(entry)
    destroy_func_entry(entry);
    
  return TRUE;
}

PMATH_API
pmath_bool_t pmath_register_approx_code(
  pmath_symbol_t   symbol,
  pmath_bool_t   (*func)(pmath_t*, double)
) {
  return pmath_register_code(
           symbol,
           (pmath_builtin_func_t)func,
           PMATH_CODE_USAGE_APPROX);
}

/*============================================================================*/

PMATH_PRIVATE pmath_bool_t _pmath_symbol_builtins_init(void) {
  int i;
  
  for(i = 0; i < PMATH_BUILTIN_SYMBOL_COUNT; ++i)
    _pmath_builtin_symbol_array[i] = PMATH_NULL;
    
  memset((void*)_code_tables, 0, CODE_TABLES_COUNT * sizeof(pmath_hashtable_t));
  
  for(i = 0; i < CODE_TABLES_COUNT; ++i) {
    pmath_hashtable_t table = pmath_ht_create(&function_table_class, 0);
    if(!table)
      goto FAIL;
      
    pmath_atomic_write_release(&_code_tables[i], (intptr_t)table);
  }
  
#define VERIFY(X)  do{ pmath_t tmp = (X); if(pmath_is_null(tmp)) goto FAIL; }while(0);
  
#define NEW_SYMBOL(name)        pmath_symbol_get(PMATH_C_STRING(name), TRUE)
#define NEW_SYSTEM_SYMBOL(name) NEW_SYMBOL("System`" name)
  
  //{ setting symbol names ...
  VERIFY(   PMATH_SYMBOL_INTERNAL_ABORTMESSAGE            = NEW_SYMBOL("Internal`AbortMessage"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_CONDITION               = NEW_SYMBOL("Internal`Condition"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_COPYSIGN                = NEW_SYMBOL("Internal`CopySign"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_CRITICALMESSAGETAG      = NEW_SYMBOL("Internal`CriticalMessageTag"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATE         = NEW_SYMBOL("Internal`DynamicEvaluate"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATEMULTIPLE = NEW_SYMBOL("Internal`DynamicEvaluateMultiple"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_DYNAMICREMOVE           = NEW_SYMBOL("Internal`DynamicRemove"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_DYNAMICUPDATED          = NEW_SYMBOL("Internal`DynamicUpdated"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_GETTHREADID             = NEW_SYMBOL("Internal`GetThreadId"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_MESSAGETHROWN           = NEW_SYMBOL("Internal`MessageThrown"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_NAMESPACEPATHSTACK      = NEW_SYMBOL("Internal`$NamespacePathStack"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_NAMESPACESTACK          = NEW_SYMBOL("Internal`$NamespaceStack"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_NEXTTOWARD              = NEW_SYMBOL("Internal`NextToward"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_REALBALLBOUNDS          = NEW_SYMBOL("Internal`RealBallBounds"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_SIGNBIT                 = NEW_SYMBOL("Internal`SignBit"))
  VERIFY(   PMATH_SYMBOL_INTERNAL_THREADIDLE              = NEW_SYMBOL("Internal`ThreadIdle"))
  
  VERIFY(   PMATH_SYMBOL_UTILITIES_GETSYSTEMSYNTAXINFORMATION  = NEW_SYMBOL("System`Utilities`GetSystemSyntaxInformation"))
  
  VERIFY(   PMATH_SYMBOL_BOXFORM_USETEXTFORMATTING = NEW_SYMBOL("System`BoxForm`$UseTextFormatting"))
  
  VERIFY(   PMATH_SYMBOL_DEVELOPER_DEBUGINFOSOURCE     = NEW_SYMBOL("Developer`DebugInfoSource"))
  VERIFY(   PMATH_SYMBOL_DEVELOPER_FILEINFORMATION     = NEW_SYMBOL("Developer`FileInformation"))
  VERIFY(   PMATH_SYMBOL_DEVELOPER_FROMPACKEDARRAY     = NEW_SYMBOL("Developer`FromPackedArray"))
  VERIFY(   PMATH_SYMBOL_DEVELOPER_GETDEBUGINFO        = NEW_SYMBOL("Developer`GetDebugInfo"))
  VERIFY(   PMATH_SYMBOL_DEVELOPER_HASBUILTINCODE      = NEW_SYMBOL("Developer`HasBuiltinCode"))
  VERIFY(   PMATH_SYMBOL_DEVELOPER_ISPACKEDARRAY       = NEW_SYMBOL("Developer`IsPackedArray"))
  VERIFY(   PMATH_SYMBOL_DEVELOPER_PACKEDARRAYFORM     = NEW_SYMBOL("Developer`PackedArrayForm"))
  VERIFY(   PMATH_SYMBOL_DEVELOPER_SETDEBUGINFOAT      = NEW_SYMBOL("Developer`SetDebugInfoAt"))
  VERIFY(   PMATH_SYMBOL_DEVELOPER_SYSTEMINFORMATION   = NEW_SYMBOL("Developer`$SystemInformation"))
  VERIFY(   PMATH_SYMBOL_DEVELOPER_TOPACKEDARRAY       = NEW_SYMBOL("Developer`ToPackedArray"))
  
  VERIFY(   PMATH_SYMBOL_ABORTED                          = NEW_SYSTEM_SYMBOL("$Aborted"))
  VERIFY(   PMATH_SYMBOL_ABORT                            = NEW_SYSTEM_SYMBOL("Abort"))
  VERIFY(   PMATH_SYMBOL_ABS                              = NEW_SYSTEM_SYMBOL("Abs"))
  VERIFY(   PMATH_SYMBOL_ALL                              = NEW_SYSTEM_SYMBOL("All"))
  VERIFY(   PMATH_SYMBOL_ALTERNATIVES                     = NEW_SYSTEM_SYMBOL("Alternatives"))
  VERIFY(   PMATH_SYMBOL_AND                              = NEW_SYSTEM_SYMBOL("And"))
  VERIFY(   PMATH_SYMBOL_ANTIALIASING                     = NEW_SYSTEM_SYMBOL("Antialiasing"))
  VERIFY(   PMATH_SYMBOL_APPEND                           = NEW_SYSTEM_SYMBOL("Append"))
  VERIFY(   PMATH_SYMBOL_APPLICATIONFILENAME              = NEW_SYSTEM_SYMBOL("$ApplicationFileName"))
  VERIFY(   PMATH_SYMBOL_APPLY                            = NEW_SYSTEM_SYMBOL("Apply"))
  VERIFY(   PMATH_SYMBOL_ARCCOS                           = NEW_SYSTEM_SYMBOL("ArcCos"))
  VERIFY(   PMATH_SYMBOL_ARCCOSH                          = NEW_SYSTEM_SYMBOL("ArcCosh"))
  VERIFY(   PMATH_SYMBOL_ARCCOT                           = NEW_SYSTEM_SYMBOL("ArcCot"))
  VERIFY(   PMATH_SYMBOL_ARCCOTH                          = NEW_SYSTEM_SYMBOL("ArcCoth"))
  VERIFY(   PMATH_SYMBOL_ARCCSC                           = NEW_SYSTEM_SYMBOL("ArcCsc"))
  VERIFY(   PMATH_SYMBOL_ARCCSCH                          = NEW_SYSTEM_SYMBOL("ArcCsch"))
  VERIFY(   PMATH_SYMBOL_ARCSEC                           = NEW_SYSTEM_SYMBOL("ArcSec"))
  VERIFY(   PMATH_SYMBOL_ARCSECH                          = NEW_SYSTEM_SYMBOL("ArcSech"))
  VERIFY(   PMATH_SYMBOL_ARCSIN                           = NEW_SYSTEM_SYMBOL("ArcSin"))
  VERIFY(   PMATH_SYMBOL_ARCSINH                          = NEW_SYSTEM_SYMBOL("ArcSinh"))
  VERIFY(   PMATH_SYMBOL_ARCTAN                           = NEW_SYSTEM_SYMBOL("ArcTan"))
  VERIFY(   PMATH_SYMBOL_ARCTANH                          = NEW_SYSTEM_SYMBOL("ArcTanh"))
  VERIFY(   PMATH_SYMBOL_ARG                              = NEW_SYSTEM_SYMBOL("Arg"))
  VERIFY(   PMATH_SYMBOL_ARRAY                            = NEW_SYSTEM_SYMBOL("Array"))
  VERIFY(   PMATH_SYMBOL_ASPECTRATIO                      = NEW_SYSTEM_SYMBOL("AspectRatio"))
  VERIFY(   PMATH_SYMBOL_ASSIGN                           = NEW_SYSTEM_SYMBOL("Assign"))
  VERIFY(   PMATH_SYMBOL_ASSIGNDELAYED                    = NEW_SYSTEM_SYMBOL("AssignDelayed"))
  VERIFY(   PMATH_SYMBOL_ASSOCIATIVE                      = NEW_SYSTEM_SYMBOL("Associative"))
  VERIFY(   PMATH_SYMBOL_ATTRIBUTES                       = NEW_SYSTEM_SYMBOL("Attributes"))
  VERIFY(   PMATH_SYMBOL_AUTODELETE                       = NEW_SYSTEM_SYMBOL("AutoDelete"))
  VERIFY(   PMATH_SYMBOL_AUTOMATIC                        = NEW_SYSTEM_SYMBOL("Automatic"))
  VERIFY(   PMATH_SYMBOL_AUTONUMBERFORMATING              = NEW_SYSTEM_SYMBOL("AutoNumberFormating"))
  VERIFY(   PMATH_SYMBOL_AXES                             = NEW_SYSTEM_SYMBOL("Axes"))
  VERIFY(   PMATH_SYMBOL_AXESORIGIN                       = NEW_SYSTEM_SYMBOL("AxesOrigin"))
  VERIFY(   PMATH_SYMBOL_AUTOSPACING                      = NEW_SYSTEM_SYMBOL("AutoSpacing"))
  VERIFY(   PMATH_SYMBOL_BACKGROUND                       = NEW_SYSTEM_SYMBOL("Background"))
  VERIFY(   PMATH_SYMBOL_BASEFORM                         = NEW_SYSTEM_SYMBOL("BaseForm"))
  VERIFY(   PMATH_SYMBOL_BASESTYLE                        = NEW_SYSTEM_SYMBOL("BaseStyle"))
  VERIFY(   PMATH_SYMBOL_BEGIN                            = NEW_SYSTEM_SYMBOL("Begin"))
  VERIFY(   PMATH_SYMBOL_BEGINPACKAGE                     = NEW_SYSTEM_SYMBOL("BeginPackage"))
  VERIFY(   PMATH_SYMBOL_BERNOULLIB                       = NEW_SYSTEM_SYMBOL("BernoulliB"))
  VERIFY(   PMATH_SYMBOL_BINARYFORMAT                     = NEW_SYSTEM_SYMBOL("BinaryFormat"))
  VERIFY(   PMATH_SYMBOL_BINARYREAD                       = NEW_SYSTEM_SYMBOL("BinaryRead"))
  VERIFY(   PMATH_SYMBOL_BINARYREADLIST                   = NEW_SYSTEM_SYMBOL("BinaryReadList"))
  VERIFY(   PMATH_SYMBOL_BINARYWRITE                      = NEW_SYSTEM_SYMBOL("BinaryWrite"))
  VERIFY(   PMATH_SYMBOL_BINOMIAL                         = NEW_SYSTEM_SYMBOL("Binomial"))
  VERIFY(   PMATH_SYMBOL_BITAND                           = NEW_SYSTEM_SYMBOL("BitAnd"))
  VERIFY(   PMATH_SYMBOL_BITCLEAR                         = NEW_SYSTEM_SYMBOL("BitClear"))
  VERIFY(   PMATH_SYMBOL_BITGET                           = NEW_SYSTEM_SYMBOL("BitGet"))
  VERIFY(   PMATH_SYMBOL_BITLENGTH                        = NEW_SYSTEM_SYMBOL("BitLength"))
  VERIFY(   PMATH_SYMBOL_BITNOT                           = NEW_SYSTEM_SYMBOL("BitNot"))
  VERIFY(   PMATH_SYMBOL_BITOR                            = NEW_SYSTEM_SYMBOL("BitOr"))
  VERIFY(   PMATH_SYMBOL_BITSET                           = NEW_SYSTEM_SYMBOL("BitSet"))
  VERIFY(   PMATH_SYMBOL_BITSHIFTLEFT                     = NEW_SYSTEM_SYMBOL("BitShiftLeft"))
  VERIFY(   PMATH_SYMBOL_BITSHIFTRIGHT                    = NEW_SYSTEM_SYMBOL("BitShiftRight"))
  VERIFY(   PMATH_SYMBOL_BITXOR                           = NEW_SYSTEM_SYMBOL("BitXor"))
  VERIFY(   PMATH_SYMBOL_BLOCK                            = NEW_SYSTEM_SYMBOL("Block"))
  VERIFY(   PMATH_SYMBOL_BOLD                             = NEW_SYSTEM_SYMBOL("Bold"))
  VERIFY(   PMATH_SYMBOL_BOOLE                            = NEW_SYSTEM_SYMBOL("Boole"))
  VERIFY(   PMATH_SYMBOL_BORDERRADIUS                     = NEW_SYSTEM_SYMBOL("BorderRadius"))
  VERIFY(   PMATH_SYMBOL_BOXDATA                          = NEW_SYSTEM_SYMBOL("BoxData"))
  VERIFY(   PMATH_SYMBOL_BOXROTATION                      = NEW_SYSTEM_SYMBOL("BoxRotation"))
  VERIFY(   PMATH_SYMBOL_BOXTRANSFORMATION                = NEW_SYSTEM_SYMBOL("BoxTransformation"))
  VERIFY(   PMATH_SYMBOL_BRACKETINGBAR                    = NEW_SYSTEM_SYMBOL("BracketingBar"))
  VERIFY(   PMATH_SYMBOL_BREAK                            = NEW_SYSTEM_SYMBOL("Break"))
  VERIFY(   PMATH_SYMBOL_BUTTON                           = NEW_SYSTEM_SYMBOL("Button"))
  VERIFY(   PMATH_SYMBOL_BUTTONBOX                        = NEW_SYSTEM_SYMBOL("ButtonBox"))
  VERIFY(   PMATH_SYMBOL_BUTTONFRAME                      = NEW_SYSTEM_SYMBOL("ButtonFrame"))
  VERIFY(   PMATH_SYMBOL_BUTTONFUNCTION                   = NEW_SYSTEM_SYMBOL("ButtonFunction"))
  VERIFY(   PMATH_SYMBOL_BYTECOUNT                        = NEW_SYSTEM_SYMBOL("ByteCount"))
  VERIFY(   PMATH_SYMBOL_BYTEORDERING                     = NEW_SYSTEM_SYMBOL("ByteOrdering"))
  VERIFY(   PMATH_SYMBOL_BYTEORDERINGDEFAULT              = NEW_SYSTEM_SYMBOL("$ByteOrdering"))
  VERIFY(   PMATH_SYMBOL_CANCELED                         = NEW_SYSTEM_SYMBOL("$Canceled"))
  VERIFY(   PMATH_SYMBOL_CASES                            = NEW_SYSTEM_SYMBOL("Cases"))
  VERIFY(   PMATH_SYMBOL_CATCH                            = NEW_SYSTEM_SYMBOL("Catch"))
  VERIFY(   PMATH_SYMBOL_CEILING                          = NEW_SYSTEM_SYMBOL("Ceiling"))
  VERIFY(   PMATH_SYMBOL_CHARACTER                        = NEW_SYSTEM_SYMBOL("Character"))
  VERIFY(   PMATH_SYMBOL_CHARACTERENCODING                = NEW_SYSTEM_SYMBOL("CharacterEncoding"))
  VERIFY(   PMATH_SYMBOL_CHARACTERENCODINGDEFAULT         = NEW_SYSTEM_SYMBOL("$CharacterEncoding"))
  VERIFY(   PMATH_SYMBOL_CHARACTERS                       = NEW_SYSTEM_SYMBOL("Characters"))
  VERIFY(   PMATH_SYMBOL_CHECKBOX                         = NEW_SYSTEM_SYMBOL("Checkbox"))
  VERIFY(   PMATH_SYMBOL_CHECKBOXBOX                      = NEW_SYSTEM_SYMBOL("CheckboxBox"))
  VERIFY(   PMATH_SYMBOL_CHOP                             = NEW_SYSTEM_SYMBOL("Chop"))
  VERIFY(   PMATH_SYMBOL_CIRCLEPLUS                       = NEW_SYSTEM_SYMBOL("CirclePlus"))
  VERIFY(   PMATH_SYMBOL_CIRCLETIMES                      = NEW_SYSTEM_SYMBOL("CircleTimes"))
  VERIFY(   PMATH_SYMBOL_CLEAR                            = NEW_SYSTEM_SYMBOL("Clear"))
  VERIFY(   PMATH_SYMBOL_CLEARALL                         = NEW_SYSTEM_SYMBOL("ClearAll"))
  VERIFY(   PMATH_SYMBOL_CLEARATTRIBUTES                  = NEW_SYSTEM_SYMBOL("ClearAttributes"))
  VERIFY(   PMATH_SYMBOL_CLIP                             = NEW_SYSTEM_SYMBOL("Clip"))
  VERIFY(   PMATH_SYMBOL_CLOCK                            = NEW_SYSTEM_SYMBOL("Clock"))
  VERIFY(   PMATH_SYMBOL_CLOSE                            = NEW_SYSTEM_SYMBOL("Close"))
  VERIFY(   PMATH_SYMBOL_COLON                            = NEW_SYSTEM_SYMBOL("Colon"))
  VERIFY(   PMATH_SYMBOL_COLUMN                           = NEW_SYSTEM_SYMBOL("Column"))
  VERIFY(   PMATH_SYMBOL_COLUMNSPACING                    = NEW_SYSTEM_SYMBOL("ColumnSpacing"))
  VERIFY(   PMATH_SYMBOL_COMMANDLINE                      = NEW_SYSTEM_SYMBOL("$CommandLine"))
  VERIFY(   PMATH_SYMBOL_COMPLEMENT                       = NEW_SYSTEM_SYMBOL("Complement"))
  VERIFY(   PMATH_SYMBOL_COMPLEX                          = NEW_SYSTEM_SYMBOL("Complex"))
  VERIFY(   PMATH_SYMBOL_COMPLEXSTRINGBOX                 = NEW_SYSTEM_SYMBOL("ComplexStringBox"))
  VERIFY(   PMATH_SYMBOL_COMPLEXINFINITY                  = NEW_SYSTEM_SYMBOL("ComplexInfinity"))
  VERIFY(   PMATH_SYMBOL_COMPRESS                         = NEW_SYSTEM_SYMBOL("Compress"))
  VERIFY(   PMATH_SYMBOL_COMPRESSSTREAM                   = NEW_SYSTEM_SYMBOL("CompressStream"))
  VERIFY(   PMATH_SYMBOL_CONDITION                        = NEW_SYSTEM_SYMBOL("Condition"))
  VERIFY(   PMATH_SYMBOL_CONDITIONALEXPRESSION            = NEW_SYSTEM_SYMBOL("ConditionalExpression"))
  VERIFY(   PMATH_SYMBOL_CONGRUENT                        = NEW_SYSTEM_SYMBOL("Congruent"))
  VERIFY(   PMATH_SYMBOL_CONJUGATE                        = NEW_SYSTEM_SYMBOL("Conjugate"))
  VERIFY(   PMATH_SYMBOL_CONSTANTARRAY                    = NEW_SYSTEM_SYMBOL("ConstantArray"))
  VERIFY(   PMATH_SYMBOL_CONTINUOUSACTION                 = NEW_SYSTEM_SYMBOL("ContinuousAction"))
  VERIFY(   PMATH_SYMBOL_CONTINUE                         = NEW_SYSTEM_SYMBOL("Continue"))
  VERIFY(   PMATH_SYMBOL_CONTROLACTIVE                    = NEW_SYSTEM_SYMBOL("ControlActive"))
  VERIFY(   PMATH_SYMBOL_CONTROLACTIVESETTING             = NEW_SYSTEM_SYMBOL("$ControlActiveSetting"))
  VERIFY(   PMATH_SYMBOL_COPYDIRECTORY                    = NEW_SYSTEM_SYMBOL("CopyDirectory"))
  VERIFY(   PMATH_SYMBOL_COPYFILE                         = NEW_SYSTEM_SYMBOL("CopyFile"))
  VERIFY(   PMATH_SYMBOL_COS                              = NEW_SYSTEM_SYMBOL("Cos"))
  VERIFY(   PMATH_SYMBOL_COSH                             = NEW_SYSTEM_SYMBOL("Cosh"))
  VERIFY(   PMATH_SYMBOL_COT                              = NEW_SYSTEM_SYMBOL("Cot"))
  VERIFY(   PMATH_SYMBOL_COTH                             = NEW_SYSTEM_SYMBOL("Coth"))
  VERIFY(   PMATH_SYMBOL_COUNT                            = NEW_SYSTEM_SYMBOL("Count"))
  VERIFY(   PMATH_SYMBOL_CREATEDIRECTORY                  = NEW_SYSTEM_SYMBOL("CreateDirectory"))
  VERIFY(   PMATH_SYMBOL_CREATEDOCUMENT                   = NEW_SYSTEM_SYMBOL("CreateDocument"))
  VERIFY(   PMATH_SYMBOL_CREATIONDATE                     = NEW_SYSTEM_SYMBOL("$CreationDate"))
  VERIFY(   PMATH_SYMBOL_CROSS                            = NEW_SYSTEM_SYMBOL("Cross"))
  VERIFY(   PMATH_SYMBOL_CSC                              = NEW_SYSTEM_SYMBOL("Csc"))
  VERIFY(   PMATH_SYMBOL_CSCH                             = NEW_SYSTEM_SYMBOL("Csch"))
  VERIFY(   PMATH_SYMBOL_CUPCAP                           = NEW_SYSTEM_SYMBOL("CupCap"))
  VERIFY(   PMATH_SYMBOL_CURRENTVALUE                     = NEW_SYSTEM_SYMBOL("CurrentValue"))
  VERIFY(   PMATH_SYMBOL_DATARANGE                        = NEW_SYSTEM_SYMBOL("DataRange"))
  VERIFY(   PMATH_SYMBOL_DATELIST                         = NEW_SYSTEM_SYMBOL("DateList"))
  VERIFY(   PMATH_SYMBOL_DECREMENT                        = NEW_SYSTEM_SYMBOL("Decrement"))
  VERIFY(   PMATH_SYMBOL_DEEPHOLDALL                      = NEW_SYSTEM_SYMBOL("DeepHoldAll"))
  VERIFY(   PMATH_SYMBOL_DEFAULT                          = NEW_SYSTEM_SYMBOL("Default"))
  VERIFY(   PMATH_SYMBOL_DEFAULTDUPLICATESECTIONSTYLE     = NEW_SYSTEM_SYMBOL("DefaultDuplicateSectionStyle"))
  VERIFY(   PMATH_SYMBOL_DEFAULTNEWSECTIONSTYLE           = NEW_SYSTEM_SYMBOL("DefaultNewSectionStyle"))
  VERIFY(   PMATH_SYMBOL_DEFAULTRETURNCREATEDSECTIONSTYLE = NEW_SYSTEM_SYMBOL("DefaultReturnCreatedSectionStyle"))
  VERIFY(   PMATH_SYMBOL_DEFAULTRULES                     = NEW_SYSTEM_SYMBOL("DefaultRules"))
  VERIFY(   PMATH_SYMBOL_DEFINITEFUNCTION                 = NEW_SYSTEM_SYMBOL("DefiniteFunction"))
  VERIFY(   PMATH_SYMBOL_DEGREE                           = NEW_SYSTEM_SYMBOL("Degree"))
  VERIFY(   PMATH_SYMBOL_DEINITIALIZATION                 = NEW_SYSTEM_SYMBOL("Deinitialization"))
  VERIFY(   PMATH_SYMBOL_DELETECONTENTS                   = NEW_SYSTEM_SYMBOL("DeleteContents"))
  VERIFY(   PMATH_SYMBOL_DELETEDIRECTORY                  = NEW_SYSTEM_SYMBOL("DeleteDirectory"))
  VERIFY(   PMATH_SYMBOL_DELETEFILE                       = NEW_SYSTEM_SYMBOL("DeleteFile"))
  VERIFY(   PMATH_SYMBOL_DEPTH                            = NEW_SYSTEM_SYMBOL("Depth"))
  VERIFY(   PMATH_SYMBOL_DET                              = NEW_SYSTEM_SYMBOL("Det"))
  VERIFY(   PMATH_SYMBOL_DIAGONALMATRIX                   = NEW_SYSTEM_SYMBOL("DiagonalMatrix"))
  VERIFY(   PMATH_SYMBOL_DIALOG                           = NEW_SYSTEM_SYMBOL("Dialog"))
  VERIFY(   PMATH_SYMBOL_DIALOGLEVEL                      = NEW_SYSTEM_SYMBOL("$DialogLevel"))
  VERIFY(   PMATH_SYMBOL_DIGITCHARACTER                   = NEW_SYSTEM_SYMBOL("DigitCharacter"))
  VERIFY(   PMATH_SYMBOL_DIMENSIONS                       = NEW_SYSTEM_SYMBOL("Dimensions"))
  VERIFY(   PMATH_SYMBOL_DIRECTEDINFINITY                 = NEW_SYSTEM_SYMBOL("DirectedInfinity"))
  VERIFY(   PMATH_SYMBOL_DIRECTIVE                        = NEW_SYSTEM_SYMBOL("Directive"))
  VERIFY(   PMATH_SYMBOL_DIRECTORY                        = NEW_SYSTEM_SYMBOL("Directory"))
  VERIFY(   PMATH_SYMBOL_DIRECTORYNAME                    = NEW_SYSTEM_SYMBOL("DirectoryName"))
  VERIFY(   PMATH_SYMBOL_DIRECTORYSTACK                   = NEW_SYSTEM_SYMBOL("$DirectoryStack"))
  VERIFY(   PMATH_SYMBOL_DIVIDEBY                         = NEW_SYSTEM_SYMBOL("DivideBy"))
  VERIFY(   PMATH_SYMBOL_DO                               = NEW_SYSTEM_SYMBOL("Do"))
  VERIFY(   PMATH_SYMBOL_DOCKEDSECTIONS                   = NEW_SYSTEM_SYMBOL("DockedSections"))
  VERIFY(   PMATH_SYMBOL_DOCUMENT                         = NEW_SYSTEM_SYMBOL("Document"))
  VERIFY(   PMATH_SYMBOL_DOCUMENTAPPLY                    = NEW_SYSTEM_SYMBOL("DocumentApply"))
  VERIFY(   PMATH_SYMBOL_DOCUMENTDELETE                   = NEW_SYSTEM_SYMBOL("DocumentDelete"))
  VERIFY(   PMATH_SYMBOL_DOCUMENTGET                      = NEW_SYSTEM_SYMBOL("DocumentGet"))
  VERIFY(   PMATH_SYMBOL_DOCUMENTREAD                     = NEW_SYSTEM_SYMBOL("DocumentRead"))
  VERIFY(   PMATH_SYMBOL_DOCUMENTWRITE                    = NEW_SYSTEM_SYMBOL("DocumentWrite"))
  VERIFY(   PMATH_SYMBOL_DOCUMENTS                        = NEW_SYSTEM_SYMBOL("Documents"))
  VERIFY(   PMATH_SYMBOL_DOCUMENTSAVE                     = NEW_SYSTEM_SYMBOL("DocumentSave"))
  VERIFY(   PMATH_SYMBOL_DOT                              = NEW_SYSTEM_SYMBOL("Dot"))
  VERIFY(   PMATH_SYMBOL_DOTEQUAL                         = NEW_SYSTEM_SYMBOL("DotEqual"))
  VERIFY(   PMATH_SYMBOL_DOUBLEBRACKETINGBAR              = NEW_SYSTEM_SYMBOL("DoubleBracketingBar"))
  VERIFY(   PMATH_SYMBOL_DOUBLEDOWNARROW                  = NEW_SYSTEM_SYMBOL("DoubleDownArrow"))
  VERIFY(   PMATH_SYMBOL_DOUBLELEFTARROW                  = NEW_SYSTEM_SYMBOL("DoubleLeftArrow"))
  VERIFY(   PMATH_SYMBOL_DOUBLELEFTRIGHTARROW             = NEW_SYSTEM_SYMBOL("DoubleLeftRightArrow"))
  VERIFY(   PMATH_SYMBOL_DOUBLELOWERRIGHTARROW            = NEW_SYSTEM_SYMBOL("DoubleLowerRightArrow"))
  VERIFY(   PMATH_SYMBOL_DOUBLELOWERLEFTARROW             = NEW_SYSTEM_SYMBOL("DoubleLowerLeftArrow"))
  VERIFY(   PMATH_SYMBOL_DOUBLERIGHTARROW                 = NEW_SYSTEM_SYMBOL("DoubleRightArrow"))
  VERIFY(   PMATH_SYMBOL_DOUBLEUPARROW                    = NEW_SYSTEM_SYMBOL("DoubleUpArrow"))
  VERIFY(   PMATH_SYMBOL_DOUBLEUPDOWNARROW                = NEW_SYSTEM_SYMBOL("DoubleUpDownArrow"))
  VERIFY(   PMATH_SYMBOL_DOUBLEUPPERLEFTARROW             = NEW_SYSTEM_SYMBOL("DoubleUpperLeftArrow"))
  VERIFY(   PMATH_SYMBOL_DOUBLEUPPERRIGHTARROW            = NEW_SYSTEM_SYMBOL("DoubleUpperRightArrow"))
  VERIFY(   PMATH_SYMBOL_DOWNARROW                        = NEW_SYSTEM_SYMBOL("DownArrow"))
  VERIFY(   PMATH_SYMBOL_DOWNRULES                        = NEW_SYSTEM_SYMBOL("DownRules"))
  VERIFY(   PMATH_SYMBOL_DROP                             = NEW_SYSTEM_SYMBOL("Drop"))
  VERIFY(   PMATH_SYMBOL_DYNAMIC                          = NEW_SYSTEM_SYMBOL("Dynamic"))
  VERIFY(   PMATH_SYMBOL_DYNAMICBOX                       = NEW_SYSTEM_SYMBOL("DynamicBox"))
  VERIFY(   PMATH_SYMBOL_DYNAMICLOCAL                     = NEW_SYSTEM_SYMBOL("DynamicLocal"))
  VERIFY(   PMATH_SYMBOL_DYNAMICLOCALBOX                  = NEW_SYSTEM_SYMBOL("DynamicLocalBox"))
  VERIFY(   PMATH_SYMBOL_DYNAMICLOCALVALUES               = NEW_SYSTEM_SYMBOL("DynamicLocalValues"))
  VERIFY(   PMATH_SYMBOL_DYNAMICSETTING                   = NEW_SYSTEM_SYMBOL("DynamicSetting"))
  VERIFY(   PMATH_SYMBOL_E                                = NEW_SYSTEM_SYMBOL("E"))
  VERIFY(   PMATH_SYMBOL_EDITABLE                         = NEW_SYSTEM_SYMBOL("Editable"))
  VERIFY(   PMATH_SYMBOL_ELEMENT                          = NEW_SYSTEM_SYMBOL("Element"))
  VERIFY(   PMATH_SYMBOL_EMIT                             = NEW_SYSTEM_SYMBOL("Emit"))
  VERIFY(   PMATH_SYMBOL_END                              = NEW_SYSTEM_SYMBOL("End"))
  VERIFY(   PMATH_SYMBOL_ENDOFFILE                        = NEW_SYSTEM_SYMBOL("EndOfFile"))
  VERIFY(   PMATH_SYMBOL_ENDOFLINE                        = NEW_SYSTEM_SYMBOL("EndOfLine"))
  VERIFY(   PMATH_SYMBOL_ENDOFSTRING                      = NEW_SYSTEM_SYMBOL("EndOfString"))
  VERIFY(   PMATH_SYMBOL_ENDPACKAGE                       = NEW_SYSTEM_SYMBOL("EndPackage"))
  VERIFY(   PMATH_SYMBOL_ENVIRONMENT                      = NEW_SYSTEM_SYMBOL("Environment"))
  VERIFY(   PMATH_SYMBOL_EQUAL                            = NEW_SYSTEM_SYMBOL("Equal"))
  VERIFY(   PMATH_SYMBOL_EULERGAMMA                       = NEW_SYSTEM_SYMBOL("EulerGamma"))
  VERIFY(   PMATH_SYMBOL_EVALUATABLE                      = NEW_SYSTEM_SYMBOL("Evaluatable"))
  VERIFY(   PMATH_SYMBOL_EVALUATE                         = NEW_SYSTEM_SYMBOL("Evaluate"))
  VERIFY(   PMATH_SYMBOL_EVALUATEDELAYED                  = NEW_SYSTEM_SYMBOL("EvaluateDelayed"))
  VERIFY(   PMATH_SYMBOL_EVALUATIONDOCUMENT               = NEW_SYSTEM_SYMBOL("EvaluationDocument"))
  VERIFY(   PMATH_SYMBOL_EVALUATIONSEQUENCE               = NEW_SYSTEM_SYMBOL("EvaluationSequence"))
  VERIFY(   PMATH_SYMBOL_EXCEPT                           = NEW_SYSTEM_SYMBOL("Except"))
  VERIFY(   PMATH_SYMBOL_EXP                              = NEW_SYSTEM_SYMBOL("Exp"))
  VERIFY(   PMATH_SYMBOL_EXPAND                           = NEW_SYSTEM_SYMBOL("Expand"))
  VERIFY(   PMATH_SYMBOL_EXPANDALL                        = NEW_SYSTEM_SYMBOL("ExpandAll"))
  VERIFY(   PMATH_SYMBOL_EXPRESSION                       = NEW_SYSTEM_SYMBOL("Expression"))
  VERIFY(   PMATH_SYMBOL_EXTENDEDGCD                      = NEW_SYSTEM_SYMBOL("ExtendedGCD"))
  VERIFY(   PMATH_SYMBOL_EXTRACT                          = NEW_SYSTEM_SYMBOL("Extract"))
  VERIFY(   PMATH_SYMBOL_FACTORIAL                        = NEW_SYSTEM_SYMBOL("Factorial"))
  VERIFY(   PMATH_SYMBOL_FACTORIAL2                       = NEW_SYSTEM_SYMBOL("Factorial2"))
  VERIFY(   PMATH_SYMBOL_FAILED                           = NEW_SYSTEM_SYMBOL("$Failed"))
  VERIFY(   PMATH_SYMBOL_FALSE                            = NEW_SYSTEM_SYMBOL("False"))
  VERIFY(   PMATH_SYMBOL_FILE                             = NEW_SYSTEM_SYMBOL("File"))
  VERIFY(   PMATH_SYMBOL_FILEBYTECOUNT                    = NEW_SYSTEM_SYMBOL("FileByteCount"))
  VERIFY(   PMATH_SYMBOL_FILENAMES                        = NEW_SYSTEM_SYMBOL("FileNames"))
  VERIFY(   PMATH_SYMBOL_FILETYPE                         = NEW_SYSTEM_SYMBOL("FileType"))
  VERIFY(   PMATH_SYMBOL_FILLBOX                          = NEW_SYSTEM_SYMBOL("FillBox"))
  VERIFY(   PMATH_SYMBOL_FILTERRULES                      = NEW_SYSTEM_SYMBOL("FilterRules"))
  VERIFY(   PMATH_SYMBOL_FINALLY                          = NEW_SYSTEM_SYMBOL("Finally"))
  VERIFY(   PMATH_SYMBOL_FIND                             = NEW_SYSTEM_SYMBOL("Find"))
  VERIFY(   PMATH_SYMBOL_FINDLIST                         = NEW_SYSTEM_SYMBOL("FindList"))
  VERIFY(   PMATH_SYMBOL_FIRST                            = NEW_SYSTEM_SYMBOL("First"))
  VERIFY(   PMATH_SYMBOL_FIXEDPOINT                       = NEW_SYSTEM_SYMBOL("FixedPoint"))
  VERIFY(   PMATH_SYMBOL_FIXEDPOINTLIST                   = NEW_SYSTEM_SYMBOL("FixedPointList"))
  VERIFY(   PMATH_SYMBOL_FLATTEN                          = NEW_SYSTEM_SYMBOL("Flatten"))
  VERIFY(   PMATH_SYMBOL_FLOOR                            = NEW_SYSTEM_SYMBOL("Floor"))
  VERIFY(   PMATH_SYMBOL_FOLD                             = NEW_SYSTEM_SYMBOL("Fold"))
  VERIFY(   PMATH_SYMBOL_FOLDLIST                         = NEW_SYSTEM_SYMBOL("FoldList"))
  VERIFY(   PMATH_SYMBOL_FONTCOLOR                        = NEW_SYSTEM_SYMBOL("FontColor"))
  VERIFY(   PMATH_SYMBOL_FONTFAMILY                       = NEW_SYSTEM_SYMBOL("FontFamily"))
  VERIFY(   PMATH_SYMBOL_FONTFEATURES                     = NEW_SYSTEM_SYMBOL("FontFeatures"))
  VERIFY(   PMATH_SYMBOL_FONTSIZE                         = NEW_SYSTEM_SYMBOL("FontSize"))
  VERIFY(   PMATH_SYMBOL_FONTSLANT                        = NEW_SYSTEM_SYMBOL("FontSlant"))
  VERIFY(   PMATH_SYMBOL_FONTWEIGHT                       = NEW_SYSTEM_SYMBOL("FontWeight"))
  VERIFY(   PMATH_SYMBOL_FOR                              = NEW_SYSTEM_SYMBOL("For"))
  VERIFY(   PMATH_SYMBOL_FORMAT                           = NEW_SYSTEM_SYMBOL("Format"))
  VERIFY(   PMATH_SYMBOL_FORMATRULES                      = NEW_SYSTEM_SYMBOL("FormatRules"))
  VERIFY(   PMATH_SYMBOL_FRACTIONALPART                   = NEW_SYSTEM_SYMBOL("FractionalPart"))
  VERIFY(   PMATH_SYMBOL_FRACTIONBOX                      = NEW_SYSTEM_SYMBOL("FractionBox"))
  VERIFY(   PMATH_SYMBOL_FRAME                            = NEW_SYSTEM_SYMBOL("Frame"))
  VERIFY(   PMATH_SYMBOL_FRAMEBOX                         = NEW_SYSTEM_SYMBOL("FrameBox"))
  VERIFY(   PMATH_SYMBOL_FRAMED                           = NEW_SYSTEM_SYMBOL("Framed"))
  VERIFY(   PMATH_SYMBOL_FRAMETICKS                       = NEW_SYSTEM_SYMBOL("FrameTicks"))
  VERIFY(   PMATH_SYMBOL_FROMCHARACTERCODE                = NEW_SYSTEM_SYMBOL("FromCharacterCode"))
  VERIFY(   PMATH_SYMBOL_FRONTENDOBJECT                   = NEW_SYSTEM_SYMBOL("FrontEndObject"))
  VERIFY(   PMATH_SYMBOL_FRONTENDTOKENEXECUTE             = NEW_SYSTEM_SYMBOL("FrontEndTokenExecute"))
  VERIFY(   PMATH_SYMBOL_FULLFORM                         = NEW_SYSTEM_SYMBOL("FullForm"))
  VERIFY(   PMATH_SYMBOL_FUNCTION                         = NEW_SYSTEM_SYMBOL("Function"))
  VERIFY(   PMATH_SYMBOL_GAMMA                            = NEW_SYSTEM_SYMBOL("Gamma"))
  VERIFY(   PMATH_SYMBOL_GATHER                           = NEW_SYSTEM_SYMBOL("Gather"))
  VERIFY(   PMATH_SYMBOL_GCD                              = NEW_SYSTEM_SYMBOL("GCD"))
  VERIFY(   PMATH_SYMBOL_GENERAL                          = NEW_SYSTEM_SYMBOL("General"))
  VERIFY(   PMATH_SYMBOL_GENERATEDSECTIONSTYLES           = NEW_SYSTEM_SYMBOL("GeneratedSectionStyles"))
  VERIFY(   PMATH_SYMBOL_GET                              = NEW_SYSTEM_SYMBOL("Get"))
  VERIFY(   PMATH_SYMBOL_GOLDENRATIO                      = NEW_SYSTEM_SYMBOL("GoldenRatio"))
  VERIFY(   PMATH_SYMBOL_GOTO                             = NEW_SYSTEM_SYMBOL("Goto"))
  VERIFY(   PMATH_SYMBOL_GRAPHICS                         = NEW_SYSTEM_SYMBOL("Graphics"))
  VERIFY(   PMATH_SYMBOL_GRAPHICSBOX                      = NEW_SYSTEM_SYMBOL("GraphicsBox"))
  VERIFY(   PMATH_SYMBOL_GRAYLEVEL                        = NEW_SYSTEM_SYMBOL("GrayLevel"))
  VERIFY(   PMATH_SYMBOL_GREATER                          = NEW_SYSTEM_SYMBOL("Greater"))
  VERIFY(   PMATH_SYMBOL_GREATEREQUAL                     = NEW_SYSTEM_SYMBOL("GreaterEqual"))
  VERIFY(   PMATH_SYMBOL_GREATEREQUALLESS                 = NEW_SYSTEM_SYMBOL("GreaterEqualLess"))
  VERIFY(   PMATH_SYMBOL_GREATERFULLEQUAL                 = NEW_SYSTEM_SYMBOL("GreaterFullEqual"))
  VERIFY(   PMATH_SYMBOL_GREATERGREATER                   = NEW_SYSTEM_SYMBOL("GreaterGreater"))
  VERIFY(   PMATH_SYMBOL_GREATERLESS                      = NEW_SYSTEM_SYMBOL("GreaterLess"))
  VERIFY(   PMATH_SYMBOL_GREATERTILDE                     = NEW_SYSTEM_SYMBOL("GreaterTilde"))
  VERIFY(   PMATH_SYMBOL_GRID                             = NEW_SYSTEM_SYMBOL("Grid"))
  VERIFY(   PMATH_SYMBOL_GRIDBOX                          = NEW_SYSTEM_SYMBOL("GridBox"))
  VERIFY(   PMATH_SYMBOL_GRIDBOXCOLUMNSPACING             = NEW_SYSTEM_SYMBOL("GridBoxColumnSpacing"))
  VERIFY(   PMATH_SYMBOL_GRIDBOXROWSPACING                = NEW_SYSTEM_SYMBOL("GridBoxRowSpacing"))
  VERIFY(   PMATH_SYMBOL_HASH                             = NEW_SYSTEM_SYMBOL("Hash"))
  VERIFY(   PMATH_SYMBOL_HEAD                             = NEW_SYSTEM_SYMBOL("Head"))
  VERIFY(   PMATH_SYMBOL_HEADS                            = NEW_SYSTEM_SYMBOL("Heads"))
  VERIFY(   PMATH_SYMBOL_HISTORY                          = NEW_SYSTEM_SYMBOL("$History"))
  VERIFY(   PMATH_SYMBOL_HISTORYLENGTH                    = NEW_SYSTEM_SYMBOL("$HistoryLength"))
  VERIFY(   PMATH_SYMBOL_HOLD                             = NEW_SYSTEM_SYMBOL("Hold"))
  VERIFY(   PMATH_SYMBOL_HOLDALL                          = NEW_SYSTEM_SYMBOL("HoldAll"))
  VERIFY(   PMATH_SYMBOL_HOLDALLCOMPLETE                  = NEW_SYSTEM_SYMBOL("HoldAllComplete"))
  VERIFY(   PMATH_SYMBOL_HOLDCOMPLETE                     = NEW_SYSTEM_SYMBOL("HoldComplete"))
  VERIFY(   PMATH_SYMBOL_HOLDFIRST                        = NEW_SYSTEM_SYMBOL("HoldFirst"))
  VERIFY(   PMATH_SYMBOL_HOLDFORM                         = NEW_SYSTEM_SYMBOL("HoldForm"))
  VERIFY(   PMATH_SYMBOL_HOLDPATTERN                      = NEW_SYSTEM_SYMBOL("HoldPattern"))
  VERIFY(   PMATH_SYMBOL_HOLDREST                         = NEW_SYSTEM_SYMBOL("HoldRest"))
  VERIFY(   PMATH_SYMBOL_HUE                              = NEW_SYSTEM_SYMBOL("Hue"))
  VERIFY(   PMATH_SYMBOL_HUMPDOWNHUMP                     = NEW_SYSTEM_SYMBOL("HumpDownHump"))
  VERIFY(   PMATH_SYMBOL_HUMPEQUAL                        = NEW_SYSTEM_SYMBOL("HumpEqual"))
  VERIFY(   PMATH_SYMBOL_I                                = NEW_SYSTEM_SYMBOL("I"))
  VERIFY(   PMATH_SYMBOL_IDENTICAL                        = NEW_SYSTEM_SYMBOL("Identical"))
  VERIFY(   PMATH_SYMBOL_IDENTITY                         = NEW_SYSTEM_SYMBOL("Identity"))
  VERIFY(   PMATH_SYMBOL_IDENTITYMATRIX                   = NEW_SYSTEM_SYMBOL("IdentityMatrix"))
  VERIFY(   PMATH_SYMBOL_IF                               = NEW_SYSTEM_SYMBOL("If"))
  VERIFY(   PMATH_SYMBOL_IGNORECASE                       = NEW_SYSTEM_SYMBOL("IgnoreCase"))
  VERIFY(   PMATH_SYMBOL_IM                               = NEW_SYSTEM_SYMBOL("Im"))
  VERIFY(   PMATH_SYMBOL_IMAGESIZE                        = NEW_SYSTEM_SYMBOL("ImageSize"))
  VERIFY(   PMATH_SYMBOL_INCREMENT                        = NEW_SYSTEM_SYMBOL("Increment"))
  VERIFY(   PMATH_SYMBOL_INEQUATION                       = NEW_SYSTEM_SYMBOL("Inequation"))
  VERIFY(   PMATH_SYMBOL_INFINITY                         = NEW_SYSTEM_SYMBOL("Infinity"))
  VERIFY(   PMATH_SYMBOL_INHERITED                        = NEW_SYSTEM_SYMBOL("Inherited"))
  VERIFY(   PMATH_SYMBOL_INITIALDIRECTORY                 = NEW_SYSTEM_SYMBOL("$InitialDirectory"))
  VERIFY(   PMATH_SYMBOL_INITIALIZATION                   = NEW_SYSTEM_SYMBOL("Initialization"))
  VERIFY(   PMATH_SYMBOL_INNER                            = NEW_SYSTEM_SYMBOL("Inner"))
  VERIFY(   PMATH_SYMBOL_INPUT                            = NEW_SYSTEM_SYMBOL("$Input"))
  VERIFY(   PMATH_SYMBOL_INPUTFIELD                       = NEW_SYSTEM_SYMBOL("InputField"))
  VERIFY(   PMATH_SYMBOL_INPUTFIELDBOX                    = NEW_SYSTEM_SYMBOL("InputFieldBox"))
  VERIFY(   PMATH_SYMBOL_INPUTFORM                        = NEW_SYSTEM_SYMBOL("InputForm"))
  VERIFY(   PMATH_SYMBOL_INTEGER                          = NEW_SYSTEM_SYMBOL("Integer"))
  VERIFY(   PMATH_SYMBOL_INTEGERPART                      = NEW_SYSTEM_SYMBOL("IntegerPart"))
  VERIFY(   PMATH_SYMBOL_INTERPRETATION                   = NEW_SYSTEM_SYMBOL("Interpretation"))
  VERIFY(   PMATH_SYMBOL_INTERPRETATIONBOX                = NEW_SYSTEM_SYMBOL("InterpretationBox"))
  VERIFY(   PMATH_SYMBOL_INTERRUPT                        = NEW_SYSTEM_SYMBOL("Interrupt"))
  VERIFY(   PMATH_SYMBOL_INTERSECTION                     = NEW_SYSTEM_SYMBOL("Intersection"))
  VERIFY(   PMATH_SYMBOL_INTERVAL                         = NEW_SYSTEM_SYMBOL("Interval"))
  VERIFY(   PMATH_SYMBOL_ISARRAY                          = NEW_SYSTEM_SYMBOL("IsArray"))
  VERIFY(   PMATH_SYMBOL_ISATOM                           = NEW_SYSTEM_SYMBOL("IsAtom"))
  VERIFY(   PMATH_SYMBOL_ISCOMPLEX                        = NEW_SYSTEM_SYMBOL("IsComplex"))
  VERIFY(   PMATH_SYMBOL_ISEVEN                           = NEW_SYSTEM_SYMBOL("IsEven"))
  VERIFY(   PMATH_SYMBOL_ISEXACTNUMBER                    = NEW_SYSTEM_SYMBOL("IsExactNumber"))
  VERIFY(   PMATH_SYMBOL_ISFLOAT                          = NEW_SYSTEM_SYMBOL("IsFloat"))
  VERIFY(   PMATH_SYMBOL_ISFREEOF                         = NEW_SYSTEM_SYMBOL("IsFreeOf"))
  VERIFY(   PMATH_SYMBOL_ISHELD                           = NEW_SYSTEM_SYMBOL("IsHeld"))
  VERIFY(   PMATH_SYMBOL_ISIMAGINARY                      = NEW_SYSTEM_SYMBOL("IsImaginary"))
  VERIFY(   PMATH_SYMBOL_ISINEXACTNUMBER                  = NEW_SYSTEM_SYMBOL("IsInexactNumber"))
  VERIFY(   PMATH_SYMBOL_ISINTEGER                        = NEW_SYSTEM_SYMBOL("IsInteger"))
  VERIFY(   PMATH_SYMBOL_ISMACHINENUMBER                  = NEW_SYSTEM_SYMBOL("IsMachineNumber"))
  VERIFY(   PMATH_SYMBOL_ISMATRIX                         = NEW_SYSTEM_SYMBOL("IsMatrix"))
  VERIFY(   PMATH_SYMBOL_ISNEGATIVE                       = NEW_SYSTEM_SYMBOL("IsNegative"))
  VERIFY(   PMATH_SYMBOL_ISNONNEGATIVE                    = NEW_SYSTEM_SYMBOL("IsNonNegative"))
  VERIFY(   PMATH_SYMBOL_ISNONPOSITIVE                    = NEW_SYSTEM_SYMBOL("IsNonPositive"))
  VERIFY(   PMATH_SYMBOL_ISNUMBER                         = NEW_SYSTEM_SYMBOL("IsNumber"))
  VERIFY(   PMATH_SYMBOL_ISNUMERIC                        = NEW_SYSTEM_SYMBOL("IsNumeric"))
  VERIFY(   PMATH_SYMBOL_ISODD                            = NEW_SYSTEM_SYMBOL("IsOdd"))
  VERIFY(   PMATH_SYMBOL_ISOPTION                         = NEW_SYSTEM_SYMBOL("IsOption"))
  VERIFY(   PMATH_SYMBOL_ISORDERED                        = NEW_SYSTEM_SYMBOL("IsOrdered"))
  VERIFY(   PMATH_SYMBOL_ISPOSITIVE                       = NEW_SYSTEM_SYMBOL("IsPositive"))
  VERIFY(   PMATH_SYMBOL_ISPRIME                          = NEW_SYSTEM_SYMBOL("IsPrime"))
  VERIFY(   PMATH_SYMBOL_ISQUOTIENT                       = NEW_SYSTEM_SYMBOL("IsQuotient"))
  VERIFY(   PMATH_SYMBOL_ISRATIONAL                       = NEW_SYSTEM_SYMBOL("IsRational"))
  VERIFY(   PMATH_SYMBOL_ISREAL                           = NEW_SYSTEM_SYMBOL("IsReal"))
  VERIFY(   PMATH_SYMBOL_ISSTRING                         = NEW_SYSTEM_SYMBOL("IsString"))
  VERIFY(   PMATH_SYMBOL_ISSYMBOL                         = NEW_SYSTEM_SYMBOL("IsSymbol"))
  VERIFY(   PMATH_SYMBOL_ISVALIDARGUMENTCOUNT             = NEW_SYSTEM_SYMBOL("IsValidArgumentCount"))
  VERIFY(   PMATH_SYMBOL_ISVECTOR                         = NEW_SYSTEM_SYMBOL("IsVector"))
  VERIFY(   PMATH_SYMBOL_ITALIC                           = NEW_SYSTEM_SYMBOL("Italic"))
  VERIFY(   PMATH_SYMBOL_JACOBISYMBOL                     = NEW_SYSTEM_SYMBOL("JacobiSymbol"))
  VERIFY(   PMATH_SYMBOL_JOIN                             = NEW_SYSTEM_SYMBOL("Join"))
  VERIFY(   PMATH_SYMBOL_KRONECKERSYMBOL                  = NEW_SYSTEM_SYMBOL("KroneckerSymbol"))
  VERIFY(   PMATH_SYMBOL_LABEL                            = NEW_SYSTEM_SYMBOL("Label"))
  VERIFY(   PMATH_SYMBOL_LANGUAGECATEGORY                 = NEW_SYSTEM_SYMBOL("LanguageCategory"))
  VERIFY(   PMATH_SYMBOL_LAST                             = NEW_SYSTEM_SYMBOL("Last"))
  VERIFY(   PMATH_SYMBOL_LCM                              = NEW_SYSTEM_SYMBOL("LCM"))
  VERIFY(   PMATH_SYMBOL_LEAFCOUNT                        = NEW_SYSTEM_SYMBOL("LeafCount"))
  VERIFY(   PMATH_SYMBOL_LENGTH                           = NEW_SYSTEM_SYMBOL("Length"))
  VERIFY(   PMATH_SYMBOL_LENGTHWHILE                      = NEW_SYSTEM_SYMBOL("LengthWhile"))
  VERIFY(   PMATH_SYMBOL_LEFTARROW                        = NEW_SYSTEM_SYMBOL("LeftArrow"))
  VERIFY(   PMATH_SYMBOL_LEFTTRIANGLE                     = NEW_SYSTEM_SYMBOL("LeftTriangle"))
  VERIFY(   PMATH_SYMBOL_LEFTTRIANGLEEQUAL                = NEW_SYSTEM_SYMBOL("LeftTriangleEqual"))
  VERIFY(   PMATH_SYMBOL_LEFTRIGHTARROW                   = NEW_SYSTEM_SYMBOL("LeftRightArrow"))
  VERIFY(   PMATH_SYMBOL_LESS                             = NEW_SYSTEM_SYMBOL("Less"))
  VERIFY(   PMATH_SYMBOL_LESSEQUAL                        = NEW_SYSTEM_SYMBOL("LessEqual"))
  VERIFY(   PMATH_SYMBOL_LESSEQUALGREATER                 = NEW_SYSTEM_SYMBOL("LessEqualGreater"))
  VERIFY(   PMATH_SYMBOL_LESSFULLEQUAL                    = NEW_SYSTEM_SYMBOL("LessFullEqual"))
  VERIFY(   PMATH_SYMBOL_LESSGREATER                      = NEW_SYSTEM_SYMBOL("LessGreater"))
  VERIFY(   PMATH_SYMBOL_LESSLESS                         = NEW_SYSTEM_SYMBOL("LessLess"))
  VERIFY(   PMATH_SYMBOL_LESSTILDE                        = NEW_SYSTEM_SYMBOL("LessTilde"))
  VERIFY(   PMATH_SYMBOL_LETTERCHARACTER                  = NEW_SYSTEM_SYMBOL("LetterCharacter"))
  VERIFY(   PMATH_SYMBOL_LEVEL                            = NEW_SYSTEM_SYMBOL("Level"))
  VERIFY(   PMATH_SYMBOL_LINE                             = NEW_SYSTEM_SYMBOL("$Line"))
  VERIFY(   PMATH_SYMBOL_LINE_GRAPHICS                    = NEW_SYSTEM_SYMBOL("Line"))
  VERIFY(   PMATH_SYMBOL_LINEBOX                          = NEW_SYSTEM_SYMBOL("LineBox"))
  VERIFY(   PMATH_SYMBOL_LINEARSOLVE                      = NEW_SYSTEM_SYMBOL("LinearSolve"))
  VERIFY(   PMATH_SYMBOL_LINEARSOLVEFUNCTION              = NEW_SYSTEM_SYMBOL("LinearSolveFunction"))
  VERIFY(   PMATH_SYMBOL_LINEBREAKWITHIN                  = NEW_SYSTEM_SYMBOL("LineBreakWithin"))
  VERIFY(   PMATH_SYMBOL_LIST                             = NEW_SYSTEM_SYMBOL("List"))
  VERIFY(   PMATH_SYMBOL_LISTABLE                         = NEW_SYSTEM_SYMBOL("Listable"))
  VERIFY(   PMATH_SYMBOL_LISTCONVOLVE                     = NEW_SYSTEM_SYMBOL("ListConvolve"))
  VERIFY(   PMATH_SYMBOL_LISTLINEPLOT                     = NEW_SYSTEM_SYMBOL("ListLinePlot"))
  VERIFY(   PMATH_SYMBOL_LITERAL                          = NEW_SYSTEM_SYMBOL("Literal"))
  VERIFY(   PMATH_SYMBOL_LOADLIBRARY                      = NEW_SYSTEM_SYMBOL("LoadLibrary"))
  VERIFY(   PMATH_SYMBOL_LOCAL                            = NEW_SYSTEM_SYMBOL("Local"))
  VERIFY(   PMATH_SYMBOL_LOG                              = NEW_SYSTEM_SYMBOL("Log"))
  VERIFY(   PMATH_SYMBOL_LOGGAMMA                         = NEW_SYSTEM_SYMBOL("LogGamma"))
  VERIFY(   PMATH_SYMBOL_LONGEST                          = NEW_SYSTEM_SYMBOL("Longest"))
  VERIFY(   PMATH_SYMBOL_LONGFORM                         = NEW_SYSTEM_SYMBOL("LongForm"))
  VERIFY(   PMATH_SYMBOL_LOWERRIGHTARROW                  = NEW_SYSTEM_SYMBOL("LowerRightArrow"))
  VERIFY(   PMATH_SYMBOL_LOWERLEFTARROW                   = NEW_SYSTEM_SYMBOL("LowerLeftArrow"))
  VERIFY(   PMATH_SYMBOL_LUDECOMPOSITION                  = NEW_SYSTEM_SYMBOL("LUDecomposition"))
  VERIFY(   PMATH_SYMBOL_MACHINEEPSILON                   = NEW_SYSTEM_SYMBOL("$MachineEpsilon"))
  VERIFY(   PMATH_SYMBOL_MACHINEPRECISION                 = NEW_SYSTEM_SYMBOL("MachinePrecision"))
  VERIFY(   PMATH_SYMBOL_MACHINEPRECISION_APPROX          = NEW_SYSTEM_SYMBOL("$MachinePrecision"))
  VERIFY(   PMATH_SYMBOL_MAGNIFICATION                    = NEW_SYSTEM_SYMBOL("Magnification"))
  VERIFY(   PMATH_SYMBOL_MAKEBOXES                        = NEW_SYSTEM_SYMBOL("MakeBoxes"))
  VERIFY(   PMATH_SYMBOL_MAKEEXPRESSION                   = NEW_SYSTEM_SYMBOL("MakeExpression"))
  VERIFY(   PMATH_SYMBOL_MAP                              = NEW_SYSTEM_SYMBOL("Map"))
  VERIFY(   PMATH_SYMBOL_MAPINDEXED                       = NEW_SYSTEM_SYMBOL("MapIndexed"))
  VERIFY(   PMATH_SYMBOL_MAPTHREAD                        = NEW_SYSTEM_SYMBOL("MapThread"))
  VERIFY(   PMATH_SYMBOL_MATCH                            = NEW_SYSTEM_SYMBOL("Match"))
  VERIFY(   PMATH_SYMBOL_MATRIXFORM                       = NEW_SYSTEM_SYMBOL("MatrixForm"))
  VERIFY(   PMATH_SYMBOL_MAX                              = NEW_SYSTEM_SYMBOL("Max"))
  VERIFY(   PMATH_SYMBOL_MAXEXTRAPRECISION                = NEW_SYSTEM_SYMBOL("$MaxExtraPrecision"))
  VERIFY(   PMATH_SYMBOL_MAXMACHINENUMBER                 = NEW_SYSTEM_SYMBOL("$MaxMachineNumber"))
  VERIFY(   PMATH_SYMBOL_MAXRECURSION                     = NEW_SYSTEM_SYMBOL("MaxRecursion"))
  VERIFY(   PMATH_SYMBOL_MEAN                             = NEW_SYSTEM_SYMBOL("Mean"))
  VERIFY(   PMATH_SYMBOL_MEMORYUSAGE                      = NEW_SYSTEM_SYMBOL("MemoryUsage"))
  VERIFY(   PMATH_SYMBOL_MESSAGE                          = NEW_SYSTEM_SYMBOL("Message"))
  VERIFY(   PMATH_SYMBOL_MESSAGECOUNT                     = NEW_SYSTEM_SYMBOL("$MessageCount"))
  VERIFY(   PMATH_SYMBOL_MESSAGEGROUPS                    = NEW_SYSTEM_SYMBOL("$MessageGroups"))
  VERIFY(   PMATH_SYMBOL_MESSAGENAME                      = NEW_SYSTEM_SYMBOL("MessageName"))
  VERIFY(   PMATH_SYMBOL_MESSAGES                         = NEW_SYSTEM_SYMBOL("Messages"))
  VERIFY(   PMATH_SYMBOL_METHOD                           = NEW_SYSTEM_SYMBOL("Method"))
  VERIFY(   PMATH_SYMBOL_MIN                              = NEW_SYSTEM_SYMBOL("Min"))
  VERIFY(   PMATH_SYMBOL_MINMAX                           = NEW_SYSTEM_SYMBOL("MinMax"))
  VERIFY(   PMATH_SYMBOL_MINMACHINENUMBER                 = NEW_SYSTEM_SYMBOL("$MinMachineNumber"))
  VERIFY(   PMATH_SYMBOL_MINUSPLUS                        = NEW_SYSTEM_SYMBOL("MinusPlus"))
  VERIFY(   PMATH_SYMBOL_MOD                              = NEW_SYSTEM_SYMBOL("Mod"))
  VERIFY(   PMATH_SYMBOL_MOST                             = NEW_SYSTEM_SYMBOL("Most"))
  VERIFY(   PMATH_SYMBOL_N                                = NEW_SYSTEM_SYMBOL("N"))
  VERIFY(   PMATH_SYMBOL_NAMES                            = NEW_SYSTEM_SYMBOL("Names"))
  VERIFY(   PMATH_SYMBOL_CURRENTNAMESPACE                 = NEW_SYSTEM_SYMBOL("$Namespace"))
  VERIFY(   PMATH_SYMBOL_NAMESPACE                        = NEW_SYSTEM_SYMBOL("Namespace"))
  VERIFY(   PMATH_SYMBOL_NAMESPACEPATH                    = NEW_SYSTEM_SYMBOL("$NamespacePath"))
  VERIFY(   PMATH_SYMBOL_NCACHE                           = NEW_SYSTEM_SYMBOL("NCache"))
  VERIFY(   PMATH_SYMBOL_NEWMESSAGE                       = NEW_SYSTEM_SYMBOL("$NewMessage"))
  VERIFY(   PMATH_SYMBOL_NEWSYMBOL                        = NEW_SYSTEM_SYMBOL("$NewSymbol"))
  VERIFY(   PMATH_SYMBOL_NEST                             = NEW_SYSTEM_SYMBOL("Nest"))
  VERIFY(   PMATH_SYMBOL_NESTLIST                         = NEW_SYSTEM_SYMBOL("NestList"))
  VERIFY(   PMATH_SYMBOL_NESTWHILE                        = NEW_SYSTEM_SYMBOL("NestWhile"))
  VERIFY(   PMATH_SYMBOL_NESTWHILELIST                    = NEW_SYSTEM_SYMBOL("NestWhileList"))
  VERIFY(   PMATH_SYMBOL_NEWTASK                          = NEW_SYSTEM_SYMBOL("NewTask"))
  VERIFY(   PMATH_SYMBOL_NEXTPRIME                        = NEW_SYSTEM_SYMBOL("NextPrime"))
  VERIFY(   PMATH_SYMBOL_NHOLDALL                         = NEW_SYSTEM_SYMBOL("NHoldAll"))
  VERIFY(   PMATH_SYMBOL_NHOLDFIRST                       = NEW_SYSTEM_SYMBOL("NHoldFirst"))
  VERIFY(   PMATH_SYMBOL_NHOLDREST                        = NEW_SYSTEM_SYMBOL("NHoldRest"))
  VERIFY(   PMATH_SYMBOL_NONE                             = NEW_SYSTEM_SYMBOL("None"))
  VERIFY(   PMATH_SYMBOL_NORM                             = NEW_SYSTEM_SYMBOL("Norm"))
  VERIFY(   PMATH_SYMBOL_NOT                              = NEW_SYSTEM_SYMBOL("Not"))
  VERIFY(   PMATH_SYMBOL_NOTCONGRUENT                     = NEW_SYSTEM_SYMBOL("NotCongruent"))
  VERIFY(   PMATH_SYMBOL_NOTCUPCAP                        = NEW_SYSTEM_SYMBOL("NotCupCap"))
  VERIFY(   PMATH_SYMBOL_NOTELEMENT                       = NEW_SYSTEM_SYMBOL("NotElement"))
  VERIFY(   PMATH_SYMBOL_NOTGREATER                       = NEW_SYSTEM_SYMBOL("NotGreaterGreater"))
  VERIFY(   PMATH_SYMBOL_NOTGREATEREQUAL                  = NEW_SYSTEM_SYMBOL("NotGreaterEqual"))
  VERIFY(   PMATH_SYMBOL_NOTGREATERLESS                   = NEW_SYSTEM_SYMBOL("NotGreaterLess"))
  VERIFY(   PMATH_SYMBOL_NOTGREATERTILDE                  = NEW_SYSTEM_SYMBOL("NotGreaterTilde"))
  VERIFY(   PMATH_SYMBOL_NOTLEFTTRIANGLE                  = NEW_SYSTEM_SYMBOL("NotLeftTriangle"))
  VERIFY(   PMATH_SYMBOL_NOTLEFTTRIANGLEEQUAL             = NEW_SYSTEM_SYMBOL("NotLeftTriangleEqual"))
  VERIFY(   PMATH_SYMBOL_NOTLESS                          = NEW_SYSTEM_SYMBOL("NotLess"))
  VERIFY(   PMATH_SYMBOL_NOTLESSEQUAL                     = NEW_SYSTEM_SYMBOL("NotLessEqual"))
  VERIFY(   PMATH_SYMBOL_NOTLESSGREATER                   = NEW_SYSTEM_SYMBOL("NotLessGreater"))
  VERIFY(   PMATH_SYMBOL_NOTLESSTILDE                     = NEW_SYSTEM_SYMBOL("NotLessTilde"))
  VERIFY(   PMATH_SYMBOL_NOTPRECEDES                      = NEW_SYSTEM_SYMBOL("NotPrecedes"))
  VERIFY(   PMATH_SYMBOL_NOTPRECEDESEQUAL                 = NEW_SYSTEM_SYMBOL("NotPrecedesEqual"))
  VERIFY(   PMATH_SYMBOL_NOTREVERSEELEMENT                = NEW_SYSTEM_SYMBOL("NotReverseElement"))
  VERIFY(   PMATH_SYMBOL_NOTRIGHTTRIANGLE                 = NEW_SYSTEM_SYMBOL("NotRightTriangle"))
  VERIFY(   PMATH_SYMBOL_NOTRIGHTTRIANGLEEQUAL            = NEW_SYSTEM_SYMBOL("NotRightTriangleEqual"))
  VERIFY(   PMATH_SYMBOL_NOTSUBSET                        = NEW_SYSTEM_SYMBOL("NotSubset"))
  VERIFY(   PMATH_SYMBOL_NOTSUBSETEQUAL                   = NEW_SYSTEM_SYMBOL("NotSubsetEqual"))
  VERIFY(   PMATH_SYMBOL_NOTSUCCEEDS                      = NEW_SYSTEM_SYMBOL("NotSucceeds"))
  VERIFY(   PMATH_SYMBOL_NOTSUCCEEDSEQUAL                 = NEW_SYSTEM_SYMBOL("NotSucceedsEqual"))
  VERIFY(   PMATH_SYMBOL_NOTSUPERSET                      = NEW_SYSTEM_SYMBOL("NotSuperset"))
  VERIFY(   PMATH_SYMBOL_NOTSUPERSETEQUAL                 = NEW_SYSTEM_SYMBOL("NotSupersetEqual"))
  VERIFY(   PMATH_SYMBOL_NOTTILDEEQUAL                    = NEW_SYSTEM_SYMBOL("NotTildeEqual"))
  VERIFY(   PMATH_SYMBOL_NOTTILDEFULLEQUAL                = NEW_SYSTEM_SYMBOL("NotTildeFullEqual"))
  VERIFY(   PMATH_SYMBOL_NOTTILDETILDE                    = NEW_SYSTEM_SYMBOL("NotTildeTilde"))
  VERIFY(   PMATH_SYMBOL_NRULES                           = NEW_SYSTEM_SYMBOL("NRules"))
  VERIFY(   PMATH_SYMBOL_NUMBER                           = NEW_SYSTEM_SYMBOL("Number"))
  VERIFY(   PMATH_SYMBOL_NUMBERSTRING                     = NEW_SYSTEM_SYMBOL("NumberString"))
  VERIFY(   PMATH_SYMBOL_NUMERICFUNCTION                  = NEW_SYSTEM_SYMBOL("NumericFunction"))
  VERIFY(   PMATH_SYMBOL_OFF                              = NEW_SYSTEM_SYMBOL("Off"))
  VERIFY(   PMATH_SYMBOL_ON                               = NEW_SYSTEM_SYMBOL("On"))
  VERIFY(   PMATH_SYMBOL_ONEIDENTITY                      = NEW_SYSTEM_SYMBOL("OneIdentity"))
  VERIFY(   PMATH_SYMBOL_OPENAPPEND                       = NEW_SYSTEM_SYMBOL("OpenAppend"))
  VERIFY(   PMATH_SYMBOL_OPENREAD                         = NEW_SYSTEM_SYMBOL("OpenRead"))
  VERIFY(   PMATH_SYMBOL_OPENWRITE                        = NEW_SYSTEM_SYMBOL("OpenWrite"))
  VERIFY(   PMATH_SYMBOL_OPERATE                          = NEW_SYSTEM_SYMBOL("Operate"))
  VERIFY(   PMATH_SYMBOL_OPTIONAL                         = NEW_SYSTEM_SYMBOL("Optional"))
  VERIFY(   PMATH_SYMBOL_OPTIONS                          = NEW_SYSTEM_SYMBOL("Options"))
  VERIFY(   PMATH_SYMBOL_OPTIONSPATTERN                   = NEW_SYSTEM_SYMBOL("OptionsPattern"))
  VERIFY(   PMATH_SYMBOL_OPTIONVALUE                      = NEW_SYSTEM_SYMBOL("OptionValue"))
  VERIFY(   PMATH_SYMBOL_OR                               = NEW_SYSTEM_SYMBOL("Or"))
  VERIFY(   PMATH_SYMBOL_ORDERING                         = NEW_SYSTEM_SYMBOL("Ordering"))
  VERIFY(   PMATH_SYMBOL_OUTPUTFORM                       = NEW_SYSTEM_SYMBOL("OutputForm"))
  VERIFY(   PMATH_SYMBOL_OVERFLOW                         = NEW_SYSTEM_SYMBOL("Overflow"))
  VERIFY(   PMATH_SYMBOL_OVERLAPS                         = NEW_SYSTEM_SYMBOL("Overlaps"))
  VERIFY(   PMATH_SYMBOL_OVERSCRIPT                       = NEW_SYSTEM_SYMBOL("Overscript"))
  VERIFY(   PMATH_SYMBOL_OVERSCRIPTBOX                    = NEW_SYSTEM_SYMBOL("OverscriptBox"))
  VERIFY(   PMATH_SYMBOL_OWNRULES                         = NEW_SYSTEM_SYMBOL("OwnRules"))
  VERIFY(   PMATH_SYMBOL_PACKAGES                         = NEW_SYSTEM_SYMBOL("$Packages"))
  VERIFY(   PMATH_SYMBOL_PADLEFT                          = NEW_SYSTEM_SYMBOL("PadLeft"))
  VERIFY(   PMATH_SYMBOL_PADRIGHT                         = NEW_SYSTEM_SYMBOL("PadRight"))
  VERIFY(   PMATH_SYMBOL_PAGEWIDTH                        = NEW_SYSTEM_SYMBOL("PageWidth"))
  VERIFY(   PMATH_SYMBOL_PAGEWIDTHDEFAULT                 = NEW_SYSTEM_SYMBOL("$PageWidth"))
  VERIFY(   PMATH_SYMBOL_PARALLEL_RETURN                  = NEW_SYSTEM_SYMBOL("Parallel`Return"))
  VERIFY(   PMATH_SYMBOL_PARALLELMAP                      = NEW_SYSTEM_SYMBOL("ParallelMap"))
  VERIFY(   PMATH_SYMBOL_PARALLELMAPINDEXED               = NEW_SYSTEM_SYMBOL("ParallelMapIndexed"))
  VERIFY(   PMATH_SYMBOL_PARALLELSCAN                     = NEW_SYSTEM_SYMBOL("ParallelScan"))
  VERIFY(   PMATH_SYMBOL_PARALLELTRY                      = NEW_SYSTEM_SYMBOL("ParallelTry"))
  VERIFY(   PMATH_SYMBOL_PARENTDIRECTORY                  = NEW_SYSTEM_SYMBOL("ParentDirectory"))
  VERIFY(   PMATH_SYMBOL_PARENTHESIZEBOXES                = NEW_SYSTEM_SYMBOL("ParenthesizeBoxes"))
  VERIFY(   PMATH_SYMBOL_PARSERARGUMENTS                  = NEW_SYSTEM_SYMBOL("ParserArguments"))
  VERIFY(   PMATH_SYMBOL_PARSESYMBOLS                     = NEW_SYSTEM_SYMBOL("ParseSymbols"))
  VERIFY(   PMATH_SYMBOL_PART                             = NEW_SYSTEM_SYMBOL("Part"))
  VERIFY(   PMATH_SYMBOL_PARTITION                        = NEW_SYSTEM_SYMBOL("Partition"))
  VERIFY(   PMATH_SYMBOL_PATH                             = NEW_SYSTEM_SYMBOL("Path"))
  VERIFY(   PMATH_SYMBOL_PATHDEFAULT                      = NEW_SYSTEM_SYMBOL("$Path"))
  VERIFY(   PMATH_SYMBOL_PATHLISTSEPARATOR                = NEW_SYSTEM_SYMBOL("$PathListSeparator"))
  VERIFY(   PMATH_SYMBOL_PATHNAMESEPARATOR                = NEW_SYSTEM_SYMBOL("$PathnameSeparator"))
  VERIFY(   PMATH_SYMBOL_PATTERN                          = NEW_SYSTEM_SYMBOL("Pattern"))
  VERIFY(   PMATH_SYMBOL_PATTERNSEQUENCE                  = NEW_SYSTEM_SYMBOL("PatternSequence"))
  VERIFY(   PMATH_SYMBOL_PAUSE                            = NEW_SYSTEM_SYMBOL("Pause"))
  VERIFY(   PMATH_SYMBOL_PI                               = NEW_SYSTEM_SYMBOL("Pi"))
  VERIFY(   PMATH_SYMBOL_PIECEWISE                        = NEW_SYSTEM_SYMBOL("Piecewise"))
  VERIFY(   PMATH_SYMBOL_PLACEHOLDER                      = NEW_SYSTEM_SYMBOL("Placeholder"))
  VERIFY(   PMATH_SYMBOL_PLAIN                            = NEW_SYSTEM_SYMBOL("Plain"))
  VERIFY(   PMATH_SYMBOL_PLOT                             = NEW_SYSTEM_SYMBOL("Plot"))
  VERIFY(   PMATH_SYMBOL_PLOTPOINTS                       = NEW_SYSTEM_SYMBOL("PlotPoints"))
  VERIFY(   PMATH_SYMBOL_PLOTRANGE                        = NEW_SYSTEM_SYMBOL("PlotRange"))
  VERIFY(   PMATH_SYMBOL_PLOTSTYLE                        = NEW_SYSTEM_SYMBOL("PlotStyle"))
  VERIFY(   PMATH_SYMBOL_PLUS                             = NEW_SYSTEM_SYMBOL("Plus"))
  VERIFY(   PMATH_SYMBOL_PLUSMINUS                        = NEW_SYSTEM_SYMBOL("PlusMinus"))
  VERIFY(   PMATH_SYMBOL_POINT                            = NEW_SYSTEM_SYMBOL("Point"))
  VERIFY(   PMATH_SYMBOL_POINTBOX                         = NEW_SYSTEM_SYMBOL("PointBox"))
  VERIFY(   PMATH_SYMBOL_POLYGAMMA                        = NEW_SYSTEM_SYMBOL("PolyGamma"))
  VERIFY(   PMATH_SYMBOL_POSITION                         = NEW_SYSTEM_SYMBOL("Position"))
  VERIFY(   PMATH_SYMBOL_POSTDECREMENT                    = NEW_SYSTEM_SYMBOL("PostDecrement"))
  VERIFY(   PMATH_SYMBOL_POSTINCREMENT                    = NEW_SYSTEM_SYMBOL("PostIncrement"))
  VERIFY(   PMATH_SYMBOL_POWER                            = NEW_SYSTEM_SYMBOL("Power"))
  VERIFY(   PMATH_SYMBOL_POWERMOD                         = NEW_SYSTEM_SYMBOL("PowerMod"))
  VERIFY(   PMATH_SYMBOL_PRECEDES                         = NEW_SYSTEM_SYMBOL("Precedes"))
  VERIFY(   PMATH_SYMBOL_PRECEDESEQUAL                    = NEW_SYSTEM_SYMBOL("PrecedesEqual"))
  VERIFY(   PMATH_SYMBOL_PRECEDESTILDE                    = NEW_SYSTEM_SYMBOL("PrecedesTilde"))
  VERIFY(   PMATH_SYMBOL_PRECISION                        = NEW_SYSTEM_SYMBOL("Precision"))
  VERIFY(   PMATH_SYMBOL_PREPEND                          = NEW_SYSTEM_SYMBOL("Prepend"))
  VERIFY(   PMATH_SYMBOL_PRINT                            = NEW_SYSTEM_SYMBOL("Print"))
  VERIFY(   PMATH_SYMBOL_PROCESSID                        = NEW_SYSTEM_SYMBOL("$ProcessId"))
  VERIFY(   PMATH_SYMBOL_PROCESSORCOUNT                   = NEW_SYSTEM_SYMBOL("$ProcessorCount"))
  VERIFY(   PMATH_SYMBOL_PROCESSORTYPE                    = NEW_SYSTEM_SYMBOL("$ProcessorType"))
  VERIFY(   PMATH_SYMBOL_PRODUCT                          = NEW_SYSTEM_SYMBOL("Product"))
  VERIFY(   PMATH_SYMBOL_PROGRESSINDICATOR                = NEW_SYSTEM_SYMBOL("ProgressIndicator"))
  VERIFY(   PMATH_SYMBOL_PROGRESSINDICATORBOX             = NEW_SYSTEM_SYMBOL("ProgressIndicatorBox"))
  VERIFY(   PMATH_SYMBOL_PROTECT                          = NEW_SYSTEM_SYMBOL("Protect"))
  VERIFY(   PMATH_SYMBOL_PROTECTED                        = NEW_SYSTEM_SYMBOL("Protected"))
  VERIFY(   PMATH_SYMBOL_PUREARGUMENT                     = NEW_SYSTEM_SYMBOL("PureArgument"))
  VERIFY(   PMATH_SYMBOL_QRDECOMPOSITION                  = NEW_SYSTEM_SYMBOL("QRDecomposition"))
  VERIFY(   PMATH_SYMBOL_QUANTILE                         = NEW_SYSTEM_SYMBOL("Quantile"))
  VERIFY(   PMATH_SYMBOL_QUIT                             = NEW_SYSTEM_SYMBOL("Quit"))
  VERIFY(   PMATH_SYMBOL_QUOTIENT                         = NEW_SYSTEM_SYMBOL("Quotient"))
  VERIFY(   PMATH_SYMBOL_RADICALBOX                       = NEW_SYSTEM_SYMBOL("RadicalBox"))
  VERIFY(   PMATH_SYMBOL_RADIOBUTTON                      = NEW_SYSTEM_SYMBOL("RadioButton"))
  VERIFY(   PMATH_SYMBOL_RADIOBUTTONBAR                   = NEW_SYSTEM_SYMBOL("RadioButtonBar"))
  VERIFY(   PMATH_SYMBOL_RADIOBUTTONBOX                   = NEW_SYSTEM_SYMBOL("RadioButtonBox"))
  VERIFY(   PMATH_SYMBOL_RANDOMINTEGER                    = NEW_SYSTEM_SYMBOL("RandomInteger"))
  VERIFY(   PMATH_SYMBOL_RANDOMREAL                       = NEW_SYSTEM_SYMBOL("RandomReal"))
  VERIFY(   PMATH_SYMBOL_RANGE                            = NEW_SYSTEM_SYMBOL("Range"))
  VERIFY(   PMATH_SYMBOL_RATIONAL                         = NEW_SYSTEM_SYMBOL("Rational"))
  VERIFY(   PMATH_SYMBOL_RAWBOXES                         = NEW_SYSTEM_SYMBOL("RawBoxes"))
  VERIFY(   PMATH_SYMBOL_RE                               = NEW_SYSTEM_SYMBOL("Re"))
  VERIFY(   PMATH_SYMBOL_READ                             = NEW_SYSTEM_SYMBOL("Read"))
  VERIFY(   PMATH_SYMBOL_READLIST                         = NEW_SYSTEM_SYMBOL("ReadList"))
  VERIFY(   PMATH_SYMBOL_READPROTECTED                    = NEW_SYSTEM_SYMBOL("ReadProtected"))
  VERIFY(   PMATH_SYMBOL_REAL                             = NEW_SYSTEM_SYMBOL("Real"))
  VERIFY(   PMATH_SYMBOL_RECORDLISTS                      = NEW_SYSTEM_SYMBOL("RecordLists"))
  VERIFY(   PMATH_SYMBOL_REFRESH                          = NEW_SYSTEM_SYMBOL("Refresh"))
  VERIFY(   PMATH_SYMBOL_REGATHER                         = NEW_SYSTEM_SYMBOL("ReGather"))
  VERIFY(   PMATH_SYMBOL_REGULAREXPRESSION                = NEW_SYSTEM_SYMBOL("RegularExpression"))
  VERIFY(   PMATH_SYMBOL_RELEASEHOLD                      = NEW_SYSTEM_SYMBOL("ReleaseHold"))
  VERIFY(   PMATH_SYMBOL_REMOVE                           = NEW_SYSTEM_SYMBOL("Remove"))
  VERIFY(   PMATH_SYMBOL_RENAMEDIRECTORY                  = NEW_SYSTEM_SYMBOL("RenameDirectory"))
  VERIFY(   PMATH_SYMBOL_RENAMEFILE                       = NEW_SYSTEM_SYMBOL("RenameFile"))
  VERIFY(   PMATH_SYMBOL_REPEATED                         = NEW_SYSTEM_SYMBOL("Repeated"))
  VERIFY(   PMATH_SYMBOL_REPLACE                          = NEW_SYSTEM_SYMBOL("Replace"))
  VERIFY(   PMATH_SYMBOL_REPLACELIST                      = NEW_SYSTEM_SYMBOL("ReplaceList"))
  VERIFY(   PMATH_SYMBOL_REPLACEPART                      = NEW_SYSTEM_SYMBOL("ReplacePart"))
  VERIFY(   PMATH_SYMBOL_REPLACEREPEATED                  = NEW_SYSTEM_SYMBOL("ReplaceRepeated"))
  VERIFY(   PMATH_SYMBOL_RESCALE                          = NEW_SYSTEM_SYMBOL("Rescale"))
  VERIFY(   PMATH_SYMBOL_RESETDIRECTORY                   = NEW_SYSTEM_SYMBOL("ResetDirectory"))
  VERIFY(   PMATH_SYMBOL_REST                             = NEW_SYSTEM_SYMBOL("Rest"))
  VERIFY(   PMATH_SYMBOL_RETURN                           = NEW_SYSTEM_SYMBOL("Return"))
  VERIFY(   PMATH_SYMBOL_RETURNCREATESNEWSECTION          = NEW_SYSTEM_SYMBOL("ReturnCreatesNewSection"))
  VERIFY(   PMATH_SYMBOL_REVERSE                          = NEW_SYSTEM_SYMBOL("Reverse"))
  VERIFY(   PMATH_SYMBOL_REVERSEELEMENT                   = NEW_SYSTEM_SYMBOL("ReverseElement"))
  VERIFY(   PMATH_SYMBOL_RGBCOLOR                         = NEW_SYSTEM_SYMBOL("RGBColor"))
  VERIFY(   PMATH_SYMBOL_RIFFLE                           = NEW_SYSTEM_SYMBOL("Riffle"))
  VERIFY(   PMATH_SYMBOL_RIGHTTRIANGLE                    = NEW_SYSTEM_SYMBOL("RightTriangle"))
  VERIFY(   PMATH_SYMBOL_RIGHTTRIANGLEEQUAL               = NEW_SYSTEM_SYMBOL("RightTriangleEqual"))
  VERIFY(   PMATH_SYMBOL_ROTATE                           = NEW_SYSTEM_SYMBOL("Rotate"))
  VERIFY(   PMATH_SYMBOL_ROTATIONBOX                      = NEW_SYSTEM_SYMBOL("RotationBox"))
  VERIFY(   PMATH_SYMBOL_ROUND                            = NEW_SYSTEM_SYMBOL("Round"))
  VERIFY(   PMATH_SYMBOL_ROW                              = NEW_SYSTEM_SYMBOL("Row"))
  VERIFY(   PMATH_SYMBOL_ROWSPACING                       = NEW_SYSTEM_SYMBOL("RowSpacing"))
  VERIFY(   PMATH_SYMBOL_RULE                             = NEW_SYSTEM_SYMBOL("Rule"))
  VERIFY(   PMATH_SYMBOL_RULEDELAYED                      = NEW_SYSTEM_SYMBOL("RuleDelayed"))
  VERIFY(   PMATH_SYMBOL_SAMETEST                         = NEW_SYSTEM_SYMBOL("SameTest"))
  VERIFY(   PMATH_SYMBOL_SCAN                             = NEW_SYSTEM_SYMBOL("Scan"))
  VERIFY(   PMATH_SYMBOL_SCRIPTSIZEMULTIPLIERS            = NEW_SYSTEM_SYMBOL("ScriptSizeMultipliers"))
  VERIFY(   PMATH_SYMBOL_SEC                              = NEW_SYSTEM_SYMBOL("Sec"))
  VERIFY(   PMATH_SYMBOL_SECH                             = NEW_SYSTEM_SYMBOL("Sech"))
  VERIFY(   PMATH_SYMBOL_SECTION                          = NEW_SYSTEM_SYMBOL("Section"))
  VERIFY(   PMATH_SYMBOL_SECTIONEDITDUPLICATE             = NEW_SYSTEM_SYMBOL("SectionEditDuplicate"))
  VERIFY(   PMATH_SYMBOL_SECTIONEDITDUPLICATEMAKESCOPY    = NEW_SYSTEM_SYMBOL("SectionEditDuplicateMakesCopy"))
  VERIFY(   PMATH_SYMBOL_SECTIONFRAME                     = NEW_SYSTEM_SYMBOL("SectionFrame"))
  VERIFY(   PMATH_SYMBOL_SECTIONFRAMECOLOR                = NEW_SYSTEM_SYMBOL("SectionFrameColor"))
  VERIFY(   PMATH_SYMBOL_SECTIONFRAMEMARGINS              = NEW_SYSTEM_SYMBOL("SectionFrameMargins"))
  VERIFY(   PMATH_SYMBOL_SECTIONGENERATED                 = NEW_SYSTEM_SYMBOL("SectionGenerated"))
  VERIFY(   PMATH_SYMBOL_SECTIONGROUP                     = NEW_SYSTEM_SYMBOL("SectionGroup"))
  VERIFY(   PMATH_SYMBOL_SECTIONGROUPPRECEDENCE           = NEW_SYSTEM_SYMBOL("SectionGroupPrecedence"))
  VERIFY(   PMATH_SYMBOL_SECTIONMARGINS                   = NEW_SYSTEM_SYMBOL("SectionMargins"))
  VERIFY(   PMATH_SYMBOL_SECTIONLABEL                     = NEW_SYSTEM_SYMBOL("SectionLabel"))
  VERIFY(   PMATH_SYMBOL_SECTIONLABELAUTODELETE           = NEW_SYSTEM_SYMBOL("SectionLabelAutoDelete"))
  VERIFY(   PMATH_SYMBOL_SECTIONPRINT                     = NEW_SYSTEM_SYMBOL("SectionPrint"))
  VERIFY(   PMATH_SYMBOL_SELECT                           = NEW_SYSTEM_SYMBOL("Select"))
  VERIFY(   PMATH_SYMBOL_SELECTABLE                       = NEW_SYSTEM_SYMBOL("Selectable"))
  VERIFY(   PMATH_SYMBOL_SELECTEDDOCUMENT                 = NEW_SYSTEM_SYMBOL("SelectedDocument"))
  VERIFY(   PMATH_SYMBOL_SEQUENCE                         = NEW_SYSTEM_SYMBOL("Sequence"))
  VERIFY(   PMATH_SYMBOL_SEQUENCEHOLD                     = NEW_SYSTEM_SYMBOL("SequenceHold"))
  VERIFY(   PMATH_SYMBOL_SETATTRIBUTES                    = NEW_SYSTEM_SYMBOL("SetAttributes"))
  VERIFY(   PMATH_SYMBOL_SETDIRECTORY                     = NEW_SYSTEM_SYMBOL("SetDirectory"))
  VERIFY(   PMATH_SYMBOL_SETOPTIONS                       = NEW_SYSTEM_SYMBOL("SetOptions"))
  VERIFY(   PMATH_SYMBOL_SETPRECISION                     = NEW_SYSTEM_SYMBOL("SetPrecision"))
  VERIFY(   PMATH_SYMBOL_SETSTREAMPOSITION                = NEW_SYSTEM_SYMBOL("SetStreamPosition"))
  VERIFY(   PMATH_SYMBOL_SETTER                           = NEW_SYSTEM_SYMBOL("Setter"))
  VERIFY(   PMATH_SYMBOL_SETTERBAR                        = NEW_SYSTEM_SYMBOL("SetterBar"))
  VERIFY(   PMATH_SYMBOL_SETTERBOX                        = NEW_SYSTEM_SYMBOL("SetterBox"))
  VERIFY(   PMATH_SYMBOL_SETTING                          = NEW_SYSTEM_SYMBOL("Setting"))
  VERIFY(   PMATH_SYMBOL_SHALLOW                          = NEW_SYSTEM_SYMBOL("Shallow"))
  VERIFY(   PMATH_SYMBOL_SHORT                            = NEW_SYSTEM_SYMBOL("Short"))
  VERIFY(   PMATH_SYMBOL_SHORTEST                         = NEW_SYSTEM_SYMBOL("Shortest"))
  VERIFY(   PMATH_SYMBOL_SHOWAUTOSTYLES                   = NEW_SYSTEM_SYMBOL("ShowAutoStyles"))
  VERIFY(   PMATH_SYMBOL_SHOWDEFINITION                   = NEW_SYSTEM_SYMBOL("ShowDefinition"))
  VERIFY(   PMATH_SYMBOL_SHOWSECTIONBRACKET               = NEW_SYSTEM_SYMBOL("ShowSectionBracket"))
  VERIFY(   PMATH_SYMBOL_SHOWSTRINGCHARACTERS             = NEW_SYSTEM_SYMBOL("ShowStringCharacters"))
  VERIFY(   PMATH_SYMBOL_SIGN                             = NEW_SYSTEM_SYMBOL("Sign"))
  VERIFY(   PMATH_SYMBOL_SIN                              = NEW_SYSTEM_SYMBOL("Sin"))
  VERIFY(   PMATH_SYMBOL_SINH                             = NEW_SYSTEM_SYMBOL("Sinh"))
  VERIFY(   PMATH_SYMBOL_SINGLEMATCH                      = NEW_SYSTEM_SYMBOL("SingleMatch"))
  VERIFY(   PMATH_SYMBOL_SKELETON                         = NEW_SYSTEM_SYMBOL("Skeleton"))
  VERIFY(   PMATH_SYMBOL_SLIDER                           = NEW_SYSTEM_SYMBOL("Slider"))
  VERIFY(   PMATH_SYMBOL_SLIDERBOX                        = NEW_SYSTEM_SYMBOL("SliderBox"))
  VERIFY(   PMATH_SYMBOL_SORT                             = NEW_SYSTEM_SYMBOL("Sort"))
  VERIFY(   PMATH_SYMBOL_SORTBY                           = NEW_SYSTEM_SYMBOL("SortBy"))
  VERIFY(   PMATH_SYMBOL_SPECIAL                          = NEW_SYSTEM_SYMBOL("Special"))
  VERIFY(   PMATH_SYMBOL_SPLIT                            = NEW_SYSTEM_SYMBOL("Split"))
  VERIFY(   PMATH_SYMBOL_SQRT                             = NEW_SYSTEM_SYMBOL("Sqrt"))
  VERIFY(   PMATH_SYMBOL_SQRTBOX                          = NEW_SYSTEM_SYMBOL("SqrtBox"))
  VERIFY(   PMATH_SYMBOL_STACK                            = NEW_SYSTEM_SYMBOL("Stack"))
  VERIFY(   PMATH_SYMBOL_STANDARDFORM                     = NEW_SYSTEM_SYMBOL("StandardForm"))
  VERIFY(   PMATH_SYMBOL_STARTOFLINE                      = NEW_SYSTEM_SYMBOL("StartOfLine"))
  VERIFY(   PMATH_SYMBOL_STARTOFSTRING                    = NEW_SYSTEM_SYMBOL("StartOfString"))
  VERIFY(   PMATH_SYMBOL_STREAMPOSITION                   = NEW_SYSTEM_SYMBOL("StreamPosition"))
  VERIFY(   PMATH_SYMBOL_STRING                           = NEW_SYSTEM_SYMBOL("String"))
  VERIFY(   PMATH_SYMBOL_STRINGCASES                      = NEW_SYSTEM_SYMBOL("StringCases"))
  VERIFY(   PMATH_SYMBOL_STRINGCOUNT                      = NEW_SYSTEM_SYMBOL("StringCount"))
  VERIFY(   PMATH_SYMBOL_STRINGDROP                       = NEW_SYSTEM_SYMBOL("StringDrop"))
  VERIFY(   PMATH_SYMBOL_STRINGEXPRESSION                 = NEW_SYSTEM_SYMBOL("StringExpression"))
  VERIFY(   PMATH_SYMBOL_STRINGFORM                       = NEW_SYSTEM_SYMBOL("StringForm"))
  VERIFY(   PMATH_SYMBOL_STRINGMATCH                      = NEW_SYSTEM_SYMBOL("StringMatch"))
  VERIFY(   PMATH_SYMBOL_STRINGPOSITION                   = NEW_SYSTEM_SYMBOL("StringPosition"))
  VERIFY(   PMATH_SYMBOL_STRINGREPLACE                    = NEW_SYSTEM_SYMBOL("StringReplace"))
  VERIFY(   PMATH_SYMBOL_STRINGSPLIT                      = NEW_SYSTEM_SYMBOL("StringSplit"))
  VERIFY(   PMATH_SYMBOL_STRINGTAKE                       = NEW_SYSTEM_SYMBOL("StringTake"))
  VERIFY(   PMATH_SYMBOL_STRINGTOBOXES                    = NEW_SYSTEM_SYMBOL("StringToBoxes"))
  VERIFY(   PMATH_SYMBOL_STRIPONINPUT                     = NEW_SYSTEM_SYMBOL("StripOnInput"))
  VERIFY(   PMATH_SYMBOL_STYLE                            = NEW_SYSTEM_SYMBOL("Style"))
  VERIFY(   PMATH_SYMBOL_STYLEBOX                         = NEW_SYSTEM_SYMBOL("StyleBox"))
  VERIFY(   PMATH_SYMBOL_STYLEDATA                        = NEW_SYSTEM_SYMBOL("StyleData"))
  VERIFY(   PMATH_SYMBOL_STYLEDEFINITIONS                 = NEW_SYSTEM_SYMBOL("StyleDefinitions"))
  VERIFY(   PMATH_SYMBOL_SUBRULES                         = NEW_SYSTEM_SYMBOL("SubRules"))
  VERIFY(   PMATH_SYMBOL_SUBSCRIPT                        = NEW_SYSTEM_SYMBOL("Subscript"))
  VERIFY(   PMATH_SYMBOL_SUBSCRIPTBOX                     = NEW_SYSTEM_SYMBOL("SubscriptBox"))
  VERIFY(   PMATH_SYMBOL_SUBSET                           = NEW_SYSTEM_SYMBOL("Subset"))
  VERIFY(   PMATH_SYMBOL_SUBSETEQUAL                      = NEW_SYSTEM_SYMBOL("SubsetEqual"))
  VERIFY(   PMATH_SYMBOL_SUBSUPERSCRIPT                   = NEW_SYSTEM_SYMBOL("Subsuperscript"))
  VERIFY(   PMATH_SYMBOL_SUBSUPERSCRIPTBOX                = NEW_SYSTEM_SYMBOL("SubsuperscriptBox"))
  VERIFY(   PMATH_SYMBOL_SUCCEEDS                         = NEW_SYSTEM_SYMBOL("Succeeds"))
  VERIFY(   PMATH_SYMBOL_SUCCEEDSEQUAL                    = NEW_SYSTEM_SYMBOL("SucceedsEqual"))
  VERIFY(   PMATH_SYMBOL_SUCCEEDSTILDE                    = NEW_SYSTEM_SYMBOL("SucceedsTilde"))
  VERIFY(   PMATH_SYMBOL_SUM                              = NEW_SYSTEM_SYMBOL("Sum"))
  VERIFY(   PMATH_SYMBOL_SUPERSCRIPT                      = NEW_SYSTEM_SYMBOL("Superscript"))
  VERIFY(   PMATH_SYMBOL_SUPERSCRIPTBOX                   = NEW_SYSTEM_SYMBOL("SuperscriptBox"))
  VERIFY(   PMATH_SYMBOL_SUPERSET                         = NEW_SYSTEM_SYMBOL("Superset"))
  VERIFY(   PMATH_SYMBOL_SUPERSETEQUAL                    = NEW_SYSTEM_SYMBOL("SupersetEqual"))
  VERIFY(   PMATH_SYMBOL_SWITCH                           = NEW_SYSTEM_SYMBOL("Switch"))
  VERIFY(   PMATH_SYMBOL_SYMBOL                           = NEW_SYSTEM_SYMBOL("Symbol"))
  VERIFY(   PMATH_SYMBOL_SYMBOLNAME                       = NEW_SYSTEM_SYMBOL("SymbolName"))
  VERIFY(   PMATH_SYMBOL_SYMMETRIC                        = NEW_SYSTEM_SYMBOL("Symmetric"))
  VERIFY(   PMATH_SYMBOL_SYNCHRONIZE                      = NEW_SYSTEM_SYMBOL("Synchronize"))
  VERIFY(   PMATH_SYMBOL_SYNCHRONOUSUPDATING              = NEW_SYSTEM_SYMBOL("SynchronousUpdating"))
  VERIFY(   PMATH_SYMBOL_SYNTAX                           = NEW_SYSTEM_SYMBOL("Syntax"))
  VERIFY(   PMATH_SYMBOL_SYNTAXINFORMATION                = NEW_SYSTEM_SYMBOL("SyntaxInformation"))
  VERIFY(   PMATH_SYMBOL_SYSTEMCHARACTERENCODING          = NEW_SYSTEM_SYMBOL("$SystemCharacterEncoding"))
  VERIFY(   PMATH_SYMBOL_SYSTEMEXCEPTION                  = NEW_SYSTEM_SYMBOL("SystemException"))
  VERIFY(   PMATH_SYMBOL_SYSTEMID                         = NEW_SYSTEM_SYMBOL("$SystemId"))
  VERIFY(   PMATH_SYMBOL_TABLE                            = NEW_SYSTEM_SYMBOL("Table"))
  VERIFY(   PMATH_SYMBOL_TAGASSIGN                        = NEW_SYSTEM_SYMBOL("TagAssign"))
  VERIFY(   PMATH_SYMBOL_TAGASSIGNDELAYED                 = NEW_SYSTEM_SYMBOL("TagAssignDelayed"))
  VERIFY(   PMATH_SYMBOL_TAGBOX                           = NEW_SYSTEM_SYMBOL("TagBox"))
  VERIFY(   PMATH_SYMBOL_TAGUNASSIGN                      = NEW_SYSTEM_SYMBOL("TagUnassign"))
  VERIFY(   PMATH_SYMBOL_TAKE                             = NEW_SYSTEM_SYMBOL("Take"))
  VERIFY(   PMATH_SYMBOL_TAKEWHILE                        = NEW_SYSTEM_SYMBOL("TakeWhile"))
  VERIFY(   PMATH_SYMBOL_TAN                              = NEW_SYSTEM_SYMBOL("Tan"))
  VERIFY(   PMATH_SYMBOL_TANH                             = NEW_SYSTEM_SYMBOL("Tanh"))
  VERIFY(   PMATH_SYMBOL_THREADID                         = NEW_SYSTEM_SYMBOL("$ThreadId"))
  VERIFY(   PMATH_SYMBOL_THREADLOCAL                      = NEW_SYSTEM_SYMBOL("ThreadLocal"))
  VERIFY(   PMATH_SYMBOL_TEMPORARY                        = NEW_SYSTEM_SYMBOL("Temporary"))
  VERIFY(   PMATH_SYMBOL_TESTPATTERN                      = NEW_SYSTEM_SYMBOL("TestPattern"))
  VERIFY(   PMATH_SYMBOL_TEXTSHADOW                       = NEW_SYSTEM_SYMBOL("TextShadow"))
  VERIFY(   PMATH_SYMBOL_THREAD                           = NEW_SYSTEM_SYMBOL("Thread"))
  VERIFY(   PMATH_SYMBOL_THROUGH                          = NEW_SYSTEM_SYMBOL("Through"))
  VERIFY(   PMATH_SYMBOL_THROW                            = NEW_SYSTEM_SYMBOL("Throw"))
  VERIFY(   PMATH_SYMBOL_TICKS                            = NEW_SYSTEM_SYMBOL("Ticks"))
  VERIFY(   PMATH_SYMBOL_TIMECONSTRAINED                  = NEW_SYSTEM_SYMBOL("TimeConstrained"))
  VERIFY(   PMATH_SYMBOL_TIMES                            = NEW_SYSTEM_SYMBOL("Times"))
  VERIFY(   PMATH_SYMBOL_TIMESBY                          = NEW_SYSTEM_SYMBOL("TimesBy"))
  VERIFY(   PMATH_SYMBOL_TIMEZONEDEFAULT                  = NEW_SYSTEM_SYMBOL("$TimeZone"))
  VERIFY(   PMATH_SYMBOL_TIMEZONE                         = NEW_SYSTEM_SYMBOL("TimeZone"))
  VERIFY(   PMATH_SYMBOL_TIMING                           = NEW_SYSTEM_SYMBOL("Timing"))
  VERIFY(   PMATH_SYMBOL_TILDEEQUAL                       = NEW_SYSTEM_SYMBOL("TildeEqual"))
  VERIFY(   PMATH_SYMBOL_TILDEFULLEQUAL                   = NEW_SYSTEM_SYMBOL("TildeFullEqual"))
  VERIFY(   PMATH_SYMBOL_TILDETILDE                       = NEW_SYSTEM_SYMBOL("TildeTilde"))
  VERIFY(   PMATH_SYMBOL_TOBOXES                          = NEW_SYSTEM_SYMBOL("ToBoxes"))
  VERIFY(   PMATH_SYMBOL_TOCHARACTERCODE                  = NEW_SYSTEM_SYMBOL("ToCharacterCode"))
  VERIFY(   PMATH_SYMBOL_TOEXPRESSION                     = NEW_SYSTEM_SYMBOL("ToExpression"))
  VERIFY(   PMATH_SYMBOL_TOFILENAME                       = NEW_SYSTEM_SYMBOL("ToFileName"))
  VERIFY(   PMATH_SYMBOL_TOOLTIP                          = NEW_SYSTEM_SYMBOL("Tooltip"))
  VERIFY(   PMATH_SYMBOL_TOOLTIPBOX                       = NEW_SYSTEM_SYMBOL("TooltipBox"))
  VERIFY(   PMATH_SYMBOL_TOSTRING                         = NEW_SYSTEM_SYMBOL("ToString"))
  VERIFY(   PMATH_SYMBOL_TOTAL                            = NEW_SYSTEM_SYMBOL("Total"))
  VERIFY(   PMATH_SYMBOL_TRACKEDSYMBOLS                   = NEW_SYSTEM_SYMBOL("TrackedSymbols"))
  VERIFY(   PMATH_SYMBOL_TRANSFORMATIONBOX                = NEW_SYSTEM_SYMBOL("TransformationBox"))
  VERIFY(   PMATH_SYMBOL_TRANSPOSE                        = NEW_SYSTEM_SYMBOL("Transpose"))
  VERIFY(   PMATH_SYMBOL_TRUE                             = NEW_SYSTEM_SYMBOL("True"))
  VERIFY(   PMATH_SYMBOL_TRY                              = NEW_SYSTEM_SYMBOL("Try"))
  VERIFY(   PMATH_SYMBOL_UNASSIGN                         = NEW_SYSTEM_SYMBOL("Unassign"))
  VERIFY(   PMATH_SYMBOL_UNCOMPRESS                       = NEW_SYSTEM_SYMBOL("Uncompress"))
  VERIFY(   PMATH_SYMBOL_UNCOMPRESSSTREAM                 = NEW_SYSTEM_SYMBOL("UncompressStream"))
  VERIFY(   PMATH_SYMBOL_UNDEFINED                        = NEW_SYSTEM_SYMBOL("Undefined"))
  VERIFY(   PMATH_SYMBOL_UNDERFLOW                        = NEW_SYSTEM_SYMBOL("Underflow"))
  VERIFY(   PMATH_SYMBOL_UNDERSCRIPT                      = NEW_SYSTEM_SYMBOL("Underscript"))
  VERIFY(   PMATH_SYMBOL_UNDERSCRIPTBOX                   = NEW_SYSTEM_SYMBOL("UnderscriptBox"))
  VERIFY(   PMATH_SYMBOL_UNDEROVERSCRIPT                  = NEW_SYSTEM_SYMBOL("Underoverscript"))
  VERIFY(   PMATH_SYMBOL_UNDEROVERSCRIPTBOX               = NEW_SYSTEM_SYMBOL("UnderoverscriptBox"))
  VERIFY(   PMATH_SYMBOL_UNEQUAL                          = NEW_SYSTEM_SYMBOL("Unequal"))
  VERIFY(   PMATH_SYMBOL_UNEVALUATED                      = NEW_SYSTEM_SYMBOL("Unevaluated"))
  VERIFY(   PMATH_SYMBOL_UNIDENTICAL                      = NEW_SYSTEM_SYMBOL("Unidentical"))
  VERIFY(   PMATH_SYMBOL_UNION                            = NEW_SYSTEM_SYMBOL("Union"))
  VERIFY(   PMATH_SYMBOL_UNITVECTOR                       = NEW_SYSTEM_SYMBOL("UnitVector"))
  VERIFY(   PMATH_SYMBOL_UNPROTECT                        = NEW_SYSTEM_SYMBOL("Unprotect"))
  VERIFY(   PMATH_SYMBOL_UNSAVEDVARIABLES                 = NEW_SYSTEM_SYMBOL("UnsavedVariables"))
  VERIFY(   PMATH_SYMBOL_UPARROW                          = NEW_SYSTEM_SYMBOL("UpArrow"))
  VERIFY(   PMATH_SYMBOL_UPDATE                           = NEW_SYSTEM_SYMBOL("Update"))
  VERIFY(   PMATH_SYMBOL_UPDATEINTERVAL                   = NEW_SYSTEM_SYMBOL("UpdateInterval"))
  VERIFY(   PMATH_SYMBOL_UPDOWNARROW                      = NEW_SYSTEM_SYMBOL("UpDownArrow"))
  VERIFY(   PMATH_SYMBOL_UPPERLEFTARROW                   = NEW_SYSTEM_SYMBOL("UpperLeftArrow"))
  VERIFY(   PMATH_SYMBOL_UPPERRIGHTARROW                  = NEW_SYSTEM_SYMBOL("UpperRightArrow"))
  VERIFY(   PMATH_SYMBOL_UPRULES                          = NEW_SYSTEM_SYMBOL("UpRules"))
  VERIFY(   PMATH_SYMBOL_VERSIONLIST                      = NEW_SYSTEM_SYMBOL("$VersionList"))
  VERIFY(   PMATH_SYMBOL_VERSIONNUMBER                    = NEW_SYSTEM_SYMBOL("$VersionNumber"))
  VERIFY(   PMATH_SYMBOL_VISIBLE                          = NEW_SYSTEM_SYMBOL("Visible"))
  VERIFY(   PMATH_SYMBOL_WAIT                             = NEW_SYSTEM_SYMBOL("Wait"))
  VERIFY(   PMATH_SYMBOL_WHICH                            = NEW_SYSTEM_SYMBOL("Which"))
  VERIFY(   PMATH_SYMBOL_WHILE                            = NEW_SYSTEM_SYMBOL("While"))
  VERIFY(   PMATH_SYMBOL_WHITESPACE                       = NEW_SYSTEM_SYMBOL("Whitespace"))
  VERIFY(   PMATH_SYMBOL_WHITESPACECHARACTER              = NEW_SYSTEM_SYMBOL("WhitespaceCharacter"))
  VERIFY(   PMATH_SYMBOL_WINDOWFRAME                      = NEW_SYSTEM_SYMBOL("WindowFrame"))
  VERIFY(   PMATH_SYMBOL_WINDOWTITLE                      = NEW_SYSTEM_SYMBOL("WindowTitle"))
  VERIFY(   PMATH_SYMBOL_WITH                             = NEW_SYSTEM_SYMBOL("With"))
  VERIFY(   PMATH_SYMBOL_WORD                             = NEW_SYSTEM_SYMBOL("Word"))
  VERIFY(   PMATH_SYMBOL_WORDBOUNDARY                     = NEW_SYSTEM_SYMBOL("WordBoundary"))
  VERIFY(   PMATH_SYMBOL_WORDCHARACTER                    = NEW_SYSTEM_SYMBOL("WordCharacter"))
  VERIFY(   PMATH_SYMBOL_WORKINGPRECISION                 = NEW_SYSTEM_SYMBOL("WorkingPrecision"))
  VERIFY(   PMATH_SYMBOL_WRITE                            = NEW_SYSTEM_SYMBOL("Write"))
  VERIFY(   PMATH_SYMBOL_WRITESTRING                      = NEW_SYSTEM_SYMBOL("WriteString"))
  VERIFY(   PMATH_SYMBOL_XOR                              = NEW_SYSTEM_SYMBOL("Xor"))
  VERIFY(   PMATH_SYMBOL_ZETA                             = NEW_SYSTEM_SYMBOL("Zeta"))
  //} ... setting symbol names
  
#ifdef PMATH_DEBUG_LOG
  for(i = 0; i < PMATH_BUILTIN_SYMBOL_COUNT; i++) {
    if(pmath_is_null(_pmath_builtin_symbol_array[i]))
      pmath_debug_print("BUILTIN SYMBOL #%d NOT USED\n", i);
  }
#endif
  
  //{ binding C functions ...
#define BIND(sym, func, use)  if(!pmath_register_code((sym), (func), (use))) goto FAIL;
#define BIND_APPROX(sym, func) if(!pmath_register_approx_code((sym), (func))) goto FAIL;
#define BIND_EARLY(sym, func)  do{BIND((sym), (func), PMATH_CODE_USAGE_EARLYCALL) BIND_DOWN(sym, func)}while(0);
#define BIND_DOWN(sym, func)   BIND((sym), (func), PMATH_CODE_USAGE_DOWNCALL)
#define BIND_SUB(sym, func)    BIND((sym), (func), PMATH_CODE_USAGE_SUBCALL)
#define BIND_UP(sym, func)     BIND((sym), (func), PMATH_CODE_USAGE_UPCALL)
  
  BIND_EARLY(  PMATH_SYMBOL_PLUS,                             builtin_plus)
  BIND_EARLY(  PMATH_SYMBOL_POWER,                            builtin_power)
  BIND_EARLY(  PMATH_SYMBOL_TIMES,                            builtin_times)
  
  BIND_SUB(    PMATH_SYMBOL_FUNCTION,                         builtin_call_function)
  BIND_SUB(    PMATH_SYMBOL_ISHELD,                           builtin_call_isheld)
  BIND_SUB(    PMATH_SYMBOL_LINEARSOLVEFUNCTION,              builtin_call_linearsolvefunction)
  
  BIND_UP(     PMATH_SYMBOL_NRULES,                           builtin_assign_symbol_rules)
  BIND_UP(     PMATH_SYMBOL_ATTRIBUTES,                       builtin_assign_attributes)
  BIND_UP(     PMATH_SYMBOL_CURRENTNAMESPACE,                 builtin_assign_namespace)
  BIND_UP(     PMATH_SYMBOL_DEFAULT,                          builtin_assign_default)
  BIND_UP(     PMATH_SYMBOL_DEFAULTRULES,                     builtin_assign_symbol_rules)
  BIND_UP(     PMATH_SYMBOL_DOWNRULES,                        builtin_assign_symbol_rules)
  BIND_UP(     PMATH_SYMBOL_ENVIRONMENT,                      builtin_assign_environment)
  BIND_UP(     PMATH_SYMBOL_FORMAT,                           builtin_assign_makeboxes_or_format)
  BIND_UP(     PMATH_SYMBOL_FORMATRULES,                      builtin_assign_symbol_rules)
  BIND_UP(     PMATH_SYMBOL_INTERNAL_NAMESPACESTACK,          builtin_assign_namespacepath)
  BIND_UP(     PMATH_SYMBOL_ISNUMERIC,                        builtin_assign_isnumeric)
  BIND_UP(     PMATH_SYMBOL_LIST,                             builtin_assign_list)
  BIND_UP(     PMATH_SYMBOL_MAKEBOXES,                        builtin_assign_makeboxes_or_format)
  BIND_UP(     PMATH_SYMBOL_MAXEXTRAPRECISION,                builtin_assign_maxextraprecision)
  BIND_UP(     PMATH_SYMBOL_MESSAGENAME,                      builtin_assign_messagename)
  BIND_UP(     PMATH_SYMBOL_MESSAGES,                         builtin_assign_messages)
  BIND_UP(     PMATH_SYMBOL_NAMESPACEPATH,                    builtin_assign_namespacepath)
  BIND_UP(     PMATH_SYMBOL_OPTIONS,                          builtin_assign_options)
  BIND_UP(     PMATH_SYMBOL_OWNRULES,                         builtin_assign_ownrules)
  BIND_UP(     PMATH_SYMBOL_PART,                             builtin_assign_part)
  BIND_UP(     PMATH_SYMBOL_SETPRECISION,                     builtin_assign_setprecision)
  BIND_UP(     PMATH_SYMBOL_SUBRULES,                         builtin_assign_symbol_rules)
  BIND_UP(     PMATH_SYMBOL_SYNTAXINFORMATION,                builtin_assign_syntaxinformation)
  BIND_UP(     PMATH_SYMBOL_UPRULES,                          builtin_assign_symbol_rules)
  
  BIND_UP(     PMATH_SYMBOL_CONDITIONALEXPRESSION,            builtin_operate_conditionalexpression)
  BIND_UP(     PMATH_SYMBOL_UNDEFINED,                        builtin_operate_undefined)
  
  BIND_DOWN(   PMATH_SYMBOL_INTERNAL_ABORTMESSAGE,            builtin_internal_abortmessage)
  BIND_DOWN(   PMATH_SYMBOL_INTERNAL_COPYSIGN,                builtin_internal_copysign)
  BIND_DOWN(   PMATH_SYMBOL_INTERNAL_CRITICALMESSAGETAG,      builtin_criticalmessagetag)
  BIND_DOWN(   PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATE,         builtin_internal_dynamicevaluate)
  BIND_DOWN(   PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATEMULTIPLE, builtin_internal_dynamicevaluatemultiple)
  BIND_DOWN(   PMATH_SYMBOL_INTERNAL_DYNAMICREMOVE,           builtin_internal_dynamicremove)
  BIND_DOWN(   PMATH_SYMBOL_INTERNAL_GETTHREADID,             builtin_getthreadid)
  BIND_DOWN(   PMATH_SYMBOL_INTERNAL_NEXTTOWARD,              builtin_internal_nexttoward)
  BIND_DOWN(   PMATH_SYMBOL_INTERNAL_REALBALLBOUNDS,          builtin_internal_realballbounds)
  BIND_DOWN(   PMATH_SYMBOL_INTERNAL_SIGNBIT,                 builtin_internal_signbit)
  BIND_DOWN(   PMATH_SYMBOL_INTERNAL_THREADIDLE,              builtin_internal_threadidle)
  
  BIND_DOWN(   PMATH_SYMBOL_DEVELOPER_FILEINFORMATION,     builtin_developer_fileinformation)
  BIND_DOWN(   PMATH_SYMBOL_DEVELOPER_FROMPACKEDARRAY,     builtin_developer_frompackedarray)
  BIND_DOWN(   PMATH_SYMBOL_DEVELOPER_GETDEBUGINFO,        builtin_developer_getdebuginfo)
  BIND_DOWN(   PMATH_SYMBOL_DEVELOPER_HASBUILTINCODE,      builtin_developer_hasbuiltincode)
  BIND_DOWN(   PMATH_SYMBOL_DEVELOPER_ISPACKEDARRAY,       builtin_developer_ispackedarray)
  BIND_DOWN(   PMATH_SYMBOL_DEVELOPER_SETDEBUGINFOAT,      builtin_developer_setdebuginfoat)
  BIND_DOWN(   PMATH_SYMBOL_DEVELOPER_TOPACKEDARRAY,       builtin_developer_topackedarray)
  
  BIND_DOWN(   PMATH_SYMBOL_ABORT,                       builtin_abort)
  BIND_DOWN(   PMATH_SYMBOL_ABS,                         builtin_abs)
  BIND_DOWN(   PMATH_SYMBOL_AND,                         builtin_and)
  BIND_DOWN(   PMATH_SYMBOL_APPLY,                       builtin_apply)
  BIND_DOWN(   PMATH_SYMBOL_APPEND,                      builtin_append)
  BIND_DOWN(   PMATH_SYMBOL_ARCSIN,                      builtin_arcsin)
  BIND_DOWN(   PMATH_SYMBOL_ARCTAN,                      builtin_arctan)
  BIND_DOWN(   PMATH_SYMBOL_ARG,                         builtin_arg)
  BIND_DOWN(   PMATH_SYMBOL_ARRAY,                       builtin_array)
  BIND_DOWN(   PMATH_SYMBOL_ASSIGN,                      builtin_assign)
  BIND_DOWN(   PMATH_SYMBOL_ASSIGNDELAYED,               builtin_assign)
  BIND_DOWN(   PMATH_SYMBOL_ATTRIBUTES,                  builtin_attributes)
  BIND_DOWN(   PMATH_SYMBOL_BASEFORM,                    builtin_baseform)
  BIND_DOWN(   PMATH_SYMBOL_BEGIN,                       builtin_begin)
  BIND_DOWN(   PMATH_SYMBOL_BEGINPACKAGE,                builtin_beginpackage)
  BIND_DOWN(   PMATH_SYMBOL_BINARYREAD,                  builtin_binaryread)
  BIND_DOWN(   PMATH_SYMBOL_BINARYREADLIST,              builtin_binaryreadlist)
  BIND_DOWN(   PMATH_SYMBOL_BINARYWRITE,                 builtin_binarywrite)
  BIND_DOWN(   PMATH_SYMBOL_BINOMIAL,                    builtin_binomial)
  BIND_DOWN(   PMATH_SYMBOL_BITAND,                      builtin_bitand)
  BIND_DOWN(   PMATH_SYMBOL_BITCLEAR,                    builtin_bitclear)
  BIND_DOWN(   PMATH_SYMBOL_BITGET,                      builtin_bitget)
  BIND_DOWN(   PMATH_SYMBOL_BITLENGTH,                   builtin_bitlength)
  BIND_DOWN(   PMATH_SYMBOL_BITNOT,                      builtin_bitnot)
  BIND_DOWN(   PMATH_SYMBOL_BITOR,                       builtin_bitor)
  BIND_DOWN(   PMATH_SYMBOL_BITSET,                      builtin_bitset)
  BIND_DOWN(   PMATH_SYMBOL_BITSHIFTLEFT,                builtin_bitshiftleft)
  BIND_DOWN(   PMATH_SYMBOL_BITSHIFTRIGHT,               builtin_bitshiftright)
  BIND_DOWN(   PMATH_SYMBOL_BITXOR,                      builtin_bitxor)
  BIND_DOWN(   PMATH_SYMBOL_BLOCK,                       builtin_block)
  BIND_DOWN(   PMATH_SYMBOL_BOOLE,                       builtin_boole)
  BIND_DOWN(   PMATH_SYMBOL_BREAK,                       general_builtin_zeroonearg)
  BIND_DOWN(   PMATH_SYMBOL_BUTTON,                      builtin_button)
  BIND_DOWN(   PMATH_SYMBOL_BYTECOUNT,                   builtin_bytecount)
  BIND_DOWN(   PMATH_SYMBOL_CASES,                       builtin_cases)
  BIND_DOWN(   PMATH_SYMBOL_CATCH,                       builtin_catch)
  BIND_DOWN(   PMATH_SYMBOL_CEILING,                     builtin_round_functions)
  BIND_DOWN(   PMATH_SYMBOL_CHARACTERS,                  builtin_characters)
  BIND_DOWN(   PMATH_SYMBOL_CHOP,                        builtin_chop)
  BIND_DOWN(   PMATH_SYMBOL_CLEAR,                       builtin_clear)
  BIND_DOWN(   PMATH_SYMBOL_CLEARALL,                    builtin_clear)
  BIND_DOWN(   PMATH_SYMBOL_CLEARATTRIBUTES,             builtin_clearattributes)
  BIND_DOWN(   PMATH_SYMBOL_CLOCK,                       builtin_clock)
  BIND_DOWN(   PMATH_SYMBOL_CLOSE,                       builtin_close)
  BIND_DOWN(   PMATH_SYMBOL_COMPLEMENT,                  builtin_complement)
  BIND_DOWN(   PMATH_SYMBOL_COMPLEX,                     builtin_complex)
  BIND_DOWN(   PMATH_SYMBOL_COMPRESS,                    builtin_compress)
  BIND_DOWN(   PMATH_SYMBOL_COMPRESSSTREAM,              builtin_compressstream)
  BIND_DOWN(   PMATH_SYMBOL_CONDITIONALEXPRESSION,       builtin_conditionalexpression)
  BIND_DOWN(   PMATH_SYMBOL_CONJUGATE,                   builtin_conjugate)
  BIND_DOWN(   PMATH_SYMBOL_CONSTANTARRAY,               builtin_constantarray)
  BIND_DOWN(   PMATH_SYMBOL_CONTINUE,                    general_builtin_zeroonearg)
  BIND_DOWN(   PMATH_SYMBOL_COPYDIRECTORY,               builtin_copydirectory_and_copyfile)
  BIND_DOWN(   PMATH_SYMBOL_COPYFILE,                    builtin_copydirectory_and_copyfile)
  BIND_DOWN(   PMATH_SYMBOL_COS,                         builtin_cos)
  BIND_DOWN(   PMATH_SYMBOL_COSH,                        builtin_cosh)
  BIND_DOWN(   PMATH_SYMBOL_COUNT,                       builtin_count)
  BIND_DOWN(   PMATH_SYMBOL_CREATEDIRECTORY,             builtin_createdirectory)
  BIND_DOWN(   PMATH_SYMBOL_CREATEDOCUMENT,              general_builtin_nofront)
  BIND_DOWN(   PMATH_SYMBOL_CURRENTVALUE,                general_builtin_nofront)
  BIND_DOWN(   PMATH_SYMBOL_DATELIST,                    builtin_datelist)
  BIND_DOWN(   PMATH_SYMBOL_DECREMENT,                   builtin_dec_or_inc_or_postdec_or_postinc)
  BIND_DOWN(   PMATH_SYMBOL_DEFAULT,                     builtin_default)
  BIND_DOWN(   PMATH_SYMBOL_DEFAULTRULES,                builtin_symbol_rules)
  BIND_DOWN(   PMATH_SYMBOL_DELETEDIRECTORY,             builtin_deletedirectory_and_deletefile)
  BIND_DOWN(   PMATH_SYMBOL_DELETEFILE,                  builtin_deletedirectory_and_deletefile)
  BIND_DOWN(   PMATH_SYMBOL_DEPTH,                       builtin_depth)
  BIND_DOWN(   PMATH_SYMBOL_DET,                         builtin_det)
  BIND_DOWN(   PMATH_SYMBOL_DIAGONALMATRIX,              builtin_diagonalmatrix)
  BIND_DOWN(   PMATH_SYMBOL_DIALOG,                      general_builtin_nofront)
  BIND_DOWN(   PMATH_SYMBOL_DIMENSIONS,                  builtin_dimensions)
  BIND_DOWN(   PMATH_SYMBOL_DIRECTEDINFINITY,            builtin_directedinfinity)
  BIND_DOWN(   PMATH_SYMBOL_DIRECTORY,                   builtin_directory)
  BIND_DOWN(   PMATH_SYMBOL_DIRECTORYNAME,               builtin_directoryname)
  BIND_DOWN(   PMATH_SYMBOL_DIVIDEBY,                    builtin_divideby_or_timesby)
  BIND_DOWN(   PMATH_SYMBOL_DO,                          builtin_do)
  BIND_DOWN(   PMATH_SYMBOL_DOCUMENTAPPLY,               general_builtin_nofront);
  BIND_DOWN(   PMATH_SYMBOL_DOCUMENTDELETE,              general_builtin_nofront);
  BIND_DOWN(   PMATH_SYMBOL_DOCUMENTGET,                 general_builtin_nofront);
  BIND_DOWN(   PMATH_SYMBOL_DOCUMENTREAD,                general_builtin_nofront);
  BIND_DOWN(   PMATH_SYMBOL_DOCUMENTWRITE,               general_builtin_nofront);
  BIND_DOWN(   PMATH_SYMBOL_DOCUMENTS,                   general_builtin_nofront);
  BIND_DOWN(   PMATH_SYMBOL_DOCUMENTSAVE,                general_builtin_nofront);
  BIND_DOWN(   PMATH_SYMBOL_DOT,                         builtin_dot)
  BIND_DOWN(   PMATH_SYMBOL_DOWNRULES,                   builtin_symbol_rules)
  BIND_DOWN(   PMATH_SYMBOL_DROP,                        builtin_drop)
  BIND_DOWN(   PMATH_SYMBOL_EMIT,                        builtin_emit)
  BIND_DOWN(   PMATH_SYMBOL_END,                         builtin_end)
  BIND_DOWN(   PMATH_SYMBOL_ENDPACKAGE,                  builtin_endpackage)
  BIND_DOWN(   PMATH_SYMBOL_ENVIRONMENT,                 builtin_environment)
  BIND_DOWN(   PMATH_SYMBOL_EQUAL,                       builtin_equal)
  BIND_DOWN(   PMATH_SYMBOL_EVALUATE,                    builtin_evaluate)
  BIND_DOWN(   PMATH_SYMBOL_EVALUATEDELAYED,             builtin_evaluatedelayed)
  BIND_DOWN(   PMATH_SYMBOL_EVALUATIONDOCUMENT,          general_builtin_nofront)
  BIND_DOWN(   PMATH_SYMBOL_EVALUATIONSEQUENCE,          builtin_evaluationsequence)
  BIND_DOWN(   PMATH_SYMBOL_EXP,                         builtin_exp)
  BIND_DOWN(   PMATH_SYMBOL_EXPAND,                      builtin_expand)
  BIND_DOWN(   PMATH_SYMBOL_EXPANDALL,                   builtin_expandall)
  BIND_DOWN(   PMATH_SYMBOL_EXTENDEDGCD,                 builtin_extendedgcd)
  BIND_DOWN(   PMATH_SYMBOL_EXTRACT,                     builtin_extract)
  BIND_DOWN(   PMATH_SYMBOL_FACTORIAL,                   builtin_factorial)
  BIND_DOWN(   PMATH_SYMBOL_FACTORIAL2,                  builtin_factorial2)
  BIND_DOWN(   PMATH_SYMBOL_FILEBYTECOUNT,               builtin_filebytecount)
  BIND_DOWN(   PMATH_SYMBOL_FILENAMES,                   builtin_filenames)
  BIND_DOWN(   PMATH_SYMBOL_FILETYPE,                    builtin_filetype)
  BIND_DOWN(   PMATH_SYMBOL_FILTERRULES,                 builtin_filterrules)
  BIND_DOWN(   PMATH_SYMBOL_FINALLY,                     builtin_finally)
  BIND_DOWN(   PMATH_SYMBOL_FIND,                        builtin_find)
  BIND_DOWN(   PMATH_SYMBOL_FINDLIST,                    builtin_findlist)
  BIND_DOWN(   PMATH_SYMBOL_FIRST,                       builtin_first)
  BIND_DOWN(   PMATH_SYMBOL_FIXEDPOINT,                  builtin_fixedpoint_and_fixedpointlist)
  BIND_DOWN(   PMATH_SYMBOL_FIXEDPOINTLIST,              builtin_fixedpoint_and_fixedpointlist)
  BIND_DOWN(   PMATH_SYMBOL_FLATTEN,                     builtin_flatten)
  BIND_DOWN(   PMATH_SYMBOL_FLOOR,                       builtin_round_functions)
  BIND_DOWN(   PMATH_SYMBOL_FOLD,                        builtin_fold)
  BIND_DOWN(   PMATH_SYMBOL_FOLDLIST,                    builtin_foldlist)
  BIND_DOWN(   PMATH_SYMBOL_FOR,                         builtin_for)
  BIND_DOWN(   PMATH_SYMBOL_FORMATRULES,                 builtin_symbol_rules)
  BIND_DOWN(   PMATH_SYMBOL_FRACTIONALPART,              builtin_fractionalpart)
  BIND_DOWN(   PMATH_SYMBOL_FROMCHARACTERCODE,           builtin_fromcharactercode)
  BIND_DOWN(   PMATH_SYMBOL_FRONTENDTOKENEXECUTE,        general_builtin_nofront)
  BIND_DOWN(   PMATH_SYMBOL_FUNCTION,                    builtin_function)
  BIND_DOWN(   PMATH_SYMBOL_GAMMA,                       builtin_gamma)
  BIND_DOWN(   PMATH_SYMBOL_GATHER,                      builtin_gather)
  BIND_DOWN(   PMATH_SYMBOL_GCD,                         builtin_gcd)
  BIND_DOWN(   PMATH_SYMBOL_GET,                         builtin_get)
  BIND_DOWN(   PMATH_SYMBOL_GOTO,                        general_builtin_onearg)
  BIND_DOWN(   PMATH_SYMBOL_GREATER,                     builtin_greater)
  BIND_DOWN(   PMATH_SYMBOL_GREATEREQUAL,                builtin_greaterequal)
  BIND_DOWN(   PMATH_SYMBOL_HASH,                        builtin_hash)
  BIND_DOWN(   PMATH_SYMBOL_HEAD,                        builtin_head)
  BIND_DOWN(   PMATH_SYMBOL_HISTORY,                     builtin_history)
  BIND_DOWN(   PMATH_SYMBOL_IDENTICAL,                   builtin_identical)
  BIND_DOWN(   PMATH_SYMBOL_IDENTITY,                    builtin_evaluate)
  BIND_DOWN(   PMATH_SYMBOL_IDENTITYMATRIX,              builtin_identitymatrix)
  BIND_DOWN(   PMATH_SYMBOL_IF,                          builtin_if)
  BIND_DOWN(   PMATH_SYMBOL_IM,                          builtin_im)
  BIND_DOWN(   PMATH_SYMBOL_INCREMENT,                   builtin_dec_or_inc_or_postdec_or_postinc)
  BIND_DOWN(   PMATH_SYMBOL_INEQUATION,                  builtin_inequation)
  BIND_DOWN(   PMATH_SYMBOL_INNER,                       builtin_inner)
  BIND_DOWN(   PMATH_SYMBOL_INTEGERPART,                 builtin_round_functions)
  BIND_DOWN(   PMATH_SYMBOL_INTERRUPT,                   general_builtin_nofront)
  BIND_DOWN(   PMATH_SYMBOL_INTERSECTION,                builtin_intersection)
  BIND_DOWN(   PMATH_SYMBOL_ISARRAY,                     builtin_isarray)
  BIND_DOWN(   PMATH_SYMBOL_ISATOM,                      builtin_isatom)
  BIND_DOWN(   PMATH_SYMBOL_ISCOMPLEX,                   builtin_iscomplex)
  BIND_DOWN(   PMATH_SYMBOL_ISEVEN,                      builtin_iseven)
  BIND_DOWN(   PMATH_SYMBOL_ISEXACTNUMBER,               builtin_isexactnumber)
  BIND_DOWN(   PMATH_SYMBOL_ISFLOAT,                     builtin_isfloat)
  BIND_DOWN(   PMATH_SYMBOL_ISFREEOF,                    builtin_isfreeof)
  BIND_DOWN(   PMATH_SYMBOL_ISHELD,                      builtin_isheld)
  BIND_DOWN(   PMATH_SYMBOL_ISIMAGINARY,                 builtin_isimaginary)
  BIND_DOWN(   PMATH_SYMBOL_ISINEXACTNUMBER,             builtin_isinexactnumber)
  BIND_DOWN(   PMATH_SYMBOL_ISINTEGER,                   builtin_isinteger)
  BIND_DOWN(   PMATH_SYMBOL_ISMACHINENUMBER,             builtin_ismachinenumber)
  BIND_DOWN(   PMATH_SYMBOL_ISMATRIX,                    builtin_ismatrix)
  BIND_DOWN(   PMATH_SYMBOL_ISNEGATIVE,                  builtin_ispos_or_isneg)
  BIND_DOWN(   PMATH_SYMBOL_ISNONNEGATIVE,               builtin_ispos_or_isneg)
  BIND_DOWN(   PMATH_SYMBOL_ISNONPOSITIVE,               builtin_ispos_or_isneg)
  BIND_DOWN(   PMATH_SYMBOL_ISNUMBER,                    builtin_isnumber)
  BIND_DOWN(   PMATH_SYMBOL_ISNUMERIC,                   builtin_isnumeric)
  BIND_DOWN(   PMATH_SYMBOL_ISODD,                       builtin_isodd)
  BIND_DOWN(   PMATH_SYMBOL_ISOPTION,                    builtin_isoption)
  BIND_DOWN(   PMATH_SYMBOL_ISORDERED,                   builtin_isordered)
  BIND_DOWN(   PMATH_SYMBOL_ISPOSITIVE,                  builtin_ispos_or_isneg)
  BIND_DOWN(   PMATH_SYMBOL_ISPRIME,                     builtin_isprime)
  BIND_DOWN(   PMATH_SYMBOL_ISQUOTIENT,                  builtin_isquotient)
  BIND_DOWN(   PMATH_SYMBOL_ISRATIONAL,                  builtin_isrational)
  BIND_DOWN(   PMATH_SYMBOL_ISREAL,                      builtin_isreal)
  BIND_DOWN(   PMATH_SYMBOL_ISSTRING,                    builtin_isstring)
  BIND_DOWN(   PMATH_SYMBOL_ISSYMBOL,                    builtin_issymbol)
  BIND_DOWN(   PMATH_SYMBOL_ISVALIDARGUMENTCOUNT,        builtin_isvalidargumentcount)
  BIND_DOWN(   PMATH_SYMBOL_ISVECTOR,                    builtin_isvector)
  BIND_DOWN(   PMATH_SYMBOL_JACOBISYMBOL,                builtin_jacobisymbol_and_kroneckersymbol)
  BIND_DOWN(   PMATH_SYMBOL_JOIN,                        builtin_join)
  BIND_DOWN(   PMATH_SYMBOL_KRONECKERSYMBOL,             builtin_jacobisymbol_and_kroneckersymbol)
  BIND_DOWN(   PMATH_SYMBOL_LABEL,                       general_builtin_onearg)
  BIND_DOWN(   PMATH_SYMBOL_LAST,                        builtin_last)
  BIND_DOWN(   PMATH_SYMBOL_LCM,                         builtin_lcm)
  BIND_DOWN(   PMATH_SYMBOL_LEAFCOUNT,                   builtin_leafcount)
  BIND_DOWN(   PMATH_SYMBOL_LENGTH,                      builtin_length)
  BIND_DOWN(   PMATH_SYMBOL_LENGTHWHILE,                 builtin_lengthwhile)
  BIND_DOWN(   PMATH_SYMBOL_LESS,                        builtin_less)
  BIND_DOWN(   PMATH_SYMBOL_LESSEQUAL,                   builtin_lessequal)
  BIND_DOWN(   PMATH_SYMBOL_LEVEL,                       builtin_level)
  BIND_DOWN(   PMATH_SYMBOL_LINEARSOLVE,                 builtin_linearsolve)
  BIND_DOWN(   PMATH_SYMBOL_LISTCONVOLVE,                builtin_listconvolve)
  BIND_DOWN(   PMATH_SYMBOL_LOADLIBRARY,                 builtin_loadlibrary)
  BIND_DOWN(   PMATH_SYMBOL_LOCAL,                       builtin_local)
  BIND_DOWN(   PMATH_SYMBOL_LOG,                         builtin_log)
  BIND_DOWN(   PMATH_SYMBOL_LOGGAMMA,                    builtin_loggamma)
  BIND_DOWN(   PMATH_SYMBOL_LUDECOMPOSITION,             builtin_ludecomposition)
  BIND_DOWN(   PMATH_SYMBOL_MAP,                         builtin_map)
  BIND_DOWN(   PMATH_SYMBOL_MAPINDEXED,                  builtin_mapindexed)
  BIND_DOWN(   PMATH_SYMBOL_MAPTHREAD,                   builtin_mapthread)
  BIND_DOWN(   PMATH_SYMBOL_MAKEBOXES,                   builtin_makeboxes)
  BIND_DOWN(   PMATH_SYMBOL_MAKEEXPRESSION,              builtin_makeexpression)
  BIND_DOWN(   PMATH_SYMBOL_MATCH,                       builtin_match)
  BIND_DOWN(   PMATH_SYMBOL_MAX,                         builtin_max)
  BIND_DOWN(   PMATH_SYMBOL_MEAN,                        builtin_mean)
  BIND_DOWN(   PMATH_SYMBOL_MEMORYUSAGE,                 builtin_memoryusage)
  BIND_DOWN(   PMATH_SYMBOL_MESSAGE,                     builtin_message)
  BIND_DOWN(   PMATH_SYMBOL_MESSAGECOUNT,                builtin_messagecount)
  BIND_DOWN(   PMATH_SYMBOL_MESSAGENAME,                 builtin_messagename)
  BIND_DOWN(   PMATH_SYMBOL_MESSAGES,                    builtin_messages)
  BIND_DOWN(   PMATH_SYMBOL_MIN,                         builtin_min)
  BIND_DOWN(   PMATH_SYMBOL_MINMAX,                      builtin_minmax)
  BIND_DOWN(   PMATH_SYMBOL_MOD,                         builtin_mod)
  BIND_DOWN(   PMATH_SYMBOL_MOST,                        builtin_most)
  BIND_DOWN(   PMATH_SYMBOL_N,                           builtin_approximate)
  BIND_DOWN(   PMATH_SYMBOL_NAMES,                       builtin_names)
  BIND_DOWN(   PMATH_SYMBOL_NAMESPACE,                   builtin_namespace)
  BIND_DOWN(   PMATH_SYMBOL_NEST,                        builtin_nest)
  BIND_DOWN(   PMATH_SYMBOL_NESTLIST,                    builtin_nestlist)
  BIND_DOWN(   PMATH_SYMBOL_NESTWHILE,                   builtin_nestwhile_and_nestwhilelist)
  BIND_DOWN(   PMATH_SYMBOL_NESTWHILELIST,               builtin_nestwhile_and_nestwhilelist)
  BIND_DOWN(   PMATH_SYMBOL_NEWTASK,                     builtin_newtask)
  BIND_DOWN(   PMATH_SYMBOL_NEXTPRIME,                   builtin_nextprime)
  BIND_DOWN(   PMATH_SYMBOL_NOT,                         builtin_not)
  BIND_DOWN(   PMATH_SYMBOL_NORM,                        builtin_norm)
  BIND_DOWN(   PMATH_SYMBOL_NRULES,                      builtin_symbol_rules)
  BIND_DOWN(   PMATH_SYMBOL_OFF,                         builtin_on_or_off)
  BIND_DOWN(   PMATH_SYMBOL_ON,                          builtin_on_or_off)
  BIND_DOWN(   PMATH_SYMBOL_OR,                          builtin_or)
  BIND_DOWN(   PMATH_SYMBOL_ORDERING,                    builtin_ordering)
  BIND_DOWN(   PMATH_SYMBOL_OPENAPPEND,                  builtin_open)
  BIND_DOWN(   PMATH_SYMBOL_OPENREAD,                    builtin_open)
  BIND_DOWN(   PMATH_SYMBOL_OPENWRITE,                   builtin_open)
  BIND_DOWN(   PMATH_SYMBOL_OPERATE,                     builtin_operate)
  BIND_DOWN(   PMATH_SYMBOL_OPTIONSPATTERN,              general_builtin_zeroonearg)
  BIND_DOWN(   PMATH_SYMBOL_OPTIONS,                     builtin_options)
  BIND_DOWN(   PMATH_SYMBOL_OPTIONVALUE,                 builtin_optionvalue)
  BIND_DOWN(   PMATH_SYMBOL_OVERFLOW,                    general_builtin_zeroargs)
  BIND_DOWN(   PMATH_SYMBOL_OWNRULES,                    builtin_ownrules)
  BIND_DOWN(   PMATH_SYMBOL_PADLEFT,                     builtin_padleft_and_padright)
  BIND_DOWN(   PMATH_SYMBOL_PADRIGHT,                    builtin_padleft_and_padright)
  BIND_DOWN(   PMATH_SYMBOL_PARALLELMAP,                 builtin_parallelmap)
  BIND_DOWN(   PMATH_SYMBOL_PARALLELMAPINDEXED,          builtin_parallelmapindexed)
  BIND_DOWN(   PMATH_SYMBOL_PARALLELSCAN,                builtin_parallelscan)
  BIND_DOWN(   PMATH_SYMBOL_PARALLELTRY,                 builtin_paralleltry)
  BIND_DOWN(   PMATH_SYMBOL_PARENTDIRECTORY,             builtin_parentdirectory)
  BIND_DOWN(   PMATH_SYMBOL_PARENTHESIZEBOXES,           builtin_parenthesizeboxes)
  BIND_DOWN(   PMATH_SYMBOL_PART,                        builtin_part)
  BIND_DOWN(   PMATH_SYMBOL_PARTITION,                   builtin_partition)
  BIND_DOWN(   PMATH_SYMBOL_PAUSE,                       builtin_pause)
  BIND_DOWN(   PMATH_SYMBOL_PIECEWISE,                   builtin_piecewise)
  BIND_DOWN(   PMATH_SYMBOL_POLYGAMMA,                   builtin_polygamma)
  BIND_DOWN(   PMATH_SYMBOL_POSITION,                    builtin_position)
  BIND_DOWN(   PMATH_SYMBOL_POSTDECREMENT,               builtin_dec_or_inc_or_postdec_or_postinc)
  BIND_DOWN(   PMATH_SYMBOL_POSTINCREMENT,               builtin_dec_or_inc_or_postdec_or_postinc)
  BIND_DOWN(   PMATH_SYMBOL_POWERMOD,                    builtin_powermod)
  BIND_DOWN(   PMATH_SYMBOL_PRECISION,                   builtin_precision)
  BIND_DOWN(   PMATH_SYMBOL_PREPEND,                     builtin_prepend)
  BIND_DOWN(   PMATH_SYMBOL_PRINT,                       builtin_print)
  BIND_DOWN(   PMATH_SYMBOL_PRODUCT,                     builtin_product)
  BIND_DOWN(   PMATH_SYMBOL_PROTECT,                     builtin_protect_or_unprotect)
  BIND_DOWN(   PMATH_SYMBOL_QUANTILE,                    builtin_quantile)
  BIND_DOWN(   PMATH_SYMBOL_QUOTIENT,                    builtin_quotient)
  BIND_DOWN(   PMATH_SYMBOL_RANDOMINTEGER,               builtin_randominteger)
  BIND_DOWN(   PMATH_SYMBOL_RANDOMREAL,                  builtin_randomreal)
  BIND_DOWN(   PMATH_SYMBOL_RANGE,                       builtin_range)
  BIND_DOWN(   PMATH_SYMBOL_RE,                          builtin_re)
  BIND_DOWN(   PMATH_SYMBOL_READ,                        builtin_read)
  BIND_DOWN(   PMATH_SYMBOL_READLIST,                    builtin_readlist)
  BIND_DOWN(   PMATH_SYMBOL_REFRESH,                     builtin_refresh)
  BIND_DOWN(   PMATH_SYMBOL_REGATHER,                    builtin_regather)
  BIND_DOWN(   PMATH_SYMBOL_RELEASEHOLD,                 builtin_releasehold)
  BIND_DOWN(   PMATH_SYMBOL_REMOVE,                      builtin_remove)
  BIND_DOWN(   PMATH_SYMBOL_RENAMEDIRECTORY,             builtin_renamedirectory_and_renamefile)
  BIND_DOWN(   PMATH_SYMBOL_RENAMEFILE,                  builtin_renamedirectory_and_renamefile)
  BIND_DOWN(   PMATH_SYMBOL_REPLACE,                     builtin_replace)
  BIND_DOWN(   PMATH_SYMBOL_REPLACELIST,                 builtin_replacelist)
  BIND_DOWN(   PMATH_SYMBOL_REPLACEPART,                 builtin_replacepart)
  BIND_DOWN(   PMATH_SYMBOL_REPLACEREPEATED,             builtin_replace)
  BIND_DOWN(   PMATH_SYMBOL_RESCALE,                     builtin_rescale)
  BIND_DOWN(   PMATH_SYMBOL_RESETDIRECTORY,              builtin_resetdirectory)
  BIND_DOWN(   PMATH_SYMBOL_REST,                        builtin_rest)
  BIND_DOWN(   PMATH_SYMBOL_RETURN,                      general_builtin_zerotwoarg)
  BIND_DOWN(   PMATH_SYMBOL_REVERSE,                     builtin_reverse)
  BIND_DOWN(   PMATH_SYMBOL_RIFFLE,                      builtin_riffle)
  BIND_DOWN(   PMATH_SYMBOL_ROUND,                       builtin_round_functions)
  BIND_DOWN(   PMATH_SYMBOL_SCAN,                        builtin_scan)
  BIND_DOWN(   PMATH_SYMBOL_SECTIONPRINT,                builtin_sectionprint)
  BIND_DOWN(   PMATH_SYMBOL_SELECT,                      builtin_select)
  BIND_DOWN(   PMATH_SYMBOL_SELECTEDDOCUMENT,            general_builtin_nofront);
  BIND_DOWN(   PMATH_SYMBOL_SETATTRIBUTES,               builtin_setattributes)
  BIND_DOWN(   PMATH_SYMBOL_SETDIRECTORY,                builtin_setdirectory)
  BIND_DOWN(   PMATH_SYMBOL_SETOPTIONS,                  builtin_setoptions)
  BIND_DOWN(   PMATH_SYMBOL_SETPRECISION,                builtin_setprecision)
  BIND_DOWN(   PMATH_SYMBOL_SETSTREAMPOSITION,           builtin_setstreamposition)
  BIND_DOWN(   PMATH_SYMBOL_SHOWDEFINITION,              builtin_showdefinition)
  BIND_DOWN(   PMATH_SYMBOL_SIGN,                        builtin_sign)
  BIND_DOWN(   PMATH_SYMBOL_SIN,                         builtin_sin)
  BIND_DOWN(   PMATH_SYMBOL_SINH,                        builtin_sinh)
  BIND_DOWN(   PMATH_SYMBOL_SINGLEMATCH,                 general_builtin_zeroonearg)
  BIND_DOWN(   PMATH_SYMBOL_SORT,                        builtin_sort)
  BIND_DOWN(   PMATH_SYMBOL_SORTBY,                      builtin_sortby)
  BIND_DOWN(   PMATH_SYMBOL_SPLIT,                       builtin_split)
  BIND_DOWN(   PMATH_SYMBOL_SQRT,                        builtin_sqrt)
  BIND_DOWN(   PMATH_SYMBOL_STACK,                       builtin_stack)
  BIND_DOWN(   PMATH_SYMBOL_STREAMPOSITION,              builtin_streamposition)
  BIND_DOWN(   PMATH_SYMBOL_STRINGCASES,                 builtin_stringcases)
  BIND_DOWN(   PMATH_SYMBOL_STRINGCOUNT,                 builtin_stringcount)
  BIND_DOWN(   PMATH_SYMBOL_STRINGDROP,                  builtin_stringdrop)
  BIND_DOWN(   PMATH_SYMBOL_STRINGEXPRESSION,            builtin_stringexpression)
  BIND_DOWN(   PMATH_SYMBOL_STRINGMATCH,                 builtin_stringmatch)
  BIND_DOWN(   PMATH_SYMBOL_STRINGPOSITION,              builtin_stringposition)
  BIND_DOWN(   PMATH_SYMBOL_STRINGREPLACE,               builtin_stringreplace)
  BIND_DOWN(   PMATH_SYMBOL_STRINGSPLIT,                 builtin_stringsplit)
  BIND_DOWN(   PMATH_SYMBOL_STRINGTAKE,                  builtin_stringtake)
  BIND_DOWN(   PMATH_SYMBOL_STRINGTOBOXES,               builtin_stringtoboxes)
  BIND_DOWN(   PMATH_SYMBOL_SUM,                         builtin_sum)
  BIND_DOWN(   PMATH_SYMBOL_SWITCH,                      builtin_switch)
  BIND_DOWN(   PMATH_SYMBOL_SUBRULES,                    builtin_symbol_rules)
  BIND_DOWN(   PMATH_SYMBOL_SYMBOLNAME,                  builtin_symbolname)
  BIND_DOWN(   PMATH_SYMBOL_SYNCHRONIZE,                 builtin_synchronize)
  BIND_DOWN(   PMATH_SYMBOL_SYNTAXINFORMATION,           builtin_syntaxinformation)
  BIND_DOWN(   PMATH_SYMBOL_TABLE,                       builtin_table)
  BIND_DOWN(   PMATH_SYMBOL_TAGASSIGN,                   builtin_tagassign)
  BIND_DOWN(   PMATH_SYMBOL_TAGASSIGNDELAYED,            builtin_tagassign)
  BIND_DOWN(   PMATH_SYMBOL_TAGUNASSIGN,                 builtin_tagunassign)
  BIND_DOWN(   PMATH_SYMBOL_TAKE,                        builtin_take)
  BIND_DOWN(   PMATH_SYMBOL_TAKEWHILE,                   builtin_takewhile)
  BIND_DOWN(   PMATH_SYMBOL_TAN,                         builtin_tan)
  BIND_DOWN(   PMATH_SYMBOL_TANH,                        builtin_tanh)
  BIND_DOWN(   PMATH_SYMBOL_THREAD,                      builtin_thread)
  BIND_DOWN(   PMATH_SYMBOL_THROUGH,                     builtin_through)
  BIND_DOWN(   PMATH_SYMBOL_THROW,                       builtin_throw)
  BIND_DOWN(   PMATH_SYMBOL_TIMECONSTRAINED,             builtin_timeconstrained)
  BIND_DOWN(   PMATH_SYMBOL_TIMING,                      builtin_timing)
  BIND_DOWN(   PMATH_SYMBOL_TIMESBY,                     builtin_divideby_or_timesby)
  BIND_DOWN(   PMATH_SYMBOL_TOBOXES,                     builtin_toboxes)
  BIND_DOWN(   PMATH_SYMBOL_TOCHARACTERCODE,             builtin_tocharactercode)
  BIND_DOWN(   PMATH_SYMBOL_TOEXPRESSION,                builtin_toexpression)
  BIND_DOWN(   PMATH_SYMBOL_TOFILENAME,                  builtin_tofilename)
  BIND_DOWN(   PMATH_SYMBOL_TOSTRING,                    builtin_tostring)
  BIND_DOWN(   PMATH_SYMBOL_TOTAL,                       builtin_total)
  BIND_DOWN(   PMATH_SYMBOL_TRY,                         builtin_try)
  BIND_DOWN(   PMATH_SYMBOL_UNASSIGN,                    builtin_unassign)
  BIND_DOWN(   PMATH_SYMBOL_UNCOMPRESS,                  builtin_uncompress)
  BIND_DOWN(   PMATH_SYMBOL_UNCOMPRESSSTREAM,            builtin_uncompressstream)
  BIND_DOWN(   PMATH_SYMBOL_UNDERFLOW,                   general_builtin_zeroargs)
  BIND_DOWN(   PMATH_SYMBOL_UNEQUAL,                     builtin_unequal)
  BIND_DOWN(   PMATH_SYMBOL_UNIDENTICAL,                 builtin_unidentical)
  BIND_DOWN(   PMATH_SYMBOL_UNION,                       builtin_union)
  BIND_DOWN(   PMATH_SYMBOL_UNITVECTOR,                  builtin_unitvector)
  BIND_DOWN(   PMATH_SYMBOL_UNPROTECT,                   builtin_protect_or_unprotect)
  BIND_DOWN(   PMATH_SYMBOL_UPDATE,                      builtin_update)
  BIND_DOWN(   PMATH_SYMBOL_UPRULES,                     builtin_symbol_rules)
  BIND_DOWN(   PMATH_SYMBOL_WAIT,                        builtin_wait)
  BIND_DOWN(   PMATH_SYMBOL_WHICH,                       builtin_which)
  BIND_DOWN(   PMATH_SYMBOL_WHILE,                       builtin_while)
  BIND_DOWN(   PMATH_SYMBOL_WITH,                        builtin_with)
  BIND_DOWN(   PMATH_SYMBOL_WRITE,                       builtin_write)
  BIND_DOWN(   PMATH_SYMBOL_WRITESTRING,                 builtin_writestring)
  BIND_DOWN(   PMATH_SYMBOL_XOR,                         builtin_xor)
  
  BIND_APPROX( PMATH_SYMBOL_E,                 builtin_approximate_e);
  BIND_APPROX( PMATH_SYMBOL_EULERGAMMA,        builtin_approximate_eulergamma);
  BIND_APPROX( PMATH_SYMBOL_MACHINEPRECISION,  builtin_approximate_machineprecision);
  BIND_APPROX( PMATH_SYMBOL_PI,                builtin_approximate_pi);
  BIND_APPROX( PMATH_SYMBOL_POWER,             builtin_approximate_power);
  
#undef BIND
#undef BIND_APPROX
#undef BIND_EARLY
#undef BIND_DOWN
#undef BIND_SUB
#undef BIND_UP
  //} ... binding C functions
  
  //{ setting attributes (except the Protected attribute) ...
#define SET_ATTRIB(sym,attrib) pmath_symbol_set_attributes((sym), (attrib))
  
#define NHOLDALL              PMATH_SYMBOL_ATTRIBUTE_NHOLDALL
#define NHOLDFIRST            PMATH_SYMBOL_ATTRIBUTE_NHOLDFIRST
#define NHOLDREST             PMATH_SYMBOL_ATTRIBUTE_NHOLDREST
#define ASSOCIATIVE           PMATH_SYMBOL_ATTRIBUTE_ASSOCIATIVE
#define DEEPHOLDALL           PMATH_SYMBOL_ATTRIBUTE_DEEPHOLDALL
#define DEFINITEFUNCTION      PMATH_SYMBOL_ATTRIBUTE_DEFINITEFUNCTION
#define HOLDALL               PMATH_SYMBOL_ATTRIBUTE_HOLDALL
#define HOLDALLCOMPLETE       PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE
#define HOLDFIRST             PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST
#define HOLDREST              PMATH_SYMBOL_ATTRIBUTE_HOLDREST
#define LISTABLE              PMATH_SYMBOL_ATTRIBUTE_LISTABLE
#define NUMERICFUNCTION       PMATH_SYMBOL_ATTRIBUTE_NUMERICFUNCTION
#define ONEIDENTITY           PMATH_SYMBOL_ATTRIBUTE_ONEIDENTITY
#define READPROTECTED         PMATH_SYMBOL_ATTRIBUTE_READPROTECTED
#define SEQUENCEHOLD          PMATH_SYMBOL_ATTRIBUTE_SEQUENCEHOLD
#define SYMMETRIC             PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC
#define THREADLOCAL           PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL
  
  SET_ATTRIB( PMATH_SYMBOL_INTERNAL_ABORTMESSAGE,       HOLDALLCOMPLETE);
  SET_ATTRIB( PMATH_SYMBOL_INTERNAL_CONDITION,          HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_INTERNAL_CRITICALMESSAGETAG, HOLDALL | THREADLOCAL);
  
  SET_ATTRIB( PMATH_SYMBOL_UTILITIES_GETSYSTEMSYNTAXINFORMATION,  HOLDALL);
  
  SET_ATTRIB( PMATH_SYMBOL_BOXFORM_USETEXTFORMATTING, THREADLOCAL);
  
  SET_ATTRIB( PMATH_SYMBOL_DEVELOPER_GETDEBUGINFO,   HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_DEVELOPER_HASBUILTINCODE, HOLDFIRST);
  
  SET_ATTRIB( PMATH_SYMBOL_ABORT,                            LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_ABS,                              DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_AND,                              ASSOCIATIVE | DEFINITEFUNCTION | HOLDALL | ONEIDENTITY);
  SET_ATTRIB( PMATH_SYMBOL_NRULES,                           HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_ARCCOS,                           DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_ARCCOSH,                          DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_ARCCOT,                           DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_ARCCOTH,                          DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_ARCCSC,                           DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_ARCCSCH,                          DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_ARCSEC,                           DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_ARCSECH,                          DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_ARCSIN,                           DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_ARCSINH,                          DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_ARCTAN,                           DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_ARG,                              DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_ARCTANH,                          DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_ASSIGN,                           HOLDFIRST | SEQUENCEHOLD);
  SET_ATTRIB( PMATH_SYMBOL_ASSIGNDELAYED,                    HOLDALL | SEQUENCEHOLD);
  SET_ATTRIB( PMATH_SYMBOL_ATTRIBUTES,                       HOLDFIRST | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_BERNOULLIB,                       DEFINITEFUNCTION | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_BINOMIAL,                         DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_BITAND,                           ASSOCIATIVE | DEFINITEFUNCTION | LISTABLE | ONEIDENTITY);
  SET_ATTRIB( PMATH_SYMBOL_BITCLEAR,                         DEFINITEFUNCTION | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_BITGET,                           DEFINITEFUNCTION | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_BITLENGTH,                        DEFINITEFUNCTION | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_BITNOT,                           DEFINITEFUNCTION | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_BITOR,                            ASSOCIATIVE | DEFINITEFUNCTION | LISTABLE | ONEIDENTITY);
  SET_ATTRIB( PMATH_SYMBOL_BITSET,                           DEFINITEFUNCTION | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_BITSHIFTLEFT,                     DEFINITEFUNCTION | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_BITSHIFTRIGHT,                    DEFINITEFUNCTION | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_BITXOR,                           ASSOCIATIVE | DEFINITEFUNCTION | LISTABLE | ONEIDENTITY);
  SET_ATTRIB( PMATH_SYMBOL_BLOCK,                            HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_BOOLE,                            DEFINITEFUNCTION | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_MAKEEXPRESSION,                   READPROTECTED);
  SET_ATTRIB( PMATH_SYMBOL_BUTTON,                           HOLDREST);
  SET_ATTRIB( PMATH_SYMBOL_CATCH,                            HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_CEILING,                          DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_CHARACTERS,                       LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_CLEAR,                            HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_CLEARALL,                         HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_CLEARATTRIBUTES,                  HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_CLIP,                             LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_CLOSE,                            LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_COMPLEX,                          DEFINITEFUNCTION | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_CONDITION,                        HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_CONJUGATE,                        DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_COS,                              DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_CONTROLACTIVE,                    HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_COSH,                             DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_COT,                              DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_COTH,                             DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_CSC,                              DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_CSCH,                             DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_CURRENTNAMESPACE,                 THREADLOCAL);
  SET_ATTRIB( PMATH_SYMBOL_DECREMENT,                        HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_DEFAULTRULES,                     HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_DEGREE,                           READPROTECTED);
  SET_ATTRIB( PMATH_SYMBOL_DIALOG,                           HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_DIRECTORYSTACK,                   THREADLOCAL);
  SET_ATTRIB( PMATH_SYMBOL_DIVIDEBY,                         HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_DO,                               HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_DOWNRULES,                        HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_DOT,                              ASSOCIATIVE | ONEIDENTITY);
  SET_ATTRIB( PMATH_SYMBOL_DYNAMIC,                          HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_DYNAMICBOX,                       HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_DYNAMICLOCAL,                     HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_DYNAMICLOCALBOX,                  HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_EVALUATEDELAYED,                  HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_EVALUATIONSEQUENCE,               HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_EXP,                              DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_EXTENDEDGCD,                      DEFINITEFUNCTION | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_EXTRACT,                          NHOLDREST);
  SET_ATTRIB( PMATH_SYMBOL_FINALLY,                          HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_FACTORIAL,                        DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_FACTORIAL2,                       DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_FLOOR,                            DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_FOR,                              HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_FORMATRULES,                      HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_FRACTIONALPART,                   DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_FUNCTION,                         HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_GAMMA,                            DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_GATHER,                           HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_GCD,                              ASSOCIATIVE | DEFINITEFUNCTION | LISTABLE | SYMMETRIC);
  SET_ATTRIB( PMATH_SYMBOL_GRAPHICSBOX,                      HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_HOLD,                             HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_HOLDCOMPLETE,                     HOLDALLCOMPLETE);
  SET_ATTRIB( PMATH_SYMBOL_HOLDFORM,                         HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_HOLDPATTERN,                      HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_IF,                               HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_IM,                               DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_INCREMENT,                        HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_INPUT,                            THREADLOCAL);
  SET_ATTRIB( PMATH_SYMBOL_INTEGERPART,                      DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATE,         HOLDALLCOMPLETE);
  SET_ATTRIB( PMATH_SYMBOL_INTERNAL_DYNAMICEVALUATEMULTIPLE, HOLDALLCOMPLETE);
  SET_ATTRIB( PMATH_SYMBOL_INTERNAL_NAMESPACEPATHSTACK,      THREADLOCAL);
  SET_ATTRIB( PMATH_SYMBOL_INTERNAL_NAMESPACESTACK,          THREADLOCAL);
  SET_ATTRIB( PMATH_SYMBOL_INTERPRETATIONBOX,                HOLDALLCOMPLETE);
  SET_ATTRIB( PMATH_SYMBOL_INTERSECTION,                     ASSOCIATIVE | ONEIDENTITY | SYMMETRIC);
  SET_ATTRIB( PMATH_SYMBOL_ISHELD,                           DEEPHOLDALL | HOLDALLCOMPLETE);
  SET_ATTRIB( PMATH_SYMBOL_ISEVEN,                           LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_ISODD,                            LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_ISPRIME,                          LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_JACOBISYMBOL,                     DEFINITEFUNCTION | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_KRONECKERSYMBOL,                  DEFINITEFUNCTION | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_LCM,                              ASSOCIATIVE | DEFINITEFUNCTION | LISTABLE | SYMMETRIC);
  SET_ATTRIB( PMATH_SYMBOL_LINEBOX,                          HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_LOCAL,                            HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_LOG,                              DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_LOGGAMMA,                         DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_MAKEBOXES,                        HOLDALLCOMPLETE);
  SET_ATTRIB( PMATH_SYMBOL_MAX,                              ASSOCIATIVE | DEFINITEFUNCTION | NUMERICFUNCTION | ONEIDENTITY | SYMMETRIC);
  SET_ATTRIB( PMATH_SYMBOL_MAXEXTRAPRECISION,                THREADLOCAL);
  SET_ATTRIB( PMATH_SYMBOL_MESSAGE,                          HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_MESSAGECOUNT,                     HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_MESSAGENAME,                      HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_MESSAGES,                         HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_MIN,                              ASSOCIATIVE | DEFINITEFUNCTION | NUMERICFUNCTION | ONEIDENTITY | SYMMETRIC);
  SET_ATTRIB( PMATH_SYMBOL_MOD,                              DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_NAMESPACE,                        HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_NAMESPACEPATH,                    THREADLOCAL);
  SET_ATTRIB( PMATH_SYMBOL_NCACHE,                           HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_NEWTASK,                          HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_NEXTPRIME,                        LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_OFF,                              HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_ON,                               HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_OR,                               ASSOCIATIVE | DEFINITEFUNCTION | HOLDALL | ONEIDENTITY);
  SET_ATTRIB( PMATH_SYMBOL_OVERFLOW,                         NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_OWNRULES,                         HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_PART,                             NHOLDREST);
  SET_ATTRIB( PMATH_SYMBOL_PATTERN,                          HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_PATTERNSEQUENCE,                  ASSOCIATIVE | ONEIDENTITY);
  SET_ATTRIB( PMATH_SYMBOL_PIECEWISE,                        HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_PLUS,                             ASSOCIATIVE | DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION | ONEIDENTITY | SYMMETRIC);
  SET_ATTRIB( PMATH_SYMBOL_POINTBOX,                         HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_POLYGAMMA,                        DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_POSTDECREMENT,                    HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_POSTINCREMENT,                    HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_POWER,                            DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION | ONEIDENTITY);
  SET_ATTRIB( PMATH_SYMBOL_POWERMOD,                         DEFINITEFUNCTION | LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_PRODUCT,                          HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_PROTECT,                          HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_PUREARGUMENT,                     NHOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_QUOTIENT,                         DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_RE,                               DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_REFRESH,                          HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_REMOVE,                           HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_RESCALE,                          NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_RETURN,                           HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_ROUND,                            DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_RULE,                             SEQUENCEHOLD);
  SET_ATTRIB( PMATH_SYMBOL_RULEDELAYED,                      HOLDREST | SEQUENCEHOLD);
  SET_ATTRIB( PMATH_SYMBOL_SEC,                              DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_SECH,                             DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_SEQUENCE,                         ASSOCIATIVE | ONEIDENTITY);
  SET_ATTRIB( PMATH_SYMBOL_SHOWDEFINITION,                   HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_SETATTRIBUTES,                    HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_SIGN,                             DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_SIN,                              DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_SINH,                             DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_SQRT,                             DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_STRINGEXPRESSION,                 ASSOCIATIVE | ONEIDENTITY);
  SET_ATTRIB( PMATH_SYMBOL_SUBRULES,                         HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_SUM,                              HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_SWITCH,                           HOLDREST);
  SET_ATTRIB( PMATH_SYMBOL_SYMBOLNAME,                       HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_SYNCHRONIZE,                      HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_SYNTAXINFORMATION,                HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_SYSTEMEXCEPTION,                  HOLDALLCOMPLETE);
  SET_ATTRIB( PMATH_SYMBOL_TABLE,                            HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_TAGASSIGN,                        HOLDALL | SEQUENCEHOLD);
  SET_ATTRIB( PMATH_SYMBOL_TAGASSIGNDELAYED,                 HOLDALL | SEQUENCEHOLD);
  SET_ATTRIB( PMATH_SYMBOL_TAGUNASSIGN,                      HOLDALL | SEQUENCEHOLD);
  SET_ATTRIB( PMATH_SYMBOL_TAN,                              DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_TANH,                             DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_TESTPATTERN,                      HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_TIMECONSTRAINED,                  HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_TIMES,                            ASSOCIATIVE | DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION | ONEIDENTITY | SYMMETRIC);
  SET_ATTRIB( PMATH_SYMBOL_TIMESBY,                          HOLDFIRST);
  SET_ATTRIB( PMATH_SYMBOL_TIMING,                           HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_TOBOXES,                          SEQUENCEHOLD);
  SET_ATTRIB( PMATH_SYMBOL_TRY,                              HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_UNASSIGN,                         HOLDFIRST | SEQUENCEHOLD);
  SET_ATTRIB( PMATH_SYMBOL_UNDERFLOW,                        NUMERICFUNCTION);
  SET_ATTRIB( PMATH_SYMBOL_UNEVALUATED,                      HOLDALLCOMPLETE);
  SET_ATTRIB( PMATH_SYMBOL_UNION,                            ASSOCIATIVE | ONEIDENTITY | SYMMETRIC);
  SET_ATTRIB( PMATH_SYMBOL_UNPROTECT,                        HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_UPRULES,                          HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_WAIT,                             LISTABLE);
  SET_ATTRIB( PMATH_SYMBOL_WHICH,                            HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_WHILE,                            HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_WITH,                             HOLDALL);
  SET_ATTRIB( PMATH_SYMBOL_XOR,                              ASSOCIATIVE | DEFINITEFUNCTION | HOLDALL | ONEIDENTITY);
  SET_ATTRIB( PMATH_SYMBOL_ZETA,                             DEFINITEFUNCTION | LISTABLE | NUMERICFUNCTION);
  
  SET_ATTRIB( PMATH_SYMBOL_INTERNAL_REALBALLBOUNDS,          LISTABLE);
  
#undef SET_ATTRIB
#undef ASSOCIATIVE
#undef DEEPHOLDALL
#undef DEFINITEFUNCTION
#undef HOLDALL
#undef HOLDALLCOMPLETE
#undef HOLDFIRST
#undef HOLDREST
#undef LISTABLE
#undef NHOLDALL
#undef NHOLDFIRST
#undef NHOLDREST
#undef NUMERICFUNCTION
#undef ONEIDENTITY
#undef SYMMETRIC
#undef THREADLOCAL
  //} ... setting attributes (except Protected attribute)
  
  return TRUE;
  
FAIL:
  for(i = 0; i < CODE_TABLES_COUNT; ++i)
    pmath_ht_destroy((pmath_hashtable_t)pmath_atomic_read_aquire(&_code_tables[i]));
    
  for(i = 0; i < PMATH_BUILTIN_SYMBOL_COUNT; ++i)
    pmath_unref(_pmath_builtin_symbol_array[i]);
    
  return FALSE;
}

PMATH_PRIVATE void _pmath_symbol_builtins_protect_all(void) {
  int i;
  
  for(i = 0; i < PMATH_BUILTIN_SYMBOL_COUNT; i++) {
    pmath_symbol_set_attributes(
      _pmath_builtin_symbol_array[i],
      PMATH_SYMBOL_ATTRIBUTE_PROTECTED
      | pmath_symbol_get_attributes(_pmath_builtin_symbol_array[i]));
  }
  
#define UNPROTECT(sym) pmath_symbol_set_attributes(sym, pmath_symbol_get_attributes(sym) & ~PMATH_SYMBOL_ATTRIBUTE_PROTECTED)
  
  UNPROTECT( PMATH_SYMBOL_CHARACTERENCODINGDEFAULT);
  UNPROTECT( PMATH_SYMBOL_CURRENTNAMESPACE);
  UNPROTECT( PMATH_SYMBOL_DIALOGLEVEL);
  UNPROTECT( PMATH_SYMBOL_HISTORY);
  UNPROTECT( PMATH_SYMBOL_HISTORYLENGTH);
  UNPROTECT( PMATH_SYMBOL_INTERNAL_CRITICALMESSAGETAG);
  UNPROTECT( PMATH_SYMBOL_INTERNAL_NAMESPACEPATHSTACK);
  UNPROTECT( PMATH_SYMBOL_INTERNAL_NAMESPACESTACK);
  UNPROTECT( PMATH_SYMBOL_LINE);
  UNPROTECT( PMATH_SYMBOL_MAXEXTRAPRECISION);
  UNPROTECT( PMATH_SYMBOL_MESSAGECOUNT);
  UNPROTECT( PMATH_SYMBOL_NAMESPACEPATH);
  UNPROTECT( PMATH_SYMBOL_NEWMESSAGE);
  UNPROTECT( PMATH_SYMBOL_NEWSYMBOL);
  UNPROTECT( PMATH_SYMBOL_PAGEWIDTHDEFAULT);
  UNPROTECT( PMATH_SYMBOL_PATHDEFAULT);
  UNPROTECT( PMATH_SYMBOL_SUBSCRIPT);
  UNPROTECT( PMATH_SYMBOL_SUBSUPERSCRIPT);
  UNPROTECT( PMATH_SYMBOL_SUPERSCRIPT);
  UNPROTECT( PMATH_SYMBOL_SYSTEMCHARACTERENCODING);
  
  UNPROTECT( PMATH_SYMBOL_BOXFORM_USETEXTFORMATTING);
  
#undef UNPROTECT
}

PMATH_PRIVATE void _pmath_symbol_builtins_done(void) {
  int i;
  
  for(i = 0; i < CODE_TABLES_COUNT; ++i)
    pmath_ht_destroy((pmath_hashtable_t)pmath_atomic_read_aquire(&_code_tables[i]));
    
  for(i = 0; i < PMATH_BUILTIN_SYMBOL_COUNT; ++i)
    pmath_unref(_pmath_builtin_symbol_array[i]);
}
